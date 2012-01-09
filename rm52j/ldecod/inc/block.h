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

#include "global.h"
#include "defines.h"

#define NUM_2D_TABLES 4
#define CODE2D_ESCAPE_SYMBOL 59

// ========================================================
// external variables
// ========================================================
extern const int V[QUANT_PERIOD][6];  //!< De-Quantization table
extern const int DQMAP[4][4];          //!< Mapping the proper de-quantization value for coefficient position
extern const int N[2];                //!< Dequantization normalization
extern const int SCAN[2][64][2];      //!< Scan positions. Positions are stored as (pix,lin).
extern const char AVS_2D_VLC[NUM_2D_TABLES][16][8];  //   Inter, Intra0-13, Intra14-21, Intra22-31
extern const unsigned short int cbp_blk_masks[4];
extern char       AVS_2D_VLC_dec[NUM_2D_TABLES][64][2]; //   inverse of last table. generated automatically in read_coef_AVS()
//Lou 1014 extern const byte QP_SCALE_CR[52] ;
extern const byte QP_SCALE_CR[64] ;
extern const int AVS_VS_SCAN[64][2];
extern const int AVS_NS_SCAN[64][2];

extern char       AVS_2DVLC_INTRA_dec[7][64][2];
extern char       AVS_2DVLC_INTER_dec[7][64][2];
extern char       AVS_2DVLC_CHROMA_dec[5][64][2];
extern const char AVS_2DVLC_INTRA[7][26][27];
extern const char AVS_2DVLC_INTER[7][26][27];
extern const char AVS_2DVLC_CHROMA[5][26][27];

extern const char VLC_Golomb_Order[3][7][2];          //qwang 11.29
extern const char MaxRun[3][7];                          // added by dj
extern const char RefAbsLevel[19][26];                   // added by dj

// functions
void inv_transform_B8  (short curr_blk[B8_SIZE][B8_SIZE]);
void get_curr_blk      (int block8x8,struct img_par *img, short curr_blk[B8_SIZE][B8_SIZE]);
void idct_dequant_B8   (int block8x8,
                        int qp,                         // Quantization parameter
                        short curr_blk[B8_SIZE][B8_SIZE],
                        struct img_par *img
                        );

int intrapred(struct img_par *img,int img_x,int img_y);
void readLumaCoeff_B8(int block8x8, struct inp_par *inp, struct img_par *img);
void readChromaCoeff_B8(int block8x8, struct inp_par *inp, struct img_par *img);
