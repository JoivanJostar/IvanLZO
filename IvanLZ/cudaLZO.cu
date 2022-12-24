#include "cuda_lzo.h"
#include "global_var.h"
#include <Windows.h>
#include <iostream>
#include "Console.h"
#include "cuda_runtime.h"
#include "cuda_runtime_api.h "




__device__ uint32_t  get4Byte(volatile const unsigned char* src) {
    uint32_t result = 0;
    if (((uint64_t)src & (uint64_t)(3)) == 0) {
        // printf("p %p 4字节对齐\n", src);
        return *(volatile int*)src;
    }
    else if (((uint64_t)src & (uint64_t)(1)) == 0) {
        ((unsigned short*)&result)[0] = ((unsigned short*)src)[0];
        ((unsigned short*)&result)[1] = ((unsigned short*)src)[1];
        return result;
    }
    else {//可以优化加上2字节对齐时的加速
     //   printf("p %p 不是4字节对齐\n", src);
        ((unsigned char*)&result)[0] = src[0];
        ((unsigned char*)&result)[1] = src[1];
        ((unsigned char*)&result)[2] = src[2];
        ((unsigned char*)&result)[3] = src[3];
        //printf("单字节访问4字节单元成功,4字节整数为 %u\n", result);
        return result;
    }
}
__device__ lzo_uint device_do_compress(const lzo_bytep in, lzo_uint  in_len,
    lzo_bytep out, lzo_uintp out_len,
    lzo_uint  ti, lzo_voidp wrkmem)
{
    const lzo_bytep ip;
    lzo_bytep op;
    const lzo_bytep const in_end = in + in_len;
    const lzo_bytep const ip_end = in + in_len - 20; //ll>20，-20是安全的
    const lzo_bytep ii;
    lzo_dict_p const dict = (lzo_dict_p)wrkmem;

    op = out;
    ip = in;
    ii = ip;

    ip += ti < 4 ? 4 - ti : 0;
    for (;;)
    {

        const lzo_bytep m_pos;
        //非Deterministic模式： 2次哈希策略，这里默认不适用


        lzo_uint m_off;
        lzo_uint m_len;
        {
            lzo_uint32_t dv;
            lzo_uint dindex;
            literal:
            ip += 1 + ((ip - ii) >> 5); //这个地方我取消了此优化，加大压缩程度
            //ip += 1;

        next:

            if (ip >= ip_end) //搜索超过了范围，需要结束
                break;
            // printf("try GET_LE32 from %p \n", ip);
            dv = get4Byte(ip);
            //dv = UA_GET_LE32(ip); //取ip处4字节的数据到dv
           // printf("GET_LE32 sucess dv is %u\n",dv);
            dindex = DINDEX(dv, ip);//get hashKey
            m_pos = in + dict[dindex];


            dict[dindex] = (unsigned short int) ((lzo_uint)(ip - in));

            if (dv != get4Byte(m_pos))//碰撞 这里直接当作字面量处理
                goto literal;
        }

        //处理新字符 输出【新字符个数】+新字符1，新字符2，新字符3
        ii -= ti; ti = 0; //回退ii指针，到上一个数据块末尾未处理的字符处
        {
            lzo_uint t = ip - ii;

            if (t != 0)
            {
                if (t <= 3)
                {
                    op[-2] = LZO_BYTE(op[-2] | t);

                    { do *op++ = *ii++; while (--t > 0); } //如果处理器不支持非对齐数据访问 此时需要朴素使用逐字节访问指令，效率降至最低
                }

                else
                {
                    if (t <= 18)
                        *op++ = LZO_BYTE(t - 3); //表示新字符个数的那个字节的值要小于16（和表示重复字段的字节区分开），因为小于3时用piggyback,
                    //所以大于3时 个数上限可以额外表示3个字符量到上限18个字符
                    else
                    {
                        lzo_uint tt = t - 18; //解码时要+18
                        *op++ = 0;
                        while (tt > 255)
                        {
                            tt -= 255;
                            UA_SET1(op, 0);//应该没问题？1字节是对齐的
                            op++;
                        }

                        *op++ = LZO_BYTE(tt);
                    }

                    {
                        do *op++ = *ii++; while (--t > 0);
                    }
                }
            }
        }
        m_len = 4; //下面这个代码块是在计算重复长度len
        {
            //朴素的长度匹配算法，理论上的最多访存次数因为GPU可能不支持非对齐访问，这里单字节访存是最安全的。
            // CPU上可以用UA访问来加速。
                      // printf("尝试长度匹配\n");
            if (ip[m_len] == m_pos[m_len]) {
                do {
                    m_len += 1;
                    if (ip[m_len] != m_pos[m_len])
                        break;
                    m_len += 1;
                    if (ip[m_len] != m_pos[m_len])
                        break;
                    m_len += 1;
                    if (ip[m_len] != m_pos[m_len])
                        break;
                    m_len += 1;
                    if (ip[m_len] != m_pos[m_len])
                        break;
                    m_len += 1;
                    if (ip[m_len] != m_pos[m_len])
                        break;
                    m_len += 1;
                    if (ip[m_len] != m_pos[m_len])
                        break;
                    m_len += 1;
                    if (ip[m_len] != m_pos[m_len])
                        break;
                    m_len += 1;
                    if (ip + m_len >= ip_end)
                        goto m_len_done;
                } while (ip[m_len] == m_pos[m_len]);
            }
            // printf("重复长度匹配成功\n");
        }
    m_len_done:
        m_off = ip - m_pos;//get Distance
        ip += m_len;//更新ip
        ii = ip;//更新ii
        //M2:len<=8 && distance<=2k   
        if (m_len <= M2_MAX_LEN && m_off <= M2_MAX_OFFSET)
        {
            m_off -= 1; //m_off 和m_len实际存储范围[1,8] 和[1,2048]
            *op++ = LZO_BYTE(((m_len - 1) << 5) | ((m_off & 7) << 2));
            *op++ = LZO_BYTE(m_off >> 3);
        }
        //M3:distance<=16k   
        else if (m_off <= M3_MAX_OFFSET)
        {
            m_off -= 1;
            if (m_len <= M3_MAX_LEN)
                *op++ = LZO_BYTE(M3_MARKER | (m_len - 2)); //M3第一个字节的低5位为len，len最大表示范围为31，又因为len必然大于2，所以上限表示范围+2
            //考虑到m_len本身>=4,故最后范围为[4,33]
            else
            {
                m_len -= M3_MAX_LEN; //解码时+33
                *op++ = M3_MARKER | 0;
                while (m_len > 255)
                {
                    m_len -= 255;
                    UA_SET1(op, 0);
                    op++;
                }//end of loop
                *op++ = LZO_BYTE(m_len);
            }
            *op++ = LZO_BYTE(m_off << 2);
            *op++ = LZO_BYTE(m_off >> 6);
        }
        //M4:16K<distance<=48k
        else
        {
            m_off -= 0x4000; //因为知道这种情况也有16k，故可以都减去16k扩充表示范围
            if (m_len <= M4_MAX_LEN) //可以用3bit表示
                *op++ = LZO_BYTE(M4_MARKER | ((m_off >> 11) & 8) | (m_len - 2));
            else
            {
                m_len -= M4_MAX_LEN;//解码时还原
                *op++ = LZO_BYTE(M4_MARKER | ((m_off >> 11) & 8));
                while (m_len > 255)
                {
                    m_len -= 255;
                    UA_SET1(op, 0);
                    op++;
                }
                *op++ = LZO_BYTE(m_len);
            }
            *op++ = LZO_BYTE(m_off << 2);
            *op++ = LZO_BYTE(m_off >> 6);
        }
        goto next;
    }

    *out_len = op - out;
    return in_end - (ii - ti);
}

__device__ int CUDA_COMPRESS_LZO(const lzo_bytep in, lzo_uint  in_len,
    lzo_bytep out, lzo_uintp out_len,
    unsigned char* wrkmem)
{
    const lzo_bytep ip = in;
    lzo_bytep op = out;
    lzo_uint l = in_len;
    lzo_uint t = 0;
    while (l > 20)
    {
        lzo_uint ll = l;
        lzo_uint ll_end;
#if 0 || (LZO_DETERMINISTIC)
        ll = LZO_MIN(ll, 49152); //DETERMINISTIC模式： src数据流大小>48k时分块处理
#endif
        ll_end = (lzo_uint)ip + ll;
        if ((ll_end + ((t + ll) >> 5)) <= ll_end || (const lzo_bytep)(ll_end + ((t + ll) >> 5)) <= ip + ll)
            break;
#if (LZO_DETERMINISTIC)

        //修改过：memset逻辑
        //int size = (lzo_uint)1 << D_BITS;
        //Reset哈希表 必须！！！！
        unsigned short int* p = (unsigned short int*)wrkmem;
        for (int i = 0; i <1u<<D_BITS; ++i) {
            p[i] = 0;
        }

        //memset(wrkmem, 0, ((lzo_uint)1 << D_BITS) * sizeof(lzo_dict_t)); //
#endif
        //printf("调用 do_compress\n");
        t = device_do_compress(ip, ll, op, out_len, t, wrkmem);
        __syncthreads();
        // printf("do_compress成功\n");
        ip += ll;
        op += *out_len;
        l -= ll;
    }
    t += l;
    // printf("尝试处理边角料\n");
    if (t > 0)
    {
        __syncthreads();
        const lzo_bytep ii = in + in_len - t;

        if (op == out && t <= 238) //sp分支
            *op++ = LZO_BYTE(17 + t);//当且仅当do_compress没有完成任何数据压缩（op指针没有移动，提供的数据块没有任何可以替换的重复字段）才会进入此分支
        else if (t <= 3)
            op[-2] = LZO_BYTE(op[-2] | t);
        else if (t <= 18)
            *op++ = LZO_BYTE(t - 3);
        else
        {
            lzo_uint tt = t - 18;

            *op++ = 0;
            while (tt > 255)
            {
                tt -= 255;
                *op++ = 0;
                //op++;
            }

            *op++ = LZO_BYTE(tt);
        }
        do
        {
            *op++ = *ii++;
        } while (--t > 0);

       // op += t;注释过
    }
    // printf("边角料处理完毕\n");
    *op++ = M4_MARKER | 1; //17
    *op++ = 0;
    *op++ = 0;

    *out_len = op - out;
    // printf("LZO压缩执行完毕 outlen=%ld\n",*out_len);
    return LZO_E_OK;
}


