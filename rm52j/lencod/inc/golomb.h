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
* File name: 
* Function: 
*
*************************************************************************************
*/
#ifndef GOLOMB_H
#define GOLOMB_H

#include "global.h"

void encode_golomb_word(unsigned int symbol,unsigned int grad0,unsigned int max_levels,unsigned int *res_bits,unsigned int *res_len); //returns symbol coded. (might be cropped if max_levels is too small)
void encode_multilayer_golomb_word(unsigned int symbol,const unsigned int *grad,const unsigned int *max_levels,unsigned int *res_bits,unsigned int *res_len); //terminate using a max_levels value of 30UL.

unsigned int decode_golomb_word(const unsigned char **buffer,unsigned int *bitoff,unsigned int grad0,unsigned int max_levels);

int writeSyntaxElement_GOLOMB(SyntaxElement *se, Bitstream *bitstream);

#endif