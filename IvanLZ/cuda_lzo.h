#pragma once
#include <vector>
#include <string>
#include "lzox64.h"
#include "FileModuleDef.h"
using namespace std;
typedef struct {
    unsigned char* in;//ȫ�������ڴ�
    unsigned char* out;//ȫ������ڴ�
    size_t block_size;//���С
    size_t remainSize;//�߽��ϴ�С
    int remainTid;//�߽����߳�id
    unsigned char* hashtable;//ȫ�ֹ�ϣ��
    size_t* outlenArray;//�����С
    int max_out_len;
    size_t inlen;
    //  int* S;//ȫ��ASCII16�ַ���
      //int* SA;//ȫ�ֺ�׺����
}EncodeResource;

void InitGPU();

int CallCudaLZO_Encode(unsigned char* in_CPU, size_t inlen, vector<outputUint>& out_CPU, int N, size_t blocksize);
bool ivan_comp(unsigned char* a, unsigned char* b, size_t len);
//N����֧�ַǶ�����ʣ���Ҫ�������� �ô�Ч�ʲ�����
__device__ uint32_t  get4Byte(volatile const unsigned char* src);

__device__ lzo_uint device_do_compress(const lzo_bytep in, lzo_uint  in_len,
    lzo_bytep out, lzo_uintp out_len,
    lzo_uint  ti, lzo_voidp wrkmem);
__device__ int CUDA_COMPRESS_LZO(const lzo_bytep in, lzo_uint  in_len,
    lzo_bytep out, lzo_uintp out_len,
    unsigned char* wrkmem);

__device__ int CUDA_DECOMPRESS_LZO(const lzo_bytep in, lzo_uint  in_len,
    lzo_bytep out, lzo_uintp out_len);

__global__ void cudaLZ(EncodeResource r);




