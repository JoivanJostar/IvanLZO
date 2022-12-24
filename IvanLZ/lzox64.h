#pragma once
#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#include "helper_cuda.h"
#define __LZO_MMODEL            /*empty*/
typedef unsigned __int64   lzo_uint;
typedef __int64            lzo_int;



#ifndef LZO_DEF
#define LZO_DEF
//����constָ�붨��ʱ����ʹ�ú궨�����typedef
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
#define D_BITS 9 //��ϣ������=1<<14  ����Ϊ16k��������һ����Ŀռ��4�ֽ� �����ֵ����һ���̶��ϼӴ�ѹ����
#define LZO_WRKMEM_SIZE   ((lzo_uint32_t) ( ((lzo_uint)(1u<<D_BITS))  *sizeof( lzo_dict_t)) )
#define LZO_SAFE_OUT_LEN(in_len) (in_len + in_len/16  + 64 + 3 ) //����compress���̽����ȵ��ô˺��ȡ��ȫ�������������С
#define ALIGNE_N(ptr,n) ( (ptr+n-1)&~(n-1) ) //��һ��ָ����N�ֽڱ߽���룬N����Ϊ2���ݴ�


#define LZO_BLOCK_BEGIN           do {
#define LZO_BLOCK_END             }while(0)
#define LZO_BYTE(x) ( (unsigned char)(x))
#define LZO_MIN(a,b)  ((a) <= (b) ? (a) : (b))
#define DINDEX(dv,p)     ( (lzo_uint)  (  ( ( (lzo_uint)(0x1824429d*dv) )>>(32-D_BITS) )  & ((1u<<D_BITS)-1  ) )) //Hash Function 

//�ڴ��д����Ϊ�˱�֤ĳЩʵʱ�Գ����µ��̰߳�ȫ�������������ʵ��ڴ��ַ��ָ�룩����Ϊvolatile����
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
/// ��������ѹ������in�������е�����ѹ�������out������
/// �����з����㹻�Ļ������ڴ�
/// ĳЩ��������ݱ����㷨ѹ������ܱ�ԭ������
/// �뱣֤out������������ڴ��in�Դ�һЩ
/// �������LZO_SAFE_OUT_LEN����ȡ����������İ�ȫ��С
/// </summary>
/// <param name="in"></param>���뻺����
/// <param name="in_len"></param>���뻺��������
/// <param name="out"></param>���������
/// <param name="out_len"></param>ָ��������ȵ�ָ�룬�ú���ִ����Ϻ�Ὣ������ݵĳ��ȴ洢����ָ��ָ�������
/// <returns></returns>LZO_E_OK:�ɹ�  LZO_E_ERROR:����
__forceinline int Ivan_Compress(const lzo_bytep in, lzo_uint  in_len,
    lzo_bytep out, lzo_uintp out_len) {

    unsigned char* lzo_wrkmem = new unsigned char[ALIGNE_N(LZO_WRKMEM_SIZE, sizeof(size_t))];
    int result = Ivan_COMPRESS_LZO(in, in_len, out, out_len, lzo_wrkmem);
    delete[] lzo_wrkmem;
    return result;

}


/// <summary>
/// ��ѹ���������з����㹻�Ļ������ڴ�
/// ����ѹ�������ݺ󱣴�һ������ѹ��ǰ�Ĵ�С
/// �Է��������ѹ��ʱ���仺�����ռ�
/// </summary>
/// <param name="in"></param>���뻺����
/// <param name="in_len"></param>���뻺��������
/// <param name="out"></param>���������
/// <param name="out_len"></param>ָ��������ȵ�ָ�룬�ú���ִ����Ϻ�Ὣ������ݵĳ��ȴ洢����ָ��ָ�������
/// <param name="wrkmem"></param>δʹ��
/// <returns></returns>LZO_E_OK:�ɹ�  LZO_E_ERROR:����


__forceinline int Ivan_DeCompress(const lzo_bytep in, lzo_uint  in_len,
    lzo_bytep out, lzo_uintp out_len) {
    return Ivan_DECOMPRESS_LZO(in, in_len, out, out_len);
}

//__device__ int CUDA_COMPRESS_LZO(const lzo_bytep in, lzo_uint  in_len,
//    lzo_bytep out, lzo_uintp out_len,
//    unsigned char* wrkmem);