__device__ int CUDA_DECOMPRESS_LZO(const lzo_bytep in, lzo_uint  in_len,
    lzo_bytep out, lzo_uintp out_len)
{

    lzo_bytep op;
    const lzo_bytep ip;
    lzo_uint t;
    const lzo_bytep m_pos;
    const lzo_bytep const ip_end = in + in_len;
    *out_len = 0;

    op = out;
    ip = in;

    if (*ip > 17) //处理特殊情况，待压缩数据长度t<238且全是不可替换数据，此时压缩后的数据第一个字节=t+17,表示之后是t个新字符
    {
        t = *ip++ - 17;//-17修正
        if (t < 4)
            goto match_next;
        //if t>4
        do *op++ = *ip++; while (--t > 0);
        goto first_literal_run;
    }

    for (;;)
    {
        t = *ip++;
        if (t >= 16)
            goto match; //是重复字段，跳转到match例程
        if (t == 0)//是新字符字段，并且个数>18 t需要+18修正
        {
            while (*ip == 0)
            {
                t += 255;
                ip++;
            }
            t += 15 + *ip++; //t还差3
        }

        //是新字符字段，但是个数<18, 需要+3修正 NEED t+3
        {
            *op++ = *ip++; *op++ = *ip++; *op++ = *ip++; //修正t+3
            do *op++ = *ip++; while (--t > 0);
        }
        //第一次 literal 替换
    first_literal_run:
        t = *ip++; //取重复字段第一个字节
        if (t >= 16) //正常情况重复字段的第一个字节就是大于等于16
            goto match;//跳转到匹配替换例程



        //BEG——————这个分支不应该在LZOX进入
        printf("进入了错误的分支,文件可能损坏\n");
        m_pos = op - (1 + M2_MAX_OFFSET);
        m_pos -= t >> 2;
        m_pos -= *ip++ << 2;
        *op++ = *m_pos++; *op++ = *m_pos++; *op++ = *m_pos;
        goto match_done;
        //END——————这个分支不应该在LZOX进入


        for (;;)
        {
        match:
            //M2
            if (t >= 64)
            {

                m_pos = op - 1;//op-1 因为distance存储时-1了，所以这里op-1是为了还原distance+1，因为是往左，所以-1！
                m_pos -= (t >> 2) & 7;
                m_pos -= *ip++ << 3;

                t = (t >> 5) - 1;//need_ip(2)

                goto copy_match;
            }
            //M3
            else if (t >= 32)
            {
                t &= 31;//t-=33; need(2)
                if (t == 0)
                {
                    while (*ip == 0)
                    {
                        t += 255;
                        ip++;
                    }
                    t += 31 + *ip++;
                }
                m_pos = op - 1;
                m_pos -= (ip[0] >> 2) + (ip[1] << 6);
                ip += 2;
            }
            //M4
            else if (t >= 16)
            {
                m_pos = op;//压缩的时候m_off没有-1,所以这里不用m_pos=op-1
                m_pos -= (t & 8) << 11;
                t &= 7;
                if (t == 0)
                {
                    while (*ip == 0)
                    {
                        t += 255;
                        ip++;

                    }
                    t += 7 + *ip++;//t还差2个
                }
                m_pos -= (ip[0] >> 2) + (ip[1] << 6);
                ip += 2;
                if (m_pos == op)//distance=0; 结束标志
                    goto eof_found;
                m_pos -= 0x4000;
            }
            //未定义的分支，正常程序运行不应该进入此分支！ 因为t>=16是必然的
            else
            {
                m_pos = op - 1;
                m_pos = op - 1;
                m_pos -= t >> 2;
                m_pos -= *ip++ << 2;
                *op++ = *m_pos++; *op++ = *m_pos;
                goto match_done;
            }

            //main CopyMatch:

            {
            copy_match:
                *op++ = *m_pos++; *op++ = *m_pos++;//修正m_len+2
                do *op++ = *m_pos++; while (--t > 0);
            }


        match_done: //重复字段处理完毕，接下来检查是否带有piggyback，如果有则进入match_next，否则退出match循环体，进入外循环，执行非piggyback新字符字段。
            t = ip[-2] & 3;
            if (t == 0)
                break;
            //有piggyback 
        match_next:
            // assert(t > 0); assert(t < 4);
#if 0
            do *op++ = *ip++; while (--t > 0);
#else
            * op++ = *ip++;
            if (t > 1) { *op++ = *ip++; if (t > 2) { *op++ = *ip++; } }
#endif
            t = *ip++;//下一重复字段，继续match循环
        }
    }

eof_found:
    *out_len = op - out;
    return (ip == ip_end ? LZO_E_OK : LZO_E_ERROR);

}


