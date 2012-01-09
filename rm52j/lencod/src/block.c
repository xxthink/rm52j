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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <memory.h>

#include "defines.h"
#include "global.h"
#include "rdopt_coding_state.h"
#include "vlc.h"
#include "block.h"
#include "golomb.h"

#define Q_BITS          21
#define EP (edgepixels+20)

#ifdef WIN32
#define int16 __int16
#else
#define int16 int16_t
#endif

//////////////////////////////////////////////////////////////////////////
unsigned short Q_TAB[64] = {
    32768,29775,27554,25268,23170,21247,19369,17770,
    16302,15024,13777,12634,11626,10624,9742,8958,
    8192,7512,6889,6305,5793,5303,4878,4467,
    4091,3756,3444,3161,2894,2654,2435,2235,
    2048,1878,1722,1579,1449,1329,1218,1117,
    1024,939,861,790,724,664,609,558,
    512,470,430,395,362,332,304,279,
    256,235,215,197,181,166,152,140
};

unsigned short IQ_TAB[64] = {

    32768,36061,38968,42495,46341,50535,55437,60424,
    32932,35734,38968,42495,46177,50535,55109,59933,
    65535,35734,38968,42577,46341,50617,55027,60097,
    32809,35734,38968,42454,46382,50576,55109,60056,
    65535,35734,38968,42495,46320,50515,55109,60076,
    65535,35744,38968,42495,46341,50535,55099,60087,
    65535,35734,38973,42500,46341,50535,55109,60097,
    32771,35734,38965,42497,46341,50535,55109,60099

};

short IQ_SHIFT[64] = {
    15,15,15,15,15,15,15,15,
    14,14,14,14,14,14,14,14,
    14,13,13,13,13,13,13,13,
    12,12,12,12,12,12,12,12,
    12,11,11,11,11,11,11,11,
    11,10,10,10,10,10,10,10,
    10,9,9,9,9,9,9,9,
    8,8,8,8,8,8,8,8

};

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

#define absm(A) ((A)<(0) ? (-(A)):(A))

const int ZD_MAT[8][8] = {
    {8,8,8,8,8,8,8,8},
    {10,9,6,2,-2,-6,-9,-10},
    {10,4,-4,-10,-10,-4,4,10},
    {9,-2,-10,-6,6,10,2,-9},
    {8,-8,-8,8,8,-8,-8,8},
    {6,-10,2,9,-9,-2,10,-6},
    {4,-10,10,-4,-4,10,-10,4},
    {2,-6,9,-10,10,-9,6,-2}
};

/*Lou End*/
int ScaleM[4][4] ={
    {32768,37958,36158,37958},
    {37958,43969,41884,43969},
    {36158,41884,39898,41884},
    {37958,43969,41884,43969}
};

extern const int AVS_SCAN[2][64][2];

#define B8_SIZE 8

void transform_B8(short curr_blk[B8_SIZE][B8_SIZE])   // block to be transformed.
{
    int xx, yy;
    int b[8];
    int tmp[8];

    // Horizontal Transform
    for(yy=0; yy<8; yy++)
    {
        // First Butterfly
        b[0]=curr_blk[yy][0] + curr_blk[yy][7];
        b[1]=curr_blk[yy][1] + curr_blk[yy][6];
        b[2]=curr_blk[yy][2] + curr_blk[yy][5];
        b[3]=curr_blk[yy][3] + curr_blk[yy][4];
        b[4]=curr_blk[yy][0] - curr_blk[yy][7];
        b[5]=curr_blk[yy][1] - curr_blk[yy][6];
        b[6]=curr_blk[yy][2] - curr_blk[yy][5];
        b[7]=curr_blk[yy][3] - curr_blk[yy][4];

        // Upright Butterfly
        tmp[0]=b[0] + b[3];
        tmp[1]=b[1] + b[2];
        tmp[2]=b[0] - b[3];
        tmp[3]=b[1] - b[2];

        b[0]=(tmp[0] + tmp[1])<<3;
        b[1]=(tmp[0] - tmp[1])<<3;

        /*Lou Change*/
        b[2]=((tmp[2]*10)+(tmp[3]<<2));
        b[3]=((tmp[2]<<2)-(tmp[3]*10));
        /*Lou End*/

        // Downright Butterfly
        tmp[4]=b[4];
        tmp[5]=b[5];
        tmp[6]=b[6];
        tmp[7]=b[7];

        /*Lou Change*/
        tmp[0]=(((tmp[4] - tmp[7])<<1) + tmp[4]);
        tmp[1]=(((tmp[5] + tmp[6])<<1) + tmp[5]);
        tmp[2]=(((tmp[5] - tmp[6])<<1) - tmp[6]);
        tmp[3]=(((tmp[4] + tmp[7])<<1) + tmp[7]);

        b[4]=(((tmp[0] + tmp[1] + tmp[3])<<1) + tmp[1]);//        10*tmp[4]   +9*tmp[5]  +6*tmp[6]  +2*tmp[7];
        b[5]=(((tmp[0] - tmp[1] + tmp[2])<<1) + tmp[0]);//        9*tmp[4]   -2*tmp[5]  -10*tmp[6]  -6*tmp[7];
        b[6]=(((-tmp[1] - tmp[2] + tmp[3])<<1) + tmp[3]);//        6*tmp[4]   -10*tmp[5]  -2*tmp[6]  +9*tmp[7];
        b[7]=(((tmp[0] - tmp[2] - tmp[3])<<1) - tmp[2]);//        2*tmp[4]   -6*tmp[5]  +9*tmp[6]  -10*tmp[7];
        /*Lou End*/

        // Output
        curr_blk[yy][0]=b[0];
        curr_blk[yy][1]=b[4];
        curr_blk[yy][2]=b[2];
        curr_blk[yy][3]=b[5];
        curr_blk[yy][4]=b[1];
        curr_blk[yy][5]=b[6];
        curr_blk[yy][6]=b[3];
        curr_blk[yy][7]=b[7];
    }


    // Vertical transform
    for(xx=0; xx<8; xx++)
    {
        // First Butterfly
        b[0]=curr_blk[0][xx] + curr_blk[7][xx];
        b[1]=curr_blk[1][xx] + curr_blk[6][xx];
        b[2]=curr_blk[2][xx] + curr_blk[5][xx];
        b[3]=curr_blk[3][xx] + curr_blk[4][xx];
        b[4]=curr_blk[0][xx] - curr_blk[7][xx];
        b[5]=curr_blk[1][xx] - curr_blk[6][xx];
        b[6]=curr_blk[2][xx] - curr_blk[5][xx];
        b[7]=curr_blk[3][xx] - curr_blk[4][xx];

        // Upright Butterfly
        tmp[0]=b[0] + b[3];
        tmp[1]=b[1] + b[2];
        tmp[2]=b[0] - b[3];
        tmp[3]=b[1] - b[2];

        b[0]=(tmp[0] + tmp[1])<<3;
        b[1]=(tmp[0] - tmp[1])<<3;
        /*Lou Change*/
        b[2]=((tmp[2]*10)+(tmp[3]<<2));
        b[3]=((tmp[2]<<2)-(tmp[3]*10));
        /*Lou End*/

        // Downright Butterfly
        tmp[4]=b[4];
        tmp[5]=b[5];
        tmp[6]=b[6];
        tmp[7]=b[7];

        /*Lou Change*/
        tmp[0]=(((tmp[4] - tmp[7])<<1) + tmp[4]);
        tmp[1]=(((tmp[5] + tmp[6])<<1) + tmp[5]);
        tmp[2]=(((tmp[5] - tmp[6])<<1) - tmp[6]);
        tmp[3]=(((tmp[4] + tmp[7])<<1) + tmp[7]);

        b[4]=(((tmp[0] + tmp[1] + tmp[3])<<1) + tmp[1]);//        10*tmp[4]   +9*tmp[5]  +6*tmp[6]  +2*tmp[7];
        b[5]=(((tmp[0] - tmp[1] + tmp[2])<<1) + tmp[0]);//        9*tmp[4]   -2*tmp[5]  -10*tmp[6]  -6*tmp[7];
        b[6]=(((-tmp[1] - tmp[2] + tmp[3])<<1) + tmp[3]);//        6*tmp[4]   -10*tmp[5]  -2*tmp[6]  +9*tmp[7];
        b[7]=(((tmp[0] - tmp[2] - tmp[3])<<1) - tmp[2]);//        2*tmp[4]   -6*tmp[5]  +9*tmp[6]  -10*tmp[7];
        /*Lou End*/

        // Output
        curr_blk[0][xx] = (short)((b[0]+(1<<4))>>5);
        curr_blk[1][xx] = (short)((b[4]+(1<<4))>>5);
        curr_blk[2][xx] = (short)((b[2]+(1<<4))>>5);
        curr_blk[3][xx] = (short)((b[5]+(1<<4))>>5);
        curr_blk[4][xx] = (short)((b[1]+(1<<4))>>5);
        curr_blk[5][xx] = (short)((b[6]+(1<<4))>>5);
        curr_blk[6][xx] = (short)((b[3]+(1<<4))>>5);
        curr_blk[7][xx] = (short)((b[7]+(1<<4))>>5);
    }
}

