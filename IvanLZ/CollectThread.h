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

//�������ֹ���ģʽ��ѹ�����̱���ֿ飬������ֿ飬ֱ���õ��߳�Encodeȥ
//��ѡģʽ��ѡ�ֿ��С������ֻҪ�Ƕ��߳�ģʽ��һ���Ƿֿ�ġ�
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

    virtual void Thread_Job() {//���̵߳�main���� ��Ҫtry catch
        try {        

            //�����ļ��������ļ�����֮�������̶����С�Ļ�������ֱ������Ϊֹ��Ȼ�����˾ͷ��͸��̳߳��е��߳�
            //Ȼ�������»���������������ֱ�����������ļ�
            unsigned char* data_pool = new unsigned char[m_block_size];
            size_t volume = 0;//��ʼ״̬������ǿյ�
            for (size_t i = 0; i < FileVectorInBFSOrder.size(); ++i) {
                FileNode* file = FileVectorInBFSOrder[i];
                if (file->isCatalog == false && file->size > 0) {//��Ŀ¼�ļ��Ϳ��ļ�ֱ������
                    infile.open(file->Full_Path, ifstream::binary);
                    while (true) {                     
                        size_t need_num = m_block_size - volume;//ÿ�ζ��ǳ��԰ѻ�����������
                        infile.read((char*)(data_pool + volume), need_num);
                        size_t actual_num = infile.gcount();
                        assert(actual_num >= 0&&actual_num<=need_num);
                        if (actual_num < need_num) {//�ļ�̫Сû�ж���������
                            //˵���ļ��Ѿ���ͷ��
                            volume += actual_num;
                            break;
                        }
                        else  {//�ļ���δ���,���һ��������� ��Ҫ���ߣ��ַ��������̣߳�Ȼ�����������ڴ�
                            BlockBuff data_block;
                            data_block.pbuff = data_pool;
                            data_block.size = m_block_size;
                            int workerId = FindAWorker();
                            outChans[workerId]->push(data_block);//���� 
                            g_read_order_record.push(workerId);//��¼˳��
                            //���·��仺���� ��ճ���
                            volume = 0;
                            data_pool= new unsigned char[m_block_size];
                            //���Ŷ� 
                            //continue;
                        }
                    }//end while(true)
                   //ѭ���˳���˵����һ���ļ��Ѿ�������
                    infile.close();
                }
            }
            //�����ļ��Ѿ����꣬���ǻ��п��ܻ��������һ�ζ�ȡʱû��������ʣ������
            //������»���������û��ʣ��ı߽���
            if (volume > 0) {
                //����voluem�ֽڵ�����
                BlockBuff data_block;
                data_block.pbuff = data_pool;
                data_block.size = volume;//ע�������С��volue
                int workerId = FindAWorker();
                outChans[workerId]->push(data_block);//���� 
                g_read_order_record.push(workerId);//��¼˳��
                //��ճ���
                volume = 0;
            }

            //����:  ������й����߳̾��Դ��������ݺ�����˳�����������һ���̣߳�����ͨ���Ѿ��ر�
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
            g_runable.store(false);//���ӹر�
           // cout << "���߳��˳�\n";

        }
        catch (std::exception& e) {
            std::cerr << e.what();
            infile.close();
        }
        catch (...) {//���Լ���SEH
            std::cout << "unknow exception in ReadThread()\n";
        }
        infile.close();
    }
