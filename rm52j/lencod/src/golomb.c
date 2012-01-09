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
* File name: golomb.c
* Function: Description
*
*************************************************************************************
*/


#include <assert.h>
#include "golomb.h"
#include "vlc.h"

/*
*************************************************************************
* Function:
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void encode_golomb_word(unsigned int symbol,unsigned int grad0,unsigned int max_levels,unsigned int *res_bits,unsigned int *res_len)
{
  unsigned int level,res,numbits;
  
  res=1UL<<grad0;
  level=1UL;numbits=1UL+grad0;

  //find golomb level
  while( symbol>=res && level<max_levels )
  {
    symbol-=res;
    res=res<<1;
    level++;
    numbits+=2UL;
  }

  if(level>=max_levels)
  {
    if(symbol>=res)
      symbol=res-1UL;  //crop if too large.
  }

  //set data bits
  *res_bits=res|symbol;
  *res_len=numbits;
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

void encode_multilayer_golomb_word(unsigned int symbol,const unsigned int *grad,const unsigned int *max_levels,unsigned int *res_bits,unsigned int *res_len)
{
  unsigned accbits,acclen,bits,len,tmp;

  accbits=acclen=0UL;
  
  while(1)
  {
    encode_golomb_word(symbol,*grad,*max_levels,&bits,&len);
    accbits=(accbits<<len)|bits;
    acclen+=len;
    assert(acclen<=32UL);  //we'l be getting problems if this gets longer than 32 bits.
    tmp=*max_levels-1UL;

    if(!(( len == (tmp<<1)+(*grad) )&&( bits == (1UL<<(tmp+*grad))-1UL )))  //is not last possible codeword? (Escape symbol?)
      break;

    tmp=*max_levels;
    symbol-=(((1UL<<tmp)-1UL)<<(*grad))-1UL;
    grad++;max_levels++;
  }
  *res_bits=accbits;
  *res_len=acclen;
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

int writeSyntaxElement_GOLOMB(SyntaxElement *se, Bitstream *bitstream)
{
 unsigned int bits,len,i;
 unsigned int grad[4],max_lev[4];

  if(!( se->golomb_maxlevels&~0xFF ))    //only bits 0-7 used? This means normal Golomb word.
    encode_golomb_word(se->value1,se->golomb_grad,se->golomb_maxlevels,&bits,&len);
  else
  {
    for(i=0UL;i<4UL;i++)
    {
      grad[i]=(se->golomb_grad>>(i<<3))&0xFFUL;
      max_lev[i]=(se->golomb_maxlevels>>(i<<3))&0xFFUL;
    }
    encode_multilayer_golomb_word(se->value1,grad,max_lev,&bits,&len);
  }

  se->len=len;
  se->bitpattern=bits;

  writeUVLC2buffer(se, bitstream);

#if TRACE
  snprintf(se->tracestring, TRACESTRING_SIZE, "Coefficients");
  if(se->type <= 1)
    trace2out (se);
#endif

  return (se->len);
}
