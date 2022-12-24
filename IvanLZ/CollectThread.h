#pragma once
#include "Thread.h"
#include <vector>
#include <fstream>
#include <string>
#include <exception>
#include "JobThread.h"
#include "lock_free_queue.h"
#include "global_var.h"
#include <random>
#include <memory>
#include <time.h>
#include "FileModuleDef.h"

using namespace std;


class CollectThread :
    public Thread
{
public:
    CollectThread(std::atomic_bool& runable) :Thread(runable) {

    }
    virtual void Thread_Job() {
    }
protected:

};

//调用这种工作模式，压缩过程必须分块，如果不分块，直接用单线程Encode去
//先选模式再选分块大小，所以只要是多线程模式就一定是分块的。
class ReadThread_Encode :public CollectThread {
public:
    ReadThread_Encode( std::atomic_bool& runable, std::vector < LockFreeLinkedQueue<BlockBuff>* > _outChans,size_t block_size)
        :CollectThread(runable),  outChans{ _outChans },m_block_size{block_size}
    {}
    size_t m_block_size;

    int FindAWorker() {
        int min = INT_MAX;
        int index = 0;
        min = outChans[0]->Count();
        index = 0;
        for (int i = 1; i < concurrcy_num; ++i) {
            if (outChans[i]->Count() < min) {
                min = outChans[i]->Count();
                index = i;
            }
        }
        return index;
    }

    virtual void Thread_Job() {//该线程的main函数 需要try catch
        try {        

            //遍历文件树，找文件，打开之，读到固定块大小的缓冲区，直到读满为止，然后满了就发送给线程池中的线程
            //然后申请新缓冲区，继续读，直到读完所有文件
            unsigned char* data_pool = new unsigned char[m_block_size];
            size_t volume = 0;//初始状态缓冲池是空的
            for (size_t i = 0; i < FileVectorInBFSOrder.size(); ++i) {
                FileNode* file = FileVectorInBFSOrder[i];
                if (file->isCatalog == false && file->size > 0) {//非目录文件和空文件直接跳过
                    infile.open(file->Full_Path, ifstream::binary);
                    while (true) {                     
                        size_t need_num = m_block_size - volume;//每次都是尝试把缓冲区读到满
                        infile.read((char*)(data_pool + volume), need_num);
                        size_t actual_num = infile.gcount();
                        assert(actual_num >= 0&&actual_num<=need_num);
                        if (actual_num < need_num) {//文件太小没有读满缓冲区
                            //说明文件已经到头了
                            volume += actual_num;
                            break;
                        }
                        else  {//文件尚未完结,并且缓冲区满了 需要移走，分发给工作线程，然后重新申请内存
                            BlockBuff data_block;
                            data_block.pbuff = data_pool;
                            data_block.size = m_block_size;
                            int workerId = FindAWorker();
                            outChans[workerId]->push(data_block);//发送 
                            g_read_order_record.push(workerId);//记录顺序
                            //重新分配缓冲区 清空池子
                            volume = 0;
                            data_pool= new unsigned char[m_block_size];
                            //接着读 
                            //continue;
                        }
                    }//end while(true)
                   //循环退出，说明上一个文件已经读完了
                    infile.close();
                }
            }
            //所有文件已经读完，但是还有可能缓冲区最后一次读取时没有满，有剩余数据
            //检查以下缓冲区还有没有剩余的边角料
            if (volume > 0) {
                //还有voluem字节的数据
                BlockBuff data_block;
                data_block.pbuff = data_pool;
                data_block.size = volume;//注意这里，大小是volue
                int workerId = FindAWorker();
                outChans[workerId]->push(data_block);//发送 
                g_read_order_record.push(workerId);//记录顺序
                //清空池子
                volume = 0;
            }

            //结束:  检查所有工作线程均以处理完数据后可以退出，并提醒下一级线程，连接通道已经关闭
            int flag = true;
            while (flag) {
                flag = false;
                for (int i = 0; i < concurrcy_num; ++i) {
                    if (outChans[i]->Count() != 0)
                    {
                        flag = true;
                        break;
                    }
                }
            }
            g_runable.store(false);//连接关闭
           // cout << "读线程退出\n";

        }
        catch (std::exception& e) {
            std::cerr << e.what();
            infile.close();
        }
        catch (...) {//可以加上SEH
            std::cout << "unknow exception in ReadThread()\n";
        }
        infile.close();
    }
private:
    std::ifstream infile;
    std::string file_name;
    std::vector < LockFreeLinkedQueue<BlockBuff>* > outChans;
};
//解压缩能否并行取决于压缩文件一开始是不是并行分块的，如果本身就没有分块，那根本就无法用并行解压处理
//但是用户可能不知道一个文件是多线程压缩过还是单线程压缩的，假如用户选了多线程解压缩模式，这种需求是有可能的
// 在开打文件前是不知道能否用多线程加速的，除非加一个预分析模块，自动判断，这里就不加了。。。。
//直接在本线程解决完解压
class ReadThread_Decode :public CollectThread {
public:
    ReadThread_Decode(PathName packgePath, PathName outPath,std::atomic_bool& runable, std::vector < LockFreeLinkedQueue<BlockBuff>* > _outChans)
        :CollectThread(runable), m_packgePath{ packgePath }, outChans{ _outChans }, m_outPath{outPath}
    {

    }
    int FindAWorker() {
        int min = INT_MAX;
        int index = 0;
        min = outChans[0]->Count();
        index = 0;
        for (int i = 1; i < concurrcy_num; ++i) {
            if (outChans[i]->Count() < min) {
                min = outChans[i]->Count();
                index = i;
            }
        }
        return index;
    }

