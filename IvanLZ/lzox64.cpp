#pragma once
#include <inttypes.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "lzox64.h"
#ifdef _M_AMD64 //x64 machine
#define LZO_ABI_LITTLE_ENDIAN 1 //定义小端存储模式
#define LZO_OPT_UNALIGNED32 0    ///这两个UA宏 请根据实际处理器是否支持4/8字节非对齐内存访问技术予以定义  一般的Intel或者AMD64是一定支持此技术的
#define LZO_OPT_UNALIGNED64 0     ///但是某些处理器平台，比如misp 是不支持非对齐数据访问的，此时执行此代码的非对齐访问模式，会触发硬件异常 影响效率
#endif 
//#define DEBUG_1


#ifdef DEBUG_1

 uint32_t  debug_get4Byte(volatile const unsigned char* src) {
    uint32_t result = 0;
    if (((uint64_t)src & (uint64_t)(3)) == 0) {
        // printf("p %p 4字节对齐\n", src);
        return *(volatile int*)src;
    }
    else {//可以优化加上2字节对齐时的加速
     //   printf("p %p 不是4字节对齐\n", src);
        ((volatile unsigned char*)&result)[0] = src[0];
        ((volatile unsigned char*)&result)[1] = src[1];
        ((volatile unsigned char*)&result)[2] = src[2];
        ((volatile unsigned char*)&result)[3] = src[3];
        //printf("单字节访问4字节单元成功,4字节整数为 %u\n", result);
        return result;
    }
}

 lzo_uint debug_device_do_compress(const lzo_bytep in, lzo_uint  in_len,
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
             // ip += 1 + ((ip - ii) >> 5); //这个地方我取消了此优化，加大压缩程度
             ip += 1;

         next:

             if (ip >= ip_end) //搜索超过了范围，需要结束
                 break;
             // printf("try GET_LE32 from %p \n", ip);
             dv = debug_get4Byte(ip);
             //dv = UA_GET_LE32(ip); //取ip处4字节的数据到dv
            // printf("GET_LE32 sucess dv is %u\n",dv);
             dindex = DINDEX(dv, ip);//get hashKey
             m_pos = in + dict[dindex];


             dict[dindex] = (unsigned short int) ((lzo_uint)(ip - in));

             if (dv != debug_get4Byte(m_pos))//碰撞 这里直接当作字面量处理
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
             //朴素的长度匹配算法，理论上的最多访存次数
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

int Ivan_COMPRESS_LZO(const lzo_bytep in, lzo_uint  in_len,
    lzo_bytep out, lzo_uintp out_len,
    void * wrkmem)
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
        memset(wrkmem, 0, ((lzo_uint)1 << D_BITS) * sizeof(lzo_dict_t)); //16K *2B=32KB
        ////修改过：memset逻辑
        int size = (lzo_uint)1 << D_BITS;
        unsigned char* p = (unsigned char*)wrkmem;
        for (int i = 0; i < LZO_WRKMEM_SIZE; ++i) {
            p[i] = 0;
        }

#endif
        //printf("调用 do_compress\n");
        t = debug_device_do_compress(ip, ll, op, out_len, t, wrkmem);
        // printf("do_compress成功\n");
        ip += ll;
        op += *out_len;
        l -= ll;
    }
    t += l;
    // printf("尝试处理边角料\n");
    if (t > 0)
    {
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
            }

            *op++ = LZO_BYTE(tt);
        }
        while (t > 0)
        {
            *op++ = *ii++;
            t--;
        } 
    }
    // printf("边角料处理完毕\n");
    *op++ = M4_MARKER | 1; //17
    *op++ = 0;
    *op++ = 0;

    *out_len = op - out;
    // printf("LZO压缩执行完毕 outlen=%ld\n",*out_len);
    return LZO_E_OK;
}