private:
    std::ifstream infile;
    std::string file_name;
    std::vector < LockFreeLinkedQueue<BlockBuff>* > outChans;
};
//��ѹ���ܷ���ȡ����ѹ���ļ�һ��ʼ�ǲ��ǲ��зֿ�ģ���������û�зֿ飬�Ǹ������޷��ò��н�ѹ����
//�����û����ܲ�֪��һ���ļ��Ƕ��߳�ѹ�������ǵ��߳�ѹ���ģ������û�ѡ�˶��߳̽�ѹ��ģʽ�������������п��ܵ�
// �ڿ����ļ�ǰ�ǲ�֪���ܷ��ö��̼߳��ٵģ����Ǽ�һ��Ԥ����ģ�飬�Զ��жϣ�����Ͳ����ˡ�������
//ֱ���ڱ��߳̽�����ѹ
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

    virtual void Thread_Job() { //�����ļ���,��������ȡ������ ��Ҫ����ģʽ���M���������
        try {
            infile.open(m_packgePath, std::ifstream::binary);
            if(!infile)throw std::logic_error("\nNo such file" + m_packgePath + "\n");
            //��ȡ�ļ�ͷ
            HEADER header;
            infile.read(header.LABLE, 4);//4�ֽڵı�ǩ
            if (string(header.LABLE, header.LABLE + 4) != "IVAN") {
                infile.close();
                throw std::logic_error("�Ƿ����ļ���ʽ,���Ǳ����֧�ֵ�ѹ���ļ�\n");
                return;
            }
            infile.read((char*)&header.M, sizeof(uint8_t));//1�ֽڵ�ģʽλ
            infile.read((char*)&header.FileSection_offset, sizeof(uint32_t));//4
            infile.read((char*)&header.StrSection_offset, sizeof(uint32_t));//4
            infile.read((char*)&header.DataSection_offset, sizeof(uint32_t));//4
            infile.read((char*)&header.padding, sizeof(uint8_t) * 15);//15�ֽڵ����

            g_M = header.M;//����ȫ��ģʽ��
            //��ԭ�ļ���:

             //��ȡ�ļ�����
            infile.seekg(header.FileSection_offset, infile.beg);
            uint32_t FileSeciotn_size = 0;
            infile.read((char*)&FileSeciotn_size, sizeof(uint32_t));//4�ֽ��ļ���������
            unsigned char* FileSection_buffer = new unsigned char[FileSeciotn_size];
            memset(FileSection_buffer, 0, FileSeciotn_size);
            infile.read((char*)FileSection_buffer, FileSeciotn_size);
            //��ȡ�ִ�����
            infile.seekg(header.StrSection_offset, infile.beg);
            uint32_t StrSection_size = 0;
            infile.read((char*)&StrSection_size, sizeof(uint32_t));//4�ֽڽ�������
            unsigned char* StrSeciton_buffer = new unsigned char[StrSection_size];
            memset(StrSeciton_buffer, 0, StrSection_size);
            infile.read((char*)StrSeciton_buffer, StrSection_size);
            //ͨ���������������л��ļ���
            DecodeTree(FileSection_buffer, FileSeciotn_size, StrSeciton_buffer, m_outPath);
            delete[] StrSeciton_buffer;
            StrSeciton_buffer = NULL;
            delete[] FileSection_buffer;
            FileSection_buffer = NULL;

            vector<outputUint> outvec;
            uint64_t Nparts = 0;
            uint64_t blockSize = 0;
            infile.seekg(header.DataSection_offset, infile.beg);//��ָ���Ƶ���ȷλ��
            infile.read((char*)&Nparts, sizeof(uint64_t)); //��������
            infile.read((char*)&blockSize, sizeof(uint64_t));//���ݿ��С
            if (Nparts == 0) {
                finish_rate = 0.65f;
                DecodeDumpToFile(outvec);
                goto end_func;
            }


            BlockBuff temp_block;      
            if (!IF_SINGLE(header.M)) {//�ֿ�       
                {   //��BWT / �ֿ�BWT
                    for (int i = 0; i < Nparts; i++) {
                        finish_rate = (i * 1.0 / Nparts);
                        temp_block.BWT_index = 0;
                        temp_block.block_size = blockSize; //��¼��������һ���߳���ȷ����ռ�
                        if (IF_BWT(header.M)) {//��BWT
                            infile.read((char*)&temp_block.BWT_index, sizeof(uint32_t));//��ȡ4�ֽڵķ���BWTindex
                        }

                        infile.read((char*)&temp_block.size, sizeof(uint32_t));//4�ֽڷ�����С
                        temp_block.pbuff = new unsigned char[temp_block.size];//
                        infile.read((char*)temp_block.pbuff, temp_block.size);//��ȡ�˷���������
                        int workerId = FindAWorker();
                        outChans[workerId]->push(temp_block);//���� 
                        g_read_order_record.push(workerId);//��¼˳��
                    }
                }
            }                                 
            //��������ò������̣߳���Ϊ�����û�зֿ飬���ǵ�һ���̸߳���
            //�����ṹ[8:��������=1][8:���ݿ��С=ԭ�����ܳ���֮��]/[8:BwtIndex]/[8:������С][data.........]
            else if (IF_SINGLE(header.M)) {
                uint64_t BWT_index = 0;
                uint64_t ipartinLen = 0;
                size_t ipartoutLen = 0;
                unsigned char* iPart_inbuffer = NULL;
                unsigned char* iPart_outbuffer = NULL;
                assert(Nparts == 1);
                if (IF_BWT(header.M)) {

                    infile.read((char*)&BWT_index, sizeof(uint64_t));//��ȡ8�ֽڵķ���bwt index
                }
                infile.read((char*)&ipartinLen, sizeof(uint64_t));//��ȡ8�ֽڵķ�����С
                iPart_inbuffer = new unsigned char[ipartinLen];
                infile.read((char*)iPart_inbuffer, ipartinLen);
                iPart_outbuffer = new unsigned char[blockSize];//blocksize����ԭ�����ݵ��ܳ�����
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
                goto end_func;//�ͷ��ڴ�+�رն�ָ��
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
            //������鲢�����̷߳���������Ϣ
            int flag = true;
            while (flag) {
                flag = false;
                for (int i = 0; i < concurrcy_num; ++i) { //�������е���һ��ͨ��
                    if (outChans[i]->Count() != 0)//����δ��������� �ȴ��������
                    {
                        flag = true;
                        break;
                    }
                }
            }
            g_runable.store(false); //�����˳�
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

            //ͳ���ļ��ܴ�С
            g_src_total_len = 0;
            for (int i = 0; i < FileVectorInBFSOrder.size(); ++i)
                if (FileVectorInBFSOrder[i]->isCatalog == false)
                    g_src_total_len += FileVectorInBFSOrder[i]->size;
            //������Ҫ���ٸ����ݿ�
            int need_block_num = (g_src_total_len + m_block_size - 1) / m_block_size;
            int handled_block_nums = 0;

            HEADER header;
            header.M = g_M;//��һ�б�������������
            outfile.write((char*)&header, 32);//д�ļ�ͷ
                //�����ļ�������С
            uint32_t File_seciotn_size = FileVectorInBFSOrder.size() * (1 + 8 + 4);//һ����ԪΪ13�ֽ�
            header.FileSection_offset = 32;
            //д��4�ֽ��ļ�������С
            outfile.write((char*)&File_seciotn_size, sizeof(uint32_t));
            //��ÿ���ļ���Ϣ��Ԫ���д���ļ�����
            for (int i = 0; i < FileVectorInBFSOrder.size(); ++i) {
                FileNode* file = FileVectorInBFSOrder[i];
                outfile.write((char*)&file->isCatalog, sizeof(bool));//1�ֽ�
                outfile.write((char*)&file->size, sizeof(size_t));//8�ֽ�
                outfile.write((char*)&file->parentId, sizeof(int32_t));//4�ֽ�
            }
            //д���ļ���������ֽڣ�����еĻ�
            size_t alreadyBytes = sizeof(uint32_t) + File_seciotn_size;//�ļ�������ǰ�Ѿ�д����ֽ��� ��4�ֽڵĽ�����С+13*N���ļ���Ϣ
            size_t paddingBytes = ALIGNE_N(alreadyBytes, 4) - alreadyBytes;//���ý�����4�ֽڶ���
            char blank = 0x00;
            for (int i = 0; i < paddingBytes; ++i)
                outfile.write(&blank, 1);//д��հ��ֽ�

            //�ó��ַ���������ƫ���ֶ�
            header.StrSection_offset = header.FileSection_offset + alreadyBytes + paddingBytes;

            //д���ַ�������
            uint32_t StrSectionSize = 0;//�ַ��������ĳ��� ������ʡȥ��д���ļ� ��Ϊ��ѹ����ԭ��Ҫ��������ռ�
            for (int i = 0; i < FileVectorInBFSOrder.size(); ++i) {
                StrSectionSize += FileVectorInBFSOrder[i]->filename.length() + 1;//�����˻���1�ֽڿհ����ڷָ��ַ���
            }
            outfile.write((char*)&StrSectionSize, sizeof(uint32_t));//4�ֽڽ������� 
            for (int i = 0; i < FileVectorInBFSOrder.size(); ++i) {
                FileNode* file = FileVectorInBFSOrder[i];
                outfile.write((char*)&file->filename[0], file->filename.length());
                outfile.write(&blank, 1);//д��1Byte�հ��ֽ�
            }
            alreadyBytes = sizeof(uint32_t) + StrSectionSize;//ע��+4
            paddingBytes = ALIGNE_N(alreadyBytes, 4) - alreadyBytes; //������4�ֽڶ���
            for (int i = 0; i < paddingBytes; ++i)
                outfile.write(&blank, 1);//д������ַ�

            //�ó����ݽ�����ƫ���ֶ�
            header.DataSection_offset = header.StrSection_offset + alreadyBytes + paddingBytes;

   
            uint64_t Nparts = 0     ;//8�ֽڷ������� ******���������ֶ���Ҫ����ƶ�ָ�����׷�Ӹ���******
            uint64_t blockSize = m_block_size;//8�ֽڴӺ����βλ�� ������ֿ����������ʾ�ܳ���(��Ӧ��������Nparts=1)
            //���̵߳�ѹ��һ���Ƿֿ�ģ�����������
        
            outfile.write((char*)&Nparts, sizeof(Nparts));//8�ֽ� �������� ��д��0 һ���ٸ���
            outfile.write((char*)&blockSize, sizeof(blockSize));//8�ֽ� ���ݿ��С



           //����һ�������߳��л�ȡ���ݣ�д���ļ�
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
                    //��������ۣ�
                    uint32_t bwt_index = 0;//�ֿ�BWT ��index��4�ֽ�
                    //cout << "��" << c++ << "�飺" << data.size << endl;
                   // debug1.push_back(data.size);
                    //sum += data.size;
                    if (IF_BWT(g_M)) {
                       bwt_index= data.BWT_index;
                       outfile.write((char*)&bwt_index, sizeof(int32_t));//4�ֽ�index
                    } 
                    uint32_t datasize = data.size;//
                    outfile.write((char*)&datasize, sizeof(uint32_t));//4�ֽڷ�����С�����ڽ�ѹ��ʱ������ʵ�out������
                    outfile.write((char*)data.pbuff, datasize);
                    Nparts++;//ע�����Ҫ���¸��ֶ�
                    delete[] (unsigned char *) data.pbuff; //�����Ѿ�д��ɹ��������ͷſռ���
                    data.pbuff = NULL;
                }
            }
          //  cout << "�ܴ�С:" << sum << endl;
            //���¸����ļ�ͷ
            outfile.seekp(4 + 1, outfile.beg);
            outfile.write((char*)&header.FileSection_offset, sizeof(uint32_t));
            outfile.write((char*)&header.StrSection_offset, sizeof(uint32_t));
            outfile.write((char*)&header.DataSection_offset, sizeof(uint32_t));
            outfile.seekp(header.DataSection_offset, outfile.beg);

            outfile.write((char*)&Nparts, sizeof(uint64_t));//����8�ֽڵķ��������ֶ�

            outfile.close();
            finish_rate = 1.0f;
            //cout << "д�߳��˳�\n";
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

        //��������һ�㣬����߳�ֱ��һ�����ռ����������ݵ�outvec��Dump,������һ���ռ�һ��Dump����Ϊ�����ļ�����̫����
        //�������ǲ���ʱ���ǵ����й����߳��˳�Ϊֹ������������̵߳�IOʱ��

        BlockBuff data;
        vector<outputUint> outvec;
        try {
            while (true) {
                if ((m_runable.load()) == false && g_read_order_record.isEmpty()) { //��ȡ��¼Ϊ�ղ��Ҷ�ȡ�߳��Ѿ��˳�(runable��־�����ж�)
                    break;//д�߳��˳�
                }
                int workerId;
                if (g_read_order_record.tryPop(workerId) == false) {
                    continue;
                }
                else {
                    // try_to_read: ��ȡ�ɽ�ѹ���̷߳��������ݵ�data 
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