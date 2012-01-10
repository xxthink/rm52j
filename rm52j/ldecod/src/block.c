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
* File name: block.c
* Function: Description
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
#include "elements.h"
#include "block.h"
#include "vlc.h"
#include "golomb_dec.h"

#define EP (edgepixels+20)

#ifdef WIN32
#define int16 __int16
#else
#define int16 int16_t
#endif

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

/*
*************************************************************************
* Function:Inverse transform 8x8 block.
* Input:
* Output:
* Return:
* Attention:input curr_blk has to be [lines][pixels]
*************************************************************************
*/
void inv_transform_B8 (short curr_blk[B8_SIZE][B8_SIZE])  // block to be inverse transformed.
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

        //b[0]=tmp[0]+tmp[2];
        // b[1]=tmp[1]+tmp[3];
        // b[2]=tmp[1]-tmp[3];
        // b[3]=tmp[0]-tmp[2];
        b[0] = Clip3(-32768, 32767, tmp[0]+tmp[2]);
        b[1] = Clip3(-32768, 32767, tmp[1]+tmp[3]);
        b[2] = Clip3(-32768, 32767, tmp[1]-tmp[3]);
        b[3] = Clip3(-32768, 32767, tmp[0]-tmp[2]);
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
        /*  Commented by cjw, 20070327
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

void idct_dequant_B8(int block8x8,
                     int qp,                         // Quantization parameter
                     short curr_blk[B8_SIZE][B8_SIZE],
struct img_par *img
    )
{
    int  xx,yy;
    int  b8_y       = (block8x8 / 2) << 3;
    int  b8_x       = (block8x8 % 2) << 3;
    int  curr_val;
    int b8_cy;

    // inverse transform
    inv_transform_B8(curr_blk);

    if   (block8x8>5)
        b8_cy=8;
    else
        b8_cy=0;

#ifdef _OUTPUT_PRED_
    if (FrameNum == _FRAME_NUM_  && img->current_mb_nr == _MBNR_ && block8x8 == _B8_ && img->type == I_IMG)
    {
        FILE* pf = fopen("debug.log", "a");
        fprintf(pf, "prediction:\n");
        for (yy=0; yy<8; yy++)
        {
            for (xx=0; xx<8; xx++)
            {
                fprintf(pf, "%4d", img->mpr[b8_x+xx][b8_y+yy]);
            }
            fprintf(pf, "\n");
        }
        fprintf(pf, "\n");
        fprintf(pf, "residual\n");
        for (yy=0; yy<8; yy++)
        {
            for (xx=0; xx<8; xx++)
            {
                fprintf(pf, "%4d", curr_blk[yy][xx]);
            }
            fprintf(pf, "\n");
        }
        fprintf(pf, "\n");
        fclose(pf);
        //exit(0);
    }
#endif
#ifdef _OUTPUT_RES_1
    if (FrameNum == _FRAME_NUM_  && img->current_mb_nr == _MBNR_ && block8x8 == _B8_ && img->type == I_IMG)
    {
        for (yy=0; yy<8; yy++)
        {
            for (xx=0; xx<8; xx++)
            {
                //printf("%4d", imgY[img->pix_y+b8_y+yy][img->pix_x+b8_x+xx]);
                printf("%4d", curr_blk[yy][xx]);
            }
            printf("\n");
        }
        printf("\n");
        //exit(0);
    }
#endif
    // normalize
    for(yy=0;yy<8;yy++)
        for(xx=0;xx<8;xx++)
        {
            if(block8x8 <= 3)
                curr_val = img->mpr[b8_x+xx][b8_y+yy] + curr_blk[yy][xx];
            else
                curr_val = img->mpr[xx][yy+b8_cy] + curr_blk[yy][xx];

            img->m7[xx][yy] = curr_blk[yy][xx] = clamp(curr_val,0,255);

            if(block8x8 <= 3)
                imgY[img->pix_y+b8_y+yy][img->pix_x+b8_x+xx] = (unsigned char)curr_blk[yy][xx];
            else
                imgUV[(block8x8-4)%2][img->pix_c_y+yy+b8_cy][img->pix_c_x+xx] = (unsigned char)curr_blk[yy][xx];
        }
#ifdef _OUTPUT_RECON_
        if (FrameNum == _FRAME_NUM_  && img->current_mb_nr == _MBNR_ && block8x8 == _B8_ && img->type == I_IMG)
        {
            FILE* pf = fopen("debug.log", "a");
            fprintf(pf, "rec\n");
            for (yy=0; yy<8; yy++)
            {
                for (xx=0; xx<8; xx++)
                {
                    fprintf(pf, "%4d", imgY[img->pix_y+b8_y+yy][img->pix_x+b8_x+xx]);
                }
                fprintf(pf, "\n");
            }
            fprintf(pf, "\n");
            fclose(pf);
            exit(0);
        }
#endif
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
void reconstruct8x8(int curr_blk[8][8],int block8x8)
{
    int yy,xx,block_y, block_x;
    int incr_y=1,off_y=0;
    Macroblock *currMB   = &mb_data[img->current_mb_nr];

    if(block8x8<4)
    {
        block_y = (block8x8/2)*8;
        block_x = (block8x8%2)*8;

        for(yy=0;yy<8;yy++)
        {
            for(xx=0;xx<8;xx++)
            {
                imgY[img->pix_y+block_y+incr_y*yy+off_y][img->pix_x+block_x+xx] = curr_blk[yy][xx];
            }
        }
    }
    else
    {
        for(yy=0;yy<8;yy++)
        {
            for(xx=0;xx<8;xx++)
            {
                imgUV[block8x8-4][img->pix_c_y+yy][img->pix_c_x+xx] = curr_blk[yy][xx];
            }
        }
    }
}

/*
*************************************************************************
* Function:Read coefficients of one 8x8 block
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/


void readLumaCoeff_B8(int block8x8, struct inp_par *inp, struct img_par *img)
{
    int i;
    int mb_nr          = img->current_mb_nr;
    Macroblock *currMB = &mb_data[mb_nr];
    const int cbp      = currMB->cbp;
    SyntaxElement currSE;
    int intra;
    int inumblk;                    /* number of blocks per CBP*/
    int inumcoeff;                  /* number of coeffs per block */
    int icoef;                      /* current coefficient */
    int ipos;
    int run, level;
    int ii,jj;
    int sbx, sby;
    int boff_x, boff_y;
    int any_coeff;
    int vlc_numcoef;
    int cbp_blk_mask;
    int tablenum;         //add by qwang
    static const int incVlc_intra[7] = { 0,1,2,4,7,10,3000};
    static const int incVlc_inter[7] = { 0,1,2,3,6,9,3000};

    int buffer_level[65];    //add by qwang
    int buffer_run[64];      //add by qwang
    int EOB_Pos_intra[7] = { -1, 8, 8, 8, 6, 0, 0};
    int EOB_Pos_inter[7] = {-1, 2, 2, 2, 2, 0, 0};
    char (*AVS_VLC_table_intra)[64][2];
    char (*AVS_VLC_table_inter)[64][2];

    int symbol2D,Golomb_se_type;
    const char (*table2D)[27];

    //digipro_1
    float QP99;
    long sum;
    int shift = 7;
    //digipro_0
    int val,  QPI;

    int qp, q_shift; // dequantization parameters
    static const int blkmode2ctx [4] = {LUMA_8x8, LUMA_8x4, LUMA_4x8, LUMA_4x4};

    // this has to be done for each subblock seperately
    intra     = IS_INTRA(currMB);

    inumblk   = 1;
    inumcoeff = 65; //  all positions + EOB

    // ========= dequantization values ===========
    qp       = currMB->qp; // using old style qp.

    q_shift  = qp/QUANT_PERIOD;

    QP99 =(float)(pow(2.0, (float)(qp-5.0)/8.0));

    for (shift=1; shift<16; shift++)
    {
        QPI = (int)((1<<shift)*QP99+0.5);
        if (QPI>(1<<16))
        {
            shift -=1;
            QPI = (int)((1<<shift)*QP99+0.5);
            break;
        }
    }

    if (shift==16) shift=15;
    QPI = (int)((1<<shift)*QP99+0.5);

    //make decoder table for 2DVLC_INTRA code
    if(AVS_2DVLC_INTRA_dec[0][0][1]<0)   // Don't need to set this every time. rewrite later.
    {
        memset(AVS_2DVLC_INTRA_dec,-1,sizeof(AVS_2DVLC_INTRA_dec));
        for(i=0;i<7;i++)
        {
            table2D=AVS_2DVLC_INTRA[i];
            for(run=0;run<26;run++)
                for(level=0;level<27;level++)
                {
                    ipos=table2D[run][level];
                    assert(ipos<64);
                    if(ipos>=0)
                    {
                        if(i==0)
                        {
                            AVS_2DVLC_INTRA_dec[i][ipos][0]=level+1;
                            AVS_2DVLC_INTRA_dec[i][ipos][1]=run;

                            AVS_2DVLC_INTRA_dec[i][ipos+1][0]=-(level+1);
                            AVS_2DVLC_INTRA_dec[i][ipos+1][1]=run;
                        }
                        else
                        {
                            AVS_2DVLC_INTRA_dec[i][ipos][0]=level;
                            AVS_2DVLC_INTRA_dec[i][ipos][1]=run;

                            if(level)
                            {
                                AVS_2DVLC_INTRA_dec[i][ipos+1][0]=-(level);
                                AVS_2DVLC_INTRA_dec[i][ipos+1][1]=run;
                            }
                        }
                    }
                }
        }
        assert(AVS_2DVLC_INTRA_dec[0][0][1]>=0);        //otherwise, tables are bad.
    }

    //make decoder table for 2DVLC_INTER code
    if(AVS_2DVLC_INTER_dec[0][0][1]<0)                                                          // Don't need to set this every time. rewrite later.
    {
        memset(AVS_2DVLC_INTER_dec,-1,sizeof(AVS_2DVLC_INTER_dec));
        for(i=0;i<7;i++)
        {
            table2D=AVS_2DVLC_INTER[i];
            for(run=0;run<26;run++)
                for(level=0;level<27;level++)
                {
                    ipos=table2D[run][level];
                    assert(ipos<64);
                    if(ipos>=0)
                    {
                        if(i==0)
                        {
                            AVS_2DVLC_INTER_dec[i][ipos][0]=level+1;
                            AVS_2DVLC_INTER_dec[i][ipos][1]=run;

                            AVS_2DVLC_INTER_dec[i][ipos+1][0]=-(level+1);
                            AVS_2DVLC_INTER_dec[i][ipos+1][1]=run;
                        }
                        else
                        {
                            AVS_2DVLC_INTER_dec[i][ipos][0]=level;
                            AVS_2DVLC_INTER_dec[i][ipos][1]=run;

                            if(level)
                            {
                                AVS_2DVLC_INTER_dec[i][ipos+1][0]=-(level);
                                AVS_2DVLC_INTER_dec[i][ipos+1][1]=run;
                            }
                        }
                    }
                }
        }
        assert(AVS_2DVLC_INTER_dec[0][0][1]>=0);        //otherwise, tables are bad.
    }

    //clear cbp_blk bits of thie 8x8 block (and not all 4!)
    cbp_blk_mask = cbp_blk_masks[0] ;
    if(block8x8&1)cbp_blk_mask<<=2;
    if(block8x8&2)cbp_blk_mask<<=8;
    currMB->cbp_blk&=~cbp_blk_mask;

    vlc_numcoef=-1;

    Golomb_se_type=SE_LUM_AC_INTER;
    if( intra )
    {
        vlc_numcoef=0;        //this means 'use numcoeffs symbol'.
        Golomb_se_type=SE_LUM_AC_INTRA;
    }

    AVS_VLC_table_intra = AVS_2DVLC_INTRA_dec;
    AVS_VLC_table_inter = AVS_2DVLC_INTER_dec;


    // === decoding ===
    if ( cbp & (1<<block8x8) )
    {
        // === set offset in current macroblock ===
        boff_x = ( (block8x8%2)<<3 );
        boff_y = ( (block8x8/2)<<3 );
        img->subblock_x = boff_x>>2;
        img->subblock_y = boff_y>>2;

        ipos  = -1;
        any_coeff=1;   //modified by qwang    any_coeff=0

        if(intra)
        {
            tablenum = 0;
            for(i=0; i<inumcoeff; i++)
            {
                //read 2D symbol
                currSE.type = Golomb_se_type;
                //currSE.golomb_grad = 2;
                //currSE.golomb_maxlevels=4;
                currSE.golomb_grad = VLC_Golomb_Order[0][tablenum][0];
                currSE.golomb_maxlevels = VLC_Golomb_Order[0][tablenum][1];
                readSyntaxElement_GOLOMB(&currSE,img,inp);
                symbol2D = currSE.value1;

                //if(symbol2D == EOB_Pos_intra[tablenum])
                if(symbol2D == EOB_Pos_intra[tablenum])
                {
                    vlc_numcoef = i;
                    break;
                }

                if(symbol2D < CODE2D_ESCAPE_SYMBOL)
                {
                    level = AVS_2DVLC_INTRA_dec[tablenum][symbol2D][0];
                    run   = AVS_2DVLC_INTRA_dec[tablenum][symbol2D][1];
                }
                else
                {
                    // changed by dj
                    run = (symbol2D-CODE2D_ESCAPE_SYMBOL)>>1;

                    //decode level
                    currSE.type=Golomb_se_type;
                    currSE.golomb_grad = 1;
                    currSE.golomb_maxlevels=11; //2007.05.09
                    readSyntaxElement_GOLOMB(&currSE,img,inp);
                    level = currSE.value1 + ((run>MaxRun[0][tablenum])?1:RefAbsLevel[tablenum][run]);
                    //        if( (symbol2D-CODE2D_ESCAPE_SYMBOL) & 1 )
                    if(symbol2D & 1)
                        level=-level;
                }

                // 保存level,run到缓冲区
                buffer_level[i] = level;
                buffer_run[i]   = run;

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
            }//loop for icoef

            //将解码的level,run写到img->m7[][];
            for(i=(vlc_numcoef-1); i>=0; i--)
            {
                ipos += (buffer_run[i]+1);

                ii = SCAN[img->picture_structure][ipos][0];
                jj = SCAN[img->picture_structure][ipos][1];

                shift = IQ_SHIFT[qp];
                QPI   = IQ_TAB[qp];
                val = buffer_level[i];
                sum = (val*QPI+(1<<(shift-2)) )>>(shift-1);

                img->m7[boff_x + ii][boff_y + jj] = sum;
            }
        }//if (intra)
        else
        {
            tablenum = 0;
            for(i=0; i<inumcoeff; i++)
            {
                //read 2D symbol
                currSE.type = Golomb_se_type;
                currSE.golomb_grad = VLC_Golomb_Order[1][tablenum][0];
                currSE.golomb_maxlevels = VLC_Golomb_Order[1][tablenum][1];
                readSyntaxElement_GOLOMB(&currSE,img,inp);
                symbol2D = currSE.value1;

                //if(symbol2D == EOB_Pos_inter[tablenum])
                if(symbol2D == EOB_Pos_inter[tablenum])
                {
                    vlc_numcoef = i;
                    break;
                }

                if(symbol2D < CODE2D_ESCAPE_SYMBOL)
                {
                    level = AVS_2DVLC_INTER_dec[tablenum][symbol2D][0];
                    run   = AVS_2DVLC_INTER_dec[tablenum][symbol2D][1];
                }
                else
                {
                    // changed by dj
                    run = (symbol2D-CODE2D_ESCAPE_SYMBOL)>>1;

                    //decode level
                    currSE.type=Golomb_se_type;
                    currSE.golomb_grad = 0;
                    currSE.golomb_maxlevels=12;  //2007.05.05
                    readSyntaxElement_GOLOMB(&currSE,img,inp);
                    level = currSE.value1 + ((run>MaxRun[1][tablenum])?1:RefAbsLevel[tablenum+7][run]);

                    if(symbol2D & 1)                         //jlzheng   7.20
                        level=-level;
                }

                // 保存level,run到缓冲区
                buffer_level[i] = level;
                buffer_run[i]   = run;

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
            }//loop for icoef

            //将解码的level,run写到img->m7[][];
            for(i=(vlc_numcoef-1); i>=0; i--)
            {
                ipos += (buffer_run[i]+1);

                ii = SCAN[img->picture_structure][ipos][0];
                jj = SCAN[img->picture_structure][ipos][1];

                shift = IQ_SHIFT[qp];
                QPI   = IQ_TAB[qp];
                val = buffer_level[i];
                sum = (val*QPI+(1<<(shift-2)) )>>(shift-1);
                img->m7[boff_x + ii][boff_y + jj] = sum;
            }
        }


        icoef = vlc_numcoef;
        // CAVLC store number of coefficients
        sbx = img->subblock_x;
        sby = img->subblock_y;

        //add by qwang
        //CA_ABT store number of coefficients
        sbx = ((block8x8%2)==0) ? 0:1;
        sby = block8x8>1 ? 1:0;

        if(any_coeff)
        {
            cbp_blk_mask = cbp_blk_masks[ ((boff_y&4)>>1)|((boff_x&4)>>2) ] ;
            if(block8x8&1)cbp_blk_mask<<=2;
            if(block8x8&2)cbp_blk_mask<<=8;

            currMB->cbp_blk |= cbp_blk_mask;
        }
    }//end if ( cbp & (1<<icbp) )
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
void readChromaCoeff_B8(int block8x8, struct inp_par *inp, struct img_par *img)
{
    int mb_nr          = img->current_mb_nr;
    Macroblock *currMB = &mb_data[mb_nr];
    const int cbp      = currMB->cbp;
    SyntaxElement currSE;
    unsigned int is_last_levelrun;
    int intra;
    int inumblk;                    /* number of blocks per CBP*/
    int inumcoeff;                  /* number of coeffs per block */
    int ipos;
    int run, level;
    int ii,jj,i;
    int sbx, sby;
    int boff_x, boff_y;
    int any_coeff;
    int vlc_numcoef;
    int tablenum;   //add by qwang
    int buffer_level[65];    //add by qwang
    int buffer_run[64];      //add by qwang
    static const int incVlc_chroma[5] = {0,1,2,4,3000};
    int EOB_Pos_chroma[2][5] = { {-1, 4, 8, 6, 4}, {-1, 0, 2, 0, 0} };
    char (*AVS_VLC_table_chroma)[64][2];

    int symbol2D,Golomb_se_type;
    const char (*table2D)[27];
    float QP99;
    int QPI;
    int shift = 7;
    int val, temp;
    static const int blkmode2ctx [4] = {LUMA_8x8, LUMA_8x4, LUMA_4x8, LUMA_4x4};
    int q_bits;
    int qp_chroma       = QP_SCALE_CR[currMB->qp]; // using old style qp.

    //digipro_1
    QP99 = (float)(pow(2.0, (float)(qp_chroma-5)/8.0));

    for (shift=1; shift<16; shift++)
    {
        QPI = (int)((1<<shift)*QP99+0.5);
        if (QPI>(1<<16))
        {
            shift -=1;
            QPI = (int)((1<<shift)*QP99+0.5);
            break;
        }
    }

    if (shift==16) shift=15;
    QPI = (int)((1<<shift)*QP99+0.5);

    // this has to be done for each subblock seperately
    intra     = IS_INTRA(currMB);

    inumblk   = 1;
    inumcoeff = 65; //  all positions + EOB

    // ========= dequantization values ===========
    q_bits = qp_chroma/QUANT_PERIOD;

    //make decoder table for 2DVLC_CHROMA code
    if(AVS_2DVLC_CHROMA_dec[0][0][1]<0)                                                          // Don't need to set this every time. rewrite later.
    {
        memset(AVS_2DVLC_CHROMA_dec,-1,sizeof(AVS_2DVLC_CHROMA_dec));
        for(i=0;i<5;i++)
        {
            table2D=AVS_2DVLC_CHROMA[i];
            for(run=0;run<26;run++)
                for(level=0;level<27;level++)
                {
                    ipos=table2D[run][level];
                    assert(ipos<64);
                    if(ipos>=0)
                    {
                        if(i==0)
                        {
                            AVS_2DVLC_CHROMA_dec[i][ipos][0]=level+1;
                            AVS_2DVLC_CHROMA_dec[i][ipos][1]=run;

                            AVS_2DVLC_CHROMA_dec[i][ipos+1][0]=-(level+1);
                            AVS_2DVLC_CHROMA_dec[i][ipos+1][1]=run;
                        }
                        else
                        {
                            AVS_2DVLC_CHROMA_dec[i][ipos][0]=level;
                            AVS_2DVLC_CHROMA_dec[i][ipos][1]=run;

                            if(level)
                            {
                                AVS_2DVLC_CHROMA_dec[i][ipos+1][0]=-(level);
                                AVS_2DVLC_CHROMA_dec[i][ipos+1][1]=run;
                            }
                        }
                    }
                }
        }
        assert(AVS_2DVLC_CHROMA_dec[0][0][1]>=0);        //otherwise, tables are bad.
    }

    //clear cbp_blk bits of thie 8x8 block (and not all 4!)
    vlc_numcoef=-1;

    Golomb_se_type=SE_LUM_AC_INTER;
    if( intra )
    {
        vlc_numcoef=0;        //this means 'use numcoeffs symbol'.
        Golomb_se_type=SE_LUM_AC_INTRA;
    }

    AVS_VLC_table_chroma = AVS_2DVLC_CHROMA_dec;


    // === set offset in current macroblock ===
    boff_x = ( (block8x8%2)<<3 );
    boff_y = ( (block8x8/2)<<3 );
    img->subblock_x = boff_x>>2;
    img->subblock_y = boff_y>>2;

    ipos  = -1;
    any_coeff=1;

    is_last_levelrun=0;

    tablenum = 0;
    for(i=0; i<inumcoeff; i++)
    {
        //read 2D symbol
        currSE.type = Golomb_se_type;
        currSE.golomb_grad = VLC_Golomb_Order[2][tablenum][0];
        currSE.golomb_maxlevels = VLC_Golomb_Order[2][tablenum][1];
        readSyntaxElement_GOLOMB(&currSE,img,inp);
        symbol2D = currSE.value1;

        //if(symbol2D == EOB_Pos_chroma[tablenum])
        if(symbol2D == EOB_Pos_chroma[1][tablenum])
        {
            vlc_numcoef = i;
            break;
        }

        if(symbol2D < CODE2D_ESCAPE_SYMBOL)
        {
            level = AVS_VLC_table_chroma[tablenum][symbol2D][0];
            run   = AVS_VLC_table_chroma[tablenum][symbol2D][1];
        }
        else
        {
            // changed by dj
            run = (symbol2D-CODE2D_ESCAPE_SYMBOL)>>1;

            //decode level
            currSE.type=Golomb_se_type;
            currSE.golomb_grad = 0;
            currSE.golomb_maxlevels=11;
            readSyntaxElement_GOLOMB(&currSE,img,inp);
            level = currSE.value1 + ((run>MaxRun[2][tablenum])?1:RefAbsLevel[tablenum+14][run]);
            //      if( (symbol2D-CODE2D_ESCAPE_SYMBOL) & 1 )
            if(symbol2D & 1)
                level=-level;
        }

        // 保存level,run到缓冲区
        buffer_level[i] = level;
        buffer_run[i]   = run;

        if(abs(level) > incVlc_chroma[tablenum])
        {
            if(abs(level) <= 2)
                tablenum = abs(level);
            else if(abs(level) <= 4)
                tablenum = 3;
            else
                tablenum = 4;
        }
    }//loop for icoef

    //将解码的level,run写到img->m7[][];
    for(i=(vlc_numcoef-1); i>=0; i--)
    {
        ipos += (buffer_run[i]+1);

        ii = SCAN[img->picture_structure][ipos][0];
        jj = SCAN[img->picture_structure][ipos][1];
        shift = IQ_SHIFT[qp_chroma];
        QPI   = IQ_TAB[qp_chroma];
        val = buffer_level[i];
        temp = (val*QPI+(1<<(shift-2)) )>>(shift-1);

        img->m8[block8x8-4][ii][jj] = temp;
    }

    // CAVLC store number of coefficients
    sbx = img->subblock_x;
    sby = img->subblock_y;
    sbx = (img->subblock_x == 2) ? 1:0;

    if(any_coeff)
    {
        currMB->cbp_blk |= 0xf0000 << ((block8x8-4) << 2);
    }
}