void quant_B8(int qp,                         // Quantization parameter
              int mode,                       // block tiling mode used for AVS. USED ALSO FOR SIGNALING INTRA (AVS_mode+4)
              short curr_blk[B8_SIZE][B8_SIZE]
) {
    int xx, yy;
    int val, temp;
    int qp_const;
    int intra    = 0;

    if (mode>3) // mode 0..inter, mode 4.. intra
    {
        intra = 1;
    }

    if(intra)
        qp_const = (1<<15)*10/31;
    else
        qp_const = (1<<15)*10/62;

    for (yy=0; yy<8; yy++)
        for (xx=0; xx<8; xx++)
        {
            val = curr_blk[yy][xx];
            temp = absm(val);
            curr_blk[yy][xx] = sign(((((temp * ScaleM[yy&3][xx&3] + (1<<18)) >> 19)*Q_TAB[qp]+qp_const)>>15), val);

        }
}

void inv_transform_B8(short curr_blk[B8_SIZE][B8_SIZE]  // block to be inverse transformed.
)
{
    int xx, yy;
    int tmp[8];
    int t;
    int b[8];

    for(yy=0; yy<8; yy++)
    {
        // Horizontal inverse transform
        // Reorder
        tmp[0]=curr_blk[yy][0];
        tmp[1]=curr_blk[yy][4];
        tmp[2]=curr_blk[yy][2];
        tmp[3]=curr_blk[yy][6];
        tmp[4]=curr_blk[yy][1];
        tmp[5]=curr_blk[yy][3];
        tmp[6]=curr_blk[yy][5];
        tmp[7]=curr_blk[yy][7];

        // Downleft Butterfly
        /*Lou Change*/
        b[0] = ((tmp[4] - tmp[7])<<1) + tmp[4];
        b[1] = ((tmp[5] + tmp[6])<<1) + tmp[5];
        b[2] = ((tmp[5] - tmp[6])<<1) - tmp[6];
        b[3] = ((tmp[4] + tmp[7])<<1) + tmp[7];

        b[4] = ((b[0] + b[1] + b[3])<<1) + b[1];
        b[5] = ((b[0] - b[1] + b[2])<<1) + b[0];
        b[6] = ((-b[1] - b[2] + b[3])<<1) + b[3];
        b[7] = ((b[0] - b[2] - b[3])<<1) - b[2];
        /*Lou End*/

        // Upleft Butterfly
        /*Lou Change*/
        t=((tmp[2]*10)+(tmp[3]<<2));
        tmp[3]=((tmp[2]<<2)-(tmp[3]*10));
        tmp[2]=t;

        t=(tmp[0]+tmp[1])<<3;
        tmp[1]=(tmp[0]-tmp[1])<<3;
        tmp[0]=t;
        /*Lou End*/

        b[0]=tmp[0]+tmp[2];
        b[1]=tmp[1]+tmp[3];
        b[2]=tmp[1]-tmp[3];
        b[3]=tmp[0]-tmp[2];

        // Last Butterfly
        /*Lou Change*/
        //curr_blk[yy][0]=(short)(((b[0]+b[4])+(1<<2))>>3);
        //curr_blk[yy][1]=(short)(((b[1]+b[5])+(1<<2))>>3);
        //curr_blk[yy][2]=(short)(((b[2]+b[6])+(1<<2))>>3);
        //curr_blk[yy][3]=(short)(((b[3]+b[7])+(1<<2))>>3);
        //curr_blk[yy][7]=(short)(((b[0]-b[4])+(1<<2))>>3);
        //curr_blk[yy][6]=(short)(((b[1]-b[5])+(1<<2))>>3);
        //curr_blk[yy][5]=(short)(((b[2]-b[6])+(1<<2))>>3);
        //curr_blk[yy][4]=(short)(((b[3]-b[7])+(1<<2))>>3);
        /*Lou End*/
        /*WANGJP CHANGE 20061226*/
        /*  Commented by cjw, 20070327
        curr_blk[yy][0]=((int16)((int16)(b[0]+b[4])+4))>>3;
        curr_blk[yy][1]=((int16)((int16)(b[1]+b[5])+4))>>3;
        curr_blk[yy][2]=((int16)((int16)(b[2]+b[6])+4))>>3;
        curr_blk[yy][3]=((int16)((int16)(b[3]+b[7])+4))>>3;
        curr_blk[yy][7]=((int16)((int16)(b[0]-b[4])+4))>>3;
        curr_blk[yy][6]=((int16)((int16)(b[1]-b[5])+4))>>3;
        curr_blk[yy][5]=((int16)((int16)(b[2]-b[6])+4))>>3;
        curr_blk[yy][4]=((int16)((int16)(b[3]-b[7])+4))>>3;
        */
        /*WANGJP END*/
        //070305 dingdandan
        curr_blk[yy][0]=(short)((Clip3(-32768,32767,((b[0]+b[4])+(1<<2))))>>3);
        curr_blk[yy][1]=(short)((Clip3(-32768,32767,((b[1]+b[5])+(1<<2))))>>3);
        curr_blk[yy][2]=(short)((Clip3(-32768,32767,((b[2]+b[6])+(1<<2))))>>3);
        curr_blk[yy][3]=(short)((Clip3(-32768,32767,((b[3]+b[7])+(1<<2))))>>3);
        curr_blk[yy][7]=(short)((Clip3(-32768,32767,((b[0]-b[4])+(1<<2))))>>3);
        curr_blk[yy][6]=(short)((Clip3(-32768,32767,((b[1]-b[5])+(1<<2))))>>3);
        curr_blk[yy][5]=(short)((Clip3(-32768,32767,((b[2]-b[6])+(1<<2))))>>3);
        curr_blk[yy][4]=(short)((Clip3(-32768,32767,((b[3]-b[7])+(1<<2))))>>3);
        //070305 dingdandan


    }
    // Vertical inverse transform
    for(xx=0; xx<8; xx++)
    {

        // Reorder
        tmp[0]=curr_blk[0][xx];
        tmp[1]=curr_blk[4][xx];
        tmp[2]=curr_blk[2][xx];
        tmp[3]=curr_blk[6][xx];
        tmp[4]=curr_blk[1][xx];
        tmp[5]=curr_blk[3][xx];
        tmp[6]=curr_blk[5][xx];
        tmp[7]=curr_blk[7][xx];

        // Downleft Butterfly
        /*Lou Change*/
        b[0] = ((tmp[4] - tmp[7])<<1) + tmp[4];
        b[1] = ((tmp[5] + tmp[6])<<1) + tmp[5];
        b[2] = ((tmp[5] - tmp[6])<<1) - tmp[6];
        b[3] = ((tmp[4] + tmp[7])<<1) + tmp[7];

        b[4] = ((b[0] + b[1] + b[3])<<1) + b[1];
        b[5] = ((b[0] - b[1] + b[2])<<1) + b[0];
        b[6] = ((-b[1] - b[2] + b[3])<<1) + b[3];
        b[7] = ((b[0] - b[2] - b[3])<<1) - b[2];
        /*Lou End*/

        // Upleft Butterfly
        /*Lou Change*/
        t=((tmp[2]*10)+(tmp[3]<<2));
        tmp[3]=((tmp[2]<<2)-(tmp[3]*10));
        tmp[2]=t;

        t=(tmp[0]+tmp[1])<<3;
        tmp[1]=(tmp[0]-tmp[1])<<3;
        tmp[0]=t;
        /*Lou End*/

        b[0]=tmp[0]+tmp[2];
        b[1]=tmp[1]+tmp[3];
        b[2]=tmp[1]-tmp[3];
        b[3]=tmp[0]-tmp[2];

        // Last Butterfly
        //curr_blk[0][xx]=(b[0]+b[4]+64)>>7;/*(Clip3(-32768,32703,b[0]+b[4])+64)>>7;*/
        //curr_blk[1][xx]=(b[1]+b[5]+64)>>7;/*(Clip3(-32768,32703,b[1]+b[5])+64)>>7;*/
        //curr_blk[2][xx]=(b[2]+b[6]+64)>>7;/*(Clip3(-32768,32703,b[2]+b[6])+64)>>7;*/
        //curr_blk[3][xx]=(b[3]+b[7]+64)>>7;/*(Clip3(-32768,32703,b[3]+b[7])+64)>>7;*/
        //curr_blk[7][xx]=(b[0]-b[4]+64)>>7;/*(Clip3(-32768,32703,b[0]-b[4])+64)>>7;*/
        //curr_blk[6][xx]=(b[1]-b[5]+64)>>7;/*(Clip3(-32768,32703,b[1]-b[5])+64)>>7;*/
        //curr_blk[5][xx]=(b[2]-b[6]+64)>>7;/*(Clip3(-32768,32703,b[2]-b[6])+64)>>7;*/
        //curr_blk[4][xx]=(b[3]-b[7]+64)>>7;/*(Clip3(-32768,32703,b[3]-b[7])+64)>>7;*/

        /*WANGJP CHANGE 20061226*/
        /* Commented by cjw, 20070327
        curr_blk[0][xx]=((int16)((int16)(b[0]+b[4])+64))>>7;
        curr_blk[1][xx]=((int16)((int16)(b[1]+b[5])+64))>>7;
        curr_blk[2][xx]=((int16)((int16)(b[2]+b[6])+64))>>7;
        curr_blk[3][xx]=((int16)((int16)(b[3]+b[7])+64))>>7;
        curr_blk[7][xx]=((int16)((int16)(b[0]-b[4])+64))>>7;
        curr_blk[6][xx]=((int16)((int16)(b[1]-b[5])+64))>>7;
        curr_blk[5][xx]=((int16)((int16)(b[2]-b[6])+64))>>7;
        curr_blk[4][xx]=((int16)((int16)(b[3]-b[7])+64))>>7;
        */
        /*WANGJP END*/

        //070305 dingdandan
        curr_blk[0][xx]=(Clip3(-32768,32767,(b[0]+b[4])+64))>>7;
        curr_blk[1][xx]=(Clip3(-32768,32767,(b[1]+b[5])+64))>>7;
        curr_blk[2][xx]=(Clip3(-32768,32767,(b[2]+b[6])+64))>>7;
        curr_blk[3][xx]=(Clip3(-32768,32767,(b[3]+b[7])+64))>>7;
        curr_blk[7][xx]=(Clip3(-32768,32767,(b[0]-b[4])+64))>>7;
        curr_blk[6][xx]=(Clip3(-32768,32767,(b[1]-b[5])+64))>>7;
        curr_blk[5][xx]=(Clip3(-32768,32767,(b[2]-b[6])+64))>>7;
        curr_blk[4][xx]=(Clip3(-32768,32767,(b[3]-b[7])+64))>>7;
        //070305 dingdandan

        //it is uncessary to check range with clip operation
        //   //range checking
        //   if((b[0]+b[4])<=-32768 || (b[0]+b[4])>=(32768-65 ))
        //     printf("error\n");
        //   if((b[1]+b[5])<=-32768 || (b[1]+b[5])>=(32768-65 ))
        //     printf("error\n");
        //   if((b[2]+b[6])<=-32768 || (b[2]+b[6])>=(32768-65 ))
        //     printf("error\n");
        //   if((b[3]+b[7])<=-32768 || (b[3]+b[7])>=(32768-65 ))
        //     printf("error\n");
        //   if((b[0]-b[4])<=-32768 || (b[0]-b[4])>=(32768-65 ))
        //     printf("error\n");
        //   if((b[1]-b[5])<=-32768 || (b[1]-b[5])>=(32768-65 ))
        //     printf("error\n");
        //   if((b[2]-b[6])<=-32768 || (b[2]-b[6])>=(32768-65 ))
        //     printf("error\n");
        //   if((b[3]-b[7])<=-32768 || (b[3]-b[7])>=(32768-65 ))
        //     printf("error\n");
    }

}


