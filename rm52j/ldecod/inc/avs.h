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
#include "defines.h"

#define NUM_2D_TABLES 4
#define CODE2D_ESCAPE_SYMBOL 59

// ========================================================
// external variables
// ========================================================
extern const int AVS_matrix[8][8];        //!< AVS transform matrices
extern const int AVS_Q[QUANT_PERIOD][6];  //!< AVS Quantization table
extern const int AVS_R[QUANT_PERIOD][6];  //!< AVS De-Quantization table
extern const int AVS_QMAP[4][4];          //!< AVS mapping the proper Q value for coefficient position
extern const int AVS_QF[4][2];               //!< AVS rounding factor
extern const int AVS_N[2];                //!< AVS quantization normalization
extern const int AVS_SHIFT0[2][2];        //!< AVS bit shift needed after transform step to stay inside 16 bits.
extern const int AVS_SCAN[2][64][2];      //!< AVS scan positions. Positions are stored as (pix,lin).
extern const int AVS_COEFF_COST[64];      //!< TML COEFF_COST 'stretched' for AVS
extern const char AVS_2D_VLC[NUM_2D_TABLES][16][8];  //   Inter, Intra0-13, Intra14-21, Intra22-31
extern const unsigned short int cbp_blk_masks[4];
extern char       AVS_2D_VLC_dec[NUM_2D_TABLES][64][2]; //   inverse of last table. generated automatically in read_coef_AVS()

extern char       ABT_2D_CVLC_I_dec[6][64][2];
extern char       ABT_2D_CVLC_P_dec[6][64][2];
extern char       ABT_2D_CVLC_B_dec[6][64][2];
extern const char ABT_2D_CVLC_I[6][25][25];
extern const char ABT_2D_CVLC_P[6][25][25];
extern const char ABT_2D_CVLC_B[6][25][25];
// ========================================================
// typedefs
// ========================================================
typedef enum
{
  PIX,
  LIN
} Direction;


// ========================================================
// functions
// ========================================================
void transform_B8      (short curr_blk[B8_SIZE][B8_SIZE]);
void inv_transform_B8  (short curr_blk[B8_SIZE][B8_SIZE]);
void quant_B8          (int qp, int mode, short curr_blk[B8_SIZE][B8_SIZE]);
int  scanquant_B8      (int qp, int mode, int b8, short curr_blk[B8_SIZE][B8_SIZE], int scrFlag, int *cbp, int *cbp_blk);


int  find_sad_8x8          (int iMode, int iSizeX, int iSizeY, int iOffX, int iOffY, int m7[MB_BLOCK_SIZE][MB_BLOCK_SIZE]);
int  sad_hadamard          (int iSizeX, int iSizeY, int iOffX, int iOffY, int m7[MB_BLOCK_SIZE][MB_BLOCK_SIZE]);
int  writeLumaCoeffAVS_B8  (int b8,int intra,int blk_off_x,int blk_off_y);
void setDirectModeAVS      (int block8x8,struct img_par *img);
int  getDirectModeAVS      (int block8x8,struct img_par *img);
void copyblock_SP_AVS      (int b8);
void get_quant_consts      (int qp,int intra,int Q[4][4],int Qrshift[4][4],int qp_const[4][4]);

void get_curr_blk( int block8x8,struct img_par *img, short curr_blk[B8_SIZE][B8_SIZE]);

void idct_dequant_AVS_sp(int block8x8,
                         int curr_blk[B8_SIZE][B8_SIZE],
                         struct img_par *img
                         );
void idct_dequant_AVS_B8(int block8x8,
                         int qp,                         // Quantization parameter
                         int curr_blk[B8_SIZE][B8_SIZE],
                         struct img_par *img
                         );

void idct_dequant_AVS_B8_Chroma(int block8x8,
                         int qp,                         // Quantization parameter
                         int curr_blk[B8_SIZE][B8_SIZE],
                         struct img_par *img
                         );

int intrapred_AVS(struct img_par *img,int img_x,int img_y);
int Mode_Decision_for_AVS_IntraBlocks(int b8,double lambda,int *min_cost);
double RDCost_for_AVSIntraBlocks(int *nonzero,int b8,int ipmode,double lambda,double  min_rdcost,int mostProbableMode);
void readLumaCoeffAVS_B8(int block8x8, struct inp_par *inp, struct img_par *img);
void readChromaCoeffAVS_B8(int block8x8, struct inp_par *inp, struct img_par *img);
#endif // _AVS_TRANSFORM_H_