#else
static __declspec(noinline) lzo_uint
do_compress(const lzo_bytep in, lzo_uint  in_len,
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
#if !(LZO_DETERMINISTIC)
     /*   LZO_DEFINE_UNINITIALIZED_VAR(lzo_uint, m_off, 0);
        lzo_uint m_len;
        lzo_uint dindex;
    next:
        if (ip >= ip_end)
            break;
        DINDEX1(dindex, ip);
        GINDEX(m_pos, m_off, dict, dindex, in);
        if (LZO_CHECK_MPOS_NON_DET(m_pos, m_off, in, ip, M4_MAX_OFFSET))
            goto literal;
#if 1
        if (m_off <= M2_MAX_OFFSET || m_pos[3] == ip[3])
            goto try_match;
        DINDEX2(dindex, ip);
#endif
        GINDEX(m_pos, m_off, dict, dindex, in);
        if (LZO_CHECK_MPOS_NON_DET(m_pos, m_off, in, ip, M4_MAX_OFFSET))
            goto literal;
        if (m_off <= M2_MAX_OFFSET || m_pos[3] == ip[3])
            goto try_match;
        goto literal;

    try_match:
#if (LZO_OPT_UNALIGNED32)
        if (UA_GET_NE32(m_pos) != UA_GET_NE32(ip))
#else
        if (m_pos[0] != ip[0] || m_pos[1] != ip[1] || m_pos[2] != ip[2] || m_pos[3] != ip[3])
#endif
        {
            literal:
            UPDATE_I(dict, 0, dindex, ip, in);
            ip += 1 + ((ip - ii) >> 5);
            continue;
        }
        UPDATE_I(dict, 0, dindex, ip, in);*/

#else
        lzo_uint m_off;
        lzo_uint m_len;
        {
            lzo_uint32_t dv;
            lzo_uint dindex;
            literal:
            // ip += 1 + ((ip - ii) >> 5); //这个地方我取消了此优化，加大压缩程度
            ip += 1;

        next:


            if (ip >= ip_end) //搜索超过了范围，需要结束
                break;
            dv = UA_GET_LE32(ip); //取ip处4字节的数据到dv
            dindex = DINDEX(dv, ip);//get hashKey
            m_pos = in + dict[dindex];
            dict[dindex] = (unsigned short int) ((lzo_uint)(ip - in));
            if (dv != UA_GET_LE32(m_pos))//碰撞 
                goto literal;
        }
#endif
        //处理新字符 输出【新字符个数】+新字符1，新字符2，新字符3
        ii -= ti; ti = 0; //回退ii指针，到上一个数据块末尾未处理的字符处
        {
            lzo_uint t = ip - ii;

            if (t != 0)
            {
                if (t <= 3)
                {
                    op[-2] = LZO_BYTE(op[-2] | t);
#if (LZO_OPT_UNALIGNED32)
                    UA_COPY4(op, ii); //如果处理器 支持非对齐数据访问 （即便数据没有4字节对齐，也能直接直接访问4字节非对齐数据，
                    //当然如果数据本身就没有对齐这必然引起2次访存，但总比逐个字节4次访存要少。
                    op += t;//虽然读取了4字节数据，但是op前进距离还是t个字节，实际上最后不会多读数据
#else
                    { do *op++ = *ii++; while (--t > 0); } //如果处理器不支持非对齐数据访问 此时需要朴素使用逐字节访问指令，效率降至最低
#endif
                }
#if (LZO_OPT_UNALIGNED64)
                //else if (t <= 16) //t<=16 一种可以用非对其访问处理器特性加速的情况
                //{
                //    *op++ = LZO_BYTE(t - 3);
                //    UA_COPY8(op, ii);
                //    UA_COPY8(op + 8, ii + 8);
                //    op += t;
                //}
#endif
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
                            UA_SET1(op, 0);
                            op++;
                        }
                        assert(tt > 0);
                        *op++ = LZO_BYTE(tt);
                    }
#if (LZO_OPT_UNALIGNED64)
                    //do {
                    //    UA_COPY8(op, ii);
                    //    UA_COPY8(op + 8, ii + 8);
                    //    op += 16; ii += 16; t -= 16;
                    //} while (t >= 16); if (t > 0)
#endif
                    {
                        do *op++ = *ii++; while (--t > 0);
                    }
                }
            }
        }
        m_len = 4; //下面这个代码块是在计算重复长度len
        {
#if (LZO_OPT_UNALIGNED64)
//            lzo_uint64_t v;
//            v = UA_GET_NE64(ip + m_len) ^ UA_GET_NE64(m_pos + m_len);//按位异或比较是否一样，如果结果全0，表示两个数据完全一致
//            if (v == 0) {//4+8字节重复
//                do {//每次循环都是试图往后找8个字节进行再次匹配，不论是哪个预处理块
//                    m_len += 8;
//                    v = UA_GET_NE64(ip + m_len) ^ UA_GET_NE64(m_pos + m_len);
//                    if (ip + m_len >= ip_end) //m_len一次+8，ip_end=in_end-20,不会越界
//                        goto m_len_done; //先处理完当前重复字段再考虑退出循环，保证退出循环时，能够处理完当前的重复字段
//                } while (v == 0);
//            }
//
//#if (LZO_ABI_LITTLE_ENDIAN) &&(lzo_bitops_cttz64)
//            m_len += lzo_bitops_cttz64(v) / CHAR_BIT;//判断后面还剩几个字节相等：因为是小端存储，根据异或结果低位0的个数除以8可以算出来。
//#elif (LZO_ABI_LITTLE_ENDIAN)
//            if ((v & UCHAR_MAX) == 0) do {
//                v >>= CHAR_BIT;
//                m_len += 1;
//            } while ((v & UCHAR_MAX) == 0);
//#else
//            if (ip[m_len] == m_pos[m_len]) do {
//                m_len += 1;
//            } while (ip[m_len] == m_pos[m_len]);
//#endif


#else //朴素的长度匹配算法，理论上的最多访存次数
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
#endif
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



int Ivan_COMPRESS_LZO(const lzo_bytep in, lzo_uint  in_len,
    lzo_bytep out, lzo_uintp out_len,
    lzo_voidp wrkmem)
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
        memset(wrkmem, 0, ((lzo_uint)1 << D_BITS) * sizeof(lzo_dict_t)); //16K *2B=32KB
#endif
        t = do_compress(ip, ll, op, out_len, t, wrkmem);
        ip += ll;
        op += *out_len;
        l -= ll;
    }
    t += l;

    if (t > 0)
    {
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
                UA_SET1(op, 0);
                op++;
            }
            assert(tt > 0);
            *op++ = LZO_BYTE(tt);
        }
        //UA_COPYN(op, ii, t);
        //op += t;
        do
        {
            *op++ = *ii++;
        } while (--t > 0);
    }

    *op++ = M4_MARKER | 1; //17
    *op++ = 0;
    *op++ = 0;

    *out_len = op - out;
    return LZO_E_OK;
}

