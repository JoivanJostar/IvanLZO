#include "Encoder.h"
#include "FileModuleDef.h"
#include "global_var.h"
#include "cuda_lzo.h"
#include "libsais64.h"
#include "lzox64.h"
#include <fstream>
#include "JobThread.h"
#include "CollectThread.h"
#include "Console.h"
using namespace std;
/*UNUSED*/
void CudaEncode(string output_path, size_t block_size) {
    unsigned char* in = NULL;
    vector<outputUint> outvec;//压缩后的数据按照数据块的相对次序放入outvec
    //计算总共需要多大的buffer
    size_t totallen = 0;
    for (int i = 0; i < FileVectorInBFSOrder.size(); ++i) {
        if (FileVectorInBFSOrder[i]->isCatalog == false) {//非目录文件
            totallen += FileVectorInBFSOrder[i]->size;
        }
    }
    if (totallen > 0) {
        in = new unsigned char[totallen];
        std::memset(in, 0, totallen * sizeof(unsigned char));

        //读取所有可压缩文件到buffer
        unsigned char* ii = in;
        for (int i = 0; i < FileVectorInBFSOrder.size(); ++i) {
            if (FileVectorInBFSOrder[i]->isCatalog == false) {//非目录文件
               // ifstream infile(tree.RootPath + FileVectorInBFSOrder[i]->filename);
                ifstream infile(FileVectorInBFSOrder[i]->Full_Path, ifstream::binary);
                if (!infile) {
                    assert(0);
                }
                infile.read((char*)ii, FileVectorInBFSOrder[i]->size);
                ii += FileVectorInBFSOrder[i]->size;
                assert(infile.gcount() == FileVectorInBFSOrder[i]->size);
                infile.close();
            }
        }
        assert(totallen == (ii - in));
        int remainSize = CallCudaLZO_Encode(in, totallen, outvec, 0, block_size);
    }


    //压缩后的数据写入文件：

    ofstream outfile{ output_path,ofstream::binary };
    if (!outfile) {
        cout << "无法输出该文件:" << output_path << endl;;
        exit(0);
    }
    HEADER header;

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

    //根据outvec写入数据节区;
    //因为CUDA不具备BWT，且为强分块模式，数据节区格式只有一种情况
    //[8:分区数量][8:数据块大小][4:分区1压缩后的长度][n1:分区1的数据]
    uint64_t Nparts = outvec.size();//8字节分区数量
    uint64_t blockSize = block_size;
    // uint64_t DataSectionSize = sizeof(Nparts)+sizeof(blockSize);
    outfile.write((char*)&Nparts, sizeof(Nparts));//8字节
    outfile.write((char*)&blockSize, sizeof(blockSize));//8字节
    for (int i = 0; i < outvec.size(); ++i) {
        uint32_t partLen = outvec[i].out_len;
        outfile.write((char*)&partLen, sizeof(uint32_t));//写入分区长度
        outfile.write((char*)outvec[i].out_buffer, partLen);//写入分区的数据
    }

    //重新更新文件头
    outfile.seekp(4 + 1, outfile.beg);
    outfile.write((char*)&header.FileSection_offset, sizeof(uint32_t));
    outfile.write((char*)&header.StrSection_offset, sizeof(uint32_t));
    outfile.write((char*)&header.DataSection_offset, sizeof(uint32_t));

    printf("\n写入完成\n");
    outfile.close();
}


