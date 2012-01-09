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
* File name: vlc.c
* Function: VLC support functions
*
*************************************************************************************
*/

#include "contributors.h"

#include <math.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <assert.h>

#include "global.h"
#include "vlc.h"
#include "elements.h"
#include "header.h"
#include "golomb_dec.h"

// A little trick to avoid those horrible #if TRACE all over the source code
#if TRACE
#define SYMTRACESTRING(s) strncpy(sym->tracestring,s,TRACESTRING_SIZE)
#else
#define SYMTRACESTRING(s) // do nothing
#endif

extern void tracebits(const char *trace_str,  int len,  int info,int value1);
extern void tracebits3(const char *trace_str,  int len,  int info,int value1);

int UsedBits;      // for internal statistics, is adjusted by se_v, ue_v, u_1

/*
*************************************************************************
* Function:ue_v, reads an ue(v) syntax element, the length in bits is stored in 
     the global UsedBits variable
* Input:
    tracestring
      the string for the trace file
        bitstream
      the stream to be read from 
* Output:
* Return: the value of the coded syntax element
* Attention:
*************************************************************************
*/

int ue_v (char *tracestring)
{
  SyntaxElement symbol, *sym=&symbol;

  assert (currStream->streamBuffer != NULL);
  sym->type = SE_HEADER;
  sym->mapping = linfo_ue;   // Mapping rule
  SYMTRACESTRING(tracestring);
  readSyntaxElement_VLC (sym);

  return sym->value1;
}

/*
*************************************************************************
* Function:ue_v, reads an se(v) syntax element, the length in bits is stored in 
     the global UsedBits variable
* Input: 
    tracestring
      the string for the trace file
        bitstream
      the stream to be read from
* Output:
* Return: the value of the coded syntax element
* Attention:
*************************************************************************
*/


int se_v (char *tracestring)
{
  SyntaxElement symbol, *sym=&symbol;

  assert (currStream->streamBuffer != NULL);
  sym->type = SE_HEADER;
  sym->mapping = linfo_se;   // Mapping rule: signed integer
  SYMTRACESTRING(tracestring);
  readSyntaxElement_VLC (sym);

  return sym->value1;
}

/*
*************************************************************************
* Function:ue_v, reads an u(v) syntax element, the length in bits is stored in 
      the global UsedBits variable
* Input:
    tracestring
     the string for the trace file
       bitstream
      the stream to be read from
* Output:
* Return: the value of the coded syntax element
* Attention:
*************************************************************************
*/

int u_v (int LenInBits, char*tracestring)
{
  SyntaxElement symbol, *sym=&symbol;

  assert (currStream->streamBuffer != NULL);
  sym->type = SE_HEADER;
  sym->mapping = linfo_ue;   // Mapping rule
  sym->len = LenInBits;
  SYMTRACESTRING(tracestring);
  readSyntaxElement_FLC (sym);

  return sym->inf;
}
int i_8(char *tracestring)   //add by wuzhongmou 0610
{
  int frame_bitoffset = currStream->frame_bitoffset;
   byte *buf = currStream->streamBuffer;
   int BitstreamLengthInBytes = currStream->bitstream_length;
   SyntaxElement symbol, *sym=&symbol;
  assert (currStream->streamBuffer != NULL);

  sym->len = 8;
  sym->type = SE_HEADER;
  sym->mapping = linfo_ue;
  SYMTRACESTRING(tracestring);
 

  if ((GetBits(buf, frame_bitoffset, &(sym->inf), BitstreamLengthInBytes, sym->len)) < 0)
    return -1;
  currStream->frame_bitoffset += sym->len; // move bitstream pointer
  sym->value1 = sym->inf;
  if (sym->inf & 0x80)
    sym->inf= -(~((int)0xffffff00 | sym->inf) + 1);  
#if TRACE
  tracebits2(sym->tracestring, sym->len, sym->inf);
#endif
  return sym->inf;          //add by wuzhongmou 0610
}
/*
*************************************************************************
* Function:ue_v, reads an u(1) syntax element, the length in bits is stored in 
     the global UsedBits variable
* Input:
    tracestring
     the string for the trace file
       bitstream
     the stream to be read from
* Output:
* Return: the value of the coded syntax element
* Attention:
*************************************************************************
*/

int u_1 (char *tracestring)
{
  return u_v (1, tracestring);
}

/*
*************************************************************************
* Function:mapping rule for ue(v) syntax elements
* Input:lenght and info
* Output:number in the code table
* Return: 
* Attention:
*************************************************************************
*/

