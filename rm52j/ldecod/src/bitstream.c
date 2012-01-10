/*
*****************************************************************************
* COPYRIGHT AND WARRANTY INFORMATION
*
* Copyright 2003, Advanced Audio Video Coding Standard, Part II
*
* DISCLAIMER OF WARRANTY
*
* The contents of this file are subject to the Mozilla Public License
* Version 1.1 (the "License"); you may not use this file except in
* compliance with the License. You may obtain a copy of the License at
* http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS"
* basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
* License for the specific language governing rights and limitations under
* the License.
*
* THIS IS NOT A GRANT OF PATENT RIGHTS - SEE THE AVS PATENT POLICY.
* The AVS Working Group doesn't represent or warrant that the programs
* furnished here under are free of infringement of any third-party patents.
* Commercial implementations of AVS, including shareware, may be
* subject to royalty fees to patent holders. Information regarding
* the AVS patent policy for standardization procedure is available at
* AVS Web site http://www.avs.org.cn. Patent Licensing is outside
* of AVS Working Group.
*
* The Original Code is Reference Software for China National Standard
* GB/T 20090.2-2006 (short for AVS-P2 or AVS Video) at version RM52J.
*
* The Initial Developer of the Original Code is Video subgroup of AVS
* Workinggroup (Audio and Video coding Standard Working Group of China).
* Contributors:   Guoping Li,    Siwei Ma,    Jian Lou,    Qiang Wang ,
*   Jianwen Chen,Haiwu Zhao,  Xiaozhen Zheng, Junhao Zheng, Zhiming Wang
*
******************************************************************************
*/