void EncodeDumpToFile(PathName output_path, size_t block_size, vector<outputUint>& outvec, uint64_t BWT_Index = 0) {
    ofstream outfile{ output_path,ofstream::binary };
    if (!outfile) {
        cout << "无法输出该文件:" << output_path << endl;;
        exit(0);
    }
    for (auto it = outvec.begin(); it != outvec.end(); ++it)
        g_out_totallen += it->out_len;//记录压缩后长度 用于benchmark


    HEADER header;
    header.M = g_M;
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

    //根据outvec写入数据节区;

    uint64_t Nparts = outvec.size();//8字节分区数量
    uint64_t blockSize = block_size;//8字节从函数形参获得 如果不分块这个参数表示总长度(对应分区数量Nparts=1)

    outfile.write((char*)&Nparts, sizeof(Nparts));//8字节 分区数量
    outfile.write((char*)&blockSize, sizeof(blockSize));//8字节 数据块大小

    //写入压缩数据实体部分，这里需要分类讨论
    if (IF_CUDA(g_M)) {
        //因为CUDA模式没有实现BWT，且为强分块模式，数据节区格式只有一种情况
        //[8:分区数量][8:数据块大小][4:分区1压缩后的长度][n1:分区1的数据]
        for (int i = 0; i < outvec.size(); ++i) {
            uint32_t partLen = outvec[i].out_len;
            outfile.write((char*)&partLen, sizeof(uint32_t));//写入4字节分区长度
            outfile.write((char*)outvec[i].out_buffer, partLen);//写入分区的数据实体
        }
    }
    else if (IF_SINGLE(g_M)) {
        //[8:分区数量][8:数据块大小]/[8:分区1 BWT Index]/[8:分区1压缩后的长度][n1:分区1的数据]
        for (int i = 0; i < outvec.size(); ++i) {
            if (IF_BWT(g_M)) {
                outfile.write((char*)&BWT_Index, sizeof(uint64_t));//写入BWT index
            }
            uint64_t partLen = outvec[i].out_len;
            outfile.write((char*)&partLen, sizeof(uint64_t));//写入分区长度
            outfile.write((char*)outvec[i].out_buffer, partLen);//写入分区数据实体
        }
    }

    //重新更新文件头
    outfile.seekp(4 + 1, outfile.beg);//写指针移动到正确的位置
    outfile.write((char*)&header.FileSection_offset, sizeof(uint32_t));
    outfile.write((char*)&header.StrSection_offset, sizeof(uint32_t));
    outfile.write((char*)&header.DataSection_offset, sizeof(uint32_t));

    //printf("\n写入完成\n");
    outfile.close();
}

void Encode(string output_path, size_t block_size) {
    finish_rate = 0.0f;
    unsigned char* in = NULL;
    vector<outputUint> outvec;//压缩后的数据按照数据块的相对次序放入outvec
    //计算总共需要多大的buffer
    size_t totallen = 0;
    for (int i = 0; i < FileVectorInBFSOrder.size(); ++i) {
        if (FileVectorInBFSOrder[i]->isCatalog == false) {//非目录文件
            totallen += FileVectorInBFSOrder[i]->size;
        }
    }
    g_src_total_len = totallen;//记录压缩前的长度 用于Benchmark
    if (totallen > 0) {
        in = new unsigned char[totallen];
        std::memset(in, 0, totallen * sizeof(unsigned char));
        g_time1 = GetTickCount64();
        //读取所有可压缩文件数据合并到一个buffer里
        unsigned char* ii = in;
        for (int i = 0; i < FileVectorInBFSOrder.size(); ++i) {
            if (FileVectorInBFSOrder[i]->isCatalog == false) {//非目录文件
               // ifstream infile(tree.RootPath + FileVectorInBFSOrder[i]->filename);
                ifstream infile(FileVectorInBFSOrder[i]->Full_Path, ifstream::binary);
                if (!infile) {
                    assert(0);
                }
                infile.read((char*)ii, FileVectorInBFSOrder[i]->size);
                ii += FileVectorInBFSOrder[i]->size;
                assert(infile.gcount() == FileVectorInBFSOrder[i]->size);
                infile.close();
            }
        }
        assert(totallen == (ii - in));
        finish_rate = 0.1f;
        //开始压缩
        if (IF_CUDA(g_M)) {
            g_time1 = 0;
            int ret=g_time1+CallCudaLZO_Encode(in, totallen, outvec, 0, block_size);//0.9
            if (ret >= 0) {
                g_time2 += ret;
            }
            else {
                terminate();
                //exit(0);
            }
            finish_rate = 0.95f;
            EncodeDumpToFile(output_path, block_size, outvec);
        }
        else if (IF_SINGLE(g_M)) {
            uint64_t bwt_index = 0;
            if (IF_BWT(g_M)) {
                int64_t* Aux = new int64_t[totallen + 1024 * 8];
                bwt_index = libsais64_bwt(in, in, Aux, totallen, 1024 * 8, NULL);
                delete[] Aux;
            }
            finish_rate = 0.45f;
            outputUint temp;
            temp.out_buffer = new unsigned char[LZO_SAFE_OUT_LEN(totallen)];
            temp.out_len = 0;
            Ivan_Compress(in, totallen, temp.out_buffer, &temp.out_len);

          
            outvec.push_back(temp);
            finish_rate = 0.65f;
            //写入文件
            EncodeDumpToFile(output_path, totallen, outvec, bwt_index);
            g_time2 = GetTickCount64();
            finish_rate = 0.8f;

            goto End_func; //释放in指针和vec
        }
        else {//分块
            cout << "错误的需求，未定义M:SINGLE标志\n";        
        }
    }
    else {
        g_out_totallen = 0;
        EncodeDumpToFile(output_path, 0, outvec);
        g_time1 = g_time2 = 0;
    }
End_func:
    finish_rate = 1.0f;
    for (size_t i = 0; i < outvec.size(); ++i) {
        if (outvec[i].out_buffer != NULL) {
            delete[] outvec[i].out_buffer;//别忘了释放输出缓冲区
            outvec[i].out_buffer = NULL;
        }

    }
    if (in != NULL) {
        delete[]in;
        in = NULL;
    }
    //MessageBox()
    //printf("\n压缩结束\n\n");

}

