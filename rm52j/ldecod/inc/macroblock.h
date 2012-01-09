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
* File name:  macroblock.h
* Function: 
*
*************************************************************************************
*/


#ifndef _MACROBLOCK_H_
#define _MACROBLOCK_H_

const byte QP_SCALE_CR[64]=
{
     0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10,11,12,13,14,15,16,17,18,19,
    20,21,22,23,24,25,26,27,28,29,
    30,31,32,33,34,35,36,37,38,39,
    40,41,42,42,43,43,44,44,45,45,
    46,46,47,47,48,48,48,49,49,49,
    50,50,50,51,
};//Lou 1014


//! gives CBP value from codeword number, both for intra and inter
/*
const byte NCBP[64][2]=
{
  //modified by qwang
  {15, 0},{63,15},{47, 4},{ 0, 8},{13, 2},{ 7, 1},{14, 3},{11,11},{31,13},{ 5,12},{12, 7},{ 3,14},
  {10,10},{ 1, 5},{ 4, 9},{ 8, 6},{ 2,31},{ 6,63},{ 9,47},{55,16},{23,32},{29,23},{39,48},{45,29},
  {61,46},{43,39},{27,43},{46,30},{30,27},{59,45},{62,21},{21,37},{37,42},{19,19},{53,26},{26,28},
  {28,55},{42,35},{44,17},{16,20},{32,61},{35,44},{51,62},{20,24},{33,59},{25,18},{18,53},{38,34},
  {41,33},{17,40},{22,36},{40,58},{36,22},{34,41},{24,51},{58,25},{49,38},{54,60},{60,52},{48,50},
  {52,49},{57,56},{56,54},{50,57}
};*/



/*const byte NCBP[64][2]=
{
  {63, 0},{31, 1},{47, 2},{15, 4},{ 0, 8},{ 7,16},{11,32},{13,48},{14, 3},{55, 5},
  {23,10},{39,12},{59,15},{27,31},{43,47},{61,63},{29, 7},{45,11},{62,13},{30,14},
  {46, 6},{ 3, 9},{ 5,19},{10,35},{12,51},{51,21},{19,37},{35,53},{53,26},{21,42},
  {37,58},{58,28},{26,44},{42,60},{60,17},{28,33},{44,49},{ 1,18},{ 2,34},{ 4,50},
  { 8,20},{ 6,36},{ 9,52},{48,24},{16,40},{32,56},{49,23},{17,39},{33,55},{50,27},
  {18,43},{34,59},{52,29},{20,45},{36,61},{56,30},{24,46},{40,62},{54,22},{22,38},
  {38,54},{57,25},{25,41},{41,57},
};*/

const byte NCBP[64][2]=                                   // jlzheng 7.20
{
  {63, 0},{15, 15},{31, 63},{47, 31},{ 0, 16},{ 14,32},{13,47},{11,13},{7, 14},{5, 11},
  {10,12},{8,5},{12,10},{61,7},{4,48},{55,3},{1, 2},{2,8},{59,4},{3,1},
  {62, 61},{ 9, 55},{ 6,59},{29,62},{45,29},{51,27},{23,23},{39,19},{27,30},{46,28},
  {53,9},{30,6},{43,60},{37,21},{60,44},{16,26},{21,51},{ 28,35},{ 19,18},{ 35,20},
  { 42,24},{ 26,53},{ 44,17},{32,37},{58,39},{24,45},{20,58},{17,43},{18,42},{48,46},
  {22,36},{33,33},{25,34},{49,40},{40,52},{36,49},{34,50},{50,56},{52,25},{54,22},
  {41,54},{56,57},{38,41},{57,38},
};

//! used to control block sizes : Not used/16x16/16x8/8x16/8x8/8x4/4x8/4x4

const int BLOCK_STEP[8][2]=
{
  {0,0},{2,2},{2,1},{1,2},{1,1},{2,1},{1,2},{1,1}
};

void readCBPandCoeffsFromNAL(struct img_par *img,struct inp_par *inp);

#endif