    virtual void Thread_Job() { //构造文件树,按分区读取，发送 需要根据模式标记M分情况讨论
        try {
            infile.open(m_packgePath, std::ifstream::binary);
            if(!infile)throw std::logic_error("\nNo such file" + m_packgePath + "\n");
            //读取文件头
            HEADER header;
            infile.read(header.LABLE, 4);//4字节的标签
            if (string(header.LABLE, header.LABLE + 4) != "IVAN") {
                infile.close();
                throw std::logic_error("非法的文件格式,不是本软件支持的压缩文件\n");
                return;
            }
            infile.read((char*)&header.M, sizeof(uint8_t));//1字节的模式位
            infile.read((char*)&header.FileSection_offset, sizeof(uint32_t));//4
            infile.read((char*)&header.StrSection_offset, sizeof(uint32_t));//4
            infile.read((char*)&header.DataSection_offset, sizeof(uint32_t));//4
            infile.read((char*)&header.padding, sizeof(uint8_t) * 15);//15字节的填充

            g_M = header.M;//更新全局模式字
            //还原文件树:

             //获取文件节区
            infile.seekg(header.FileSection_offset, infile.beg);
            uint32_t FileSeciotn_size = 0;
            infile.read((char*)&FileSeciotn_size, sizeof(uint32_t));//4字节文件节区长度
            unsigned char* FileSection_buffer = new unsigned char[FileSeciotn_size];
            memset(FileSection_buffer, 0, FileSeciotn_size);
            infile.read((char*)FileSection_buffer, FileSeciotn_size);
            //获取字串节区
            infile.seekg(header.StrSection_offset, infile.beg);
            uint32_t StrSection_size = 0;
            infile.read((char*)&StrSection_size, sizeof(uint32_t));//4字节节区长度
            unsigned char* StrSeciton_buffer = new unsigned char[StrSection_size];
            memset(StrSeciton_buffer, 0, StrSection_size);
            infile.read((char*)StrSeciton_buffer, StrSection_size);
            //通过两个节区反序列化文件树
            DecodeTree(FileSection_buffer, FileSeciotn_size, StrSeciton_buffer, m_outPath);
            delete[] StrSeciton_buffer;
            StrSeciton_buffer = NULL;
            delete[] FileSection_buffer;
            FileSection_buffer = NULL;

            vector<outputUint> outvec;
            uint64_t Nparts = 0;
            uint64_t blockSize = 0;
            infile.seekg(header.DataSection_offset, infile.beg);//读指针移到正确位置
            infile.read((char*)&Nparts, sizeof(uint64_t)); //分区数量
            infile.read((char*)&blockSize, sizeof(uint64_t));//数据块大小
            if (Nparts == 0) {
                finish_rate = 0.65f;
                DecodeDumpToFile(outvec);
                goto end_func;
            }


            BlockBuff temp_block;      
            if (!IF_SINGLE(header.M)) {//分块       
                {   //无BWT / 分块BWT
                    for (int i = 0; i < Nparts; i++) {
                        finish_rate = (i * 1.0 / Nparts);
                        temp_block.BWT_index = 0;
                        temp_block.block_size = blockSize; //记录下来给下一级线程正确分配空间
                        if (IF_BWT(header.M)) {//有BWT
                            infile.read((char*)&temp_block.BWT_index, sizeof(uint32_t));//读取4字节的分区BWTindex
                        }

                        infile.read((char*)&temp_block.size, sizeof(uint32_t));//4字节分区大小
                        temp_block.pbuff = new unsigned char[temp_block.size];//
                        infile.read((char*)temp_block.pbuff, temp_block.size);//读取此分区的数据
                        int workerId = FindAWorker();
                        outChans[workerId]->push(temp_block);//发送 
                        g_read_order_record.push(workerId);//记录顺序
                    }
                }
            }                                 
            //下面情况用不到多线程，因为本身就没有分块，还是得一个线程干完
            //分区结构[8:分区数量=1][8:数据块大小=原数据总长度之和]/[8:BwtIndex]/[8:分区大小][data.........]
            else if (IF_SINGLE(header.M)) {
                uint64_t BWT_index = 0;
                uint64_t ipartinLen = 0;
                size_t ipartoutLen = 0;
                unsigned char* iPart_inbuffer = NULL;
                unsigned char* iPart_outbuffer = NULL;
                assert(Nparts == 1);
                if (IF_BWT(header.M)) {

                    infile.read((char*)&BWT_index, sizeof(uint64_t));//读取8字节的分区bwt index
                }
                infile.read((char*)&ipartinLen, sizeof(uint64_t));//读取8字节的分区大小
                iPart_inbuffer = new unsigned char[ipartinLen];
                infile.read((char*)iPart_inbuffer, ipartinLen);
                iPart_outbuffer = new unsigned char[blockSize];//blocksize就是原来数据的总长度了
                ipartoutLen = 0;
                finish_rate = 0.25f;
                Ivan_DeCompress(iPart_inbuffer, ipartinLen, iPart_outbuffer, &ipartoutLen);
                delete[] iPart_inbuffer;
                iPart_inbuffer = NULL;
                assert(ipartoutLen == blockSize);
                if (IF_BWT(header.M)) {
                    int64_t* aux = new int64_t[ipartoutLen + 8 * 1024];
                    finish_rate = 0.40f;
                    libsais64_unbwt(iPart_outbuffer, iPart_outbuffer, aux, ipartoutLen, NULL, BWT_index);
                    delete[] aux;
                }
                outputUint temp;
                temp.out_buffer = iPart_outbuffer;
                temp.out_len = ipartoutLen;
                outvec.push_back(temp);   
                finish_rate = 0.85f;
                DecodeDumpToFile(outvec);
                goto end_func;//释放内存+关闭读指针
            }         
        end_func:
            finish_rate = 1.0f;
            for (size_t i = 0; i < outvec.size(); ++i) {
                if (outvec[i].out_buffer != NULL) {
                    delete[] outvec[i].out_buffer;
                    outvec[i].out_buffer = NULL;
                }
            }
            infile.close();
            //结束检查并向工作线程发送提醒信息
            int flag = true;
            while (flag) {
                flag = false;
                for (int i = 0; i < concurrcy_num; ++i) { //遍历所有的下一级通道
                    if (outChans[i]->Count() != 0)//还有未处理的数据 等待处理完毕
                    {
                        flag = true;
                        break;
                    }
                }
            }
            g_runable.store(false); //允许退出
            return;

        }
        catch (std::exception& e) {        
            g_runable = false;
            infile.close();
            std::cerr << e.what();
        }
        catch (...) {
            g_runable = false;
            infile.close();
            std::cout << "unknow exception in ReadThread()\n";
        }
        g_runable = false;
        infile.close();
    }
private:
    std::ifstream infile;
    PathName m_packgePath;
    PathName m_outPath;
    std::vector < LockFreeLinkedQueue<BlockBuff>* > outChans;
};