__global__ void cudaLZ(EncodeResource r) {
 
    //__shared__ unsigned char hash[32 * 1024];
    int tid = blockDim.x * blockIdx.x + threadIdx.x;//获取当前线程id
    unsigned char* mybegin = r.in + tid * r.block_size;
    size_t  myinlen = 0;
    if (r.inlen < tid * r.block_size)
        return;

    if (tid == r.remainTid) {
        if (r.remainSize != 0) {
            //printf("cuda线程%d 为边角料线程 处理的数据量为 %d\n", tid, r.remainSize);
            myinlen = r.remainSize;
        }
        else {
            myinlen = r.block_size;//不需要边角料线程
        }

    }
    else {
        myinlen = r.block_size;
    }
    __syncthreads();
    //    cudaThreadSynchronize();
    unsigned char* myout = r.out + tid * r.max_out_len;
    unsigned char* myhashtable = r.hashtable + tid * LZO_WRKMEM_SIZE;
    //myhashtable = hash + threadIdx.x * LZO_WRKMEM_SIZE;
    size_t* myoutlen = r.outlenArray + tid;
    *myoutlen = 0;
    CUDA_COMPRESS_LZO(mybegin, myinlen, myout, myoutlen, myhashtable);
   // printf("1\n");

    return;
}



void PrintGPUInfo(cudaDeviceProp& Prop) {
    //cout<<"GPU:"
    printf("%25s: %s\n", "名称",Prop.name);
    printf("%25s: %d\n", "SM流处理器数量", Prop.multiProcessorCount);
    printf("%25s: %d KHZ\n", "核心频率", Prop.clockRate);
    printf("%25s: %d\n", "SM线程束大小(", Prop.warpSize);
    printf("%25s: %d /Per Block\n", "最大线程数量", Prop.maxThreadsPerBlock);
    printf("%25s: %d /Per Grid\n", "最大板块数量 (第1维)", Prop.maxGridSize[0]);
    printf("%25s: %d /Per Grid\n", "最大板块数量 (第2维)", Prop.maxGridSize[1]);
    printf("%25s: %d /Per Grid\n", "最大板块数量 (第3维)", Prop.maxGridSize[2]);
    printf("%25s: %d MBytes\n", "GDDR5全局内存大小", Prop.totalGlobalMem/1024/1024);
    printf("%25s: %d KBytes/Per Block\n", "板块可用最大共享内存大小", Prop.sharedMemPerBlock/1024);
    printf("%25s: %d KHZ\n", "内存频率", Prop.memoryClockRate);
    printf("%25s: %d KBytes\n", "SM共享L2缓存大小", Prop.l2CacheSize/1024);
    printf("\n");

}

