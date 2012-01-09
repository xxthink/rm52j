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

#ifndef _AVS_TRANSFORM_H_
#define _AVS_TRANSFORM_H_

#include "global.h"

#define NUM_2D_TABLES 4
#define CODE2D_ESCAPE_SYMBOL 59

// external variables
extern const int AVS_COEFF_COST[64];      //!< TML COEFF_COST 'stretched' for AVS
extern const char AVS_2D_VLC[NUM_2D_TABLES][16][8];  //   Inter, Intra0-13, Intra14-21, Intra22-31

extern const char AVS_2DVLC_INTRA[7][26][27];     
extern const char AVS_2DVLC_INTER[7][26][27];
extern const char AVS_2DVLC_CHROMA[5][26][27];

extern const char VLC_Golomb_Order[3][7][2];          //qwang 11.29
extern const char MaxRun[3][7];                          // added by dj
extern const char RefAbsLevel[19][26];                   // added by dj

//99
short MB_16[8][8], MB_16_temp[8][8];

// typedefs
typedef enum
{
    PIX,
    LIN
} Direction;

// functions
void transform_B8      (short curr_blk[B8_SIZE][B8_SIZE]);
void inv_transform_B8  (short curr_blk[B8_SIZE][B8_SIZE]);
void quant_B8          (int qp, int mode, short curr_blk[B8_SIZE][B8_SIZE]);
int  scanquant_B8      (int qp, int mode, int b8, short curr_blk[B8_SIZE][B8_SIZE], int scrFlag, int *cbp, int *cbp_blk);

int  find_sad_8x8          (int iMode, int iSizeX, int iSizeY, int iOffX, int iOffY, int m7[MB_BLOCK_SIZE][MB_BLOCK_SIZE]);
int  sad_hadamard          (int iSizeX, int iSizeY, int iOffX, int iOffY, int m7[MB_BLOCK_SIZE][MB_BLOCK_SIZE]);
int  writeLumaCoeffAVS_B8  (int b8,int intra);
int  writeChromaCoeffAVS_B8(int b8,int intra);
int Mode_Decision_for_AVS_IntraBlocks(int b8,double lambda,int *min_cost);
double RDCost_for_AVSIntraBlocks(int *nonzero,int b8,int ipmode,double lambda,double  min_rdcost,int mostProbableMode);
void idct_transform(short *mb, short *temp) ;

#endif // _AVS_TRANSFORM_H_