class WriteThread_Encode :public CollectThread {
public:
    WriteThread_Encode(PathName outputPath, std::atomic_bool& _runable, std::vector < LockFreeLinkedQueue<BlockBuff>* >& _inChans,size_t blocksize)
        :CollectThread(_runable), m_outputPath{ outputPath }, inChans{ _inChans }, m_block_size{ blocksize }
    { }

    virtual void Thread_Job() {
        std::ofstream outfile;
        try {
            outfile.open(m_outputPath, std::ofstream::binary);
            if (!outfile) throw std::logic_error("create file failuer: " + m_outputPath + "\n");

            //统计文件总大小
            g_src_total_len = 0;
            for (int i = 0; i < FileVectorInBFSOrder.size(); ++i)
                if (FileVectorInBFSOrder[i]->isCatalog == false)
                    g_src_total_len += FileVectorInBFSOrder[i]->size;
            //计算需要多少个数据块
            int need_block_num = (g_src_total_len + m_block_size - 1) / m_block_size;
            int handled_block_nums = 0;

            HEADER header;
            header.M = g_M;//这一行别忘啊！！！！
            outfile.write((char*)&header, 32);//写文件头
                //计算文件节区大小
            uint32_t File_seciotn_size = FileVectorInBFSOrder.size() * (1 + 8 + 4);//一个单元为13字节
            header.FileSection_offset = 32;
            //写入4字节文件节区大小
            outfile.write((char*)&File_seciotn_size, sizeof(uint32_t));
            //将每个文件信息单元逐个写入文件节区
            for (int i = 0; i < FileVectorInBFSOrder.size(); ++i) {
                FileNode* file = FileVectorInBFSOrder[i];
                outfile.write((char*)&file->isCatalog, sizeof(bool));//1字节
                outfile.write((char*)&file->size, sizeof(size_t));//8字节
                outfile.write((char*)&file->parentId, sizeof(int32_t));//4字节
            }
            //写入文件节区填充字节，如果有的话
            size_t alreadyBytes = sizeof(uint32_t) + File_seciotn_size;//文件节区当前已经写入的字节数 即4字节的节区大小+13*N个文件信息
            size_t paddingBytes = ALIGNE_N(alreadyBytes, 4) - alreadyBytes;//将该节区向4字节对齐
            char blank = 0x00;
            for (int i = 0; i < paddingBytes; ++i)
                outfile.write(&blank, 1);//写入空白字节

            //得出字符串节区的偏移字段
            header.StrSection_offset = header.FileSection_offset + alreadyBytes + paddingBytes;

            //写入字符串节区
            uint32_t StrSectionSize = 0;//字符串节区的长度 不可以省去不写入文件 因为解压缩还原需要它来分配空间
            for (int i = 0; i < FileVectorInBFSOrder.size(); ++i) {
                StrSectionSize += FileVectorInBFSOrder[i]->filename.length() + 1;//别忘了还有1字节空白用于分隔字符串
            }
            outfile.write((char*)&StrSectionSize, sizeof(uint32_t));//4字节节区长度 
            for (int i = 0; i < FileVectorInBFSOrder.size(); ++i) {
                FileNode* file = FileVectorInBFSOrder[i];
                outfile.write((char*)&file->filename[0], file->filename.length());
                outfile.write(&blank, 1);//写入1Byte空白字节
            }
            alreadyBytes = sizeof(uint32_t) + StrSectionSize;//注意+4
            paddingBytes = ALIGNE_N(alreadyBytes, 4) - alreadyBytes; //节区向4字节对齐
            for (int i = 0; i < paddingBytes; ++i)
                outfile.write(&blank, 1);//写入填充字符

            //得出数据节区的偏移字段
            header.DataSection_offset = header.StrSection_offset + alreadyBytes + paddingBytes;

   
            uint64_t Nparts = 0     ;//8字节分区数量 ******分区数量字段需要最后移动指针进行追加更新******
            uint64_t blockSize = m_block_size;//8字节从函数形参获得 如果不分块这个参数表示总长度(对应分区数量Nparts=1)
            //多线程的压缩一定是分块的！！！！！！
        
            outfile.write((char*)&Nparts, sizeof(Nparts));//8字节 分区数量 先写入0 一会再更新
            outfile.write((char*)&blockSize, sizeof(blockSize));//8字节 数据块大小



           //从上一级工作线程中获取数据，写入文件
            int WorkerId=0;
            //int c = 0;
            //int sum = 0;
            while (1) {
                if ((m_runable.load()) == false && g_read_order_record.isEmpty()) {
                    break;
                }
                if (g_read_order_record.tryPop(WorkerId) == false) {
                    continue;
                }
                else {
                    // try_to_read:
                    handled_block_nums++;
                    finish_rate = (handled_block_nums * 1.0f / need_block_num);
                    BlockBuff data;
                    while (inChans[WorkerId]->tryPop(data) == false) {
                        continue;
                    }
                    //分情况讨论：
                    uint32_t bwt_index = 0;//分块BWT 的index是4字节
                    //cout << "第" << c++ << "块：" << data.size << endl;
                   // debug1.push_back(data.size);
                    //sum += data.size;
                    if (IF_BWT(g_M)) {
                       bwt_index= data.BWT_index;
                       outfile.write((char*)&bwt_index, sizeof(int32_t));//4字节index
                    } 
                    uint32_t datasize = data.size;//
                    outfile.write((char*)&datasize, sizeof(uint32_t));//4字节分区大小，用于解压缩时申请合适的out缓冲区
                    outfile.write((char*)data.pbuff, datasize);
                    Nparts++;//注意最后要更新该字段
                    delete[] (unsigned char *) data.pbuff; //数据已经写入成功，可以释放空间了
                    data.pbuff = NULL;
                }
            }
          //  cout << "总大小:" << sum << endl;
            //重新更新文件头
            outfile.seekp(4 + 1, outfile.beg);
            outfile.write((char*)&header.FileSection_offset, sizeof(uint32_t));
            outfile.write((char*)&header.StrSection_offset, sizeof(uint32_t));
            outfile.write((char*)&header.DataSection_offset, sizeof(uint32_t));
            outfile.seekp(header.DataSection_offset, outfile.beg);

            outfile.write((char*)&Nparts, sizeof(uint64_t));//更新8字节的分区数量字段

            outfile.close();
            finish_rate = 1.0f;
            //cout << "写线程退出\n";
        }
        catch (std::exception e) {
            std::cerr << "Write thread " << std::this_thread::get_id() << "throw an exception :";
            std::cerr << e.what();
            outfile.close();
        }
        catch (...) {
            outfile.close();
            std::cout << "unknow exception in WriteThread()\n";
        }

    }
private:
    PathName m_outputPath;
    size_t m_block_size;
    std::vector < LockFreeLinkedQueue<BlockBuff>* > inChans;
};