/*
*************************************************************************************
* File name: bitstream.c
* Function: decode bitstream
*
*************************************************************************************
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "global.h"
#include "annexb.h"
#include "memalloc.h"

#define SVA_STREAM_BUF_SIZE 1024

FILE *bitsfile = NULL;    //!< the bit stream file
unsigned char bit[8] = {0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};

typedef struct {
    FILE *f;
    unsigned char  buf[SVA_STREAM_BUF_SIZE];  //流缓冲区,size must be large than 3 bytes
    unsigned int  uClearBits;//不含填充位的位缓冲，32位，初始值是0xFFFFFFFF
    unsigned int  uPre3Bytes;//  含填充位的位缓冲，32位，初始值是0x00000000
    int  iBytePosition;    //当前字节位置
    int  iBufBytesNum;    //最近一次读入缓冲区的字节数
    int  iClearBitsNum;    //不含填充位的位的个数
    int  iStuffBitsNum;    //已剔除的填充位的个数，遇到开始码时置0
    int iBitsCount;      //码流总位数
} InputStream;

InputStream IRABS;
InputStream *pIRABS = &IRABS;

void OpenIRABS(InputStream *p, char *fname)
{
    p->f = fopen(fname,"rb");
    if(p->f==NULL){printf ("\nCan't open file %s",fname);exit(-1);}

    p->uClearBits      = 0xffffffff;
    p->iBytePosition    = 0;
    p->iBufBytesNum      = 0;
    p->iClearBitsNum    = 0;
    p->iStuffBitsNum    = 0;
    p->iBitsCount      = 0;
    p->uPre3Bytes      = 0;
}

void CloseIRABS(InputStream *p)
{
    fclose(p->f);
}
//------------------------------------------------------------------------
// move iBytePosition to the next byte of start code prefix
//return
//    0 : OK
//   -1 : arrive at stream end and start code is not found
//   -2 : p->iBytePosition error
//-------------------------------------------------------------------------
int NextStartCode(InputStream *p)
{
    int i, m;
    unsigned char a,b;  // a b 0 1 2 3 4 ... M-3 M-2 M-1
    m=0;  // cjw 20060323 for linux envi

    while(1)
    {
        if(p->iBytePosition >= p->iBufBytesNum - 2)  //if all bytes in buffer has been searched
        {
            m = p->iBufBytesNum - p->iBytePosition;
            if(m <0)  return -2;  // p->iBytePosition error
            if(m==1)  b=p->buf[p->iBytePosition+1];
            if(m==2){ b=p->buf[p->iBytePosition+1]; a=p->buf[p->iBytePosition];}
            p->iBufBytesNum = fread(p->buf,1,SVA_STREAM_BUF_SIZE,p->f);
            p->iBytePosition = 0;
        }

        if(p->iBufBytesNum + m < 3)
            return -1;  //arrive at stream end and start code is not found

        if(m==1 && b==0 && p->buf[0]==0 && p->buf[1]==1)
        {
            p->iBytePosition  = 2;
            p->iClearBitsNum  = 0;
            p->iStuffBitsNum  = 0;
            p->iBitsCount    += 24;
            p->uPre3Bytes    = 1;
            return 0;
        }

        if(m==2 && b==0 && a==0 && p->buf[0]==1)
        {
            p->iBytePosition  = 1;
            p->iClearBitsNum  = 0;
            p->iStuffBitsNum  = 0;
            p->iBitsCount    += 24;
            p->uPre3Bytes    = 1;
            return 0;
        }

        if(m==2 && b==0 && p->buf[0]==0 && p->buf[1]==1)
        {
            p->iBytePosition  = 2;
            p->iClearBitsNum  = 0;
            p->iStuffBitsNum  = 0;
            p->iBitsCount    += 24;
            p->uPre3Bytes    = 1;
            return 0;
        }

        for(i = p->iBytePosition; i < p->iBufBytesNum - 2; i++)
        {
            if(p->buf[i]==0 && p->buf[i+1]==0 && p->buf[i+2]==1)
            {
                p->iBytePosition  = i+3;
                p->iClearBitsNum  = 0;
                p->iStuffBitsNum  = 0;
                p->iBitsCount    += 24;
                p->uPre3Bytes    = 1;
                return 0;
            }
            p->iBitsCount += 8;
        }
        p->iBytePosition = i;
    }
}
//----------------------------------------------------------------------
//return
//    0 : OK
//   -1 : arrive at stream end
//   -2 : meet another start code
//----------------------------------------------------------------------
int ClearNextByte(InputStream *p)
{
    int i,k,j;
    unsigned char temp[3];
    i = p->iBytePosition;
    k = p->iBufBytesNum - i;
    if(k < 3)
    {
        for(j=0;j<k;j++) temp[j] = p->buf[i+j];
        p->iBufBytesNum = fread(p->buf+k,1,SVA_STREAM_BUF_SIZE-k,p->f);
        if(p->iBufBytesNum == 0)
        {
            if(k>0)
            {
                p->uPre3Bytes = ((p->uPre3Bytes<<8) | p->buf[i]) & 0x00ffffff;
                if(p->uPre3Bytes < 4 && demulate_enable) //modified by X ZHENG, 20080515
                {
                    p->uClearBits = (p->uClearBits << 6) | (p->buf[i] >> 2);
                    p->iClearBitsNum += 6;
                }
                else
                {
                    p->uClearBits = (p->uClearBits << 8) | p->buf[i];
                    p->iClearBitsNum += 8;
                }
                p->iBytePosition++;
                return 0;
            }
            else
            {
                return -1;//arrive at stream end
            }
        }
        else
        {
            for(j=0;j<k;j++) p->buf[j] = temp[j];
            p->iBufBytesNum += k;
            i = p->iBytePosition = 0;
        }
    }
    if(p->buf[i]==0 && p->buf[i+1]==0 && p->buf[i+2]==1)  return -2;// meet another start code
    p->uPre3Bytes = ((p->uPre3Bytes<<8) | p->buf[i]) & 0x00ffffff;
    if(p->uPre3Bytes < 4 && demulate_enable) //modified by X ZHENG, 20080515
    {
        p->uClearBits = (p->uClearBits << 6) | (p->buf[i] >> 2);
        p->iClearBitsNum += 6;
    }
    else
    {
        p->uClearBits = (p->uClearBits << 8) | p->buf[i];
        p->iClearBitsNum += 8;
    }
    p->iBytePosition++;
    return 0;
}

//----------------------------------------------------------
//return
//    0 : OK
//   -1 : arrive at stream end
//   -2 : meet another start code
//----------------------------------------------------------
int read_n_bit(InputStream *p,int n,int *v)
{
    int r;
    unsigned int t;
    while(n > p->iClearBitsNum)
    {
        r = ClearNextByte( p );
        if(r)
        {
            if(r==-1)
            {
                if(p->iBufBytesNum - p->iBytePosition > 0)
                    break;
            }
            return r;
        }
    }
    t = p->uClearBits;
    r = 32 - p->iClearBitsNum;
    *v = (t << r) >> (32 - n);
    p->iClearBitsNum -= n;
    return 0;
}
//==================================================================================

void OpenBitstreamFile (char *fn)
{
    OpenIRABS(pIRABS, fn);
}

void CloseBitstreamFile()
{
    CloseIRABS(pIRABS);
}

// Added by Carmen, 2008/01/22, For supporting multiple sequences in a stream
int IsEndOfBitstream ()
{
    int ret=feof(pIRABS->f);
    return ((!ret)?(0):(1));
}

static int FindStartCode (unsigned char *Buf, int i)
{
    if(Buf[i]==0 && Buf[i+1]==0 && Buf[i+2]==1)
        return 1;
    else
        return 0;
}

////////////////////////////////////////////////////////////////////////////
//check slice start code    jlzheng  6.30
////////////////////////////////////////////////////////////////////////////
int checkstartcode()   //check slice start code    jlzheng  6.30
{
    int temp_i, temp_val;
    if (currStream->bitstream_length*8 - currStream->frame_bitoffset == 0)
        return 1;

    if(img->current_mb_nr == 0)
    {
        //--- added by Yulj 2004.07.15
        if (  currStream->bitstream_length*8 - currStream->frame_bitoffset <=8
            && currStream->bitstream_length*8 - currStream->frame_bitoffset > 0  ){
                temp_i = currStream->bitstream_length*8 - currStream->frame_bitoffset;
                assert( temp_i > 0 );
                temp_val = get_uv(temp_i, "filling data") ;
        }
    }

    //  if(img->current_mb_nr == 0) //commented by cjw AVS 20070204
    //  {
    //    if(StartCodePosition>6 && StartCodePosition<20)
    //       return 1;
    //    else
    //    {
    //       currStream->frame_bitoffset = currentbitoffset;
    //       return 0;
    //    }
    //  }

    if(img->current_mb_nr == 0) //added by cjw AVS 20070327
    {
        //    if(StartCodePosition>6 && StartCodePosition<0x000fffff)//zhangji  2007 01 30  // 20071009
        if(StartCodePosition>4 && StartCodePosition<0x000fffff)              // 20071009
            return 1;
        else
        {
            currStream->frame_bitoffset = currentbitoffset;
            return 0;
        }
    }

    if(img->current_mb_nr != 0)
    {
        //--- added by Yulj 2004.07.15
        if (currStream->bitstream_length*8 - currStream->frame_bitoffset <= 8
            && currStream->bitstream_length*8 - currStream->frame_bitoffset >0){
                temp_i = currStream->bitstream_length*8 - currStream->frame_bitoffset;
                assert( temp_i > 0 );
                temp_val = get_uv(temp_i, "filling data") ;
                if ( temp_val == (1 << (temp_i -1))  && img->cod_counter <= 0   )
                    return 1; // last MB in current slice
        }
        return 0;        // not last MB in current slice
        //---end
    }

    return 1;
}

////////////////////////////////////////////////////////////////////////////
// check start code's type    X ZHENG, 20080515
// if the start code is video_sequence_start_code,user_data_start_code
// or extension_start_code, the demulation mechanism is forbidded.
////////////////////////////////////////////////////////////////////////////
void CheckType(int startcode){
    startcode = startcode&0x000000ff;
    switch(startcode)
    {
    case 0xb0:
    case 0xb2:
    case 0xb5:
        demulate_enable = 0;
        break;
    default:
        demulate_enable = 1;
        break;
    }
}

//------------------------------------------------------------
// buf          : buffer
// startcodepos :
// length       :
int GetOneUnit (char *buf,int *startcodepos,int *length)
{
    int i,j,k;
    i = NextStartCode(pIRABS);
    if(i!=0)
    {
        if(i==-1) printf("\narrive at stream end and start code is not found!");
        if(i==-2) printf("\np->iBytePosition error!");
        exit(i);
    }
    buf[0] = 0;
    buf[1] = 0;
    buf[2] = 1;
    *startcodepos = 3;
    i = read_n_bit(pIRABS,8,&j);
    buf[3] = (char)j;
    CheckType(buf[3]);  //X ZHENG, 20080515, for demulation
    if(buf[3]==SEQUENCE_END_CODE)
    {
        *length = 4;
        return -1;
    }
    k = 4;
    while(1)
    {
        i = read_n_bit(pIRABS,8,&j);
        if(i<0) break;
        buf[k++] = (char)j;
    }
    if(pIRABS->iClearBitsNum>0)
    {
        int shift;
        shift = 8 - pIRABS->iClearBitsNum;
        i = read_n_bit(pIRABS,pIRABS->iClearBitsNum,&j);

        if(j!=0)  //qhg 20060327 for de-emulation.
            buf[k++] = (char)(j<<shift);
    }
    *length = k;
    return k;
}
