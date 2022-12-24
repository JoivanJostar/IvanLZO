#include "Decoder.h"
#include "lzox64.h"
#include "global_var.h"
#include "FileModuleDef.h"
#include <assert.h>
#include <direct.h>
#include <io.h>
#include "CollectThread.h"
#include "JobThread.h"
#include "Console.h"

//将解压缩后的数据Dump到文件，数据存储在vec里面，如果是分块数据则会在内部重组为一块数据
//对于原来在vec的指针不调用delete，请在调用此函数结束后自行释放outvec成员指向的内存
void DecodeDumpToFile(vector<outputUint>& outvec) {
    bool useExtaMem = false;
    //检查数据是否存在缺失
    size_t out_total_len = 0;
    for (int i = 0; i < outvec.size(); ++i) {
        out_total_len += outvec[i].out_len;
    }
    size_t check = 0;
    for (int i = 0; i < FileVectorInBFSOrder.size(); ++i) {
        if (FileVectorInBFSOrder[i]->isCatalog == false)
            check += FileVectorInBFSOrder[i]->size;
    }
    if (check != out_total_len) {
        cout << "压缩文件已损坏 程序即将退出\n";
        exit(0);
    }
    //组成一个buffer

    unsigned char* out_final = NULL;
    unsigned char* op = out_final;
    if (out_total_len > 0) {
        if (outvec.size() > 1) {
            out_final = new unsigned char[out_total_len];
            op = out_final;
            useExtaMem = true;
            memset(out_final, 0, out_total_len);
            for (int i = 0; i < outvec.size(); ++i) {
                memcpy(op, outvec[i].out_buffer, outvec[i].out_len);
                op += outvec[i].out_len;
            }
        }
        else if (outvec.size() == 1) {
            out_final = outvec[0].out_buffer;
        }
    }
    else {
        out_final = new unsigned char[1];
        out_final[0] = 0;
    }


    //正确输出到每个文件：
    op = out_final;
    for (int i = 0; i < FileVectorInBFSOrder.size(); ++i) {
        FileNode* file = FileVectorInBFSOrder[i];
        string Path = file->Full_Path;
        if (file->isCatalog == true) {//目录文件
            if (0 != access(Path.c_str(), 0))// 如果目录不存在就创建一个
            {
                if (-1 == mkdir(Path.c_str())) {// 返回 0 表示创建成功，-1 表示失败
                    cout << "创建输出文件夹失败\n";
                    exit(0);
                }
            }
        }
        else {//普通文件
            FileNode* file = FileVectorInBFSOrder[i];
            ofstream outfile(Path, ofstream::binary);
            outfile.write((char*)op, file->size);
            op += file->size;
            outfile.close();
        }
    }

    if (useExtaMem) {
        delete[]  out_final;
        out_final = NULL;
    }
}
bool SingleDecode(PathName filePath, PathName outputPath) {
    ifstream infile(filePath, ifstream::binary);
    if (!infile) {
        cout << "文件不存在 :" << filePath << endl;
        //throw 
        return false;
    }
    //读取文件头
    HEADER header;
    infile.read(header.LABLE, 4);//4字节的标签
    if (string(header.LABLE, header.LABLE + 4) != "IVAN") {
        infile.close();
        cout << "非法的文件格式,不是本软件支持的压缩文件\n";
        return false;
    }
    infile.read((char*)&header.M, sizeof(uint8_t));//1字节的模式位
    infile.read((char*)&header.FileSection_offset, sizeof(uint32_t));//4
    infile.read((char*)&header.StrSection_offset, sizeof(uint32_t));//4
    infile.read((char*)&header.DataSection_offset, sizeof(uint32_t));//4
    infile.read((char*)&header.padding, sizeof(uint8_t) * 15);//15字节的填充
    g_M = header.M;
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
    DecodeTree(FileSection_buffer, FileSeciotn_size, StrSeciton_buffer, outputPath);
    delete[] StrSeciton_buffer;
    StrSeciton_buffer = NULL;
    delete[] FileSection_buffer;
    FileSection_buffer = NULL;
    //下面这一句放在压缩完成后显示   
    PrintfFileTree(tree);
    //解压缩
    vector<outputUint> outvec;
    uint64_t Nparts = 0;
    uint64_t blockSize = 0;
    infile.seekg(header.DataSection_offset, infile.beg);//读指针移到正确位置
    infile.read((char*)&Nparts, sizeof(uint64_t)); //分区数量
    infile.read((char*)&blockSize, sizeof(uint64_t));//数据块大小
    assert(Nparts >= 0);
    if (Nparts == 0) {
        DecodeDumpToFile(outvec);
        goto end_func;
    }
        
    if (!IF_SINGLE(header.M)) {
        uint32_t ipartinLen = 0;
        size_t ipartoutLen = 0;
        uint32_t local_bwt_index = 0;
        //uint64_t global_BWT_index = 0;
        unsigned char* iPart_inbuffer = NULL;
        unsigned char* iPart_outbuffer = NULL;
        //if (IF_BWT(header.M) && !IF_P_BWT(header.M)) {
        ////分区结构: /[8:BWT 全局index]/ + /[4:bwtIndex]/ [4:PartSize][data......]
        //    infile.read((char*)&global_BWT_index, sizeof(uint64_t));//读取8字节的全局bwt index
        //}
        for (int i = 0; i < Nparts; ++i) {
            if (IF_BWT(header.M) && IF_P_BWT(header.M)) {
                infile.read((char*)&local_bwt_index, sizeof(uint32_t));//读取4字节的分区bwt index
            }
            infile.read((char*)&ipartinLen, sizeof(uint32_t));//读取4字节的分区大小
            iPart_inbuffer = new unsigned char[ipartinLen];
            infile.read((char*)iPart_inbuffer, ipartinLen);
            iPart_outbuffer = new unsigned char[blockSize];
            ipartoutLen = 0;
            Ivan_DeCompress(iPart_inbuffer, ipartinLen, iPart_outbuffer, &ipartoutLen);
            delete[] iPart_inbuffer;
            iPart_inbuffer = NULL;
            if (IF_BWT(header.M) && IF_P_BWT(header.M)) {
                int64_t* aux = new int64_t[ipartoutLen + 8 * 1024];
                libsais64_unbwt(iPart_outbuffer, iPart_outbuffer, aux, ipartoutLen, NULL, local_bwt_index);
                delete[] aux;
            }
            outputUint temp;
            temp.out_buffer = iPart_outbuffer;
            temp.out_len = ipartoutLen;
            outvec.push_back(temp);
        }
        ////特殊情况处理：

        //if (IF_BWT(header.M) && !IF_P_BWT(header.M)) {
        //    //合成一个buffer  解全局BWT unBwt
        //    size_t bwt_out_total_len = 0;
        //    for (size_t i = 0; i < outvec.size(); ++i)
        //        bwt_out_total_len += outvec[i].out_len;
        //    unsigned char* bwt_out = new unsigned char[bwt_out_total_len];
        //    unsigned char* op = bwt_out;
        //    for (size_t i = 0; i < outvec.size(); ++i) {
        //        memcpy(op, outvec[i].out_buffer, outvec[i].out_len);
        //        op += outvec[i].out_len;
        //        delete[] outvec[i].out_buffer;
        //        outvec[i].out_buffer = NULL;
        //    }
        //    assert(bwt_out_total_len == (op - bwt_out));
        //    int64_t* aux = new int64_t[bwt_out_total_len + 1];
        //    libsais64_unbwt(bwt_out, bwt_out, aux, bwt_out_total_len, NULL, global_BWT_index);
        //    delete[] aux;
        //    //清空outvec;////////////////////////
        //    for (size_t i = 0; i < outvec.size(); ++i) {
        //        delete[] outvec[i].out_buffer;
        //        outvec[i].out_buffer = NULL;
        //    }
        //    outvec.clear();
        //    /// /////////////////////////////
        //    outputUint temp;
        //    temp.out_buffer = bwt_out;
        //    temp.out_len = bwt_out_total_len;
        //    outvec.push_back(temp);
        //    DecodeDumpToFile(outvec);//向量中只有一块内存，正常的话，DecodeDumpToFile不会在内部delete掉outvec的内存
        //    goto end_func;//检查释放内存和关闭文件
        //}
        ///end of 特殊情况
        DecodeDumpToFile(outvec);
        goto end_func;
    }
    else {//不分块的压缩文件，
        //[8:分区数量 = 1] [8:数据块大小 = 原数据总长度之和] /[8:BWT index1]/ [8:分区1大小] [data.........]
        uint64_t global_BWT_index = 0;
        uint64_t global_PartinLen = 0;
        size_t global_PartoutLen = 0;
        unsigned char* global_inbuffer = NULL;
        unsigned char* global_outbuffer = NULL;
        
        assert(Nparts == 1);
        if (IF_BWT(header.M)) {
            infile.read((char*)&global_BWT_index, sizeof(uint64_t));//读取8字节的分区bwt index
        }
        infile.read((char*)&global_PartinLen, sizeof(uint64_t));//读取8字节的分区大小
        global_inbuffer = new unsigned char[global_PartinLen];
        infile.read((char*)global_inbuffer, global_PartinLen);
        global_outbuffer = new unsigned char[blockSize];//blocksize就是原来数据的总长度了
        Ivan_DeCompress(global_inbuffer, global_PartinLen, global_outbuffer, &global_PartoutLen);
        delete[] global_inbuffer; global_inbuffer = NULL;
        assert(global_PartoutLen == blockSize);
        if (IF_BWT(header.M)) {
            int64_t* aux = new int64_t[global_PartoutLen + 1];
            libsais64_unbwt(global_outbuffer, global_outbuffer, aux, global_PartoutLen, NULL, global_BWT_index);
            delete[] aux;
        }
        outputUint temp;
        temp.out_buffer = global_outbuffer;
        temp.out_len = global_PartoutLen;
        outvec.push_back(temp);
        DecodeDumpToFile(outvec);
        goto end_func;
    }



end_func:
    for (size_t i = 0; i < outvec.size(); ++i) {
        if (outvec[i].out_buffer != NULL) {
            delete[] outvec[i].out_buffer;
            outvec[i].out_buffer = NULL;
        }
    }
    if (infile)
        infile.close();

    return true;
}