class WriteThread_Decode :public CollectThread {
public:
    WriteThread_Decode(std::string _filename, std::atomic_bool& _runable, std::vector < LockFreeLinkedQueue<BlockBuff>* >& _inChans)
        :CollectThread(_runable), filename{ _filename }, inChans{ _inChans }
    {
    }

    virtual void Thread_Job() {

        //这里做简单一点，这个线程直接一次性收集完所有数据到outvec再Dump,而不是一边收集一边Dump，因为后者文件操作太复杂
        //并且我们测试时间是到所有工作线程退出为止，不包括这个线程的IO时间

        BlockBuff data;
        vector<outputUint> outvec;
        try {
            while (true) {
                if ((m_runable.load()) == false && g_read_order_record.isEmpty()) { //读取记录为空并且读取线程已经退出(runable标志可以判断)
                    break;//写线程退出
                }
                int workerId;
                if (g_read_order_record.tryPop(workerId) == false) {
                    continue;
                }
                else {
                    // try_to_read: 读取由解压缩线程发来的数据到data 
                    while (inChans[workerId]->tryPop(data) == false) {
                        continue;
                    }
                    outputUint uint;
                    uint.out_buffer = (unsigned char*)data.pbuff;
                    uint.out_len = data.size;
                    outvec.push_back(uint);
                }

            }
            if (!IF_SINGLE(g_M)) {
                DecodeDumpToFile(outvec);
            }
            for (int i = 0; i < outvec.size(); ++i) {
                if (outvec[i].out_buffer != NULL)
                    delete outvec[i].out_buffer;
            }
        }
        catch (std::exception e) {
            std::cerr << "Write thread " << std::this_thread::get_id() << "throw an exception :";
            std::cerr << e.what();
           
        }
        catch (...) {
            std::cout << "unknow exception in WriteThread()\n";
        }

    }
private:
    std::string filename;

    std::vector < LockFreeLinkedQueue<BlockBuff>* > inChans;
};