/*
*************************************************************************
* Function:Copy region of img->m7 corresponding to block8x8 to curr_blk[][].
* Input:
* Output:
* Return:
* Attention:img->m7 is [x][y] and curr_blk is [y][x].
*************************************************************************
*/

void get_curr_blk( int block8x8,struct img_par *img, short curr_blk[B8_SIZE][B8_SIZE])
{
    int  xx, yy;
    int  mb_y       = (block8x8 / 2) << 3;
    int  mb_x       = (block8x8 % 2) << 3;
    Macroblock *currMB   = &mb_data[img->current_mb_nr];/*lgp*/

    for (yy=0; yy<B8_SIZE; yy++)
        for (xx=0; xx<B8_SIZE; xx++)
        {
            if (block8x8<=3)
                curr_blk[yy][xx] = img->m7[mb_x+xx][mb_y+yy];
            else
                curr_blk[yy][xx] = img->m8[block8x8-4][xx][yy];
        }
}

/*
*************************************************************************
* Function:Make Intra prediction for all 9 modes for larger blocks than 4*4,
that is for 4*8, 8*4 and 8*8 blocks.
bs_x and bs_y may be only 4 and 8.
img_x and img_y are pixels offsets in the picture.
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/
int intrapred(struct img_par *img,int img_x,int img_y)
{
    unsigned char edgepixels[40];
    int x,y,last_pix,new_pix;
    unsigned int x_off,y_off;
    int i,predmode;
    int b4_x,b4_y;
    int b4,b8,mb;
    int block_available_up,block_available_up_right;
    int block_available_left,block_available_left_down;
    int bs_x,bs_y;
    int b8_x,b8_y;
    Macroblock *currMB   = &mb_data[img->current_mb_nr];/*lgp*/
    int MBRowSize = img->width / MB_BLOCK_SIZE;/*lgp*/
    int off_up=1,incr_y=1,off_y=0; /*lgp*/
    /*oliver*/
    const int mb_nr    = img->current_mb_nr;    /*oliver*/
    int mb_left_available = (img_x >= MB_BLOCK_SIZE)?currMB->slice_nr == mb_data[mb_nr-1].slice_nr:0;  /*oliver*/
    int mb_up_available = (img_y >= MB_BLOCK_SIZE)?currMB->slice_nr == mb_data[mb_nr-MBRowSize].slice_nr:0;  /*oliver*/
    int mb_up_right_available;
    int mb_left_down_available;

    if((img->mb_y==0)||(img->mb_x==img->width/MB_BLOCK_SIZE-1))
        mb_up_right_available =1;
    else if((img_y-img->pix_y)>0)
        mb_up_right_available =(img_x-img->pix_x)>0? (currMB->slice_nr == mb_data[mb_nr+1].slice_nr):1;  /*oliver*/
    else
        mb_up_right_available =((img_x-img->pix_x)>0? (currMB->slice_nr == mb_data[mb_nr-MBRowSize+1].slice_nr):(currMB->slice_nr== mb_data[mb_nr-MBRowSize].slice_nr));  /*oliver*/


    if((img->mb_x==0)||(img->mb_y==img->height/MB_BLOCK_SIZE-1))
        mb_left_down_available = 1;
    else if(img_x-img->pix_x>0)
        mb_left_down_available =(img_y-img->pix_y)>0? (currMB->slice_nr == mb_data[mb_nr+MBRowSize].slice_nr):1;  /*oliver*/
    else
        mb_left_down_available =((img_y-img->pix_y)>0? (currMB->slice_nr == mb_data[mb_nr+MBRowSize-1].slice_nr):(currMB->slice_nr == mb_data[mb_nr-1].slice_nr));  /*oliver*/
    //0512



    b8_x=img_x>>3;
    b8_y=img_y>>3;

    bs_x =  bs_y = 8;
    x_off=img_x&0x0FU;
    y_off=img_y&0x0FU;

    predmode = img->ipredmode[img_x/(2*BLOCK_SIZE)+1][img_y/(2*BLOCK_SIZE)+1];
    assert(predmode>=0);

    {
        b4_x=img_x>>2;
        b4_y=img_y>>2;
        mb=(b4_x>>2)+(b4_y>>2)*(img->width>>2);  b8=((b4_x&2)>>1)|(b4_y&2);  b4=(b4_x&1)|((b4_y&1)<<1);

        //check block up
        block_available_up=( b8_y-1>=0 && (mb_up_available||(img_y/8)%2));

        //check block up right
        block_available_up_right=( b8_x+1<(img->width>>3) && b8_y-1>=0 &&  mb_up_right_available);

        //check block left
        block_available_left=( b8_x - 1 >=0 && ( mb_left_available)||((img_x/8)%2));

        //check block left down
        block_available_left_down=( b8_x - 1>=0 && b8_y + 1 < (img->height>>3) &&  mb_left_down_available);

        if( ((img_x/8)%2)  && ((img_y/8)%2))
            block_available_up_right=0;

        if( ((img_x/8)%2)  || ((img_y/8)%2))
            block_available_left_down=0;

        //get prediciton pixels
        if(block_available_up)
        {
            for(x=0;x<bs_x;x++)
                EP[x+1]=imgY[img_y-1][img_x+x];
            if(block_available_up_right)
            {
                for(x=0;x<bs_x;x++)
                    EP[1+x+bs_x]=imgY[img_y-1][img_x+bs_x+x];
                for(;x<bs_y;x++)
                    EP[1+x+bs_x]=imgY[img_y-1][img_x+bs_x+bs_x-1];
            }else
                for(x=0;x<bs_y;x++)
                    EP[1+x+bs_x]=EP[bs_x];
            for(;x<bs_y+2;x++)
                EP[1+x+bs_x]=EP[bs_x+x];
            EP[0]=imgY[img_y-1][img_x];
        }
        if(block_available_left)
        {
            for(y=0;y<bs_y;y++)
                EP[-1-y]=imgY[img_y+y][img_x-1];
            if(block_available_left_down)
            {
                for(y=0;y<bs_y;y++)
                    EP[-1-y-bs_y]=imgY[img_y+bs_y+y][img_x-1];
                for(;y<bs_x;y++)
                    EP[-1-y-bs_y]=imgY[img_y+bs_y+bs_y-1][img_x-1];
            }
            else
                for(y=0;y<bs_x;y++)
                    EP[-1-y-bs_y]=EP[-bs_y];

            for(;y<bs_x+2;y++)
                EP[-1-y-bs_y]=EP[-y-bs_y];

            EP[0]=imgY[img_y][img_x-1];
        }
        if(block_available_up&&block_available_left)
            EP[0]=imgY[img_y-1][img_x-1];

        //lowpass (Those emlements that are not needed will not disturb)
        last_pix=EP[-(bs_x+bs_y)];
        for(i=-(bs_x+bs_y);i<=(bs_x+bs_y);i++)
        {
            new_pix=( last_pix + (EP[i]<<1) + EP[i+1] + 2 )>>2;
            last_pix=EP[i];
            EP[i]=(unsigned char)new_pix;
        }
    }

    switch(predmode)
    {
    case DC_PRED:// 0 DC
        if(!block_available_up && !block_available_left)
            for(y=0UL;y<bs_y;y++)
                for(x=0UL;x<bs_x;x++)
                    img->mpr[x+x_off][y+y_off]=128UL;

        if(block_available_up && !block_available_left)
            for(y=0UL;y<bs_y;y++)
                for(x=0UL;x<bs_x;x++)
                    img->mpr[x+x_off][y+y_off]=EP[1+x];

        if(!block_available_up && block_available_left)
            for(y=0UL;y<bs_y;y++)
                for(x=0UL;x<bs_x;x++)
                    img->mpr[x+x_off][y+y_off]=EP[-1-y];

        if(block_available_up && block_available_left)
            for(y=0UL;y<bs_y;y++)
                for(x=0UL;x<bs_x;x++)
                    img->mpr[x+x_off][y+y_off]=(EP[1+x]+EP[-1-y])>>1;

        break;

    case VERT_PRED:// 1 vertical
        if(!block_available_up)
            printf("!!invalid intrapred mode %d!!  %d \n",predmode, img->current_mb_nr);

        for(y=0UL;y<bs_y;y++)
            for(x=0UL;x<bs_x;x++)
                img->mpr[x+x_off][y+y_off]=imgY[img_y-1][img_x+x];

        break;

    case HOR_PRED:// 2 horizontal
        if(!block_available_left)
            printf("!!invalid intrapred mode %d!!  %d  \n",predmode, img->current_mb_nr);

        for(y=0UL;y<bs_y;y++)
            for(x=0UL;x<bs_x;x++)
                img->mpr[x+x_off][y+y_off]=imgY[img_y+y][img_x-1];

        break;

    case DOWN_RIGHT_PRED:// 3 down-right
        if(!block_available_left||!block_available_up)
            printf("!!invalid intrapred mode %d!!  %d \n",predmode,  img->current_mb_nr);

        for(y=0UL;y<bs_y;y++)
            for(x=0UL;x<bs_x;x++)
                img->mpr[x+x_off][y+y_off]=EP[(int)x-(int)y];

        break;

    case DOWN_LEFT_PRED:// 4 up-right bidirectional
        if(!block_available_left||!block_available_up)
            printf("!!invalid intrapred mode %d!!  %d  \n",predmode,  img->current_mb_nr);

        for(y=0UL;y<bs_y;y++)
            for(x=0UL;x<bs_x;x++)
                img->mpr[x+x_off][y+y_off]=(EP[2+x+y]+EP[-2-(int)(x+y)])>>1;

        break;

    }//end switch predmode

    return DECODING_OK;
}
