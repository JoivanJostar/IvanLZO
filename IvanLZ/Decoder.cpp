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

//����ѹ���������Dump���ļ������ݴ洢��vec���棬����Ƿֿ�����������ڲ�����Ϊһ������
//����ԭ����vec��ָ�벻����delete�����ڵ��ô˺��������������ͷ�outvec��Աָ����ڴ�
void DecodeDumpToFile(vector<outputUint>& outvec) {
    bool useExtaMem = false;
    //��������Ƿ����ȱʧ
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
        cout << "ѹ���ļ����� ���򼴽��˳�\n";
        exit(0);
    }
    //���һ��buffer

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


    //��ȷ�����ÿ���ļ���
    op = out_final;
    for (int i = 0; i < FileVectorInBFSOrder.size(); ++i) {
        FileNode* file = FileVectorInBFSOrder[i];
        string Path = file->Full_Path;
        if (file->isCatalog == true) {//Ŀ¼�ļ�
            if (0 != access(Path.c_str(), 0))// ���Ŀ¼�����ھʹ���һ��
            {
                if (-1 == mkdir(Path.c_str())) {// ���� 0 ��ʾ�����ɹ���-1 ��ʾʧ��
                    cout << "��������ļ���ʧ��\n";
                    exit(0);
                }
            }
        }
        else {//��ͨ�ļ�
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
        cout << "�ļ������� :" << filePath << endl;
        //throw 
        return false;
    }
    //��ȡ�ļ�ͷ
    HEADER header;
    infile.read(header.LABLE, 4);//4�ֽڵı�ǩ
    if (string(header.LABLE, header.LABLE + 4) != "IVAN") {
        infile.close();
        cout << "�Ƿ����ļ���ʽ,���Ǳ����֧�ֵ�ѹ���ļ�\n";
        return false;
    }
    infile.read((char*)&header.M, sizeof(uint8_t));//1�ֽڵ�ģʽλ
    infile.read((char*)&header.FileSection_offset, sizeof(uint32_t));//4
    infile.read((char*)&header.StrSection_offset, sizeof(uint32_t));//4
    infile.read((char*)&header.DataSection_offset, sizeof(uint32_t));//4
    infile.read((char*)&header.padding, sizeof(uint8_t) * 15);//15�ֽڵ����
    g_M = header.M;
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
    DecodeTree(FileSection_buffer, FileSeciotn_size, StrSeciton_buffer, outputPath);
    delete[] StrSeciton_buffer;
    StrSeciton_buffer = NULL;
    delete[] FileSection_buffer;
    FileSection_buffer = NULL;
    //������һ�����ѹ����ɺ���ʾ   
    PrintfFileTree(tree);
    //��ѹ��
    vector<outputUint> outvec;
    uint64_t Nparts = 0;
    uint64_t blockSize = 0;
    infile.seekg(header.DataSection_offset, infile.beg);//��ָ���Ƶ���ȷλ��
    infile.read((char*)&Nparts, sizeof(uint64_t)); //��������
    infile.read((char*)&blockSize, sizeof(uint64_t));//���ݿ��С
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
        ////�����ṹ: /[8:BWT ȫ��index]/ + /[4:bwtIndex]/ [4:PartSize][data......]
        //    infile.read((char*)&global_BWT_index, sizeof(uint64_t));//��ȡ8�ֽڵ�ȫ��bwt index
        //}
        for (int i = 0; i < Nparts; ++i) {
            if (IF_BWT(header.M) && IF_P_BWT(header.M)) {
                infile.read((char*)&local_bwt_index, sizeof(uint32_t));//��ȡ4�ֽڵķ���bwt index
            }
            infile.read((char*)&ipartinLen, sizeof(uint32_t));//��ȡ4�ֽڵķ�����С
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
        ////�����������

        //if (IF_BWT(header.M) && !IF_P_BWT(header.M)) {
        //    //�ϳ�һ��buffer  ��ȫ��BWT unBwt
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
        //    //���outvec;////////////////////////
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
        //    DecodeDumpToFile(outvec);//������ֻ��һ���ڴ棬�����Ļ���DecodeDumpToFile�������ڲ�delete��outvec���ڴ�
        //    goto end_func;//����ͷ��ڴ�͹ر��ļ�
        //}
        ///end of �������
        DecodeDumpToFile(outvec);
        goto end_func;
    }
    else {//���ֿ��ѹ���ļ���
        //[8:�������� = 1] [8:���ݿ��С = ԭ�����ܳ���֮��] /[8:BWT index1]/ [8:����1��С] [data.........]
        uint64_t global_BWT_index = 0;
        uint64_t global_PartinLen = 0;
        size_t global_PartoutLen = 0;
        unsigned char* global_inbuffer = NULL;
        unsigned char* global_outbuffer = NULL;
        
        assert(Nparts == 1);
        if (IF_BWT(header.M)) {
            infile.read((char*)&global_BWT_index, sizeof(uint64_t));//��ȡ8�ֽڵķ���bwt index
        }
        infile.read((char*)&global_PartinLen, sizeof(uint64_t));//��ȡ8�ֽڵķ�����С
        global_inbuffer = new unsigned char[global_PartinLen];
        infile.read((char*)global_inbuffer, global_PartinLen);
        global_outbuffer = new unsigned char[blockSize];//blocksize����ԭ�����ݵ��ܳ�����
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