size_t getFileLength(string filePath) {
    ifstream infile(filePath, ifstream::binary);
    if (!infile) {
        infile.close();
        cout << "I/O Exception  No such File :" + filePath << endl;
        return -1;
    }
    infile.seekg(0, infile.end);
    size_t file_length = infile.tellg();
    infile.seekg(0, infile.beg);
    return file_length;
}

void InitGPU() {
    int NumsGpus = 0;
    cudaError_t cudaStatus;
    cudaDeviceProp Prop;
    Console::ClearScreen();
    cout << "正在查询显卡信息。。。。。。\n";
    Sleep(500);
    checkCudaErrors(cudaGetDeviceCount(&NumsGpus));
    if (NumsGpus == 0) {
        cout << "\n No NVIDA CUDA Device is available\n";
        cout << "当前设备不支持NVIDA CUDA 无可用NVIDA显卡\n";
        exit(-1);
    }
    for (int i = 0; i < NumsGpus; ++i) {
        cudaStatus = cudaGetDeviceProperties(&Prop, i);
        if (cudaStatus != cudaSuccess) {
            cout << "Fail to get cuda device properties\n";
            exit(0);
        }
        printf("GPU \[%d\]:\n\n", i);
        PrintGPUInfo(Prop);
    }
    string select;
    int gpu_index;
select_gpu:
    cout << "\n选择需要运行的GPU\n";
    cin >> select;
    try {
        gpu_index = stoul(select.c_str());
    }
    catch (std::invalid_argument e) {
        cout << "错误的输入\n";
        goto select_gpu;
    }
    if (!(0 <= gpu_index && gpu_index < NumsGpus)) {
        cout << "错误的输入\n";
        goto select_gpu;
    }


    checkCudaErrors(cudaGetDeviceProperties(&Prop, gpu_index));//获取0号GPU的信息

    checkCudaErrors(cudaSetDevice(gpu_index)); //选择在select号GPU上执行本程序

}

  
//函数返回执行的毫秒数
int CallCudaLZO_Encode(unsigned char* in_CPU, size_t inlen, vector<outputUint>& out_CPU, int N, size_t blocksize) {
    unsigned char* GPU_in = NULL;
    unsigned char* GPU_out = NULL;
    unsigned short int* GPU_HashTable = NULL;
    size_t* GPU_outlenArray = NULL;
    cudaError_t error;
    float elapsedTime = 0.0f;
    int remainSize = inlen % blocksize;//剩余边角料大小
    int threadNum = (inlen + blocksize - 1) / (blocksize);//取上整 
    int remainTid = threadNum - 1;//计算边角料线程的id，当remainSize=0时不需要边角料线程

    //把线程数量调大一点
    int blockdim = g_config.block_dim;
    threadNum = ALIGNE_N(threadNum, blockdim);
    EncodeResource r = { 0 };

    
        error = cudaMalloc(&GPU_in, inlen + 1024);

        if (error != cudaSuccess) {
            printf("WARNING::内存分配失败，显存不足,请尝试使用cpu压缩\n");
            return -1;
        }
        //[512k][512k].....[512k][remain]
        int max_out_len_per_thread = ( 48*1024 + LZO_SAFE_OUT_LEN(blocksize));
        int GPU_out_buffer_size = max_out_len_per_thread * threadNum;
        error = cudaMalloc(&GPU_out, GPU_out_buffer_size);
        //[LZO_SAFE(512k)][pading][LZO_SAFE(512k)][pading]......[LZO_SAFE(remain)][pading]
        if (error != cudaSuccess) {
            printf("WARNING::内存分配失败，显存不足,请尝试使用cpu压缩\n");
            cudaFree(GPU_in);
            return -1;
        }
        finish_rate = 0.2f;
        error = cudaMalloc(&GPU_HashTable, LZO_WRKMEM_SIZE * threadNum);//每个线程一个独立的16k*2=32k字节哈希表
        //[hash32k0][hash32k].....[hash32k N-1]
        if (error != cudaSuccess) {
            printf("WARNING::内存分配失败，显存不足,请尝试使用cpu压缩\n");
            cudaFree(GPU_in);
            cudaFree(GPU_out);
            return -1;
        }
        error = cudaMalloc(&GPU_outlenArray, sizeof(size_t) * threadNum);//每个线程一个outlen
        //outlen0,outlen1,outlen2,.....outlen N-1
        if (error != cudaSuccess) {
            printf("WARNING::内存分配失败，显存不足,请尝试使用cpu压缩\n");
            cudaFree(GPU_in);
            cudaFree(GPU_out);
            cudaFree(GPU_HashTable);
            return -1;
        }

        //挨个初始化
        //把输入待压缩数据传递到显存
        checkCudaErrors(cudaMemcpy(GPU_in, in_CPU, inlen, cudaMemcpyHostToDevice));
        //给显存的out内存初始化为0
        checkCudaErrors(cudaMemset(GPU_out, 0, GPU_out_buffer_size));
        //给显存哈希表初始化
        checkCudaErrors(cudaMemset(GPU_HashTable, 0, LZO_WRKMEM_SIZE * threadNum));
        //给outlenAray初始化
        checkCudaErrors(cudaMemset(GPU_outlenArray, 0, sizeof(size_t) * threadNum));

   
    finish_rate = 0.25f;
    //给资源参数赋值
    r.block_size = blocksize;
    r.remainTid = remainTid;
    r.remainSize = remainSize;
    r.in = GPU_in;
    r.out = GPU_out;
    r.hashtable = (unsigned char*)GPU_HashTable;
    r.outlenArray = GPU_outlenArray;
    r.max_out_len = max_out_len_per_thread;
    r.inlen = inlen;
    //////////////////////////////////
    cudaEvent_t time1, time2;
    cudaEventCreate(&time1);
    cudaEventCreate(&time2);
    cudaEventRecord(time1);
    cudaLZ << <threadNum/blockdim, blockdim >> > (r);//调用压缩

   /////////////////////////////
    cudaDeviceSynchronize();//等待同步
    finish_rate = 0.75f;
    cudaEventRecord(time2);
    checkCudaErrors(cudaEventSynchronize(time1));
    checkCudaErrors(cudaEventSynchronize(time2));
    checkCudaErrors(cudaEventElapsedTime(&elapsedTime, time1, time2));
  //  cout << "CUDA 耗时: " << elapsedTime << "ms" << endl;
    //将输出长度数组outlenArray拷贝回CPU内存
    size_t* CPU_outlenArray = new size_t[threadNum];
    memset(CPU_outlenArray, 0, sizeof(size_t) * threadNum);
    checkCudaErrors(cudaMemcpy(CPU_outlenArray, GPU_outlenArray, sizeof(size_t) * threadNum, cudaMemcpyDeviceToHost));
    //for (int i = 0; i < threadNum; ++i)
    //    cout << "outlen=" << CPU_outlenArray[i] << endl;
    //根据输出长度数组，申请对应的cpu out输出内存，并从显存拷贝回来
    
    for (int i = 0; i < threadNum; ++i) {
        outputUint temp;
        temp.out_len = CPU_outlenArray[i];
        temp.out_buffer = new unsigned char[temp.out_len + 1024];
        memset(temp.out_buffer, 0, temp.out_len + 1024);
        //从GPU_out[i*max_out_len_perthread,(i+1)*max_out_len_perthread)一共max_out_len_perthread大小的内存为当前线程可用的输出空间
        checkCudaErrors(cudaMemcpy(temp.out_buffer, GPU_out + (i * max_out_len_per_thread), temp.out_len, cudaMemcpyDeviceToHost));
        out_CPU.push_back(temp);
       // debug2.push_back(out_CPU[i].out_len);
    }
    finish_rate = 0.90f;
    // cudaDeviceSynchronize();//等待同步
     //释放显存
    cudaFree(GPU_in);
    cudaFree(GPU_out);
    cudaFree(GPU_HashTable);
    cudaFree(GPU_outlenArray);
    return int(elapsedTime);
}