void linfo_ue(int len, int info, int *value1, int *dummy)
{
  *value1 = (int)pow(2,(len/2))+info-1; // *value1 = (int)(2<<(len>>1))+info-1;
}

/*
*************************************************************************
* Function:mapping rule for se(v) syntax elements
* Input:lenght and info
* Output:signed mvd
* Return: 
* Attention:
*************************************************************************
*/


void linfo_se(int len,  int info, int *value1, int *dummy)
{
  int n;
  n = (int)pow(2,(len/2))+info-1;
  *value1 = (n+1)/2;
  if((n & 0x01)==0)                           // lsb is signed bit
    *value1 = -*value1;

}

/*
*************************************************************************
* Function:lenght and info
* Input:
* Output:cbp (intra)
* Return: 
* Attention:
*************************************************************************
*/


void linfo_cbp_intra(int len,int info,int *cbp, int *dummy)
{
 // extern const byte NCBP[48][2];
  extern const byte NCBP[64][2];    //jlzheng 7.20
    int cbp_idx;
  linfo_ue(len,info,&cbp_idx,dummy);
    *cbp=NCBP[cbp_idx][0];
}

/*
*************************************************************************
* Function:
* Input:lenght and info
* Output:cbp (inter)
* Return: 
* Attention:
*************************************************************************
*/

void linfo_cbp_inter(int len,int info,int *cbp, int *dummy)
{
  //extern const byte NCBP[48][2];
  extern const byte NCBP[64][2];  //cjw 20060321
  int cbp_idx;
  linfo_ue(len,info,&cbp_idx,dummy);
    *cbp=NCBP[cbp_idx][1];
}

