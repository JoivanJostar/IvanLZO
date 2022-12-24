#pragma once
#include <inttypes.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "lzox64.h"
#ifdef _M_AMD64 //x64 machine
#define LZO_ABI_LITTLE_ENDIAN 1 //����С�˴洢ģʽ
#define LZO_OPT_UNALIGNED32 0    ///������UA�� �����ʵ�ʴ������Ƿ�֧��4/8�ֽڷǶ����ڴ���ʼ������Զ���  һ���Intel����AMD64��һ��֧�ִ˼�����
#define LZO_OPT_UNALIGNED64 0     ///����ĳЩ������ƽ̨������misp �ǲ�֧�ַǶ������ݷ��ʵģ���ʱִ�д˴���ķǶ������ģʽ���ᴥ��Ӳ���쳣 Ӱ��Ч��
#endif 
//#define DEBUG_1


#ifdef DEBUG_1

 uint32_t  debug_get4Byte(volatile const unsigned char* src) {
    uint32_t result = 0;
    if (((uint64_t)src & (uint64_t)(3)) == 0) {
        // printf("p %p 4�ֽڶ���\n", src);
        return *(volatile int*)src;
    }
    else {//�����Ż�����2�ֽڶ���ʱ�ļ���
     //   printf("p %p ����4�ֽڶ���\n", src);
        ((volatile unsigned char*)&result)[0] = src[0];
        ((volatile unsigned char*)&result)[1] = src[1];
        ((volatile unsigned char*)&result)[2] = src[2];
        ((volatile unsigned char*)&result)[3] = src[3];
        //printf("���ֽڷ���4�ֽڵ�Ԫ�ɹ�,4�ֽ�����Ϊ %u\n", result);
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
             // ip += 1 + ((ip - ii) >> 5); //����ط���ȡ���˴��Ż����Ӵ�ѹ���̶�
             ip += 1;

         next:

             if (ip >= ip_end) //���������˷�Χ����Ҫ����
                 break;
             // printf("try GET_LE32 from %p \n", ip);
             dv = debug_get4Byte(ip);
             //dv = UA_GET_LE32(ip); //ȡip��4�ֽڵ����ݵ�dv
            // printf("GET_LE32 sucess dv is %u\n",dv);
             dindex = DINDEX(dv, ip);//get hashKey
             m_pos = in + dict[dindex];


             dict[dindex] = (unsigned short int) ((lzo_uint)(ip - in));

             if (dv != debug_get4Byte(m_pos))//��ײ ����ֱ�ӵ�������������
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
             //���صĳ���ƥ���㷨�������ϵ����ô����
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
        ll = LZO_MIN(ll, 49152); //DETERMINISTICģʽ�� src��������С>48kʱ�ֿ鴦��
#endif
        ll_end = (lzo_uint)ip + ll;
        if ((ll_end + ((t + ll) >> 5)) <= ll_end || (const lzo_bytep)(ll_end + ((t + ll) >> 5)) <= ip + ll)
            break;
#if (LZO_DETERMINISTIC)
        memset(wrkmem, 0, ((lzo_uint)1 << D_BITS) * sizeof(lzo_dict_t)); //16K *2B=32KB
        ////�޸Ĺ���memset�߼�
        int size = (lzo_uint)1 << D_BITS;
        unsigned char* p = (unsigned char*)wrkmem;
        for (int i = 0; i < LZO_WRKMEM_SIZE; ++i) {
            p[i] = 0;
        }

#endif
        //printf("���� do_compress\n");
        t = debug_device_do_compress(ip, ll, op, out_len, t, wrkmem);
        // printf("do_compress�ɹ�\n");
        ip += ll;
        op += *out_len;
        l -= ll;
    }
    t += l;
    // printf("���Դ���߽���\n");
    if (t > 0)
    {
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
            }

            *op++ = LZO_BYTE(tt);
        }
        while (t > 0)
        {
            *op++ = *ii++;
            t--;
        } 
    }
    // printf("�߽��ϴ������\n");
    *op++ = M4_MARKER | 1; //17
    *op++ = 0;
    *op++ = 0;

    *out_len = op - out;
    // printf("LZOѹ��ִ����� outlen=%ld\n",*out_len);
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
            // ip += 1 + ((ip - ii) >> 5); //����ط���ȡ���˴��Ż����Ӵ�ѹ���̶�
            ip += 1;

        next:


            if (ip >= ip_end) //���������˷�Χ����Ҫ����
                break;
            dv = UA_GET_LE32(ip); //ȡip��4�ֽڵ����ݵ�dv
            dindex = DINDEX(dv, ip);//get hashKey
            m_pos = in + dict[dindex];
            dict[dindex] = (unsigned short int) ((lzo_uint)(ip - in));
            if (dv != UA_GET_LE32(m_pos))//��ײ 
                goto literal;
        }
