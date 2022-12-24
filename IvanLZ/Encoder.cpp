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
    vector<outputUint> outvec;//ѹ��������ݰ������ݿ����Դ������outvec
    //�����ܹ���Ҫ����buffer
    size_t totallen = 0;
    for (int i = 0; i < FileVectorInBFSOrder.size(); ++i) {
        if (FileVectorInBFSOrder[i]->isCatalog == false) {//��Ŀ¼�ļ�
            totallen += FileVectorInBFSOrder[i]->size;
        }
    }
    if (totallen > 0) {
        in = new unsigned char[totallen];
        std::memset(in, 0, totallen * sizeof(unsigned char));

        //��ȡ���п�ѹ���ļ���buffer
        unsigned char* ii = in;
        for (int i = 0; i < FileVectorInBFSOrder.size(); ++i) {
            if (FileVectorInBFSOrder[i]->isCatalog == false) {//��Ŀ¼�ļ�
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


    //ѹ���������д���ļ���

    ofstream outfile{ output_path,ofstream::binary };
    if (!outfile) {
        cout << "�޷�������ļ�:" << output_path << endl;;
        exit(0);
    }
    HEADER header;

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

    //����outvecд�����ݽ���;
    //��ΪCUDA���߱�BWT����Ϊǿ�ֿ�ģʽ�����ݽ�����ʽֻ��һ�����
    //[8:��������][8:���ݿ��С][4:����1ѹ����ĳ���][n1:����1������]
    uint64_t Nparts = outvec.size();//8�ֽڷ�������
    uint64_t blockSize = block_size;
    // uint64_t DataSectionSize = sizeof(Nparts)+sizeof(blockSize);
    outfile.write((char*)&Nparts, sizeof(Nparts));//8�ֽ�
    outfile.write((char*)&blockSize, sizeof(blockSize));//8�ֽ�
    for (int i = 0; i < outvec.size(); ++i) {
        uint32_t partLen = outvec[i].out_len;
        outfile.write((char*)&partLen, sizeof(uint32_t));//д���������
        outfile.write((char*)outvec[i].out_buffer, partLen);//д�����������
    }

    //���¸����ļ�ͷ
    outfile.seekp(4 + 1, outfile.beg);
    outfile.write((char*)&header.FileSection_offset, sizeof(uint32_t));
    outfile.write((char*)&header.StrSection_offset, sizeof(uint32_t));
    outfile.write((char*)&header.DataSection_offset, sizeof(uint32_t));

    printf("\nд�����\n");
    outfile.close();
}


void EncodeDumpToFile(PathName output_path, size_t block_size, vector<outputUint>& outvec, uint64_t BWT_Index = 0) {
    ofstream outfile{ output_path,ofstream::binary };
    if (!outfile) {
        cout << "�޷�������ļ�:" << output_path << endl;;
        exit(0);
    }
    for (auto it = outvec.begin(); it != outvec.end(); ++it)
        g_out_totallen += it->out_len;//��¼ѹ���󳤶� ����benchmark


    HEADER header;
    header.M = g_M;
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

    //����outvecд�����ݽ���;

    uint64_t Nparts = outvec.size();//8�ֽڷ�������
    uint64_t blockSize = block_size;//8�ֽڴӺ����βλ�� ������ֿ����������ʾ�ܳ���(��Ӧ��������Nparts=1)

    outfile.write((char*)&Nparts, sizeof(Nparts));//8�ֽ� ��������
    outfile.write((char*)&blockSize, sizeof(blockSize));//8�ֽ� ���ݿ��С

    //д��ѹ������ʵ�岿�֣�������Ҫ��������
    if (IF_CUDA(g_M)) {
        //��ΪCUDAģʽû��ʵ��BWT����Ϊǿ�ֿ�ģʽ�����ݽ�����ʽֻ��һ�����
        //[8:��������][8:���ݿ��С][4:����1ѹ����ĳ���][n1:����1������]
        for (int i = 0; i < outvec.size(); ++i) {
            uint32_t partLen = outvec[i].out_len;
            outfile.write((char*)&partLen, sizeof(uint32_t));//д��4�ֽڷ�������
            outfile.write((char*)outvec[i].out_buffer, partLen);//д�����������ʵ��
        }
    }
    else if (IF_SINGLE(g_M)) {
        //[8:��������][8:���ݿ��С]/[8:����1 BWT Index]/[8:����1ѹ����ĳ���][n1:����1������]
        for (int i = 0; i < outvec.size(); ++i) {
            if (IF_BWT(g_M)) {
                outfile.write((char*)&BWT_Index, sizeof(uint64_t));//д��BWT index
            }
            uint64_t partLen = outvec[i].out_len;
            outfile.write((char*)&partLen, sizeof(uint64_t));//д���������
            outfile.write((char*)outvec[i].out_buffer, partLen);//д���������ʵ��
        }
    }

    //���¸����ļ�ͷ
    outfile.seekp(4 + 1, outfile.beg);//дָ���ƶ�����ȷ��λ��
    outfile.write((char*)&header.FileSection_offset, sizeof(uint32_t));
    outfile.write((char*)&header.StrSection_offset, sizeof(uint32_t));
    outfile.write((char*)&header.DataSection_offset, sizeof(uint32_t));

    //printf("\nд�����\n");
    outfile.close();
}

void Encode(string output_path, size_t block_size) {
    finish_rate = 0.0f;
    unsigned char* in = NULL;
    vector<outputUint> outvec;//ѹ��������ݰ������ݿ����Դ������outvec
    //�����ܹ���Ҫ����buffer
    size_t totallen = 0;
    for (int i = 0; i < FileVectorInBFSOrder.size(); ++i) {
        if (FileVectorInBFSOrder[i]->isCatalog == false) {//��Ŀ¼�ļ�
            totallen += FileVectorInBFSOrder[i]->size;
        }
    }
    g_src_total_len = totallen;//��¼ѹ��ǰ�ĳ��� ����Benchmark
    if (totallen > 0) {
        in = new unsigned char[totallen];
        std::memset(in, 0, totallen * sizeof(unsigned char));
        g_time1 = GetTickCount64();
        //��ȡ���п�ѹ���ļ����ݺϲ���һ��buffer��
        unsigned char* ii = in;
        for (int i = 0; i < FileVectorInBFSOrder.size(); ++i) {
            if (FileVectorInBFSOrder[i]->isCatalog == false) {//��Ŀ¼�ļ�
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
        //��ʼѹ��
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
            //д���ļ�
            EncodeDumpToFile(output_path, totallen, outvec, bwt_index);
            g_time2 = GetTickCount64();
            finish_rate = 0.8f;

            goto End_func; //�ͷ�inָ���vec
        }
        else {//�ֿ�
            cout << "���������δ����M:SINGLE��־\n";        
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
            delete[] outvec[i].out_buffer;//�������ͷ����������
            outvec[i].out_buffer = NULL;
        }

    }
    if (in != NULL) {
        delete[]in;
        in = NULL;
    }
    //MessageBox()
    //printf("\nѹ������\n\n");

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
        g_time2 = GetTickCount64();//���½���ʱ��
        g_out_totallen = g_parallel_outlen.load();//�ܴ�С�ɹ����߳�ʵʱ��¼����
      
        write_thread.Join();//д�߳�ʵʱ���µ�ǰ����

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