/*
*************************************************************************
* Function:read next UVLC codeword from UVLC-partition and
      map it to the corresponding syntax element
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/


int readSyntaxElement_VLC(SyntaxElement *sym)
{
  int frame_bitoffset = currStream->frame_bitoffset;
  byte *buf = currStream->streamBuffer;
  int BitstreamLengthInBytes = currStream->bitstream_length;

  sym->len =  GetVLCSymbol (buf, frame_bitoffset, &(sym->inf), BitstreamLengthInBytes);

  if (sym->len == -1)
    return -1;

  currStream->frame_bitoffset += sym->len;
  sym->mapping(sym->len,sym->inf,&(sym->value1),&(sym->value2));

#if TRACE
  tracebits(sym->tracestring, sym->len, sym->inf, sym->value1);
#endif

  return 1;
}

/*
*************************************************************************
* Function:read next UVLC codeword from UVLC-partition and
      map it to the corresponding syntax element
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/


int readSyntaxElement_UVLC(SyntaxElement *sym,  struct inp_par *inp)
{
  int frame_bitoffset = currStream->frame_bitoffset;
  byte *buf = currStream->streamBuffer;
  int BitstreamLengthInBytes = currStream->bitstream_length;

  if( sym->golomb_maxlevels && (sym->type==SE_LUM_DC_INTRA||sym->type==SE_LUM_AC_INTRA||sym->type==SE_LUM_DC_INTER||sym->type==SE_LUM_AC_INTER) )
    return readSyntaxElement_GOLOMB(sym,img,inp);

  if(sym->type == SE_REFFRAME)
  {
    sym->len = 1;
    
    if ((GetVLCSymbol_refidx(buf, frame_bitoffset, &(sym->inf), BitstreamLengthInBytes)) < 0)
      return -1;
    
    sym->value1 = sym->inf;
    currStream->frame_bitoffset += sym->len;
    
#if TRACE
    tracebits3(sym->tracestring, sym->len, sym->inf, sym->value1);//sym->inf, sym->value1);
#endif
    
  }
  else
  {
    sym->len =  GetVLCSymbol (buf, frame_bitoffset, &(sym->inf), BitstreamLengthInBytes);
    if (sym->len == -1)
      return -1;
    currStream->frame_bitoffset += sym->len;
    sym->mapping(sym->len,sym->inf,&(sym->value1),&(sym->value2));
    
#if TRACE
    tracebits(sym->tracestring, sym->len, sym->inf, sym->value1);
#endif

  }
  
  return 1;
}

/*
*************************************************************************
* Function:read next VLC codeword for 4x4 Intra Prediction Mode and
     map it to the corresponding Intra Prediction Direction
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/


int readSyntaxElement_Intra8x8PredictionMode(SyntaxElement *sym)
{
  int         frame_bitoffset        = currStream->frame_bitoffset;
  byte        *buf                   = currStream->streamBuffer;
  int         BitstreamLengthInBytes = currStream->bitstream_length;

  sym->len = GetVLCSymbol_IntraMode (buf, frame_bitoffset, &(sym->inf), BitstreamLengthInBytes);

  if (sym->len == -1)
    return -1;

  currStream->frame_bitoffset += sym->len;
  sym->value1                  = sym->len == 1 ? -1 : sym->inf;

#if TRACE
  tracebits2(sym->tracestring, sym->len, sym->value1);
#endif

  return 1;
}

/*
*************************************************************************
* Function:
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/
int GetVLCSymbol_IntraMode (byte buffer[],int totbitoffset,int *info, int bytecount)
{

  register int inf;
  long byteoffset;      // byte from start of buffer
  int bitoffset;      // bit from start of byte
  int ctr_bit=0;      // control bit for current bit posision
  int bitcounter=1;
  int len;
  int info_bit;

  byteoffset = totbitoffset/8;
  bitoffset  = 7-(totbitoffset%8);
  ctr_bit    = (buffer[byteoffset] & (0x01<<bitoffset));   // set up control bit
  len        = 1;

  //First bit
  if (ctr_bit)
  {
    *info = 0;
    return bitcounter;
  }
  else
    len=3;

  // make infoword
  inf=0;                          // shortest possible code is 1, then info is always 0
  for(info_bit=0;(info_bit<(len-1)); info_bit++)
  {
    bitcounter++;
    bitoffset-=1;
    if (bitoffset<0)
    {                 // finished with current byte ?
      bitoffset=bitoffset+8;
      byteoffset++;
    }
    if (byteoffset > bytecount)
    {
      return -1;
    }
    inf=(inf<<1);
    if(buffer[byteoffset] & (0x01<<(bitoffset)))
      inf |=1;
  }

  *info = inf;
  return bitcounter;           // return absolute offset in bit from start of frame
}

/*
*************************************************************************
* Function: Moves the read pointer of the partition forward by one symbol
* Input:
    byte buffer[]
     containing VLC-coded data bits
    int totbitoffset
     bit offset from start of partition
     int type
     expected data type (Partiotion ID)
* Output:
* Return: Length and Value of the next symbol
* Attention:As in both nal_bits.c and nal_part.c all data of one partition, slice,
      picture was already read into a buffer, there is no need to read any data
     here again.
  \par
      This function could (and should) be optimized considerably
    \par
      If it is ever decided to have different VLC tables for different symbol
      types, then this would be the place for the implementation
    \par
      An alternate VLC table is implemented based on exponential Golomb codes.
     The encoder must have a matching define selected.
   \par
      GetVLCInfo was extracted because there should be only one place in the
      source code that has knowledge about symbol extraction, regardless of
      the number of different NALs.
*************************************************************************
*/

int GetVLCSymbol (byte buffer[],int totbitoffset,int *info, int bytecount)
{

  register int inf;
  long byteoffset;      // byte from start of buffer
  int bitoffset;      // bit from start of byte
  int ctr_bit=0;      // control bit for current bit posision
  int bitcounter=1;
  int len;
  int info_bit;

  byteoffset= totbitoffset/8;
  bitoffset= 7-(totbitoffset%8);
  ctr_bit = (buffer[byteoffset] & (0x01<<bitoffset));   // set up control bit

  len=1;
  while (ctr_bit==0)
  {                 // find leading 1 bit
    len++;
    bitoffset-=1;           
    bitcounter++;
    if (bitoffset<0)
    {                 // finish with current byte ?
      bitoffset=bitoffset+8;
      byteoffset++;
    }
    ctr_bit=buffer[byteoffset] & (0x01<<(bitoffset));
  }

  // make infoword
  inf=0;                          // shortest possible code is 1, then info is always 0
  for(info_bit=0;(info_bit<(len-1)); info_bit++)
  {
    bitcounter++;
    bitoffset-=1;
    if (bitoffset<0)
    {                 // finished with current byte ?
      bitoffset=bitoffset+8;
      byteoffset++;
    }
    if (byteoffset > bytecount)
    {
      return -1;
    }
    inf=(inf<<1);
    if(buffer[byteoffset] & (0x01<<(bitoffset)))
      inf |=1;
  }

  *info = inf;

  return bitcounter;           // return absolute offset in bit from start of frame
}