/*
*************************************************************************
* Function:
Quantization, scan and reconstruction of a transformed 8x8 bock
Return coeff_cost of the block.
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

int scanquant_B8(int qp, int mode, int block8x8, // block8x8: number of current 8x8 block
                 short curr_blk[B8_SIZE][B8_SIZE],
                 int scrFlag,
                 int *cbp,
                 int *cbp_blk
                 )
{
    int  run;
    int  xx, yy;
    int  intra;
    int  icoef, ipos;
    int  b8_y          = (block8x8 / 2) << 3;
    int  b8_x          = (block8x8 % 2) << 3;
    int  coeff_cost    = 0;
    int* ACLevel;
    int* ACRun;
    int  curr_val,b8_cy;

    // Quantization
    quant_B8(qp, mode, curr_blk);

    // Scan
    intra=0;
    if(mode>3)
        intra=1;
    mode %= 4; // mode+=4 is used to signal intra coding to quant_B8().

    // General block information
    ACLevel  = img->cofAC[block8x8][0][0];
    ACRun    = img->cofAC[block8x8][0][1];
    for (xx=0; xx<65; xx++)
        ACRun[xx] = ACLevel[xx] = 0;

    run  = -1;
    ipos = 0;

    for (icoef=0; icoef<64; icoef++)
    {
        run++;
        xx = AVS_SCAN[img->picture_structure][icoef][0];
        yy = AVS_SCAN[img->picture_structure][icoef][1];

        curr_val = curr_blk[yy][xx];

        if (curr_val != 0)
        {
            ACLevel[ipos] = curr_val;
            ACRun[ipos]   = run;

            if (scrFlag && absm(ACLevel[ipos])==1)
                coeff_cost += (scrFlag==1)? 1 : AVS_COEFF_COST[run];
            else
                coeff_cost += MAX_VALUE; // block has to be saved

            {
                int val, temp, shift, QPI;
                shift = IQ_SHIFT[qp];
                QPI   = IQ_TAB[qp];
                val = curr_blk[yy][xx];
                temp = (val*QPI+(1<<(shift-2)) )>>(shift-1);
                curr_blk[yy][xx] = temp;// dequantization & descale

            }

            run = -1;
            ipos++;
        }
    }

    if (ipos>0) // there are coefficients
        if (block8x8 <= 3)
            (*cbp)     |= (1<<block8x8);
        else
            (*cbp)     |= (1<<(block8x8));/*lgp*dct*/


    // Inverse transform
    inv_transform_B8(curr_blk);
    if   (block8x8>5)    // wzm,422
        b8_cy=8;
    else
        b8_cy=0;

    // reconstruct current 8x8 block and add to prediction block
    for(yy=0;yy<8;yy++)
        for(xx=0;xx<8;xx++)
        {
            if(block8x8 <= 3)
                curr_val = img->mpr[b8_x+xx][b8_y+yy] + curr_blk[yy][xx];
            else
                curr_val = img->mpr[xx][yy+b8_cy] + curr_blk[yy][xx]; //wzm,422

            img->m7[xx][yy] = curr_blk[yy][xx] = clamp(curr_val,0,255);

            if(block8x8 <= 3)
                imgY[img->pix_y+b8_y+yy][img->pix_x+b8_x+xx] = (unsigned char)curr_blk[yy][xx];
            else
                //imgUV[block8x8-4][img->pix_c_y+yy][img->pix_c_x+xx] =  (unsigned char) curr_blk[yy][xx];//wzm,422
                imgUV[(block8x8-4)%2][img->pix_c_y+b8_cy+yy][img->pix_c_x+xx] = (unsigned char) curr_blk[yy][xx]; //wzm,422
        }
        for(yy=0;yy<8;yy++)
            for(xx=0;xx<8;xx++)
            {
                MB_16_temp[yy][xx] =  curr_blk[yy][xx]-128;
            }


            return coeff_cost;
}