void BootMutilThread_Decode(PathName fromWhere, PathName toWhere, int ThreadNum) {

    concurrcy_num = ThreadNum;// std::thread::hardware_concurrency();// -6;//; / 2 - 3; //
    concurrcy_num = concurrcy_num <= 0 ? 2 : concurrcy_num;
    LockFreeLinkedQueue<BlockBuff>* inChan = new LockFreeLinkedQueue<BlockBuff>[concurrcy_num];
    LockFreeLinkedQueue<BlockBuff>* outChan = new LockFreeLinkedQueue<BlockBuff>[concurrcy_num];
    vector<JobThread_Decode*> worker;
    try {
        g_runable = true;
        g_thread_active_num = 0;
        g_parallel_outlen = 0;
        for (int i = 0; i < concurrcy_num; ++i) {
            JobThread_Decode* tmp = new JobThread_Decode{ g_runable,(uint64_t)i,inChan[i],outChan[i] };
            worker.push_back(tmp);

        }
        std::for_each(worker.begin(), worker.end(), [](JobThread_Decode* worker) {
            worker->Run();
        });


        vector< LockFreeLinkedQueue<BlockBuff>*> vec_for_read;
        for (int i = 0; i < concurrcy_num; ++i) {
            vec_for_read.push_back(&inChan[i]);
        }
        ReadThread_Decode read_thread(fromWhere, toWhere, g_runable, vec_for_read);
        vector< LockFreeLinkedQueue<BlockBuff>*> vec_for_write;
        for (int i = 0; i < concurrcy_num; ++i) {
            vec_for_write.push_back(&outChan[i]);
        }
        WriteThread_Decode write_thread(toWhere, g_runable, vec_for_write);
        read_thread.Run();
        write_thread.Run();
        Console::PrintProcedure();
        read_thread.Join();
        std::for_each(worker.begin(), worker.end(), [](JobThread_Decode* worker) {
            worker->Join();
        });

        uint64_t outlen = g_parallel_outlen.load();

        write_thread.Join();

        std::for_each(worker.begin(), worker.end(), [](JobThread_Decode* worker) {
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

        std::for_each(worker.begin(), worker.end(), [](JobThread_Decode* worker) {
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

