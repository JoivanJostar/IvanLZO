#pragma once
#include <vector>
#include <string>
#include "lzox64.h"
#include "FileModuleDef.h"
using namespace std;
typedef struct {
    unsigned char* in;//全局输入内存
    unsigned char* out;//全局输出内存
    size_t block_size;//块大小
    size_t remainSize;//边角料大小
    int remainTid;//边角料线程id
    unsigned char* hashtable;//全局哈希表
    size_t* outlenArray;//输出大小
    int max_out_len;
    size_t inlen;
    //  int* S;//全局ASCII16字符串
      //int* SA;//全局后缀数组
}EncodeResource;

void InitGPU();

int CallCudaLZO_Encode(unsigned char* in_CPU, size_t inlen, vector<outputUint>& out_CPU, int N, size_t blocksize);
bool ivan_comp(unsigned char* a, unsigned char* b, size_t len);
//N卡不支持非对齐访问，需要挨个拷贝 访存效率并不高
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