/*
*************************************************************************
* Function: Calculate SAD or SATD for a prediction error block of size
iSizeX x iSizeY.
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

int find_sad_8x8(int iMode,
                 int iSizeX,
                 int iSizeY,
                 int iOffX,
                 int iOffY,
                 int m7[MB_BLOCK_SIZE][MB_BLOCK_SIZE]
)
{
    int i,j;
    int bmode;
    int ishift = 0;
    int sad    = 0;

    assert( (iSizeX+iOffX) <= MB_BLOCK_SIZE );
    assert( (iSizeY+iOffY) <= MB_BLOCK_SIZE );

    // m7[y,j,line][x,i,pixel]
    switch (iMode)
    {
    case 0 : // ---------- SAD ----------
        for (j=iOffY; j < iOffY+iSizeY; j++)
            for (i=iOffX; i < iOffX+iSizeX; i++)
                sad += absm(m7[j][i]);
        break;

    case 1 : // --------- SATD ----------
        bmode = iSizeX+iSizeY;
        if (bmode<24) // 8x8
        {
            sad = sad_hadamard(iSizeY,iSizeX,iOffY,iOffX,m7); // Attention: sad_hadamard() is X/Y flipped
            ishift = 2;
            sad = (sad + (1<<(ishift-1)))>>ishift;
        }
        else // 8x16-16x16
        {
            switch (bmode)
            {
            case 24 :               // 16x8 8x16
                sad  = sad_hadamard(8,8,iOffY,iOffX,m7);
                sad += sad_hadamard(8,8,iOffY+((iSizeY==16)?8:0) , iOffX+((iSizeX==16)?8:0) ,m7);
                ishift = 2;
                break;
            case 32 :               // 16x16
                sad  = sad_hadamard(8,8,0,0,m7);
                sad += sad_hadamard(8,8,8,0,m7);
                sad += sad_hadamard(8,8,0,8,m7);
                sad += sad_hadamard(8,8,8,8,m7);
                ishift = 2;
                break;
            default :
                assert(0==1);
            }
            sad = (sad + (1<<(ishift-1)))>>ishift;
        }
        break;

    default :
        assert(0==1);                // more switches may be added here later
    }

    return sad;
}

/*
*************************************************************************
* Function:
calculates the SAD of the Hadamard transformed block of
size iSizeX*iSizeY. Block may have an offset of (iOffX,iOffY).
If offset!=0 then iSizeX/Y has to be <=8.
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

int sad_hadamard(int iSizeX, int iSizeY, int iOffX, int iOffY, int m7[MB_BLOCK_SIZE][MB_BLOCK_SIZE])
{
    int i,j,ii;
    int m1[MB_BLOCK_SIZE][MB_BLOCK_SIZE];
    int m2[MB_BLOCK_SIZE][MB_BLOCK_SIZE];
    int m3[MB_BLOCK_SIZE][MB_BLOCK_SIZE];
    int sad = 0;

    int iy[MB_BLOCK_SIZE] =
    {
        iOffY,iOffY,iOffY,iOffY,
        iOffY,iOffY,iOffY,iOffY,
        iOffY,iOffY,iOffY,iOffY,
        iOffY,iOffY,iOffY,iOffY
    };

    // in this routine, cols are j,y and rows are i,x

    assert( ((iOffX==0)||(iSizeX<=8)) && ((iOffY==0)||(iSizeY<=8)) );

    for (j=1; j<iSizeY; j++)
        iy[j] += j;

    // vertical transform
    if (iSizeY == 4)
        for (i=0; i < iSizeX; i++)
        {
            ii = i+iOffX;

            m1[i][0] = m7[ii][iy[0]] + m7[ii][iy[3]];
            m1[i][1] = m7[ii][iy[1]] + m7[ii][iy[2]];
            m1[i][2] = m7[ii][iy[1]] - m7[ii][iy[2]];
            m1[i][3] = m7[ii][iy[0]] - m7[ii][iy[3]];

            m3[i][0] = m1[i][0] + m1[i][1];
            m3[i][1] = m1[i][0] - m1[i][1];
            m3[i][2] = m1[i][2] + m1[i][3];
            m3[i][3] = m1[i][3] - m1[i][2];
        }
    else
        for (i=0; i < iSizeX; i++)
        {
            ii = i+iOffX;

            m1[i][0] = m7[ii][iy[0]] + m7[ii][iy[4]];
            m1[i][1] = m7[ii][iy[1]] + m7[ii][iy[5]];
            m1[i][2] = m7[ii][iy[2]] + m7[ii][iy[6]];
            m1[i][3] = m7[ii][iy[3]] + m7[ii][iy[7]];
            m1[i][4] = m7[ii][iy[0]] - m7[ii][iy[4]];
            m1[i][5] = m7[ii][iy[1]] - m7[ii][iy[5]];
            m1[i][6] = m7[ii][iy[2]] - m7[ii][iy[6]];
            m1[i][7] = m7[ii][iy[3]] - m7[ii][iy[7]];

            m2[i][0] = m1[i][0] + m1[i][2];
            m2[i][1] = m1[i][1] + m1[i][3];
            m2[i][2] = m1[i][0] - m1[i][2];
            m2[i][3] = m1[i][1] - m1[i][3];
            m2[i][4] = m1[i][4] + m1[i][6];
            m2[i][5] = m1[i][5] + m1[i][7];
            m2[i][6] = m1[i][4] - m1[i][6];
            m2[i][7] = m1[i][5] - m1[i][7];

            m3[i][0] = m2[i][0] + m2[i][1];
            m3[i][1] = m2[i][0] - m2[i][1];
            m3[i][2] = m2[i][2] + m2[i][3];
            m3[i][3] = m2[i][2] - m2[i][3];
            m3[i][4] = m2[i][4] + m2[i][5];
            m3[i][5] = m2[i][4] - m2[i][5];
            m3[i][6] = m2[i][6] + m2[i][7];
            m3[i][7] = m2[i][6] - m2[i][7];
        }

        // horizontal transform
        if (iSizeX == 4)
            for (j=0; j < iSizeY; j++)
            {
                m1[0][j]=m3[0][j]+m3[3][j];
                m1[1][j]=m3[1][j]+m3[2][j];
                m1[2][j]=m3[1][j]-m3[2][j];
                m1[3][j]=m3[0][j]-m3[3][j];

                m2[0][j]=m1[0][j]+m1[1][j];
                m2[1][j]=m1[0][j]-m1[1][j];
                m2[2][j]=m1[2][j]+m1[3][j];
                m2[3][j]=m1[3][j]-m1[2][j];

                for (i=0; i < iSizeX; i++)
                    sad += absm(m2[i][j]);
            }
        else
            for (j=0; j < iSizeY; j++)
            {
                m2[0][j] = m3[0][j] + m3[4][j];
                m2[1][j] = m3[1][j] + m3[5][j];
                m2[2][j] = m3[2][j] + m3[6][j];
                m2[3][j] = m3[3][j] + m3[7][j];
                m2[4][j] = m3[0][j] - m3[4][j];
                m2[5][j] = m3[1][j] - m3[5][j];
                m2[6][j] = m3[2][j] - m3[6][j];
                m2[7][j] = m3[3][j] - m3[7][j];

                m1[0][j] = m2[0][j] + m2[2][j];
                m1[1][j] = m2[1][j] + m2[3][j];
                m1[2][j] = m2[0][j] - m2[2][j];
                m1[3][j] = m2[1][j] - m2[3][j];
                m1[4][j] = m2[4][j] + m2[6][j];
                m1[5][j] = m2[5][j] + m2[7][j];
                m1[6][j] = m2[4][j] - m2[6][j];
                m1[7][j] = m2[5][j] - m2[7][j];

                m2[0][j] = m1[0][j] + m1[1][j];
                m2[1][j] = m1[0][j] - m1[1][j];
                m2[2][j] = m1[2][j] + m1[3][j];
                m2[3][j] = m1[2][j] - m1[3][j];
                m2[4][j] = m1[4][j] + m1[5][j];
                m2[5][j] = m1[4][j] - m1[5][j];
                m2[6][j] = m1[6][j] + m1[7][j];
                m2[7][j] = m1[6][j] - m1[7][j];

                for (i=0; i < iSizeX; i++)
                    sad += absm(m2[i][j]);
            }

            return(sad);
}

int writeLumaCoeffAVS_B8(int b8,int intra)
{
    int no_bits        = 0;
    int mb_nr          = img->current_mb_nr;
    Macroblock *currMB = &img->mb_data[mb_nr];
    const int cbp      = currMB->cbp;
    int *bitCount      = currMB->bitcounter;
    SyntaxElement *currSE = &img->MB_SyntaxElements[currMB->currSEnr];

    const char (*AVS_2DVLC_table_intra)[26][27];
    const char (*AVS_2DVLC_table_inter)[26][27];

    int inumblk;                    /* number of blocks per CBP*/
    int inumcoeff;                  /* number of coeffs per block */
    int iblk;                       /* current block */
    int icoef;                      /* current coefficient */
    int ipos;                       /* current position in cof_AVS */
    int run, level;
    int i, j;
    int b4;
    int* ACLevel;
    int* ACRun;

    int symbol2D;
    int escape_level_diff; // added by dj

    int tablenum;
    static const int incVlc_intra[7] = { 0,1,2,4,7,10,3000};
    static const int incVlc_inter[7] = { 0,1,2,3,6,9,3000};

    static const int blk_table[4] = {0,-1,-1,-1};
    static const int blkmode2ctx [4] = {LUMA_8x8, LUMA_8x4, LUMA_4x8, LUMA_4x4};

    inumblk   = 1;
    inumcoeff = 65; // all positions + EOB

    AVS_2DVLC_table_intra = AVS_2DVLC_INTRA;
    AVS_2DVLC_table_inter = AVS_2DVLC_INTER;

    if ( cbp & (1<<b8) )
    {
        //code all symbols
        for (iblk=0; iblk<1; iblk++)
        {
            i=0;        // =b4
            j=(i&~1)<<1; //off_x
            i=(i&1)<<2;  //off_y
            if( (i<0) || (i>=8) || (j<0) || (j>=8) )
                continue;        //used in AVS Intra RDopt. code one one subblock.

            b4 = blk_table[iblk];

            level = 1; // get inside loop
            ipos  = 0;
            ACLevel = img->cofAC[b8][b4][0];
            ACRun   = img->cofAC[b8][b4][1];

            for(icoef=0;icoef<inumcoeff;icoef++) //count coeffs
                if(!ACLevel[icoef])
                    break;

            if(intra)
            {
                tablenum = 0;
                for(icoef; icoef>=0; icoef--)
                {
                    if(icoef == 0)   //EOB
                    {
                        level = 0;
                        run   = 0;
                    }
                    else
                    {
                        level = ACLevel[icoef-1];
                        run   = ACRun[icoef-1];
                    }

                    symbol2D=CODE2D_ESCAPE_SYMBOL;        //symbol for out-of-table
                    if(level>-27 && level<27 && run<26)
                    {
                        if(tablenum == 0)
                            symbol2D = AVS_2DVLC_table_intra[tablenum][run][abs(level)-1];
                        else
                            symbol2D = AVS_2DVLC_table_intra[tablenum][run][abs(level)];
                        if(symbol2D >= 0 && level < 0)
                            symbol2D++;
                        if(symbol2D < 0)
                            symbol2D = (CODE2D_ESCAPE_SYMBOL + (run<<1) + ((level>0)?1:0));
                    }
                    else
                    {
                        symbol2D = (CODE2D_ESCAPE_SYMBOL + (run<<1) + ((level>0)?1:0));
                    }

                    currSE->type = SE_LUM_AC_INTER;
                    currSE->value1=symbol2D;
                    currSE->value2=0;
                    currSE->golomb_grad = VLC_Golomb_Order[0][tablenum][0];
                    currSE->golomb_maxlevels = VLC_Golomb_Order[0][tablenum][1];

                    writeSyntaxElement_GOLOMB(currSE,currBitStream);
                    bitCount[BITS_COEFF_Y_MB]+=currSE->len;
                    no_bits+=currSE->len;
                    currSE++;// proceed to next SE
                    currMB->currSEnr++;

                    if(icoef == 0) break;

                    if(symbol2D>=CODE2D_ESCAPE_SYMBOL)
                    {
                        currSE->type = SE_LUM_AC_INTER;
                        currSE->golomb_grad = 1;
                        currSE->golomb_maxlevels = 11; //2007.05.05
                        escape_level_diff = abs(level)-((run>MaxRun[0][tablenum])?1:RefAbsLevel[tablenum][run]);
                        currSE->value1 = escape_level_diff;

                        writeSyntaxElement_GOLOMB(currSE,currBitStream);
                        bitCount[BITS_COEFF_Y_MB]+=currSE->len;
                        no_bits+=currSE->len;
                        currSE++;// proceed to next SE
                        currMB->currSEnr++;
                    }

                    if(abs(level) > incVlc_intra[tablenum])
                    {
                        if(abs(level) <= 2)
                            tablenum = abs(level);
                        else if(abs(level) <= 4)
                            tablenum = 3;
                        else if(abs(level) <= 7)
                            tablenum = 4;
                        else if(abs(level) <= 10)
                            tablenum = 5;
                        else
                            tablenum = 6;
                    }
                }
            }
            else //!intra
            {
                tablenum = 0;
                for(icoef; icoef>=0; icoef--)
                {
                    if(icoef == 0)   //EOB
                    {
                        level = 0;
                        run   = 0;
                    }
                    else
                    {
                        level = ACLevel[icoef-1];
                        run   = ACRun[icoef-1];
                    }

                    symbol2D=CODE2D_ESCAPE_SYMBOL;        //symbol for out-of-table
                    if(level>-27 && level<27 && run<26)
                    {
                        if(tablenum == 0)
                            symbol2D = AVS_2DVLC_table_inter[tablenum][run][abs(level)-1];
                        else
                            symbol2D = AVS_2DVLC_table_inter[tablenum][run][abs(level)];
                        if(symbol2D >= 0 && level < 0)
                            symbol2D++;
                        if(symbol2D < 0)
                            symbol2D = (CODE2D_ESCAPE_SYMBOL + (run<<1) + ((level>0)?1:0));
                    }
                    else
                    {
                        symbol2D = (CODE2D_ESCAPE_SYMBOL + (run<<1) + ((level>0)?1:0));
                    }

                    currSE->type = SE_LUM_AC_INTER;
                    currSE->value1=symbol2D;
                    currSE->value2=0;

                    currSE->golomb_grad = VLC_Golomb_Order[1][tablenum][0];
                    currSE->golomb_maxlevels = VLC_Golomb_Order[1][tablenum][1];

                    writeSyntaxElement_GOLOMB(currSE,currBitStream);
                    bitCount[BITS_COEFF_Y_MB]+=currSE->len;
                    no_bits+=currSE->len;
                    currSE++;// proceed to next SE
                    currMB->currSEnr++;

                    if(icoef == 0) break;

                    if(symbol2D>=CODE2D_ESCAPE_SYMBOL)
                    {
                        currSE->type = SE_LUM_AC_INTER;
                        currSE->golomb_grad = 0;
                        currSE->golomb_maxlevels = 12; //2007.05.09
                        escape_level_diff = abs(level)-((run>MaxRun[1][tablenum])?1:RefAbsLevel[tablenum+7][run]);
                        currSE->value1 = escape_level_diff;

                        writeSyntaxElement_GOLOMB(currSE,currBitStream);
                        bitCount[BITS_COEFF_Y_MB]+=currSE->len;
                        no_bits+=currSE->len;
                        currSE++;// proceed to next SE
                        currMB->currSEnr++;
                    }

                    if(abs(level) > incVlc_inter[tablenum])
                    {
                        if(abs(level) <= 3)
                            tablenum = abs(level);
                        else if(abs(level) <= 6)
                            tablenum = 4;
                        else if(abs(level) <= 9)
                            tablenum = 5;
                        else
                            tablenum = 6;
                    }
                }

            }// inter

        }//loop for iblk
    }//if ( cbp & (1<<b8) )

    return no_bits;
}

