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
        // printf("p %p 4�ֽڶ���\n", src);
        return *(volatile int*)src;
    }
    else if (((uint64_t)src & (uint64_t)(1)) == 0) {
        ((unsigned short*)&result)[0] = ((unsigned short*)src)[0];
        ((unsigned short*)&result)[1] = ((unsigned short*)src)[1];
        return result;
    }
    else {//�����Ż�����2�ֽڶ���ʱ�ļ���
     //   printf("p %p ����4�ֽڶ���\n", src);
        ((unsigned char*)&result)[0] = src[0];
        ((unsigned char*)&result)[1] = src[1];
        ((unsigned char*)&result)[2] = src[2];
        ((unsigned char*)&result)[3] = src[3];
        //printf("���ֽڷ���4�ֽڵ�Ԫ�ɹ�,4�ֽ�����Ϊ %u\n", result);
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
    const lzo_bytep const ip_end = in + in_len - 20; //ll>20��-20�ǰ�ȫ��
    const lzo_bytep ii;
    lzo_dict_p const dict = (lzo_dict_p)wrkmem;

    op = out;
    ip = in;
    ii = ip;

    ip += ti < 4 ? 4 - ti : 0;
    for (;;)
    {

        const lzo_bytep m_pos;
        //��Deterministicģʽ�� 2�ι�ϣ���ԣ�����Ĭ�ϲ�����


        lzo_uint m_off;
        lzo_uint m_len;
        {
            lzo_uint32_t dv;
            lzo_uint dindex;
            literal:
            ip += 1 + ((ip - ii) >> 5); //����ط���ȡ���˴��Ż����Ӵ�ѹ���̶�
            //ip += 1;

        next:

            if (ip >= ip_end) //���������˷�Χ����Ҫ����
                break;
            // printf("try GET_LE32 from %p \n", ip);
            dv = get4Byte(ip);
            //dv = UA_GET_LE32(ip); //ȡip��4�ֽڵ����ݵ�dv
           // printf("GET_LE32 sucess dv is %u\n",dv);
            dindex = DINDEX(dv, ip);//get hashKey
            m_pos = in + dict[dindex];


            dict[dindex] = (unsigned short int) ((lzo_uint)(ip - in));

            if (dv != get4Byte(m_pos))//��ײ ����ֱ�ӵ�������������
                goto literal;
        }

        //�������ַ� ��������ַ�������+���ַ�1�����ַ�2�����ַ�3
        ii -= ti; ti = 0; //����iiָ�룬����һ�����ݿ�ĩβδ������ַ���
        {
            lzo_uint t = ip - ii;

            if (t != 0)
            {
                if (t <= 3)
                {
                    op[-2] = LZO_BYTE(op[-2] | t);

                    { do *op++ = *ii++; while (--t > 0); } //�����������֧�ַǶ������ݷ��� ��ʱ��Ҫ����ʹ�����ֽڷ���ָ�Ч�ʽ������
                }

                else
                {
                    if (t <= 18)
                        *op++ = LZO_BYTE(t - 3); //��ʾ���ַ��������Ǹ��ֽڵ�ֵҪС��16���ͱ�ʾ�ظ��ֶε��ֽ����ֿ�������ΪС��3ʱ��piggyback,
                    //���Դ���3ʱ �������޿��Զ����ʾ3���ַ���������18���ַ�
                    else
                    {
                        lzo_uint tt = t - 18; //����ʱҪ+18
                        *op++ = 0;
                        while (tt > 255)
                        {
                            tt -= 255;
                            UA_SET1(op, 0);//Ӧ��û���⣿1�ֽ��Ƕ����
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
        m_len = 4; //���������������ڼ����ظ�����len
        {
            //���صĳ���ƥ���㷨�������ϵ����ô������ΪGPU���ܲ�֧�ַǶ�����ʣ����ﵥ�ֽڷô����ȫ�ġ�
            // CPU�Ͽ�����UA���������١�
                      // printf("���Գ���ƥ��\n");
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
            // printf("�ظ�����ƥ��ɹ�\n");
        }
    m_len_done:
        m_off = ip - m_pos;//get Distance
        ip += m_len;//����ip
        ii = ip;//����ii
        //M2:len<=8 && distance<=2k   
        if (m_len <= M2_MAX_LEN && m_off <= M2_MAX_OFFSET)
        {
            m_off -= 1; //m_off ��m_lenʵ�ʴ洢��Χ[1,8] ��[1,2048]
            *op++ = LZO_BYTE(((m_len - 1) << 5) | ((m_off & 7) << 2));
            *op++ = LZO_BYTE(m_off >> 3);
        }
        //M3:distance<=16k   
        else if (m_off <= M3_MAX_OFFSET)
        {
            m_off -= 1;
            if (m_len <= M3_MAX_LEN)
                *op++ = LZO_BYTE(M3_MARKER | (m_len - 2)); //M3��һ���ֽڵĵ�5λΪlen��len����ʾ��ΧΪ31������Ϊlen��Ȼ����2���������ޱ�ʾ��Χ+2
            //���ǵ�m_len����>=4,�����ΧΪ[4,33]
            else
            {
                m_len -= M3_MAX_LEN; //����ʱ+33
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
            m_off -= 0x4000; //��Ϊ֪���������Ҳ��16k���ʿ��Զ���ȥ16k�����ʾ��Χ
            if (m_len <= M4_MAX_LEN) //������3bit��ʾ
                *op++ = LZO_BYTE(M4_MARKER | ((m_off >> 11) & 8) | (m_len - 2));
            else
            {
                m_len -= M4_MAX_LEN;//����ʱ��ԭ
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
        ll = LZO_MIN(ll, 49152); //DETERMINISTICģʽ�� src��������С>48kʱ�ֿ鴦��
#endif
        ll_end = (lzo_uint)ip + ll;
        if ((ll_end + ((t + ll) >> 5)) <= ll_end || (const lzo_bytep)(ll_end + ((t + ll) >> 5)) <= ip + ll)
            break;
#if (LZO_DETERMINISTIC)

        //�޸Ĺ���memset�߼�
        //int size = (lzo_uint)1 << D_BITS;
        //Reset��ϣ�� ���룡������
        unsigned short int* p = (unsigned short int*)wrkmem;
        for (int i = 0; i <1u<<D_BITS; ++i) {
            p[i] = 0;
        }

        //memset(wrkmem, 0, ((lzo_uint)1 << D_BITS) * sizeof(lzo_dict_t)); //
#endif
        //printf("���� do_compress\n");
        t = device_do_compress(ip, ll, op, out_len, t, wrkmem);
        __syncthreads();
        // printf("do_compress�ɹ�\n");
        ip += ll;
        op += *out_len;
        l -= ll;
    }
    t += l;
    // printf("���Դ���߽���\n");
    if (t > 0)
    {
        __syncthreads();
        const lzo_bytep ii = in + in_len - t;

        if (op == out && t <= 238) //sp��֧
            *op++ = LZO_BYTE(17 + t);//���ҽ���do_compressû������κ�����ѹ����opָ��û���ƶ����ṩ�����ݿ�û���κο����滻���ظ��ֶΣ��Ż����˷�֧
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

       // op += t;ע�͹�
    }
    // printf("�߽��ϴ������\n");
    *op++ = M4_MARKER | 1; //17
    *op++ = 0;
    *op++ = 0;

    *out_len = op - out;
    // printf("LZOѹ��ִ����� outlen=%ld\n",*out_len);
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

    if (*ip > 17) //���������������ѹ�����ݳ���t<238��ȫ�ǲ����滻���ݣ���ʱѹ��������ݵ�һ���ֽ�=t+17,��ʾ֮����t�����ַ�
    {
        t = *ip++ - 17;//-17����
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
            goto match; //���ظ��ֶΣ���ת��match����
        if (t == 0)//�����ַ��ֶΣ����Ҹ���>18 t��Ҫ+18����
        {
            while (*ip == 0)
            {
                t += 255;
                ip++;
            }
            t += 15 + *ip++; //t����3
        }

        //�����ַ��ֶΣ����Ǹ���<18, ��Ҫ+3���� NEED t+3
        {
            *op++ = *ip++; *op++ = *ip++; *op++ = *ip++; //����t+3
            do *op++ = *ip++; while (--t > 0);
        }
        //��һ�� literal �滻
    first_literal_run:
        t = *ip++; //ȡ�ظ��ֶε�һ���ֽ�
        if (t >= 16) //��������ظ��ֶεĵ�һ���ֽھ��Ǵ��ڵ���16
            goto match;//��ת��ƥ���滻����



        //BEG�����������������֧��Ӧ����LZOX����
        printf("�����˴���ķ�֧,�ļ�������\n");
        m_pos = op - (1 + M2_MAX_OFFSET);
        m_pos -= t >> 2;
        m_pos -= *ip++ << 2;
        *op++ = *m_pos++; *op++ = *m_pos++; *op++ = *m_pos;
        goto match_done;
        //END�����������������֧��Ӧ����LZOX����


        for (;;)
        {
        match:
            //M2
            if (t >= 64)
            {

                m_pos = op - 1;//op-1 ��Ϊdistance�洢ʱ-1�ˣ���������op-1��Ϊ�˻�ԭdistance+1����Ϊ����������-1��
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
                m_pos = op;//ѹ����ʱ��m_offû��-1,�������ﲻ��m_pos=op-1
                m_pos -= (t & 8) << 11;
                t &= 7;
                if (t == 0)
                {
                    while (*ip == 0)
                    {
                        t += 255;
                        ip++;

                    }
                    t += 7 + *ip++;//t����2��
                }
                m_pos -= (ip[0] >> 2) + (ip[1] << 6);
                ip += 2;
                if (m_pos == op)//distance=0; ������־
                    goto eof_found;
                m_pos -= 0x4000;
            }
            //δ����ķ�֧�������������в�Ӧ�ý���˷�֧�� ��Ϊt>=16�Ǳ�Ȼ��
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
                *op++ = *m_pos++; *op++ = *m_pos++;//����m_len+2
                do *op++ = *m_pos++; while (--t > 0);
            }


        match_done: //�ظ��ֶδ�����ϣ�����������Ƿ����piggyback������������match_next�������˳�matchѭ���壬������ѭ����ִ�з�piggyback���ַ��ֶΡ�
            t = ip[-2] & 3;
            if (t == 0)
                break;
            //��piggyback 
        match_next:
            // assert(t > 0); assert(t < 4);
#if 0
            do *op++ = *ip++; while (--t > 0);
#else
            * op++ = *ip++;
            if (t > 1) { *op++ = *ip++; if (t > 2) { *op++ = *ip++; } }
#endif
            t = *ip++;//��һ�ظ��ֶΣ�����matchѭ��
        }
    }

eof_found:
    *out_len = op - out;
    return (ip == ip_end ? LZO_E_OK : LZO_E_ERROR);

}


__global__ void cudaLZ(EncodeResource r) {
 
    //__shared__ unsigned char hash[32 * 1024];
    int tid = blockDim.x * blockIdx.x + threadIdx.x;//��ȡ��ǰ�߳�id
    unsigned char* mybegin = r.in + tid * r.block_size;
    size_t  myinlen = 0;
    if (r.inlen < tid * r.block_size)
        return;

    if (tid == r.remainTid) {
        if (r.remainSize != 0) {
            //printf("cuda�߳�%d Ϊ�߽����߳� �����������Ϊ %d\n", tid, r.remainSize);
            myinlen = r.remainSize;
        }
        else {
            myinlen = r.block_size;//����Ҫ�߽����߳�
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
    printf("%25s: %s\n", "����",Prop.name);
    printf("%25s: %d\n", "SM������������", Prop.multiProcessorCount);
    printf("%25s: %d KHZ\n", "����Ƶ��", Prop.clockRate);
    printf("%25s: %d\n", "SM�߳�����С(", Prop.warpSize);
    printf("%25s: %d /Per Block\n", "����߳�����", Prop.maxThreadsPerBlock);
    printf("%25s: %d /Per Grid\n", "��������� (��1ά)", Prop.maxGridSize[0]);
    printf("%25s: %d /Per Grid\n", "��������� (��2ά)", Prop.maxGridSize[1]);
    printf("%25s: %d /Per Grid\n", "��������� (��3ά)", Prop.maxGridSize[2]);
    printf("%25s: %d MBytes\n", "GDDR5ȫ���ڴ��С", Prop.totalGlobalMem/1024/1024);
    printf("%25s: %d KBytes/Per Block\n", "������������ڴ��С", Prop.sharedMemPerBlock/1024);
    printf("%25s: %d KHZ\n", "�ڴ�Ƶ��", Prop.memoryClockRate);
    printf("%25s: %d KBytes\n", "SM����L2�����С", Prop.l2CacheSize/1024);
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
    cout << "���ڲ�ѯ�Կ���Ϣ������������\n";
    Sleep(500);
    checkCudaErrors(cudaGetDeviceCount(&NumsGpus));
    if (NumsGpus == 0) {
        cout << "\n No NVIDA CUDA Device is available\n";
        cout << "��ǰ�豸��֧��NVIDA CUDA �޿���NVIDA�Կ�\n";
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
    cout << "\nѡ����Ҫ���е�GPU\n";
    cin >> select;
    try {
        gpu_index = stoul(select.c_str());
    }
    catch (std::invalid_argument e) {
        cout << "���������\n";
        goto select_gpu;
    }
    if (!(0 <= gpu_index && gpu_index < NumsGpus)) {
        cout << "���������\n";
        goto select_gpu;
    }


    checkCudaErrors(cudaGetDeviceProperties(&Prop, gpu_index));//��ȡ0��GPU����Ϣ

    checkCudaErrors(cudaSetDevice(gpu_index)); //ѡ����select��GPU��ִ�б�����

}

  
//��������ִ�еĺ�����
int CallCudaLZO_Encode(unsigned char* in_CPU, size_t inlen, vector<outputUint>& out_CPU, int N, size_t blocksize) {
    unsigned char* GPU_in = NULL;
    unsigned char* GPU_out = NULL;
    unsigned short int* GPU_HashTable = NULL;
    size_t* GPU_outlenArray = NULL;
    cudaError_t error;
    float elapsedTime = 0.0f;
    int remainSize = inlen % blocksize;//ʣ��߽��ϴ�С
    int threadNum = (inlen + blocksize - 1) / (blocksize);//ȡ���� 
    int remainTid = threadNum - 1;//����߽����̵߳�id����remainSize=0ʱ����Ҫ�߽����߳�

    //���߳���������һ��
    int blockdim = g_config.block_dim;
    threadNum = ALIGNE_N(threadNum, blockdim);
    EncodeResource r = { 0 };

    
        error = cudaMalloc(&GPU_in, inlen + 1024);

        if (error != cudaSuccess) {
            printf("WARNING::�ڴ����ʧ�ܣ��Դ治��,�볢��ʹ��cpuѹ��\n");
            return -1;
        }
        //[512k][512k].....[512k][remain]
        int max_out_len_per_thread = ( 48*1024 + LZO_SAFE_OUT_LEN(blocksize));
        int GPU_out_buffer_size = max_out_len_per_thread * threadNum;
        error = cudaMalloc(&GPU_out, GPU_out_buffer_size);
        //[LZO_SAFE(512k)][pading][LZO_SAFE(512k)][pading]......[LZO_SAFE(remain)][pading]
        if (error != cudaSuccess) {
            printf("WARNING::�ڴ����ʧ�ܣ��Դ治��,�볢��ʹ��cpuѹ��\n");
            cudaFree(GPU_in);
            return -1;
        }
        finish_rate = 0.2f;
        error = cudaMalloc(&GPU_HashTable, LZO_WRKMEM_SIZE * threadNum);//ÿ���߳�һ��������16k*2=32k�ֽڹ�ϣ��
        //[hash32k0][hash32k].....[hash32k N-1]
        if (error != cudaSuccess) {
            printf("WARNING::�ڴ����ʧ�ܣ��Դ治��,�볢��ʹ��cpuѹ��\n");
            cudaFree(GPU_in);
            cudaFree(GPU_out);
            return -1;
        }
        error = cudaMalloc(&GPU_outlenArray, sizeof(size_t) * threadNum);//ÿ���߳�һ��outlen
        //outlen0,outlen1,outlen2,.....outlen N-1
        if (error != cudaSuccess) {
            printf("WARNING::�ڴ����ʧ�ܣ��Դ治��,�볢��ʹ��cpuѹ��\n");
            cudaFree(GPU_in);
            cudaFree(GPU_out);
            cudaFree(GPU_HashTable);
            return -1;
        }

        //������ʼ��
        //�������ѹ�����ݴ��ݵ��Դ�
        checkCudaErrors(cudaMemcpy(GPU_in, in_CPU, inlen, cudaMemcpyHostToDevice));
        //���Դ��out�ڴ��ʼ��Ϊ0
        checkCudaErrors(cudaMemset(GPU_out, 0, GPU_out_buffer_size));
        //���Դ��ϣ���ʼ��
        checkCudaErrors(cudaMemset(GPU_HashTable, 0, LZO_WRKMEM_SIZE * threadNum));
        //��outlenAray��ʼ��
        checkCudaErrors(cudaMemset(GPU_outlenArray, 0, sizeof(size_t) * threadNum));

   
    finish_rate = 0.25f;
    //����Դ������ֵ
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
    cudaLZ << <threadNum/blockdim, blockdim >> > (r);//����ѹ��

   /////////////////////////////
    cudaDeviceSynchronize();//�ȴ�ͬ��
    finish_rate = 0.75f;
    cudaEventRecord(time2);
    checkCudaErrors(cudaEventSynchronize(time1));
    checkCudaErrors(cudaEventSynchronize(time2));
    checkCudaErrors(cudaEventElapsedTime(&elapsedTime, time1, time2));
  //  cout << "CUDA ��ʱ: " << elapsedTime << "ms" << endl;
    //�������������outlenArray������CPU�ڴ�
    size_t* CPU_outlenArray = new size_t[threadNum];
    memset(CPU_outlenArray, 0, sizeof(size_t) * threadNum);
    checkCudaErrors(cudaMemcpy(CPU_outlenArray, GPU_outlenArray, sizeof(size_t) * threadNum, cudaMemcpyDeviceToHost));
    //for (int i = 0; i < threadNum; ++i)
    //    cout << "outlen=" << CPU_outlenArray[i] << endl;
    //��������������飬�����Ӧ��cpu out����ڴ棬�����Դ濽������
    
    for (int i = 0; i < threadNum; ++i) {
        outputUint temp;
        temp.out_len = CPU_outlenArray[i];
        temp.out_buffer = new unsigned char[temp.out_len + 1024];
        memset(temp.out_buffer, 0, temp.out_len + 1024);
        //��GPU_out[i*max_out_len_perthread,(i+1)*max_out_len_perthread)һ��max_out_len_perthread��С���ڴ�Ϊ��ǰ�߳̿��õ�����ռ�
        checkCudaErrors(cudaMemcpy(temp.out_buffer, GPU_out + (i * max_out_len_per_thread), temp.out_len, cudaMemcpyDeviceToHost));
        out_CPU.push_back(temp);
       // debug2.push_back(out_CPU[i].out_len);
    }
    finish_rate = 0.90f;
    // cudaDeviceSynchronize();//�ȴ�ͬ��
     //�ͷ��Դ�
    cudaFree(GPU_in);
    cudaFree(GPU_out);
    cudaFree(GPU_HashTable);
    cudaFree(GPU_outlenArray);
    return int(elapsedTime);
}

//Ӧ�������ⲿ�Ѹ������������ݶ��뵽����������
// CPU_out[block_size*Nparts];
//Nparts:��������
//blocksize:���ݿ��С�����ڽ�ѹ����ȷ�����ڴ�
//�߽��Ͻ����ֱ�ӷ��䵽���С��������϶����ˡ�
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
//        S[i] = str[i] + 1; //�ַ���ASCII8bit[0,255]����ӳ�䵽ASCII16bit[1,256] �ַ�����С+1
//    }
//    S[strlength - 1] = 0;//���һ��ASCII16 bit :0����С�ַ�
//    for (int i = 0; i < 6; ++i) {
//        printf("%d ", S[i]);
//    }
//    printf("\n��ʼ����cuda_bwt_encodd\n");
//    //cuda_create_suffix_array(SA, S, strlength, 257);
//    //int* SA = new int[strlength];
//    int index =  cuda_bwtEncode(str, out, strlength, S, SA);
//
//    printf("����cuda_create_sa����\n");
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
//N����֧�ַǶ�����ʣ���Ҫ�������� �ô�Ч�ʲ�����
