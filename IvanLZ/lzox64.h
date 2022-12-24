#pragma once
#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#include "helper_cuda.h"
#define __LZO_MMODEL            /*empty*/
typedef unsigned __int64   lzo_uint;
typedef __int64            lzo_int;



#ifndef LZO_DEF
#define LZO_DEF
//避免const指针定义时出错，使用宏定义代替typedef
#define lzo_bytep               unsigned char __LZO_MMODEL *
#define lzo_charp               char __LZO_MMODEL *
#define lzo_voidp               void __LZO_MMODEL *
#define lzo_shortp              short __LZO_MMODEL *
#define lzo_ushortp             unsigned short __LZO_MMODEL *
#define lzo_intp                lzo_int __LZO_MMODEL *
#define lzo_uintp               lzo_uint __LZO_MMODEL *
#define lzo_voidpp              lzo_voidp __LZO_MMODEL *
#define lzo_bytepp              lzo_bytep __LZO_MMODEL *
#define lzo_uint8_t             unsigned char
#define lzo_uint16_t            unsigned short int
#define lzo_uint32_t            unsigned long int
#define lzo_uint64_t            unsigned long long int
#define lzo_dict_t              lzo_uint16_t
#define lzo_dict_p              lzo_dict_t *

#define LZO_DETERMINISTIC 1

#define M2_MIN_LEN      3
#define M2_MAX_LEN      8
#define M3_MIN_LEN      3
#define M3_MAX_LEN      33
#define M4_MIN_LEN      3
#define M4_MAX_LEN      9
#define M2_MARKER       64
#define M3_MARKER       32
#define M4_MARKER       16
#define M2_MAX_OFFSET   0x0800
#define M3_MAX_OFFSET   0x4000
#define M4_MAX_OFFSET   0xbfff
#define LZO_E_OK 1
#define LZO_E_ERROR -1
#define D_BITS 9 //哈希表容量=1<<14  这里为16k的容量，一个条目占用4字节 增大此值可以一定程度上加大压缩率
#define LZO_WRKMEM_SIZE   ((lzo_uint32_t) ( ((lzo_uint)(1u<<D_BITS))  *sizeof( lzo_dict_t)) )
#define LZO_SAFE_OUT_LEN(in_len) (in_len + in_len/16  + 64 + 3 ) //对于compress过程建议先调用此宏获取安全的输出缓冲区大小
#define ALIGNE_N(ptr,n) ( (ptr+n-1)&~(n-1) ) //将一个指针向N字节边界对齐，N必须为2的幂次


#define LZO_BLOCK_BEGIN           do {
#define LZO_BLOCK_END             }while(0)
#define LZO_BYTE(x) ( (unsigned char)(x))
#define LZO_MIN(a,b)  ((a) <= (b) ? (a) : (b))
#define DINDEX(dv,p)     ( (lzo_uint)  (  ( ( (lzo_uint)(0x1824429d*dv) )>>(32-D_BITS) )  & ((1u<<D_BITS)-1  ) )) //Hash Function 

//内存读写操作为了保证某些实时性场景下的线程安全，建议对所需访问的内存地址（指针）设置为volatile类型
#define UA_SET1(dd,cc) \
    LZO_BLOCK_BEGIN \
    volatile unsigned char * d__1 = (volatile unsigned char *) (void *) (dd); \
    d__1[0] = LZO_BYTE(cc); \
    LZO_BLOCK_END

#define UA_GET_LE32(ss)    (* (const volatile unsigned __int32 * ) (const void *) (ss))
#define UA_GET_NE64(ss)    (* (const volatile unsigned __int64 * ) (const void *) (ss))
#define LZO_MEMOPS_COPY8(dd,ss) \
    * (volatile unsigned __int64  *) (void *) (dd) = * (const volatile unsigned __int64*) (const void *) (ss)

#define LZO_MEMOPS_COPY4(dd,ss) \
    * (volatile unsigned __int32 *) (void *) (dd) = * (const volatile unsigned __int32 *) (const void *) (ss)


#define UA_COPY4            LZO_MEMOPS_COPY4
#define UA_COPY8            LZO_MEMOPS_COPY8
#define UA_COPYN(dd,ss,nn) \
    LZO_BLOCK_BEGIN \
    unsigned char * d__n = ( unsigned char *) (void *) (dd); \
    const unsigned char * s__n = (const unsigned char *) (const void *) (ss); \
    lzo_uint n__n = (nn); \
    while ((void)0, n__n >= 8) { LZO_MEMOPS_COPY8(d__n, s__n); d__n += 8; s__n += 8; n__n -= 8; } \
    if ((void)0, n__n >= 4) { LZO_MEMOPS_COPY4(d__n, s__n); d__n += 4; s__n += 4; n__n -= 4; } \
    if ((void)0, n__n > 0) do { *d__n++ = *s__n++; } while (--n__n > 0); \
    LZO_BLOCK_END

#endif // !LZO_DEF




int Ivan_COMPRESS_LZO(const lzo_bytep in, lzo_uint  in_len,
    lzo_bytep out, lzo_uintp out_len,
    lzo_voidp wrkmem);

int Ivan_DECOMPRESS_LZO(const lzo_bytep in, lzo_uint  in_len,
    lzo_bytep out, lzo_uintp out_len);

/// <summary>
/// 无损数据压缩，将in缓冲区中的数据压缩后放入out缓冲区
/// 请自行分配足够的缓冲区内存
/// 某些情况下数据被该算法压缩后可能比原来更长
/// 请保证out缓冲区分配的内存比in略大一些
/// 建议调用LZO_SAFE_OUT_LEN来获取输出缓冲区的安全大小
/// </summary>
/// <param name="in"></param>输入缓冲区
/// <param name="in_len"></param>输入缓冲区长度
/// <param name="out"></param>输出缓冲区
/// <param name="out_len"></param>指向输出长度的指针，该函数执行完毕后会将输出数据的长度存储到该指针指向的内容
/// <returns></returns>LZO_E_OK:成功  LZO_E_ERROR:出错
__forceinline int Ivan_Compress(const lzo_bytep in, lzo_uint  in_len,
    lzo_bytep out, lzo_uintp out_len) {

    unsigned char* lzo_wrkmem = new unsigned char[ALIGNE_N(LZO_WRKMEM_SIZE, sizeof(size_t))];
    int result = Ivan_COMPRESS_LZO(in, in_len, out, out_len, lzo_wrkmem);
    delete[] lzo_wrkmem;
    return result;

}


/// <summary>
/// 解压缩，请自行分配足够的缓冲区内存
/// 建议压缩完数据后保存一份数据压缩前的大小
/// 以方便后续解压缩时分配缓存区空间
/// </summary>
/// <param name="in"></param>输入缓冲区
/// <param name="in_len"></param>输入缓冲区长度
/// <param name="out"></param>输出缓冲区
/// <param name="out_len"></param>指向输出长度的指针，该函数执行完毕后会将输出数据的长度存储到该指针指向的内容
/// <param name="wrkmem"></param>未使用
/// <returns></returns>LZO_E_OK:成功  LZO_E_ERROR:出错


__forceinline int Ivan_DeCompress(const lzo_bytep in, lzo_uint  in_len,
    lzo_bytep out, lzo_uintp out_len) {
    return Ivan_DECOMPRESS_LZO(in, in_len, out, out_len);
}

//__device__ int CUDA_COMPRESS_LZO(const lzo_bytep in, lzo_uint  in_len,
//    lzo_bytep out, lzo_uintp out_len,
//    unsigned char* wrkmem);