#endif
        //�������ַ� ��������ַ�������+���ַ�1�����ַ�2�����ַ�3
        ii -= ti; ti = 0; //����iiָ�룬����һ�����ݿ�ĩβδ������ַ���
        {
            lzo_uint t = ip - ii;

            if (t != 0)
            {
                if (t <= 3)
                {
                    op[-2] = LZO_BYTE(op[-2] | t);
#if (LZO_OPT_UNALIGNED32)
                    UA_COPY4(op, ii); //��������� ֧�ַǶ������ݷ��� ����������û��4�ֽڶ��룬Ҳ��ֱ��ֱ�ӷ���4�ֽڷǶ������ݣ�
                    //��Ȼ������ݱ����û�ж������Ȼ����2�ηô棬���ܱ�����ֽ�4�ηô�Ҫ�١�
                    op += t;//��Ȼ��ȡ��4�ֽ����ݣ�����opǰ�����뻹��t���ֽڣ�ʵ������󲻻�������
#else
                    { do *op++ = *ii++; while (--t > 0); } //�����������֧�ַǶ������ݷ��� ��ʱ��Ҫ����ʹ�����ֽڷ���ָ�Ч�ʽ������
#endif
                }
#if (LZO_OPT_UNALIGNED64)
                //else if (t <= 16) //t<=16 һ�ֿ����÷Ƕ�����ʴ��������Լ��ٵ����
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
                        *op++ = LZO_BYTE(t - 3); //��ʾ���ַ��������Ǹ��ֽڵ�ֵҪС��16���ͱ�ʾ�ظ��ֶε��ֽ����ֿ�������ΪС��3ʱ��piggyback,
                    //���Դ���3ʱ �������޿��Զ����ʾ3���ַ���������18���ַ�
                    else
                    {
                        lzo_uint tt = t - 18; //����ʱҪ+18
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
        m_len = 4; //���������������ڼ����ظ�����len
        {
#if (LZO_OPT_UNALIGNED64)
//            lzo_uint64_t v;
//            v = UA_GET_NE64(ip + m_len) ^ UA_GET_NE64(m_pos + m_len);//��λ���Ƚ��Ƿ�һ����������ȫ0����ʾ����������ȫһ��
//            if (v == 0) {//4+8�ֽ��ظ�
//                do {//ÿ��ѭ��������ͼ������8���ֽڽ����ٴ�ƥ�䣬�������ĸ�Ԥ�����
//                    m_len += 8;
//                    v = UA_GET_NE64(ip + m_len) ^ UA_GET_NE64(m_pos + m_len);
//                    if (ip + m_len >= ip_end) //m_lenһ��+8��ip_end=in_end-20,����Խ��
//                        goto m_len_done; //�ȴ����굱ǰ�ظ��ֶ��ٿ����˳�ѭ������֤�˳�ѭ��ʱ���ܹ������굱ǰ���ظ��ֶ�
//                } while (v == 0);
//            }
//
//#if (LZO_ABI_LITTLE_ENDIAN) &&(lzo_bitops_cttz64)
//            m_len += lzo_bitops_cttz64(v) / CHAR_BIT;//�жϺ��滹ʣ�����ֽ���ȣ���Ϊ��С�˴洢�������������λ0�ĸ�������8�����������
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


#else //���صĳ���ƥ���㷨�������ϵ����ô����
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
        ll = LZO_MIN(ll, 49152); //DETERMINISTICģʽ�� src��������С>48kʱ�ֿ鴦��
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

    if (*ip > 17) //���������������ѹ�����ݳ���t<238��ȫ�ǲ����滻���ݣ���ʱѹ��������ݵ�һ���ֽ�=t+17,��ʾ֮����t�����ַ�
    {
        t = *ip++ - 17;//-17����
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
        assert(t > 0);
        //�����ַ��ֶΣ����Ǹ���<18, ��Ҫ+3���� NEED t+3
#if (LZO_OPT_UNALIGNED64) && (LZO_OPT_UNALIGNED32)
//        t += 3;//����t
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
//            if (--t > 0) //����t �ȴ���4����t���Լ�1��������3��
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
            *op++ = *ip++; *op++ = *ip++; *op++ = *ip++; //����t+3
            do *op++ = *ip++; while (--t > 0);
        }
#endif
        //��һ�� literal �滻
    first_literal_run:
        t = *ip++; //ȡ�ظ��ֶε�һ���ֽ�
        if (t >= 16) //��������ظ��ֶεĵ�һ���ֽھ��Ǵ��ڵ���16
            goto match;//��ת��ƥ���滻����



        //BEG�����������������֧��Ӧ����LZOX����
        //assert(0);
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
                t += (3 - 1);//����t+2
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
                *op++ = *m_pos++; *op++ = *m_pos++;//����m_len+2
                do *op++ = *m_pos++; while (--t > 0);
            }


        match_done: //�ظ��ֶδ�����ϣ�����������Ƿ����piggyback������������match_next�������˳�matchѭ���壬������ѭ����ִ�з�piggyback���ַ��ֶΡ�
            t = ip[-2] & 3;
            if (t == 0)
                break;
            //��piggyback 
        match_next:
            assert(t > 0); assert(t < 4);
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


/*
����������
���ַ���
Sp:����ȫ�����ַ����Ҹ���<238 ����ʱ+17
���ַ�����<=18: ����ʱ-3
���ַ�����>18:  ����ʱ-18
�ظ��ֶΣ�
M2�� m_len����ʱ-1  m_off����-1
M3; m_off-1 ����5bit: m_len-2 ������5bit:m_len-33
M4:m_off-0x4000  ����3bit:m_len-2 ������3bit:m_len-9

*/