/*
*************************************************************************
* Function:
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/
int GetVLCSymbol_refidx (byte buffer[],int totbitoffset,int *info, int bytecount)
{
  
  register int inf;
  long byteoffset;      // byte from start of buffer
  int bitoffset;      // bit from start of byte
  int bitcounter=1;
  int len;
  int info_bit;
  
  byteoffset= totbitoffset/8;
  bitoffset= 7-(totbitoffset%8);
  
  len=1;
  inf=0;
  
  for(info_bit=0;(info_bit<len); info_bit++)
  {
    if (bitoffset<0)
    {                 // finished with current byte ?
      bitoffset=bitoffset+8;
      byteoffset++;
    }
    
    if (byteoffset > bytecount)
    {
      return -1;
    }
    
    if(buffer[byteoffset] & (0x01<<(bitoffset)))
      inf = 1;
    
    bitcounter++;
    bitoffset-=1;
  }
  
  *info = inf;

  return bitcounter;           // return absolute offset in bit from start of frame
}

extern void tracebits2(const char *trace_str,  int len,  int info) ;

/*
*************************************************************************
* Function:read FLC codeword from UVLC-partition 
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int readSyntaxElement_FLC(SyntaxElement *sym)
{
  int frame_bitoffset = currStream->frame_bitoffset;
  byte *buf = currStream->streamBuffer;
  int BitstreamLengthInBytes = currStream->bitstream_length;

  if ((GetBits(buf, frame_bitoffset, &(sym->inf), BitstreamLengthInBytes, sym->len)) < 0)
    return -1;

  currStream->frame_bitoffset += sym->len; // move bitstream pointer
  sym->value1 = sym->inf;

#if TRACE
  tracebits2(sym->tracestring, sym->len, sym->inf);
#endif

  return 1;
}

/*
*************************************************************************
* Function:read Level VLC0 codeword from UVLC-partition 
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int readSyntaxElement_Level_VLC0(SyntaxElement *sym)
{
  int frame_bitoffset = currStream->frame_bitoffset;
  byte *buf = currStream->streamBuffer;
  int BitstreamLengthInBytes = currStream->bitstream_length;
  int len, sign, level, code;

  len = 0;
  
  while (!ShowBits(buf, frame_bitoffset+len, BitstreamLengthInBytes, 1))
    len++;

  len++;
  code = 1;
  frame_bitoffset += len;

  if (len < 15)
  {
    sign = (len - 1) & 1;
    level = (len-1) / 2 + 1;
  }
  else if (len == 15)
  {
    // escape code
    code = (code << 4) | ShowBits(buf, frame_bitoffset, BitstreamLengthInBytes, 4);
    len += 4;
    frame_bitoffset += 4;
    sign = (code & 1);
    level = ((code >> 1) & 0x7) + 8;
  }
  else if (len == 16)
  {
    // escape code
    code = (code << 12) | ShowBits(buf, frame_bitoffset, BitstreamLengthInBytes, 12);
    len += 12;
    frame_bitoffset += 12;
    sign =  (code & 1);
    level = ((code >> 1) & 0x7ff) + 16;
  }
  else
  {
    printf("ERROR reading Level code\n");
    exit(-1);
  }

  if (sign)
    level = -level;

  sym->inf = level;
  sym->len = len;

#if TRACE
  tracebits2(sym->tracestring, sym->len, code);
#endif

  currStream->frame_bitoffset = frame_bitoffset;

  return 0;
}

/*
*************************************************************************
* Function:read Level VLC codeword from UVLC-partition 
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int readSyntaxElement_Level_VLCN(SyntaxElement *sym, int vlc)  
{
  int frame_bitoffset = currStream->frame_bitoffset;
  byte *buf = currStream->streamBuffer;
  int BitstreamLengthInBytes = currStream->bitstream_length;
  
  int levabs, sign;
  int len = 0;
  int code, sb;
  
  int numPrefix;
  int shift = vlc-1;
  int escape = (15<<shift)+1;
  
  // read pre zeros
  numPrefix = 0;

  while (!ShowBits(buf, frame_bitoffset+numPrefix, BitstreamLengthInBytes, 1))
    numPrefix++;
  
  len = numPrefix+1;
  code = 1;
  
  if (numPrefix < 15)
  {
    levabs = (numPrefix<<shift) + 1;
    
    // read (vlc-1) bits -> suffix
    if (vlc-1)
    {
      sb =  ShowBits(buf, frame_bitoffset+len, BitstreamLengthInBytes, vlc-1);
      code = (code << (vlc-1) )| sb;
      levabs += sb;
      len += (vlc-1);
    }
    
    // read 1 bit -> sign
    sign = ShowBits(buf, frame_bitoffset+len, BitstreamLengthInBytes, 1);
    code = (code << 1)| sign;
    len ++;
  }
  else  // escape
  {
    // read 11 bits -> levabs
    sb = ShowBits(buf, frame_bitoffset+len, BitstreamLengthInBytes, 11);
    code = (code << 11 )| sb;
    
    levabs =  sb + escape;
    len+=11;
    
    // read 1 bit -> sign
    sign = ShowBits(buf, frame_bitoffset+len, BitstreamLengthInBytes, 1);
    code = (code << 1)| sign;
    len++;
  }
  
  sym->inf = (sign)?-levabs:levabs;
  sym->len = len;
  
  currStream->frame_bitoffset = frame_bitoffset+len;
  
#if TRACE
  tracebits2(sym->tracestring, sym->len, code);
#endif
  
  return 0;
}

/*
*************************************************************************
* Function:Reads bits from the bitstream buffer
* Input:
    byte buffer[]
   containing VLC-coded data bits
      int totbitoffset
     bit offset from start of partition
        int bytecount
     total bytes in bitstream
      int numbits
     number of bits to read
* Output:
* Return: 
* Attention:
*************************************************************************
*/


