#pragma once
#include "Thread.h"
#include <stdexcept>
#include <exception>
#include "seh_exception.cpp"
#include <string>
#include <sstream>
#include <queue>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include "lock_free_queue.h"
#include "global_var.h"
#include "libsais64.h"
#include "lzox64.h"
struct BlockBuff {
    void* pbuff;
    uint32_t size;
    uint64_t BWT_index;
    uint32_t block_size;
    BlockBuff() {
        pbuff = NULL;
        size = 0;
        BWT_index = 0;
        block_size = 0;
    }
    BlockBuff(void * ptr,size_t sz, uint64_t index,uint32_t _block_size) {
        pbuff = ptr;
        size = sz;
        BWT_index = index;
        block_size = _block_size;
    }
};

class JobThread_Encode :public Thread
{
public:
    uint64_t id;

    JobThread_Encode(
        std::atomic_bool& runable,
        uint64_t _id,
        LockFreeLinkedQueue<BlockBuff>& _inChan,
        LockFreeLinkedQueue<BlockBuff>& _outChan)  //�󶨹��������͹���ͨ��
        : Thread(runable), inChan(_inChan), outChan(_outChan), id{_id},  actived{false}
    { }
   
    void virtual Thread_Job()override {
        BlockBuff data;
        try {
            while ((m_runable.load(std::memory_order_acquire)) == true || !inChan.isEmpty()) { //��ͨ���Ѿ��رգ�����ͨ����û��ʣ��δ���������ʱ�߳��˳�        
            //read_data:
                if (!inChan.isEmpty()) { //������
                    if (inChan.tryPop(data) == true) {
                        goto do_compress;
                    }
                    else {
                        continue;
                    }
                }
                else {
                    continue;
                }

            do_compress:
                //���̰߳汾Ĭ���Ƿֿ��
                unsigned char* temp_data=NULL;
                int64_t bwt_index = -1;
                //��L3Cache�ᵽL2Cache
                temp_data = new unsigned char[data.size];
                assert(data.pbuff!=NULL);
                std::memcpy(temp_data, data.pbuff, data.size);//����Դ���ݵ�temp_data
                delete[] (unsigned char *)data.pbuff;//�ͷ�ǰһ���߳������Դ�����ڴ�
                data.pbuff = NULL;
                //��������prefetch��Ч��

                //�����BTW�������ԭ�� in-place��ת��
                if (IF_BWT(g_M)) {
                    int64_t* aux = new int64_t[data.size+1024]; //�������� �ǵ��ͷ� 
                    bwt_index = libsais64_bwt(temp_data, temp_data, aux, data.size, 1024, NULL);
                    delete[] aux;//�ͷŸ�������
                }
                //LZO:
                size_t inlen = data.size;              
                size_t outlen = 0;              
                unsigned char* out = new unsigned char[LZO_SAFE_OUT_LEN(inlen)];//��Ҫ����һ���̶߳�ȡ���ͷ�      

                //_mm_prefetch(temp_data)��������prefetch��Ч��
                Ivan_Compress(temp_data, inlen, out, &outlen);  
                delete[] temp_data;//�ͷ��м���
                temp_data = NULL;
                if (!actived) {//��¼����״̬
                    g_thread_active_num++;
                    actived = true;
                }
            write_data:
                if (!outChan.isFull()) {
                    //BlockBuff(void* ptr, size_t sz, uint64_t index)
                    BlockBuff tmp(out, outlen, bwt_index,-1);
                    outChan.push(tmp);
                    g_parallel_outlen += tmp.size;
                    continue;
                }
                else {
                    goto write_data;
                }
            }

        }
        catch (std::exception e) {
            std::cout << e.what();
        }
        catch (...) {
            std::cout << "unknow exception in JobThread()\n";
        }
       

        return;
    }

private:
    bool actived;
    LockFreeLinkedQueue<BlockBuff>& inChan;
    LockFreeLinkedQueue<BlockBuff>& outChan;

    
};

class JobThread_Decode:public Thread
{
  
public:
    uint64_t id;
    JobThread_Decode(
        std::atomic_bool& runable,
        uint64_t _id,
        LockFreeLinkedQueue<BlockBuff>& _inChan,
        LockFreeLinkedQueue<BlockBuff>& _outChan  //�󶨹��������͹���ͨ��
        )
        : Thread(runable), inChan(_inChan), outChan(_outChan),  id{ _id },  actived{ false }
    { }
    void virtual Thread_Job()override {
        BlockBuff data;
        try {
            while ((m_runable.load(std::memory_order_acquire)) == true || !inChan.isEmpty()) { //��ͨ���Ѿ��رգ�����ͨ����û��ʣ��δ���������ʱ�߳��˳�        
            //read_data:
                if (!inChan.isEmpty()) { //������
                    if (inChan.tryPop(data) == true) {
                        goto do_decompress;
                    }
                    else {
                        continue;
                    }

                }
                else {
                    continue;
                }

            do_decompress:
                //��ȡ��һ���̵߳����� ����û����Cache�Ż�              
                //de-LZO
                uint32_t block_size=data.block_size;
                unsigned char* out = new unsigned char[block_size];
                size_t outlen = 0;
                Ivan_DeCompress((unsigned char*)data.pbuff, data.size, out, &outlen);
                if (IF_BWT(g_M)) {
                    int64_t* aux = new int64_t[outlen + 1]; //�������� �ǵ��ͷ�  Ҫ+1
                    if (libsais64_unbwt(out, out, aux, outlen, NULL, data.BWT_index) != 0) {
                        delete[] aux;
                        throw std::logic_error("BWT Reverse����\n");
                    }
                    delete[] aux;
                }
                //�Լ��ͷŻ�ȡ��in����(��һ�����ں���д��ɹ������)     
                if (!actived) {
                    g_thread_active_num++;
                    actived = true;
                }

            write_data:
                if (!outChan.isFull()) {
                    BlockBuff tmp;
                    tmp.pbuff = out;
                    tmp.size = outlen;
                    outChan.push(tmp);
                    delete[] (unsigned char *)data.pbuff;//����������Ѿ����ӹ���ϳɹ����͵���һ��������������
                    data.pbuff = NULL;
                    continue;
                }
                else {//���ͨ�����ˣ��ȴ���ͨ����������
                    goto write_data;
                }
            }

        }
        catch (std::exception e) {
            std::cout << e.what();
        }
        catch (...) {
            std::cout << "unknow exception in JobThread()\n";
        }
        return;
    }

private:
    bool actived;
    LockFreeLinkedQueue<BlockBuff>& inChan;
    LockFreeLinkedQueue<BlockBuff>& outChan;

};