/*
*************************************************************************
* Function:Write chroma coefficients of one 8x8 block
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/


int writeChromaCoeffAVS_B8(int b8,int intra)
{
    int no_bits        = 0;
    int mb_nr          = img->current_mb_nr;
    Macroblock *currMB = &img->mb_data[mb_nr];
    const int cbp      = currMB->cbp;
    const int cbp01      = currMB->cbp01;   //wzm,422
    int *bitCount      = currMB->bitcounter;
    SyntaxElement *currSE = &img->MB_SyntaxElements[currMB->currSEnr];
    const char (*AVS_2DVLC_table_chroma)[26][27];

    int inumblk;                    /* number of blocks per CBP*/
    int inumcoeff;                  /* number of coeffs per block */
    int iblk;                       /* current block */
    int icoef;                      /* current coefficient */
    int ipos;                       /* current position in cof_AVS */
    int run, level;
    int i,j;

    int b4;
    int* ACLevel;
    int* ACRun;

    int tablenum;   //add by qwang
    //int incVlc_chroma[] = {0,1,2,4,3000};   //add by qwang
    static const int incVlc_chroma[5] = { 0,1,2,4,3000};

    int symbol2D;
    int escape_level_diff; // added by dj

    static const int blk_table[4] = {0,-1,-1,-1};
    static const int blkmode2ctx [4] = {LUMA_8x8, LUMA_8x4, LUMA_4x8, LUMA_4x4};
    int flag=0;

    inumblk   = 1;
    inumcoeff = 65; // all positions + EOB

    AVS_2DVLC_table_chroma = AVS_2DVLC_CHROMA;

    //added by wzm,422
    if(b8<6)
        flag=(cbp & (1<<b8));
    else
        flag=((cbp01>>(b8-6)) & 1);

    if(flag) //wzm,422
    {
        //code all symbols
        for (iblk=0; iblk<1; iblk++)
        {
            i=0;        // =b4
            j=(i&~1)<<1; //off_x
            i=(i&1)<<2;  //off_y
            if( (i<0) || (i>=8) || (j<0) || (j>=8) )
                continue;        //used in AVS Intra RDopt. code one one subblock.

            b4 = blk_table[iblk];
            level = 1; // get inside loop
            ipos  = 0;
            ACLevel = img->cofAC[b8][b4][0];
            ACRun   = img->cofAC[b8][b4][1];

            for(icoef=0;icoef<inumcoeff;icoef++) //count coeffs
            {
                if(!ACLevel[icoef])
                    break;
            }

            tablenum = 0;
            for(icoef; icoef>=0; icoef--)
            {
                if(icoef == 0)   //EOB
                {
                    level = 0;
                    run   = 0;
                }
                else
                {
                    level = ACLevel[icoef-1];
                    run   = ACRun[icoef-1];
                }

                symbol2D=CODE2D_ESCAPE_SYMBOL;        //symbol for out-of-table
                if(level>-27 && level<27 && run<26)
                {
                    if(tablenum == 0)
                        //symbol2D = AVS_2DVLC_CHROMA[tablenum][run][abs(level)-1];
                        symbol2D = AVS_2DVLC_table_chroma[tablenum][run][abs(level)-1];
                    else
                        //symbol2D = AVS_2DVLC_CHROMA[tablenum][run][abs(level)];
                        symbol2D = AVS_2DVLC_table_chroma[tablenum][run][abs(level)];
                    if(symbol2D >= 0 && level < 0)
                        symbol2D++;
                    if(symbol2D < 0)
                        symbol2D = (CODE2D_ESCAPE_SYMBOL + (run<<1) + ((level>0)?1:0));
                }
                // added by dj
                else
                {
                    symbol2D = (CODE2D_ESCAPE_SYMBOL + (run<<1) + ((level>0)?1:0));
                }
                //end

                currSE->type = SE_LUM_AC_INTER;
                currSE->value1=symbol2D;
                currSE->value2=0;
                currSE->golomb_grad = VLC_Golomb_Order[2][tablenum][0];
                currSE->golomb_maxlevels = VLC_Golomb_Order[2][tablenum][1];

                writeSyntaxElement_GOLOMB(currSE,currBitStream);
                bitCount[BITS_COEFF_UV_MB]+=currSE->len;
                no_bits+=currSE->len;
                currSE++;// proceed to next SE
                currMB->currSEnr++;

                if(icoef == 0) break;

                if(symbol2D>=CODE2D_ESCAPE_SYMBOL)   //???? select the optimal kth_Golomb code
                {
                    // changed by dj
                    currSE->type = SE_LUM_AC_INTER;
                    currSE->golomb_grad = 0;
                    currSE->golomb_maxlevels = 11;
                    escape_level_diff = abs(level)-((run>MaxRun[2][tablenum])?1:RefAbsLevel[tablenum+14][run]);
                    currSE->value1 = escape_level_diff;

                    writeSyntaxElement_GOLOMB(currSE,currBitStream);
                    bitCount[BITS_COEFF_Y_MB]+=currSE->len;
                    no_bits+=currSE->len;
                    currSE++;// proceed to next SE
                    currMB->currSEnr++;
                    // end
                }

                if(abs(level) > incVlc_chroma[tablenum])
                {
                    if(abs(level) <= 2)
                        tablenum = abs(level);
                    else if(abs(level) <= 4)
                        tablenum = 3;
                    else
                        tablenum = 4;
                }
            }

        }  //loop for iblk
    }//if ( cbp & (1<<b8) )

    return no_bits;
}