#endif

int Ivan_DECOMPRESS_LZO(const lzo_bytep in, lzo_uint  in_len,
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
        assert(t > 0);
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
        assert(t > 0);
        //是新字符字段，但是个数<18, 需要+3修正 NEED t+3
#if (LZO_OPT_UNALIGNED64) && (LZO_OPT_UNALIGNED32)
//        t += 3;//修正t
//        if (t >= 8) do
//        {
//            UA_COPY8(op, ip);
//            op += 8; ip += 8; t -= 8;
//        } while (t >= 8);
//        if (t >= 4)
//        {
//            UA_COPY4(op, ip);
//            op += 4; ip += 4; t -= 4;
//        }
//        if (t > 0)
//        {
//            *op++ = *ip++;
//            if (t > 1) { *op++ = *ip++; if (t > 2) { *op++ = *ip++; } }
//        }
//#elif (LZO_OPT_UNALIGNED32) || (LZO_ALIGNED_OK_4)
//#if !(LZO_OPT_UNALIGNED32)
//        if (PTR_ALIGNED2_4(op, ip))
//        {
//#endif
//            UA_COPY4(op, ip);
//            op += 4; ip += 4;
//            if (--t > 0) //修正t 先处理4个，t再自减1个，正好3个
//            {
//                if (t >= 4)
//                {
//                    do {
//                        UA_COPY4(op, ip);
//                        op += 4; ip += 4; t -= 4;
//                    } while (t >= 4);
//                    if (t > 0) do *op++ = *ip++; while (--t > 0);
//                }
//                else
//                    do *op++ = *ip++; while (--t > 0);
//            }
//#if !(LZO_OPT_UNALIGNED32)
//        }
//        else
//#endif
#endif
#if !(LZO_OPT_UNALIGNED32)
        {
            *op++ = *ip++; *op++ = *ip++; *op++ = *ip++; //修正t+3
            do *op++ = *ip++; while (--t > 0);
        }
#endif
        //第一次 literal 替换
    first_literal_run:
        t = *ip++; //取重复字段第一个字节
        if (t >= 16) //正常情况重复字段的第一个字节就是大于等于16
            goto match;//跳转到匹配替换例程



        //BEG——————这个分支不应该在LZOX进入
        //assert(0);
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

                assert(t > 0);
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
               // assert(0);

                m_pos = op - 1;
                m_pos -= t >> 2;
                m_pos -= *ip++ << 2;
                *op++ = *m_pos++; *op++ = *m_pos;
                goto match_done;
            }


            assert(t > 0);
            //main CopyMatch:
#if (LZO_OPT_UNALIGNED64) && (LZO_OPT_UNALIGNED32)
            if (op - m_pos >= 8)
            {
                t += (3 - 1);//修正t+2
                if (t >= 8) do
                {
                    UA_COPY8(op, m_pos);
                    op += 8; m_pos += 8; t -= 8;
                } while (t >= 8);
                if (t >= 4)
                {
                    UA_COPY4(op, m_pos);
                    op += 4; m_pos += 4; t -= 4;
                }
                if (t > 0)
                {
                    *op++ = m_pos[0];
                    if (t > 1) { *op++ = m_pos[1]; if (t > 2) { *op++ = m_pos[2]; } }
                }
            }
            else

#endif
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
            assert(t > 0); assert(t < 4);
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


/*
编码修正表：
新字符：
Sp:数据全是新字符，且个数<238 编码时+17
新字符个数<=18: 编码时-3
新字符个数>18:  编码时-18
重复字段：
M2： m_len编码时-1  m_off编码-1
M3; m_off-1 可用5bit: m_len-2 不可用5bit:m_len-33
M4:m_off-0x4000  可用3bit:m_len-2 不可用3bit:m_len-9

*/