int GetBits (byte buffer[],int totbitoffset,int *info, int bytecount, 
             int numbits)
{
  register int inf;
  long byteoffset;      // byte from start of buffer
  int bitoffset;      // bit from start of byte

  int bitcounter=numbits;

  byteoffset= totbitoffset/8;
  bitoffset= 7-(totbitoffset%8);

  inf=0;
  while (numbits)
  {
    inf <<=1;
    inf |= (buffer[byteoffset] & (0x01<<bitoffset))>>bitoffset;
    numbits--;
    bitoffset--;
    if (bitoffset < 0)
    {
      byteoffset++;
      bitoffset += 8;
      if (byteoffset > bytecount)
      {
        return -1;
      }
    }
  }

  *info = inf;

  return bitcounter;           // return absolute offset in bit from start of frame
}      

/*
*************************************************************************
* Function:Reads bits from the bitstream buffer
* Input:
    byte buffer[]
     containing VLC-coded data bits
        int totbitoffset
     bit offset from start of partition
    int bytecount
     total bytes in bitstream
    int numbits
     number of bits to read
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int ShowBits (byte buffer[],int totbitoffset,int bytecount, int numbits)
{

  register int inf;
  long byteoffset;      // byte from start of buffer
  int bitoffset;      // bit from start of byte

  byteoffset= totbitoffset/8;
  bitoffset= 7-(totbitoffset%8);

  inf=0;
  while (numbits)
  {
    inf <<=1;
    inf |= (buffer[byteoffset] & (0x01<<bitoffset))>>bitoffset;
    numbits--;
    bitoffset--;
    if (bitoffset < 0)
    {
      byteoffset++;
      bitoffset += 8;
      if (byteoffset > bytecount)
      {
        return -1;
      }
    }
  }

  return inf;           // return absolute offset in bit from start of frame
}     

////////////////////////////////////////////////////////////////////////////
// Yulj 2004.07.15
// for decision of slice end.
////////////////////////////////////////////////////////////////////////////
int get_uv (int LenInBits, char*tracestring)
{
  SyntaxElement symbol, *sym=&symbol;

  assert (currStream->streamBuffer != NULL);
  sym->mapping = linfo_ue;   // Mapping rule
  sym->len = LenInBits;
  SYMTRACESTRING(tracestring);
  GetSyntaxElement_FLC (sym);

  return sym->inf;
}

/////////////////////////////////////////////////////////////
// Yulj 2004.07.15
// for decision of slice end.
/////////////////////////////////////////////////////////////
int GetSyntaxElement_FLC(SyntaxElement *sym)
{
  int frame_bitoffset = currStream->frame_bitoffset;
  byte *buf = currStream->streamBuffer;
  int BitstreamLengthInBytes = currStream->bitstream_length;

  if ((GetBits(buf, frame_bitoffset, &(sym->inf), BitstreamLengthInBytes, sym->len)) < 0)
    return -1;

  sym->value1 = sym->inf;

#if TRACE
  tracebits2(sym->tracestring, sym->len, sym->inf);
#endif

  return 1;
}