//应该先在外部把各个分区的数据读入到输入向量中
// CPU_out[block_size*Nparts];
//Nparts:节区数量
//blocksize:数据块大小，用于解压缩正确分配内存
//边角料解决：直接分配到块大小，多出来肯定够了。
bool CallCudaLZO_Decode(vector<inputUint>& compressed_data, unsigned char* CPU_out, int Npats, size_t blocksize)
{
    return true;

}

bool ivan_comp(unsigned char* a, unsigned char* b, size_t len) {

    for (int i = 0; i < len; ++i)
        if (a[i] != b[i])
            return false;

    return true;
}
//__global__ void cudabwt()//(int * arr, int *seq,int n,int alphabetsize) 
//{
//    int strlength;
//    unsigned char* str = NULL;
//    strlength = 6;
//    str = new unsigned char[strlength];
//    str[0] = 'a'; str[1] = 'b'; str[2] = 'a'; str[3] = 'b'; str[4] = 'c';
//    str[strlength - 1] = '$';
//    unsigned char* out = new unsigned char[strlength];
//    int *S=new int [6];
//    int *SA =new int[6];
//    for (int i = 0; i < 6; ++i) {
//        S[i] = 0; SA[i] = 0;
//    }
//    for (int i = 0; i < strlength - 1; ++i) {
//        S[i] = str[i] + 1; //字符从ASCII8bit[0,255]重新映射到ASCII16bit[1,256] 字符集大小+1
//    }
//    S[strlength - 1] = 0;//添加一个ASCII16 bit :0号最小字符
//    for (int i = 0; i < 6; ++i) {
//        printf("%d ", S[i]);
//    }
//    printf("\n开始调用cuda_bwt_encodd\n");
//    //cuda_create_suffix_array(SA, S, strlength, 257);
//    //int* SA = new int[strlength];
//    int index =  cuda_bwtEncode(str, out, strlength, S, SA);
//
//    printf("调用cuda_create_sa结束\n");
//    for (int i = 0; i < 6; ++i) {
//        printf("%d\n", SA[i]);
//    }
//    printf("%s\n", out);
//    unsigned char decode[6];
//    int *ca = new int[257];
//    for (int i = 0; i < 257; ++i)
//        ca[i] = 0;
//    int ja[6] = { 0 };
//    cuda_bwtDecode(out, decode, ca, ja, strlength, index);
//    printf("%s\n", decode);
//}
//N卡不支持非对齐访问，需要挨个拷贝 访存效率并不高