/*
*************************************************************************
* Function:
Make Intra prediction for all 9 modes for 8*8,
img_x and img_y are pixels offsets in the picture.
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/


void intrapred_luma_AVS(int img_x,int img_y)
{
    unsigned char edgepixels[40];
    int x,y,last_pix,new_pix;
    int b8_x,b8_y;
    int i;
    int block_available_up,block_available_up_right;
    int block_available_left,block_available_left_down;
    int bs_x=8;
    int bs_y=8;
    int MBRowSize = img->width / MB_BLOCK_SIZE;/*lgp*/
    int off_up=1,incr_y=1,off_y=0;/*lgp*/
    const int mb_nr    = img->current_mb_nr;    /*oliver*/
    Macroblock *currMB = &img->mb_data[mb_nr];  /*oliver*/
    int mb_left_available = (img_x >= MB_BLOCK_SIZE)?currMB->slice_nr == img->mb_data[mb_nr-1].slice_nr:0;  /*oliver*/
    int mb_up_available = (img_y >= MB_BLOCK_SIZE)?currMB->slice_nr == img->mb_data[mb_nr-MBRowSize].slice_nr:0;  /*oliver*/
    int mb_up_right_available;
    int mb_left_down_available;


    if((img->mb_y==0)||(img->mb_x==img->width/MB_BLOCK_SIZE-1))
        mb_up_right_available =1;
    else if((img_y-img->pix_y)>0)
        mb_up_right_available =(img_x-img->pix_x)>0? (currMB->slice_nr == img->mb_data[mb_nr+1].slice_nr):1;  /*oliver*/
    else
        mb_up_right_available =((img_x-img->pix_x)>0? (currMB->slice_nr == img->mb_data[mb_nr-MBRowSize+1].slice_nr):(currMB->slice_nr== img->mb_data[mb_nr-MBRowSize].slice_nr));  /*oliver*/


    if((img->mb_x==0)||(img->mb_y==img->height/MB_BLOCK_SIZE-1))
        mb_left_down_available = 1;
    else if(img_x-img->pix_x>0)
        mb_left_down_available =(img_y-img->pix_y)>0? (currMB->slice_nr == img->mb_data[mb_nr+MBRowSize].slice_nr):1;  /*oliver*/
    else
        mb_left_down_available =((img_y-img->pix_y)>0? (currMB->slice_nr == img->mb_data[mb_nr+MBRowSize-1].slice_nr):(currMB->slice_nr == img->mb_data[mb_nr-1].slice_nr));  /*oliver*/



    b8_x=img_x>>3;
    b8_y=img_y>>3;

    //check block up

    block_available_up=( b8_y-1>=0 && (mb_up_available||(img_y/8)%2));

    //check block up right
    block_available_up_right=( b8_x+1<(img->width>>3) && b8_y-1>=0 && mb_up_right_available);

    //check block left
    block_available_left=( b8_x - 1 >=0 && (mb_left_available || (img_x/8)%2));

    //check block left down
    block_available_left_down=( b8_x - 1>=0 && b8_y + 1 < (img->height>>3) && mb_left_down_available);
    if( ((img_x/8)%2)  && ((img_y/8)%2))
        block_available_up_right=0;

    if( ((img_x/8)%2)  || ((img_y/8)%2))
        block_available_left_down=0;

    //get prediciton pixels
    if(block_available_up)
    {
        for(x=0;x<bs_x;x++)
            EP[x+1]=imgY[img_y-/*1*/off_up/*lgp*/][img_x+x];

        if(block_available_up_right)
        {
            for(x=0;x<bs_x;x++)
                EP[1+x+bs_x]=imgY[img_y-/*1*/off_up/*lgp*/][img_x+bs_x+x];
            for(;x<bs_y;x++)
                EP[1+x+bs_x]=imgY[img_y-/*1*/off_up/*lgp*/][img_x+bs_x+bs_x-1];
        }
        else
        {
            for(x=0;x<bs_y;x++)
                EP[1+x+bs_x]=EP[bs_x];
        }

        for(;x<bs_y+2;x++)
            EP[1+x+bs_x]=EP[bs_x+x];

        EP[0]=imgY[img_y-/*1*/off_up/*lgp*/][img_x];
    }
    if(block_available_left)
    {
        for(y=0;y<bs_y;y++)
            EP[-1-y]=imgY[img_y+/*y*/incr_y*y-off_y][img_x-1];

        if(block_available_left_down)
        {
            for(y=0;y<bs_y;y++)
                EP[-1-y-bs_y]=imgY[img_y+bs_y+y][img_x-1];
            for(;y<bs_x;y++)
                EP[-1-y-bs_y]=imgY[img_y+bs_y+bs_y-1][img_x-1];
        }
        else
        {
            for(y=0;y<bs_x;y++)
                EP[-1-y-bs_y]=EP[-bs_y];
        }

        for(;y<bs_x+2;y++)
            EP[-1-y-bs_y]=EP[-y-bs_y];

        EP[0]=imgY[img_y-off_y/*lgp*/][img_x-1];
    }
    if(block_available_up&&block_available_left)
        EP[0]=imgY[img_y-/*1*/off_y-off_up/*lgp*/][img_x-1];

    //lowpass (Those emlements that are not needed will not disturb)
    last_pix=EP[-(bs_x+bs_y)];
    for(i=-(bs_x+bs_y);i<=(bs_x+bs_y);i++)
    {
        new_pix=( last_pix + (EP[i]<<1) + EP[i+1] + 2 )>>2;
        last_pix=EP[i];
        EP[i]=(unsigned char)new_pix;
    }
    for(i=0;i<9;i++)
        img->mprr[i][0][0]=-1;
    // 0 DC
    if(!block_available_up && !block_available_left)
        for(y=0UL;y<bs_y;y++)
            for(x=0UL;x<bs_x;x++)
                img->mprr[DC_PRED][y][x]=128UL;

    if(block_available_up && !block_available_left)
        for(y=0UL;y<bs_y;y++)
            for(x=0UL;x<bs_x;x++)
                img->mprr[DC_PRED][y][x]=EP[1+x];

    if(!block_available_up && block_available_left)
        for(y=0UL;y<bs_y;y++)
            for(x=0UL;x<bs_x;x++)
                img->mprr[DC_PRED][y][x]=EP[-1-y];


    if(block_available_up && block_available_left)
        for(y=0UL;y<bs_y;y++)
            for(x=0UL;x<bs_x;x++)
                img->mprr[DC_PRED][y][x]=(EP[1+x]+EP[-1-y])>>1;

    // 1 vertical
    if(block_available_up)
        for(y=0UL;y<bs_y;y++)
            for(x=0UL;x<bs_x;x++)
                img->mprr[VERT_PRED][y][x]=imgY[img_y-1][img_x+x];

    // 2 horizontal
    if(block_available_left)
        for(y=0UL;y<bs_y;y++)
            for(x=0UL;x<bs_x;x++)
                img->mprr[HOR_PRED][y][x]=imgY[img_y+y][img_x-1];

    // 3 down-right
    if(block_available_left&&block_available_up)
        for(y=0UL;y<bs_y;y++)
            for(x=0UL;x<bs_x;x++)
                img->mprr[DOWN_RIGHT_PRED][y][x]=EP[x-y];

    // 4 up-right bidirectional
    if(block_available_left&&block_available_up)
        for(y=0UL;y<bs_y;y++)
            for(x=0UL;x<bs_x;x++)
                img->mprr[DOWN_LEFT_PRED][y][x]=(EP[2+x+y]+EP[-2-(x+y)])>>1;

}

