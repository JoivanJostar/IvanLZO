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
        LockFreeLinkedQueue<BlockBuff>& _outChan)  //绑定工作函数和工作通道
        : Thread(runable), inChan(_inChan), outChan(_outChan), id{_id},  actived{false}
    { }
   
    void virtual Thread_Job()override {
        BlockBuff data;
        try {
            while ((m_runable.load(std::memory_order_acquire)) == true || !inChan.isEmpty()) { //读通道已经关闭，并且通道中没有剩余未处理的数据时线程退出        
            //read_data:
                if (!inChan.isEmpty()) { //有数据
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
                //多线程版本默认是分块的
                unsigned char* temp_data=NULL;
                int64_t bwt_index = -1;
                //从L3Cache搬到L2Cache
                temp_data = new unsigned char[data.size];
                assert(data.pbuff!=NULL);
                std::memcpy(temp_data, data.pbuff, data.size);//拷贝源数据到temp_data
                delete[] (unsigned char *)data.pbuff;//释放前一级线程申请的源数据内存
                data.pbuff = NULL;
                //测试以下prefetch的效果

                //如果有BTW，则进行原地 in-place的转换
                if (IF_BWT(g_M)) {
                    int64_t* aux = new int64_t[data.size+1024]; //辅助数组 记得释放 
                    bwt_index = libsais64_bwt(temp_data, temp_data, aux, data.size, 1024, NULL);
                    delete[] aux;//释放辅助数组
                }
                //LZO:
                size_t inlen = data.size;              
                size_t outlen = 0;              
                unsigned char* out = new unsigned char[LZO_SAFE_OUT_LEN(inlen)];//需要在下一级线程读取后释放      

                //_mm_prefetch(temp_data)测试以下prefetch的效果
                Ivan_Compress(temp_data, inlen, out, &outlen);  
                delete[] temp_data;//释放中间结果
                temp_data = NULL;
                if (!actived) {//记录工作状态
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
        LockFreeLinkedQueue<BlockBuff>& _outChan  //绑定工作函数和工作通道
        )
        : Thread(runable), inChan(_inChan), outChan(_outChan),  id{ _id },  actived{ false }
    { }
    void virtual Thread_Job()override {
        BlockBuff data;
        try {
            while ((m_runable.load(std::memory_order_acquire)) == true || !inChan.isEmpty()) { //读通道已经关闭，并且通道中没有剩余未处理的数据时线程退出        
            //read_data:
                if (!inChan.isEmpty()) { //有数据
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
                //获取上一级线程的数据 这里没有做Cache优化              
                //de-LZO
                uint32_t block_size=data.block_size;
                unsigned char* out = new unsigned char[block_size];
                size_t outlen = 0;
                Ivan_DeCompress((unsigned char*)data.pbuff, data.size, out, &outlen);
                if (IF_BWT(g_M)) {
                    int64_t* aux = new int64_t[outlen + 1]; //辅助数组 记得释放  要+1
                    if (libsais64_unbwt(out, out, aux, outlen, NULL, data.BWT_index) != 0) {
                        delete[] aux;
                        throw std::logic_error("BWT Reverse错误\n");
                    }
                    delete[] aux;
                }
                //以及释放获取的in数据(这一步放在后面写入成功后进行)     
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
                    delete[] (unsigned char *)data.pbuff;//读入的数据已经被加工完毕成功输送到下一级，可以销毁了
                    data.pbuff = NULL;
                    continue;
                }
                else {//输出通道满了，等待畅通继续工作。
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