void BootMutilThread_Encode( PathName toWhere, int ThreadNum, size_t blocksize) {

    concurrcy_num = ThreadNum;
    concurrcy_num = concurrcy_num <= 0 ? 2 : concurrcy_num;
    LockFreeLinkedQueue<BlockBuff>* inChan = new LockFreeLinkedQueue<BlockBuff>[concurrcy_num];
    LockFreeLinkedQueue<BlockBuff>* outChan = new LockFreeLinkedQueue<BlockBuff>[concurrcy_num];
    vector<JobThread_Encode*> worker;
    try {
        g_runable = true;
        g_thread_active_num = 0;
        g_parallel_outlen = 0;
        finish_rate = 0.0f;
        for (int i = 0; i < concurrcy_num; ++i) {
            JobThread_Encode* tmp = new JobThread_Encode{ g_runable,(uint64_t)i,inChan[i],outChan[i] };
            worker.push_back(tmp);

        }
        std::for_each(worker.begin(), worker.end(), [](JobThread_Encode* worker) {
            worker->Run();
        });


        vector< LockFreeLinkedQueue<BlockBuff>*> vec_for_read;
        for (int i = 0; i < concurrcy_num; ++i) {
            vec_for_read.push_back(&inChan[i]);
        }
        ReadThread_Encode read_thread( g_runable, vec_for_read, blocksize);
        vector< LockFreeLinkedQueue<BlockBuff>*> vec_for_write;
        for (int i = 0; i < concurrcy_num; ++i) {
            vec_for_write.push_back(&outChan[i]);
        }
        WriteThread_Encode write_thread(toWhere, g_runable, vec_for_write, blocksize);
        read_thread.Run();
        write_thread.Run();
        g_time1 = GetTickCount64();
        Console::PrintProcedure();
        read_thread.Join();
        std::for_each(worker.begin(), worker.end(), [](JobThread_Encode* worker) {
            worker->Join();
        });
        g_time2 = GetTickCount64();//记下结束时间
        g_out_totallen = g_parallel_outlen.load();//总大小由工作线程实时记录更新
      
        write_thread.Join();//写线程实时更新当前比例

        finish_rate = 1.0f;
        std::for_each(worker.begin(), worker.end(), [](JobThread_Encode* worker) {
            if (worker != NULL)
                delete worker;
        });
        if (NULL != inChan) {
            delete[] inChan;
            inChan = NULL;
        }
        if (NULL != outChan) {
            delete[] outChan;
            outChan = NULL;
        }

    }
    catch (exception e) {

        std::for_each(worker.begin(), worker.end(), [](JobThread_Encode* worker) {
            if (worker != NULL)
                delete worker;
            worker = NULL;
        });
        if (NULL != inChan) {
            delete[] inChan;
            inChan = NULL;
        }
        if (NULL != outChan) {
            delete[] outChan;
            outChan = NULL;
        }

        throw e;
    }
    catch (...) {
        cout << "unknow exception in TestMultiThread()\n";
    }


}
