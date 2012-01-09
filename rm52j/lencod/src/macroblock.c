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
* File name: macroblock.c
* Function: Process one macroblock
*
*************************************************************************************
*/


#include "contributors.h"

#include <math.h>
#include <stdlib.h>
#include <assert.h>

#include "macroblock.h"
#include "refbuf.h"
#include "vlc.h"
#include "block.h"
#include "header.h"
#include "golomb.h"
#include "ratectl.h"

extern const byte QP_SCALE_CR[64];
extern const int NCBP[64][2];

int chroma_trans8x8(int tmp_block[2][8][8],int tmp_mpr[2][8][8],int *cbpc);/*lgp*/

//Rate control
int predict_error,dq;
extern int DELTA_QP,DELTA_QP2;
extern int QP,QP2;

extern  int check_mv_range(int mv_x, int mv_y, int pix_x, int pix_y, int blocktype);  // mv_range, 20071009


//cjw 20060321 for MV limit
#define MAX_V_SEARCH_RANGE 1024
#define MAX_V_SEARCH_RANGE_FIELD 512
#define MAX_H_SEARCH_RANGE 8192

extern StatParameters  stats;
/*
*************************************************************************
* Function:Update the coordinates for the next macroblock to be processed
* Input:mb: MB address in scan order
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void set_MB_parameters (int mb)
{
    const int number_mb_per_row = img->width / MB_BLOCK_SIZE ;  //add by wuzhongmou 0610

    img->current_mb_nr = mb;
    img->mb_x = mb % number_mb_per_row;
    img->mb_y = mb / number_mb_per_row;

    // Define vertical positions
    img->block8_y= img->mb_y * BLOCK_SIZE/2;/*lgp*/
    img->block_y = img->mb_y * BLOCK_SIZE;      // vertical luma block position
    img->pix_y   = img->mb_y * MB_BLOCK_SIZE;   // vertical luma macroblock position
    if(input->chroma_format==2)
        img->pix_c_y = img->mb_y * MB_BLOCK_SIZE; // vertical chroma macroblock position  //wzm,422
    else
        img->pix_c_y = img->mb_y * MB_BLOCK_SIZE/2; // vertical chroma macroblock position

    // Define horizontal positions
    img->block8_x= img->mb_x * BLOCK_SIZE/2;/*lgp*/
    img->block_x = img->mb_x * BLOCK_SIZE;        // luma block
    img->pix_x   = img->mb_x * MB_BLOCK_SIZE;     // luma pixel
    img->block_c_x = img->mb_x * BLOCK_SIZE/2;    // chroma block
    img->pix_c_x   = img->mb_x * MB_BLOCK_SIZE/2; // chroma pixel  
}

/*
*************************************************************************
* Function:Update the coordinates and statistics parameter for the
next macroblock
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void proceed2nextMacroblock()
{
    Macroblock *currMB = &img->mb_data[img->current_mb_nr];
    int*        bitCount = currMB->bitcounter;

#if TRACE
    int i;
    if (p_trace)
    {
        fprintf(p_trace, "\n*********** Pic: %i (I/P) MB: %i Slice: %i **********\n\n", frame_no, img->current_mb_nr, img->current_slice_nr);
        // Write out the tracestring for each symbol
        for (i=0; i<currMB->currSEnr; i++)
            trace2out(&(img->MB_SyntaxElements[i]));
    }
#endif

    // Update the statistics
    stats.bit_use_mb_type[img->type]      += bitCount[BITS_MB_MODE];
    stats.bit_use_coeffY[img->type]       += bitCount[BITS_COEFF_Y_MB] ;
    stats.tmp_bit_use_cbp[img->type]      += bitCount[BITS_CBP_MB];
    stats.bit_use_coeffC[img->type]       += bitCount[BITS_COEFF_UV_MB];
    stats.bit_use_delta_quant[img->type]  += bitCount[BITS_DELTA_QUANT_MB];

    if (img->type==INTRA_IMG)
        stats.mode_use_intra[currMB->mb_type]++;
    else
        if (img->type != B_IMG)
        {
            stats.mode_use_inter[0][currMB->mb_type]++;
            stats.bit_use_mode_inter[0][currMB->mb_type]+= bitCount[BITS_INTER_MB];

        }
        else
        {
            stats.bit_use_mode_inter[1][currMB->mb_type]+= bitCount[BITS_INTER_MB];
            stats.mode_use_inter[1][currMB->mb_type]++;
        }

        // Statistics
        if ((img->type == INTER_IMG) )
        {
            stats.quant0++;
            stats.quant1 += img->qp;      // to find average quant for inter frames
        }

}

/*
*************************************************************************
* Function:initializes the current macroblock
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void start_macroblock()
{
    int i,j,k,l;
    Macroblock *currMB = &img->mb_data[img->current_mb_nr];

    // Save the slice number of this macroblock. When the macroblock below
    // is coded it will use this to decide if prediction for above is possible
    img->current_slice_nr = img->mb_data[img->current_mb_nr].slice_nr;     // jlzheng 6.30
    currMB->slice_nr = img->current_slice_nr;

    // Rate control
    if(input->RCEnable)
    {
        /*frame layer rate control*/
        if(input->basicunit==img->Frame_Total_Number_MB)
        {
            currMB->delta_qp = 0;
            currMB->qp       = img->qp;
        }
        /*basic unit layer rate control*/
        else
        {
            /*each I or B frame has only one QP*/
            if((img->type==INTRA_IMG)||(img->type==B_IMG))
            {
                currMB->delta_qp = 0;
                currMB->qp       = img->qp;
            }
            else if(img->type==INTER_IMG)
            {
                if (img->current_mb_nr == 0) //first macroblock
                {
                    // Initialize delta qp change from last macroblock. Feature may be used for future rate control
                    currMB->delta_qp = 0;
                    currMB->qp       = img->qp;
                    DELTA_QP = DELTA_QP2 = currMB->delta_qp;
                    QP = QP2 = currMB->qp;
                }
                else if (input->slice_row_nr && (img->current_mb_nr>0 && img->mb_data[img->current_mb_nr].slice_nr != img->mb_data[img->current_mb_nr-1].slice_nr))
                { // the first macroblock in the slice, xiaozhen zheng, 20071224
                    currMB->delta_qp = 0;
                    currMB->qp       = img->qp;
                    DELTA_QP = DELTA_QP2 = currMB->delta_qp;
                    QP = QP2 = currMB->qp;
                }
                else
                {
                    if (img->mb_data[img->current_mb_nr-1].prev_cbp == 1)
                    {
                        currMB->delta_qp = 0;
                        currMB->qp       = img->qp;
                    }      
                    else
                    {
                        currMB->qp = img->mb_data[img->current_mb_nr-1].prev_qp;
                        currMB->delta_qp = currMB->qp - img->mb_data[img->current_mb_nr-1].qp;
                        img->qp = currMB->qp;
                    }
                    DELTA_QP = DELTA_QP2 = currMB->delta_qp;
                    QP = QP2 = currMB->qp;
                }

                /*compute the quantization parameter for each basic unit of P frame*/
                if((img->NumberofCodedMacroBlocks>0)\
                    &&(img->NumberofCodedMacroBlocks%img->BasicUnit==0))
                {

                    /*frame coding*/
                    if(input->InterlaceCodingOption==0)
                    {
                        updateRCModel();
                        img->BasicUnitQP=updateQuantizationParameter(img->TopFieldFlag);
                    }
                    /*adaptive field/frame coding*/
                    else if((input->InterlaceCodingOption==2)&&(img->IFLAG==0))
                    {
                        updateRCModel();
                        img->BasicUnitQP=updateQuantizationParameter(img->TopFieldFlag);
                    }
                    /*field coding*/
                    else if((input->InterlaceCodingOption==1)&&(img->IFLAG==0))
                    {
                        updateRCModel();
                        img->BasicUnitQP=updateQuantizationParameter(img->TopFieldFlag);
                    }
                }


                if(img->current_mb_nr==0)
                    img->BasicUnitQP=img->qp;

                currMB->predict_qp=img->BasicUnitQP;

                if(currMB->predict_qp>currMB->qp+25)
                    currMB->predict_qp=currMB->qp+25;
                else if(currMB->predict_qp<currMB->qp-26)
                    currMB->predict_qp=currMB->qp-26; 

                currMB->prev_qp = currMB->predict_qp;

                dq = currMB->delta_qp + currMB->predict_qp-currMB->qp;
                if(dq < -26) 
                {
                    dq = -26;
                    predict_error = dq-currMB->delta_qp;
                    img->qp = img->qp+predict_error;
                    currMB->delta_qp = -26;
                }
                else if(dq > 25)
                {
                    dq = 25;
                    predict_error = dq - currMB->delta_qp;
                    img->qp = img->qp + predict_error;
                    currMB->delta_qp = 25;
                }
                else
                {
                    currMB->delta_qp = dq;
                    predict_error=currMB->predict_qp-currMB->qp;
                    img->qp = currMB->predict_qp;
                }
                currMB->qp =  img->qp;
                currMB->predict_error=predict_error;
            } 
        }
    }
    else
    {
        currMB->delta_qp = 0;
        currMB->qp       = img->qp;       // needed in loop filter (even if constant QP is used)
    }

    // Initialize counter for MB symbols
    currMB->currSEnr=0;

    // If MB is next to a slice boundary, mark neighboring blocks unavailable for prediction
    CheckAvailabilityOfNeighbors(img);

    currMB->mb_type   = 0;

    // Reset vectors before doing motion search in motion_search().
    if (img->type != B_IMG)  
    {
        for (k=0; k < 2; k++)
        {
            for (j=0; j < BLOCK_MULTIPLE; j++)
                for (i=0; i < BLOCK_MULTIPLE; i++)
                    tmp_mv[k][img->block_y+j][img->block_x+i+4]=0;
        }
    }

    // Reset syntax element entries in MB struct
    currMB->mb_type   = 0;
    currMB->cbp_blk   = 0;
    currMB->cbp       = 0;
    currMB->cbp01     = 0;  //wzm,422
    currMB->mb_field  = 0;

    for (l=0; l < 2; l++)
        for (j=0; j < BLOCK_MULTIPLE; j++)
            for (i=0; i < BLOCK_MULTIPLE; i++)
                for (k=0; k < 2; k++)
                    currMB->mvd[l][j][i][k] = 0;

    currMB->cbp_bits   = 0;
    currMB->c_ipred_mode = DC_PRED_8; //GB
    currMB->c_ipred_mode_2 = DC_PRED_8; //X ZHENG,chroma 422

    for (i=0; i < (BLOCK_MULTIPLE*BLOCK_MULTIPLE); i++)
        currMB->intra_pred_modes[i] = DC_PRED;

    // store filtering parameters for this MB; For now, we are using the
    // same offset throughout the sequence
    currMB->lf_disable = input->loop_filter_disable;
    currMB->lf_alpha_c0_offset = input->alpha_c_offset;
    currMB->lf_beta_offset = input->beta_offset;

    // Initialize bitcounters for this macroblock
    if(img->current_mb_nr == 0) // No slice header to account for
    {
        currMB->bitcounter[BITS_HEADER] = 0;
    }
    else if (currMB->slice_nr == img->mb_data[img->current_mb_nr-1].slice_nr) // current MB belongs to the
        // same slice as the last MB
    {
        currMB->bitcounter[BITS_HEADER] = 0;
    }

    currMB->bitcounter[BITS_MB_MODE] = 0;
    currMB->bitcounter[BITS_COEFF_Y_MB] = 0;
    currMB->bitcounter[BITS_INTER_MB] = 0;
    currMB->bitcounter[BITS_CBP_MB] = 0;
    currMB->bitcounter[BITS_DELTA_QUANT_MB] = 0;
    currMB->bitcounter[BITS_COEFF_UV_MB] = 0;

}

/*
*************************************************************************
* Function:Terminate processing of the current macroblock depending
on the chosen slice mode
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void terminate_macroblock(Boolean *end_of_picture)
{
    Macroblock    *currMB    = &img->mb_data[img->current_mb_nr];
    SyntaxElement *currSE    = &img->MB_SyntaxElements[currMB->currSEnr];


    int rlc_bits=0;
    static int skip = FALSE;
    int mb_width = img->width/16;
    int slice_mb = input->slice_row_nr*mb_width;


    img->coded_mb_nr++;

    if(input->slice_row_nr && (img->coded_mb_nr != img->total_number_mb))
    {
        if(img->coded_mb_nr%slice_mb == 0 )
        {
            img->mb_data[img->current_mb_nr+1].slice_nr = img->mb_data[img->current_mb_nr].slice_nr+1;  // last MB of slice, begin next slice   by jlzheng  6.30
            img->mb_no_currSliceLastMB =  min(img->mb_no_currSliceLastMB + slice_mb, img->total_number_mb) ;  // Yulj 2004.07.15     
        }
        else
            img->mb_data[img->current_mb_nr+1].slice_nr = img->mb_data[img->current_mb_nr].slice_nr;
    }


    if (img->coded_mb_nr == img->total_number_mb) // maximum number of MBs reached
    {
        *end_of_picture = TRUE;
        img->coded_mb_nr= 0;
    }


    if(*end_of_picture == TRUE && img->cod_counter)
    {
        currSE->value1 = img->cod_counter;
        currSE->mapping = ue_linfo;
        currSE->type = SE_MBTYPE;
        writeSyntaxElement_UVLC(  currSE, currBitStream);
        rlc_bits=currSE->len;
        currMB->bitcounter[BITS_MB_MODE]+=rlc_bits;
        img->cod_counter = 0;
    }

}

/*
*************************************************************************
* Function: Checks the availability of neighboring macroblocks of
the current macroblock for prediction and context determination;
marks the unavailable MBs for intra prediction in the
ipredmode-array by -1. Only neighboring MBs in the causal
past of the current MB are checked.
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void CheckAvailabilityOfNeighbors()
{
    int i,j;

    //const int mb_width = input->img_width/MB_BLOCK_SIZE;
    const int mb_width = img->width/MB_BLOCK_SIZE; //add by wuzhongmou 0610
    const int mb_nr    = img->current_mb_nr;
    Macroblock *currMB = &img->mb_data[mb_nr];
    int   pix_x   = img->pix_x;  
    int   tmp_x   = img->block_x;
    int   tmp_y   = img->block_y;
    int   block_x = img->block_x/2;
    int   pix_y   = img->pix_y;   
    int   block_y = img->block_y/2;
    int   remove_prediction;

    img->block_x = block_x;
    img->block_y = block_y;

    // mark all neighbors as unavailable
    for (i=0; i<3; i++)
        for (j=0; j<3; j++)
        {
            img->mb_data[mb_nr].mb_available[i][j]=NULL;
        }

        img->mb_data[mb_nr].mb_available[1][1]=currMB; // current MB

        // Check MB to the left
        if(pix_x >= MB_BLOCK_SIZE)
        {
            remove_prediction = currMB->slice_nr != img->mb_data[mb_nr-1].slice_nr;
            // upper blocks
            if (remove_prediction)
            {
                img->ipredmode[img->block_x][img->block_y+1] = -1;
                img->ipredmode[img->block_x][img->block_y+2] = -1; 
            }

            // lower blocks
            if (remove_prediction)
            {
                img->ipredmode[img->block_x][img->block_y+3] = -1;
                img->ipredmode[img->block_x][img->block_y+4] = -1;
            }

            if (!remove_prediction)
            {
                currMB->mb_available[1][0]=&(img->mb_data[mb_nr-1]);
            }
        }

        // Check MB above
        if(pix_y >= MB_BLOCK_SIZE) 
        {
            remove_prediction = currMB->slice_nr != img->mb_data[mb_nr-mb_width].slice_nr;

            // upper blocks
            if (remove_prediction)
            {
                img->ipredmode[img->block_x+1][img->block_y] = -1;
                img->ipredmode[img->block_x+2][img->block_y] = -1;
            }

            if (!remove_prediction)
            {  
                currMB->mb_available[0][1]=&(img->mb_data[mb_nr-mb_width]);
            }
        }

        // Check MB left above
        if(pix_x >= MB_BLOCK_SIZE && pix_y >= MB_BLOCK_SIZE )
        {
            remove_prediction = currMB->slice_nr != img->mb_data[mb_nr-mb_width-1].slice_nr;

            if (remove_prediction)
            { 

                img->ipredmode[img->block_x][img->block_y] = -1;
            }

            if (!remove_prediction)
            {
                currMB->mb_available[0][0]=&(img->mb_data[mb_nr-mb_width-1]);
            }
        }

        // Check MB right above
        if(pix_y >= MB_BLOCK_SIZE && img->pix_x < (img->width-MB_BLOCK_SIZE ))
        {
            if(currMB->slice_nr == img->mb_data[mb_nr-mb_width+1].slice_nr)
                currMB->mb_available[0][2]=&(img->mb_data[mb_nr-mb_width+1]);
        }

        img->block_x = tmp_x;
        img->block_y = tmp_y;
}

/*
*************************************************************************
* Function:Predict one component of a 4x4 Luma block
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/
void
OneComponentLumaPrediction4x4 (int*   mpred,      //  --> array of prediction values (row by row)
                               int    pic_pix_x,  // <--  absolute horizontal coordinate of 4x4 block
                               int    pic_pix_y,  // <--  absolute vertical   coordinate of 4x4 block
                               int*   mv,         // <--  motion vector
                               int    ref)        // <--  reference frame (0.. / -1:backward)
{
    int incr=0;/*lgp*13*/
    pel_t** ref_pic;
    int     pix_add = 4;
    int     j0      = (pic_pix_y << 2) + mv[1], j1=j0+pix_add, j2=j1+pix_add, j3=j2+pix_add;
    int     i0      = (pic_pix_x << 2) + mv[0], i1=i0+pix_add, i2=i1+pix_add, i3=i2+pix_add;
    int     i,j;

    pel_t (*get_pel) (pel_t**, int, int) = UMVPelY_14;


    //////cjw 20060321  for MV limit//////////////////////// 
    //MAX_V_SEARCH_RANGE  [-256, +255.75]
    //MAX_V_SEARCH_RANGE_FIELD  [-128, +127.75]
    //MAX_H_SEARCH_RANGE  [-2048, +2047.75]
    // MaxMVHRange= MAX_H_SEARCH_RANGE;
    //  MaxMVVRange= MAX_V_SEARCH_RANGE;
    //    if(!img->picture_structure) //field coding
    //  {
    //    MaxMVVRange=MAX_V_SEARCH_RANGE_FIELD;
    //  }
    //
    //  if ( mv[0] < -MaxMVHRange || mv[0] > MaxMVHRange-1 
    //    || mv[1] < -MaxMVVRange || mv[1] > MaxMVVRange-1 )
    //  {
    //    mv_out_of_range = 1;
    //  }
    //////////end cjw //////////////////////////////////////

    mv_out_of_range *= check_mv_range(mv[0], mv[1], pic_pix_x, pic_pix_y, 5);    // mv_range, 20071009

    incr = 1;
    if(!img->picture_structure) //field coding
    {
        incr =2;
    }

    ref_pic   = img->type==B_IMG? mref [ref+incr] : mref [ref];

    for(j=0;j<4;j++)
    {
        for(i=0;i<4;i++)
        {
            *mpred++ = get_pel (ref_pic, j0+pix_add*j, i0+pix_add*i);
        }
    }
}

/*
*************************************************************************
* Function:Predict one 4x4 Luma block
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void
LumaPrediction4x4 (int  block_x,    // <--  relative horizontal block coordinate of 4x4 block
                   int  block_y,    // <--  relative vertical   block coordinate of 4x4 block
                   int  fw_mode,    // <--  forward  prediction mode (1-7, 0=DIRECT if bw_mode=0)
                   int  bw_mode,    // <--  backward prediction mode (1-7, 0=DIRECT if fw_mode=0)
                   int  fw_ref,      // <--  reference frame for forward prediction (-1: Intra4x4 pred. with fw_mode)
                   int  bw_ref  )    
{
    static int fw_pred[16];
    static int bw_pred[16];
    int  i, j;
    int  block_x4  = block_x+4;
    int  block_y4  = block_y+4;
    int  pic_pix_x = img->pix_x + block_x;
    int  pic_pix_y = img->pix_y + block_y;
    int  bx        = block_x >> 3;
    int  by        = block_y >> 3;
    int* fpred     = fw_pred;
    int* bpred     = bw_pred;
    int  direct    = (fw_mode == 0 && bw_mode == 0 && (img->type==B_IMG));
    int  skipped   = (fw_mode == 0 && bw_mode == 0 && (img->type!=B_IMG));
    //int  *****fmv_array = img->all_mv;    // For MB level frame/field coding
    int scale_frame;
    int  *****fmv_array = (fw_mode && bw_mode)?img->all_omv:img->all_mv;    // For MB level frame/field coding
    int  *****bmv_array = img->all_bmv;   // For MB level frame/field coding
    //cjw 20060112 for weighted prediction
    int fw_lum_scale , fw_lum_shift ;
    int bw_lum_scale , bw_lum_shift ;
    int bw_ref_num ;
    int fw_ref_num ;

    //cjw weighted prediction parameter map 20060112
    /*///frame coding/////////
    P    img->lum_scale[0]  fw[0] 
    img->lum_scale[1]  fw[1]      
    B      img->lum_scale[0]  fw[0]
    img->lum_scale[1]  bw[0]
    ///field coding////////
    P    img->lum_scale[0]  fw[0] ; img->lum_scale[1]  fw[1] 
    img->lum_scale[2]  fw[2] ; img->lum_scale[3]  fw[3]       
    B    img->lum_scale[0]  fw[0] ; img->lum_scale[1]  bw[0] 
    img->lum_scale[2]  fw[1] ; img->lum_scale[3]  bw[1] 
    */      

    if (direct)    
    {
        fw_ref= 0;//max(refFrArr[ipdirect_y][ipdirect_x],0);
        bw_ref= 0;//min(refFrArr[ipdirect_y][ipdirect_x],0);
        if(!img->picture_structure) //field coding
        {
            scale_frame = refFrArr[img->block8_y + by][img->block8_x+bx];
            bw_ref = 0;//-1; xyji 11.25
            if (img->current_mb_nr_fld < img->total_number_mb) //top field
            {
                fw_ref =  scale_frame >= 1 ? 1 : 0;
                bw_ref = scale_frame >= 0 ?1 :0; 
            }
            else
            {
                fw_ref =(scale_frame == 1 || scale_frame < 0)? 0 : 1; 
                bw_ref = 0;
            }

            if(scale_frame < 0)
                bw_ref = 1;
        }
    }


    direct_mode = direct;

    if (fw_mode ||(direct && (fw_ref !=-1) )|| skipped)
    {
        if(!img->picture_structure)  // !! field shenyanfei 
            OneComponentLumaPrediction4x4 (fw_pred, pic_pix_x, pic_pix_y, fmv_array [bx][by][fw_ref][fw_mode], fw_ref);
        else            // !! frame shenyanfei 
            OneComponentLumaPrediction4x4 (fw_pred, pic_pix_x, pic_pix_y, fmv_array [bx][by][fw_ref][fw_mode], (direct ?0:fw_ref));
    }
    if (bw_mode || (direct && (bw_ref !=-1) ) ||(direct && !img->picture_structure && (bw_ref ==-1) ) )
    {
        int delta_P,TRp,DistanceIndexFw,DistanceIndexBw,refframe,delta_PB;
        int mv[2];
        refframe = fw_ref;
        delta_P = 2*(img->imgtr_next_P_frm - img->imgtr_last_P_frm);
        delta_P = (delta_P + 512)%512;   // Added by Xiaozhen ZHENG, 2007.05.05
        if(img->picture_structure)
            TRp = (refframe+1)*delta_P;  //the lates backward reference
        else
        {
            TRp = delta_P;//refframe == 0 ? delta_P-1 : delta_P+1;
        }
        delta_PB = 2*(picture_distance - img->imgtr_last_P_frm);     // Tsinghua  200701
        TRp  = (TRp + 512)%512;
        delta_PB = (delta_PB + 512)%512;  // Added by Xiaozhen ZHENG, 2007.05.05
        if(!img->picture_structure)
        {
            if(img->current_mb_nr_fld < img->total_number_mb) //top field
                DistanceIndexFw =  refframe == 0 ? delta_PB-1:delta_PB;
            else
                DistanceIndexFw =  refframe == 0 ? delta_PB:delta_PB+1;
        }
        else
            DistanceIndexFw = delta_PB;      

        //DistanceIndexBw    = TRp - DistanceIndexFw;      
        DistanceIndexBw    = (TRp - DistanceIndexFw+512)%512; // Added by Zhijie Yang, 20070419, Broadcom

        mv[0] = - ((fmv_array[bx][by][fw_ref][fw_mode][0]*DistanceIndexBw*(512/DistanceIndexFw)+256)>>9);
        mv[1] = - ((fmv_array[bx][by][fw_ref][fw_mode][1]*DistanceIndexBw*(512/DistanceIndexFw)+256)>>9);
        if(fw_mode && bw_mode)
            OneComponentLumaPrediction4x4 (bw_pred, pic_pix_x, pic_pix_y, mv,
            (!img->picture_structure) ? (-2+bw_ref) : (-1-bw_ref));
        else
            OneComponentLumaPrediction4x4 (bw_pred, pic_pix_x, pic_pix_y, bmv_array[bx][by][bw_ref][bw_mode],
            (!img->picture_structure) ? (-2+bw_ref) : (-1-bw_ref));
    }

    // !! start shenyanfei
    if (direct || (fw_mode && bw_mode))
    {  
        if(!img->picture_structure)
            bw_ref=1-bw_ref;   //cjw used for bw reference buffer address changing 20060112 bw

        fw_ref_num=(img->picture_structure)?(fw_ref):(2*fw_ref);  //cjw 20060112 fw 
        bw_ref_num=(img->picture_structure)?(bw_ref+1):(2*bw_ref+1);  //cjw 20060112 bw

        fw_lum_scale = img->lum_scale[fw_ref_num];
        fw_lum_shift = img->lum_shift[fw_ref_num];
        bw_lum_scale = img->lum_scale[bw_ref_num];
        bw_lum_shift = img->lum_shift[bw_ref_num];

        for(j=block_y; j<block_y4; j++)
            for (i=block_x; i<block_x4; i++){
                img->mpr[i][j] = (*fpred + *bpred + 1) / 2;

                if(img->LumVarFlag == 1)
                    img->mpr_weight[i][j] = (Clip1((((*fpred)*fw_lum_scale + 16)>>5) + fw_lum_shift)+
                    Clip1((((*bpred)*bw_lum_scale + 16)>>5) + bw_lum_shift) + 1)/2;

                fpred++;
                bpred++;
            }
    }
    else if (fw_mode || skipped)  //P fw and B one direction fw 
    {
        if(img->type==B_IMG){
            fw_ref_num=(img->picture_structure)?(fw_ref):(2*fw_ref);  //cjw 20060112 fw 

            fw_lum_scale = img->lum_scale[fw_ref_num];  //cjw 20060112
            fw_lum_shift = img->lum_shift[fw_ref_num];
        }
        else{
            fw_lum_scale = img->lum_scale[fw_ref];
            fw_lum_shift = img->lum_shift[fw_ref];
        }

        for(j=block_y; j<block_y4; j++)
            for (i=block_x; i<block_x4; i++){
                img->mpr[i][j] = *fpred;
                if((!skipped) && (img->LumVarFlag == 1)){
                    img->mpr_weight[i][j] = Clip1(((((*fpred)*fw_lum_scale + 16)>>5) + fw_lum_shift));
                }
                fpred++;
            }
    }
    else  //B one direction bw
    {
        if(!img->picture_structure)//cjw used for bw reference buffer address changing 20060112 bw
            bw_ref=1-bw_ref;   

        bw_ref_num=(img->picture_structure)?(bw_ref+1):(2*bw_ref+1);  //cjw 20060112 bw

        bw_lum_scale = img->lum_scale[bw_ref_num];
        bw_lum_shift = img->lum_shift[bw_ref_num];
        for(j=block_y; j<block_y4; j++)
            for (i=block_x; i<block_x4; i++){
                img->mpr[i][j] = *bpred;

                if(img->LumVarFlag == 1){
                    img->mpr_weight[i][j] = Clip1(((((*bpred)*bw_lum_scale + 16)>>5) + bw_lum_shift));
                }
                bpred++;
            }
    }
    // !! end shenyanfei

}

/*
*************************************************************************
* Function:Residual Coding of an 8x8 Luma block (not for intra)
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int                                       //  ==> coefficient cost
LumaResidualCoding8x8 (int  *cbp,         //  --> cbp (updated according to processed 8x8 luminance block)
                       int  *cbp_blk,     //  --> block cbp (updated according to processed 8x8 luminance block)
                       int  block8x8,     // <--  block number of 8x8 block
                       int  fw_mode,      // <--  forward  prediction mode (1-7, 0=DIRECT)
                       int  bw_mode,      // <--  backward prediction mode (1-7, 0=DIRECT)
                       int  fw_refframe,  // <--  reference frame for forward prediction
                       int  bw_refframe   // <--  reference frame for backward prediction
                       )
{
    int    block_y, block_x, pic_pix_y, pic_pix_x, i, j, cbp_blk_mask;
    int    coeff_cost = 0;
    int    mb_y       = (block8x8 / 2) << 3;
    int    mb_x       = (block8x8 % 2) << 3;
    int    cbp_mask   = 1 << block8x8;
    int    bxx, byy;                   // indexing curr_blk
    int    scrFlag = 0;                // 0=noSCR, 1=strongSCR, 2=jmSCR
    byte** imgY_original = imgY_org;
    int  pix_x    = img->pix_x;
    int  pix_y    = img->pix_y;
    Macroblock* currMB = &img->mb_data[img->current_mb_nr];
    int    direct    = (fw_mode == 0 && bw_mode == 0 && (img->type==B_IMG));
    int    skipped   = (fw_mode == 0 && bw_mode == 0 && (img->type!=B_IMG));
    short    curr_blk[B8_SIZE][B8_SIZE]; // AVS 8x8 pred.error buffer
    int  incr_y=1,off_y=0; /*lgp*/

    int IntraPrediction=IS_INTRA (currMB);  //cjw 20060321


    if (img->type==B_IMG)
        scrFlag = 1;


    //===== loop over 4x4 blocks =====
    for (byy=0, block_y=mb_y; block_y<mb_y+8; byy+=4, block_y+=4)
    {
        //pic_pix_y = pix_y + block_y;/*lgp*/
        pic_pix_y = pix_y + mb_y;/*lgp*/

        for (bxx=0, block_x=mb_x; block_x<mb_x+8; bxx+=4, block_x+=4)
        {
            pic_pix_x = pix_x + block_x;

            cbp_blk_mask = (block_x>>2) + block_y;

            //===== prediction of 4x4 block =====
            LumaPrediction4x4 (block_x, block_y, fw_mode, bw_mode, fw_refframe, bw_refframe);


            // !! start shenyanfei 
            //if(((!direct)&&(!skipped)&&(img->LumVarFlag == 1) && (img->mb_weighting_flag == 0)&&(img->weighting_prediction == 1))
            //if(((!skipped)&&(!direct)&&(img->LumVarFlag == 1) && (img->mb_weighting_flag == 1)&&(img->weighting_prediction == 1))  //cjw 20051230
            if(((!skipped) && (img->LumVarFlag == 1) && (img->mb_weighting_flag == 1)&&(img->weighting_prediction == 1)) //cjw 20060321
                ||((img->LumVarFlag == 1) && (img->mb_weighting_flag == 0))){
                    for (j=0; j<4; j++)
                        for (i=0; i<4; i++)
                            img->mpr[i+block_x][j+block_y] = img->mpr_weight[i+block_x][j+block_y];
            }
            // !! end shenyanfei

            //===== get displaced frame difference ======
            for (j=0; j<4; j++)
                for (i=0; i<4; i++)
                {
                    img->m7[i][j] = curr_blk[byy+j][bxx+i] = 
                        imgY_original[pic_pix_y+incr_y*(j+byy)+off_y/*lgp*/][pic_pix_x+i] - img->mpr[i+block_x][j+block_y];
                }
        }
    }

    if (!skipped)
    {
        transform_B8(curr_blk);
        coeff_cost = scanquant_B8   (img->qp-MIN_QP, 0, block8x8, curr_blk, scrFlag, cbp, cbp_blk);  
    }
    /*
    The purpose of the action below is to prevent that single or 'expensive' coefficients are coded.
    With 4x4 transform there is larger chance that a single coefficient in a 8x8 or 16x16 block may be nonzero.
    A single small (level=1) coefficient in a 8x8 block will cost: 3 or more bits for the coefficient,
    4 bits for EOBs for the 4x4 blocks,possibly also more bits for CBP.  Hence the total 'cost' of that single
    coefficient will typically be 10-12 bits which in a RD consideration is too much to justify the distortion improvement.
    The action below is to watch such 'single' coefficients and set the reconstructed block equal to the prediction according
    to a given criterium.  The action is taken only for inter luma blocks.

    Notice that this is a pure encoder issue and hence does not have any implication on the standard.
    coeff_cost is a parameter set in dct_luma() and accumulated for each 8x8 block.  If level=1 for a coefficient,
    coeff_cost is increased by a number depending on RUN for that coefficient.The numbers are (see also dct_luma()): 3,2,2,1,1,1,0,0,...
    when RUN equals 0,1,2,3,4,5,6, etc.
    If level >1 coeff_cost is increased by 9 (or any number above 3). The threshold is set to 3. This means for example:
    1: If there is one coefficient with (RUN,level)=(0,1) in a 8x8 block this coefficient is discarded.
    2: If there are two coefficients with (RUN,level)=(1,1) and (4,1) the coefficients are also discarded
    sum_cnt_nonz is the accumulation of coeff_cost over a whole macro block.  If sum_cnt_nonz is 5 or less for the whole MB,
    all nonzero coefficients are discarded for the MB and the reconstructed block is set equal to the prediction.
    *///Lou


    return coeff_cost;
}


void                                       //  ==> coefficient cost
LumaPrediction (int  *cbp,         //  --> cbp (updated according to processed 8x8 luminance block)
                int  *cbp_blk,     //  --> block cbp (updated according to processed 8x8 luminance block)
                int  block8x8,     // <--  block number of 8x8 block
                int  fw_mode,      // <--  forward  prediction mode (1-7, 0=DIRECT)
                int  bw_mode,      // <--  backward prediction mode (1-7, 0=DIRECT)
                int  fw_refframe,  // <--  reference frame for forward prediction
                int  bw_refframe   // <--  reference frame for backward prediction
                )
{
    int    block_y, block_x, pic_pix_y, pic_pix_x, cbp_blk_mask;
    int    coeff_cost = 0;
    int    mb_y       = (block8x8 / 2) << 3;
    int    mb_x       = (block8x8 % 2) << 3;
    int    cbp_mask   = 1 << block8x8;
    int    bxx, byy;                   // indexing curr_blk
    int    scrFlag = 0;                // 0=noSCR, 1=strongSCR, 2=jmSCR
    byte** imgY_original = imgY_org;
    int  pix_x    = img->pix_x;
    int  pix_y    = img->pix_y;
    Macroblock* currMB = &img->mb_data[img->current_mb_nr];
    int    skipped    = (fw_mode == 0 && bw_mode == 0 && (img->type!=B_IMG));
    int  incr_y=1,off_y=0; /*lgp*/

    if (img->type==B_IMG)
        scrFlag = 1;

    //===== loop over 4x4 blocks =====
    for (byy=0, block_y=mb_y; block_y<mb_y+8; byy+=4, block_y+=4)
    {

        pic_pix_y = pix_y + mb_y;/*lgp*/

        for (bxx=0, block_x=mb_x; block_x<mb_x+8; bxx+=4, block_x+=4)
        {
            pic_pix_x = pix_x + block_x;

            cbp_blk_mask = (block_x>>2) + block_y;
            //===== prediction of 4x4 block =====
            LumaPrediction4x4 (block_x, block_y, fw_mode, bw_mode, fw_refframe, bw_refframe);
        }
    }

    return ;
}

/*
*************************************************************************
* Function:Set mode parameters and reference frames for an 8x8 block
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void
SetModesAndRefframe (int b8, int* fw_mode, int* bw_mode, int* fw_ref, int* bw_ref)
{
    Macroblock* currMB = &img->mb_data[img->current_mb_nr];
    int         j      = (b8/2);
    int         i      = (b8%2);
    int**     frefarr = refFrArr;   // For MB level field/frame coding
    int**     fw_refarr = fw_refFrArr;  // For MB level field/frame coding
    int**     bw_refarr = bw_refFrArr;  // For MB level field/frame coding
    int     block_x = img->block_x; 
    int     block_y = img->block_y; // For MB level field/frame coding

    *fw_mode = *bw_mode = *fw_ref = *bw_ref = -1;

    if (img->type!=B_IMG)
    {
        *fw_ref = frefarr[(block_y>>1)+j][(block_x>>1)+i];
        *bw_ref = 0;
        *bw_mode  = 0;
        *fw_mode  = currMB->b8mode[b8];
    }
    else
    {
        if (currMB->b8pdir[b8]==-1)
        {
            *fw_ref   = -1;
            *bw_ref   = -1;
            *fw_mode  =  0;
            *bw_mode  =  0;
        }
        else if (currMB->b8pdir[b8]==0)
        {
            *fw_ref   = fw_refarr[(block_y>>1)+j][(block_x>>1)+i];
            *bw_ref   = 0;
            *fw_mode  = currMB->b8mode[b8];
            *bw_mode  = 0;
        }
        else if (currMB->b8pdir[b8]==1)
        {
            *fw_ref   = 0;
            *bw_ref   = bw_refarr[(block_y>>1)+j][(block_x>>1)+i];
            *fw_mode  = 0;
            *bw_mode  = currMB->b8mode[b8];
        }
        else
        {
            *fw_ref   = fw_refarr[(block_y>>1)+j][(block_x>>1)+i];
            *bw_ref   = bw_refarr[(block_y>>1)+j][(block_x>>1)+i];
            *fw_mode  = currMB->b8mode[b8];
            *bw_mode  = currMB->b8mode[b8];

            if (currMB->b8mode[b8]==0) // direct
            {

                if (img->type==B_IMG)
                {
                    //sw
                    *fw_ref = 0;// max(0,frefarr[(block_y>>1)+j][(block_x>>1)+i]);
                    *bw_ref = 0;
                }
                else
                {
                    *fw_ref = max(0,frefarr[(block_y>>1)+j][(block_x>>1)+i]);
                    *bw_ref = 0;
                }
            }
        }
    }
}

/*
*************************************************************************
* Function:Residual Coding of a Luma macroblock (not for intra)
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void LumaResidualCoding ()
{
    int i,j,block8x8;
    int fw_mode, bw_mode, refframe;
    int sum_cnt_nonz;
    Macroblock *currMB = &img->mb_data[img->current_mb_nr];
    int incr_y=1,off_y=0;/*lgp*/
    int stage_block8x8_pos=0;/*lgp*/
    int skipped;/*lgp*dct*/
    int sad1 , sad2 ;
    byte** imgY_original = imgY_org;

    currMB->cbp     = 0 ;
    currMB->cbp_blk = 0 ;
    sum_cnt_nonz    = 0 ;

    // !! start shenyanfei 
    if((img->LumVarFlag == 1) && (img->mb_weighting_flag == 1)){
        for (block8x8=stage_block8x8_pos; block8x8<4; block8x8++){
            int bw_ref;
            SetModesAndRefframe (block8x8, &fw_mode, &bw_mode, &refframe, &bw_ref);
            LumaPrediction(&(currMB->cbp), &(currMB->cbp_blk), block8x8,fw_mode, bw_mode, refframe, bw_ref);
        }
        sad1 = sad2 = 0 ;
        for (j=0; j < MB_BLOCK_SIZE; j++)
        {
            for (i=0; i < MB_BLOCK_SIZE; i++){
                sad1 += abs(imgY_original[img->pix_y+j][img->pix_x+i] -img->mpr[i][j]);
                sad2 += abs(imgY_original[img->pix_y+j][img->pix_x+i] -img->mpr_weight[i][j]);
            }
        }
        if(sad1 > sad2){
            img->weighting_prediction = 1 ;
        }
        else{
            img->weighting_prediction = 0 ;
        }
    }
    // !! end shenyanfei

    currMB->cbp     = 0 ;
    currMB->cbp_blk = 0 ;
    sum_cnt_nonz    = 0 ;

    for (block8x8=stage_block8x8_pos/*lgp*/; block8x8<4; block8x8++)
    {
        int bw_ref;

        SetModesAndRefframe (block8x8, &fw_mode, &bw_mode, &refframe, &bw_ref);

        skipped    = (fw_mode == 0 && bw_mode == 0 && (img->type!=B_IMG));
        sum_cnt_nonz += LumaResidualCoding8x8 (&(currMB->cbp), &(currMB->cbp_blk), block8x8,fw_mode, bw_mode, refframe, bw_ref);
    }

    if (sum_cnt_nonz <= _LUMA_COEFF_COST_)
    {
        currMB->cbp     &= 0xfffff0 ;
        currMB->cbp_blk &= 0xff0000 ;
        for (i=0; i < MB_BLOCK_SIZE; i++)
        {
            for (j=0; j < MB_BLOCK_SIZE; j++)
            {
                imgY[img->pix_y+j][img->pix_x+i]=img->mpr[i][j];
            }
        }
    }
}

/*
*************************************************************************
* Function: Predict one component of a chroma 4x4 block
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void
OneComponentChromaPrediction4x4 (int*     mpred,      //  --> array to store prediction values
                                 int      pix_c_x,    // <--  horizontal pixel coordinate of 4x4 block
                                 int      pix_c_y,    // <--  vertical   pixel coordinate of 4x4 block
                                 int***** mv,         // <--  motion vector array
                                 int      ref,        // <--  reference frame parameter (0.../ -1: backward)
                                 int      blocktype,  // <--  block type
                                 int      uv,
                                 int      directforword)         // <--  chroma component
{
    int     i, j, ii, jj, ii0, jj0, ii1, jj1, if0, if1, jf0, jf1;
    int     incr;
    int*    mvb;
    int     refframe  = (ref<0 ?      0 :    ref);
    pel_t** refimage;
    int     je        = pix_c_y + 4;
    int     ie        = pix_c_x + 4;
    int     f1        = 8 , f2=f1-1, f3=f1*f1, f4=f3>>1;
    int     s1        = 3;
    int     img_pic_c_x = img->pix_c_x;
    int     img_pic_c_y = img->pix_c_y;
    int     scale   = 1;
    int field_mode;
    int     fpred = ref < 0 ? 0: 1;

    //xyji 11.27
    if (!img->picture_structure) // field coding
    {
        if (img->type==B_IMG)
            refframe = ref<0 ? ref+2 : ref;
    }

    field_mode = (!img->picture_structure);


    incr = 1;
    if(img->type==B_IMG && !img->picture_structure)
        incr = 2;

    ref = (img->type==B_IMG) ? ref+incr : ref;

    refimage  = mcef [ref][uv];

    field_mode = (!img->picture_structure);


    for (j=pix_c_y; j<je; j++)
        for (i=pix_c_x; i<ie; i++)
        {
            if(input->chroma_format==2)  { //wzm,422
                mvb  = mv [(i-img_pic_c_x)>>2][(j-img_pic_c_y)>>3][refframe][blocktype]; 
                ii   = (i<<s1) + mvb[0];
                jj   = (j<<s1) + 2*mvb[1];   
            }
            else { //wzm,422
                mvb  = mv [(i-img_pic_c_x)>>2][(j-img_pic_c_y)>>2][refframe][blocktype];
                ii   = (i<<s1) + mvb[0];
                jj   = (j<<s1) + mvb[1];
            }      

            ii0  = max (0, min (img->width_cr -1, ii>>s1     ));
            jj0  = max (0, min (img->height_cr/scale-1, jj>>s1     ));    // For MB level field/frame -- scale chroma height by 2
            ii1  = max (0, min (img->width_cr -1, (ii+f2)>>s1));
            jj1  = max (0, min (img->height_cr/scale-1, (jj+f2)>>s1));

            if1  = (ii&f2);  if0 = f1-if1;
            jf1  = (jj&f2);  jf0 = f1-jf1;

            *mpred++ = (if0 * jf0 * refimage[jj0][ii0] +
                if1 * jf0 * refimage[jj0][ii1] +
                if0 * jf1 * refimage[jj1][ii0] +
                if1 * jf1 * refimage[jj1][ii1] + f4) / f3;

        }
}


/*
*************************************************************************
* Function:Predict one component of a chroma 4x4 block
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void
OneComponentChromaPrediction4x4_dir (int*     mpred,      //  --> array to store prediction values
                                     int      pix_c_x,    // <--  horizontal pixel coordinate of 4x4 block
                                     int      pix_c_y,    // <--  vertical   pixel coordinate of 4x4 block
                                     int***** mv,         // <--  motion vector array
                                     int      ref,        // <--  reference frame parameter (0.../ -1: backward)
                                     int      blocktype,  // <--  block type
                                     int      uv,
                                     int    refframe
                                     )
{
    int     i, j, ii, jj, ii0, jj0, ii1, jj1, if0, if1, jf0, jf1;
    int     incr;
    int*    mvb;
    //int     refframe  = (ref<0 ?      0 :    ref);
    pel_t** refimage;
    int     je        = pix_c_y + 4;
    int     ie        = pix_c_x + 4;
    int     f1        = 8 , f2=f1-1, f3=f1*f1, f4=f3>>1;
    int     s1        = 3;
    int     img_pic_c_y = img->pix_c_y;
    int   scale   = 1;
    int field_mode;


    incr = 1;
    if(img->type==B_IMG && !img->picture_structure)
        incr = 2;

    ref = (img->type==B_IMG) ? ref+incr : ref;

    field_mode = (!img->picture_structure);
    refimage  = mcef [ref][uv];


    for (j=pix_c_y; j<je; j++)
        for (i=pix_c_x; i<ie; i++)
        {
            //xyji 11.27
            //sw 9.30
            if(input->chroma_format==2) //wzm,422
                mvb  = mv [(i-img->pix_c_x)>>2][(j-img_pic_c_y)>>3][refframe][blocktype]; 
            else 
                mvb  = mv [(i-img->pix_c_x)>>2][(j-img_pic_c_y)>>2][refframe][blocktype];
            {     
                int delta_P,TRp,DistanceIndexFw,DistanceIndexBw,delta_PB;       
                delta_P = 2*(img->imgtr_next_P_frm - img->imgtr_last_P_frm);
                delta_P = (delta_P + 512)%512;  // Added by Xiaozhen ZHENG, 2007.05.05
                if(img->picture_structure)
                    //         TRp = (refframe+1)*delta_P;  //the lates backward reference    // 20071009
                    TRp = refframe==0 ? delta_P : (2*(img->imgtr_next_P_frm - img->imgtr_last_prev_P_frm)+512)%512;  // 20071009
                else
                {
                    TRp = delta_P;//refframe == 0 ? delta_P-1 : delta_P+1;
                }

                delta_PB = 2*(picture_distance - img->imgtr_last_P_frm);     // Tsinghua 200701
                TRp  = (TRp + 512)%512;
                delta_PB = (delta_PB + 512)%512;  // Added by Xiaozhen ZHENG, 2007.05.05

                if(!img->picture_structure)
                {
                    if(img->current_mb_nr_fld < img->total_number_mb) //top field
                        DistanceIndexFw =  refframe == 0 ? delta_PB-1:delta_PB;
                    else
                        DistanceIndexFw =  refframe == 0 ? delta_PB:delta_PB+1;
                }  
                else
                    DistanceIndexFw = delta_PB;  

                //DistanceIndexBw    = TRp - DistanceIndexFw;
                DistanceIndexBw    = (TRp - DistanceIndexFw+512)%512; // Added by Zhijie Yang, 20070419, Broadcom

                ii   = (i<<s1) - ((mvb[0]*DistanceIndexBw*(512/DistanceIndexFw)+256)>>9);
                if(input->chroma_format==2) //wzm,422
                    jj   = (j<<s1) - 2*((mvb[1]*DistanceIndexBw*(512/DistanceIndexFw)+256)>>9); 
                else 
                    jj   = (j<<s1) - ((mvb[1]*DistanceIndexBw*(512/DistanceIndexFw)+256)>>9);
            }

            ii0  = max (0, min (img->width_cr -1, ii>>s1     ));
            jj0  = max (0, min (img->height_cr/scale-1, jj>>s1     ));    // For MB level field/frame -- scale chroma height by 2
            ii1  = max (0, min (img->width_cr -1, (ii+f2)>>s1));
            jj1  = max (0, min (img->height_cr/scale-1, (jj+f2)>>s1));

            if1  = (ii&f2);  if0 = f1-if1;
            jf1  = (jj&f2);  jf0 = f1-jf1;

            *mpred++ = (if0 * jf0 * refimage[jj0][ii0] +
                if1 * jf0 * refimage[jj0][ii1] +
                if0 * jf1 * refimage[jj1][ii0] +
                if1 * jf1 * refimage[jj1][ii1] + f4) / f3;
        }
}

/*
*************************************************************************
* Function:Predict an intra chroma 4x4 block
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void IntraChromaPrediction4x4 (int  uv,       // <-- colour component
                               int  block_x,  // <-- relative horizontal block coordinate of 4x4 block
                               int  block_y)  // <-- relative vertical   block coordinate of 4x4 block
{
    int mode = img->mb_data[img->current_mb_nr].c_ipred_mode;
    int i, j;

    //X ZHENG,chroma 422
    if(input->chroma_format==2 && block_y/8!=0)
        mode = img->mb_data[img->current_mb_nr].c_ipred_mode_2;

    //===== prediction =====
    for (j=block_y; j<block_y+4; j++)
        for (i=block_x; i<block_x+4; i++)
        {
            img->mpr[i][j] = img->mprr_c[uv][mode][i][j];
        }
}

/*
*************************************************************************
* Function:Predict one chroma 4x4 block
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void
ChromaPrediction4x4 (int  uv,           // <-- colour component
                     int  block_x,      // <-- relative horizontal block coordinate of 4x4 block
                     int  block_y,      // <-- relative vertical   block coordinate of 4x4 block
                     int  fw_mode,      // <-- forward  prediction mode (1-7, 0=DIRECT if bw_mode=0)
                     int  bw_mode,      // <-- backward prediction mode (1-7, 0=DIRECT if fw_mode=0)
                     int  fw_ref_frame, // <-- reference frame for forward prediction (if (<0) -> intra prediction)
                     int  bw_ref_frame) // <-- reference frame for backward prediction 
{
    static int fw_pred[16];
    static int bw_pred[16];

    int  i, j;
    int  block_x4  = block_x+4;
    int  block_y4  = block_y+4;
    int  pic_pix_x = img->pix_c_x + block_x;
    int  pic_pix_y = img->pix_c_y + block_y;
    int* fpred     = fw_pred;
    int* bpred     = bw_pred;
    int  by /*= block_y >>2*/; //wzm,422
    int  bx = (block_x >>2);

    int  direct    = (fw_mode == 0 && bw_mode == 0 && (img->type==B_IMG));

    int  skipped   = (fw_mode == 0 && bw_mode == 0 && (img->type!=B_IMG));
    int  *****fmv_array = (fw_mode && bw_mode)?img->all_omv:img->all_mv;    // For MB level frame/field coding

    int***** bmv_array = img->all_bmv;
    int fw_ref_idx, bw_ref_idx;
    int scale_frame;

    int directforward = (img->type==B_IMG && fw_mode==0);

    int fw_scale , fw_shift ;
    int bw_scale , bw_shift ;
    int bw_ref_num ;
    int fw_ref_num ;

    //wzm,422
    if(input->chroma_format ==2)
        by= (block_y >>3);
    else
        by= (block_y >>2);
    //wzm,422

    direct_mode = direct;

    //===== INTRA PREDICTION =====
    if (fw_ref_frame < 0)
    {
        IntraChromaPrediction4x4 (uv, block_x, block_y);
        return;
    }

    if (direct)    
    {
        fw_ref_frame= 0;//max(refFrArr[ipdirect_y][ipdirect_x],0);
        bw_ref_frame= 0;//min(refFrArr[ipdirect_y][ipdirect_x],0);
        if(!img->picture_structure)
        {
            scale_frame = refFrArr[img->block8_y + by][img->block8_x+bx];
            bw_ref_frame = 0;//-1; xyji 11.25
            if (img->current_mb_nr_fld < img->total_number_mb)
            {
                fw_ref_frame =  scale_frame >= 1 ? 1 : 0;
                bw_ref_frame = scale_frame >= 0 ? 1 : 0;
            }
            else
            {
                fw_ref_frame = (scale_frame == 1 || scale_frame < 0)? 0 : 1; 
                bw_ref_frame = 0;
            }

            if(scale_frame < 0)
                bw_ref_frame = 1;
        }
    }

    fw_ref_idx = fw_ref_frame;
    bw_ref_idx = bw_ref_frame;

    //===== INTER PREDICTION =====
    if (fw_mode || (direct && (fw_ref_frame!=-1)) || skipped)
    {
        if(!img->picture_structure)
            OneComponentChromaPrediction4x4 (fw_pred, pic_pix_x, pic_pix_y, fmv_array , fw_ref_frame, fw_mode, uv, (directforward ?1 :0));
        else
            OneComponentChromaPrediction4x4 (fw_pred, pic_pix_x, pic_pix_y, fmv_array , (directforward ?0 :fw_ref_frame), fw_mode, uv, (directforward ?1 :0));
    }
    if (bw_mode || (direct && (bw_ref_frame!=-1)) ||(direct && !img->picture_structure && (bw_ref_frame ==-1))) 
    {
        //OneComponentChromaPrediction4x4 (bw_pred, pic_pix_x, pic_pix_y, bmv_array,           -1, bw_mode, uv, 0 );
        if(fw_mode && bw_mode)
            OneComponentChromaPrediction4x4_dir (bw_pred, pic_pix_x, pic_pix_y, fmv_array, 
            (!img->picture_structure) ? -2+bw_ref_frame : -1, bw_mode, uv,fw_ref_frame);
        else
            OneComponentChromaPrediction4x4 (bw_pred, pic_pix_x, pic_pix_y, bmv_array, 
            (!img->picture_structure) ? -2+bw_ref_frame : -1, bw_mode, uv,0);
    }

    if (direct || (fw_mode && bw_mode))
    {
        if(!img->picture_structure)
            bw_ref_frame=1-bw_ref_frame;   //cjw used for bw reference buffer address changing 20060112 bw

        fw_ref_num=(img->picture_structure)?(fw_ref_frame):(2*fw_ref_frame);  //cjw 20060112 fw 
        bw_ref_num=(img->picture_structure)?(bw_ref_frame+1):(2*bw_ref_frame+1);  //cjw 20060112 bw

        fw_scale = img->chroma_scale[fw_ref_num];
        fw_shift = img->chroma_shift[fw_ref_num];
        bw_scale = img->chroma_scale[bw_ref_num];
        bw_shift = img->chroma_shift[bw_ref_num];
        for (j=block_y; j<block_y4; j++)
            for (i=block_x; i<block_x4; i++){
                img->mpr[i][j] = (*fpred + *bpred + 1) / 2;

                if(img->LumVarFlag == 1)  //cjw 20051219 Weighted Predition
                    img->mpr_weight[i][j] = (Clip1((((*fpred)*fw_scale+16)>>5) + fw_shift)
                    + Clip1((((*bpred)*bw_scale+16)>>5) + bw_shift) + 1) / 2;

                fpred++ ; 
                bpred++ ;
            }
    }
    else if (fw_mode || skipped)  //P fw and B one direction fw
    {

        if(img->type==B_IMG){
            fw_ref_num=(img->picture_structure)?(fw_ref_frame):(2*fw_ref_frame);  //cjw 20060112 B fw 

            fw_scale = img->chroma_scale[fw_ref_num]; 
            fw_shift = img->chroma_shift[fw_ref_num];
        }
        else{   //P fw 
            fw_scale = img->chroma_scale[fw_ref_frame];
            fw_shift = img->chroma_shift[fw_ref_frame];
        }

        for (j=block_y; j<block_y4; j++)
            for (i=block_x; i<block_x4; i++){  
                img->mpr[i][j] = *fpred;

                if((!skipped) &&(img->LumVarFlag == 1))  //cjw 20051219 Weighted Predition
                    img->mpr_weight[i][j] = Clip1(((((*fpred)*fw_scale+16)>>5) + fw_shift));

                fpred++;
            }
    }
    else  //B bw 
    {
        if(!img->picture_structure)
            bw_ref_frame=1-bw_ref_frame;

        bw_ref_num=(img->picture_structure)?(bw_ref_frame+1):(2*bw_ref_frame+1);  //cjw 20060112 bw

        bw_scale = img->chroma_scale[bw_ref_num];
        bw_shift = img->chroma_shift[bw_ref_num];
        for (j=block_y; j<block_y4; j++)
            for (i=block_x; i<block_x4; i++){  
                img->mpr[i][j] = *bpred;

                if(img->LumVarFlag == 1)  //cjw 20051219 Weighted Predition
                    img->mpr_weight[i][j] = Clip1(((((*bpred)*bw_scale+16)>>5) + bw_shift));

                bpred++;
            }
    }
}

/*
*************************************************************************
* Function:Chroma residual coding for an macroblock
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void ChromaResidualCoding (int* cr_cbp)
{
    int   uv, block8, block_y, block_x, j, i,k;
    int   fw_mode, bw_mode, refframe;
    int   skipped = (img->mb_data[img->current_mb_nr].mb_type == 0 && img->type == INTER_IMG);
    int   incr = 1, offset = 0; // For MB level field/frame coding 
    int   incr_x = 1, offset_x = 0; 
    int   bw_ref;
    short   tmp_block_88[8][8];
    int   b8;     //wzm,422
    int   tmp_cbp_blk;
    int   tmp_block[2][8][8];/*lgp*/
    int   tmp_mpr[2][8][8];/*lgp*/
    int   incr_y=1,off_y=0;/*lgp*/
    int   cbpc=0;/*lgp*/
    int   stage_block8x8_pos=0;/*lgp*/

    //cjw 20051219
    Macroblock *currMB   = &img->mb_data[img->current_mb_nr];
    int IntraPrediction=IS_INTRA (currMB);
    int  direct;

    int   block8x8_idx[4][4] =   //wzm,422
    { {0, 1, 0, 0}, 
    {0, 1, 0, 0}, 
    {2, 3, 0, 0},
    {2, 3, 0, 0}  
    };  

    currMB->cbp01=0;  //wzm,422

    if(input->chroma_format==2) { //wzm,422
        for (*cr_cbp=0, uv=0; uv<2; uv++)
        {
            //===== prediction of chrominance blocks ===d==
            b8 = 0;
            for (block_y=0/*lgp*/; block_y<16; block_y+=4)  
                for (block_x=0; block_x<8; block_x+=4)
                {

                    b8 = block8x8_idx[block_y>>2][block_x>>2];    


                    SetModesAndRefframe (b8, &fw_mode, &bw_mode, &refframe, &bw_ref);

                    ChromaPrediction4x4 (uv, block_x, block_y, fw_mode, bw_mode, refframe, bw_ref);

                    direct    = (fw_mode == 0 && bw_mode == 0 && (img->type==B_IMG));

                    if (skipped||img->NoResidueDirect)
                    {
                        for (j=block_y; j<block_y+4; j++)
                            for (i=block_x; i<block_x+4; i++)
                            {
                                imgUV[uv][img->pix_c_y+j][img->pix_c_x+i] = img->mpr[i][j];
                            }
                    }
                    else
                    {
                        //X ZHENG, 422
                        if(k==1 && refframe<0) //The intra reference pixels for block 6&7 haven't been available at the moment. 
                            continue;

                        for (j=block_y; j<block_y+4; j++)
                            for (i=block_x; i<block_x+4; i++)
                            {
                                img->m7[i][j] = imgUV_org[uv][img->pix_c_y+j][img->pix_c_x+i] - img->mpr[i][j];                          
                            }
                    }
                }


                //===== DCT, Quantization, inverse Quantization, IDCT, and Reconstruction =====
                //===== Call function for skip mode in SP frames to properly process frame ====

                if ( !skipped&&!img->NoResidueDirect)
                {
                    for(k=0;k<2;k++)
                    {
                        //X ZHENG, 422
                        if(k==1 && refframe<0) //The intra reference pixels for block 6&7 haven't been available at the moment. 
                            continue;

                        for (j=0;j<8; j++)
                            for (i=0;i<8; i++)
                            {
                                tmp_block_88[j][i] = img->m7[i][j+8*k];
                            }
                            transform_B8(tmp_block_88);
                            scanquant_B8   (QP_SCALE_CR[ img->qp-MIN_QP], 4, 4+2*k+uv, tmp_block_88,0,cr_cbp,&tmp_cbp_blk);
                            //Lou 1013   scanquant_B8 (QP_SCALE_CR[ img->qp+QP_OFS-MIN_QP],4,4+uv,tmp_block_88,0,cr_cbp,&tmp_cbp_blk);
                    }                
                }

        }

        //block 6&7's intra mode decision and intra coding.
        //added by X ZHENG,422
        if(refframe<0)
            IntraChromaPrediction8x8_422(NULL, NULL, NULL);
        for (uv=0; uv<2; uv++)
        {
            if(refframe>=0)
                continue;      
            for (block_y=8; block_y<16; block_y+=4)  
                for (block_x=0; block_x<8; block_x+=4)
                {
                    IntraChromaPrediction4x4 (uv, block_x, block_y);

                    for (j=block_y; j<block_y+4; j++)
                        for (i=block_x; i<block_x+4; i++)
                        {
                            img->m7[i][j] = imgUV_org[uv][img->pix_c_y+j][img->pix_c_x+i] - img->mpr[i][j];                          
                        }        
                }
                for (j=0;j<8; j++)
                    for (i=0;i<8; i++)
                    {
                        tmp_block_88[j][i] = img->m7[i][j+8];
                    }  
                    transform_B8(tmp_block_88);
                    scanquant_B8   (QP_SCALE_CR[ img->qp-MIN_QP], 4, 6+uv, tmp_block_88,0,cr_cbp,&tmp_cbp_blk);
        }
        //X ZHENG,422

        img->mb_data[img->current_mb_nr].cbp +=(*cr_cbp); 
    } 
    else { //wzm,422
        for (*cr_cbp=0, uv=0; uv<2; uv++)
        {
            //===== prediction of chrominance blocks ===d==
            block8 = 0;
            for (block_y=/*0*/4*(stage_block8x8_pos/2)/*lgp*/; block_y<8; block_y+=4)
                for (block_x=0; block_x<8; block_x+=4, block8++)
                {
                    block8 = (block_y/4)*2 + block_x/4;/*lgp*/

                    SetModesAndRefframe (block8, &fw_mode, &bw_mode, &refframe, &bw_ref);

                    ChromaPrediction4x4 (uv, block_x, block_y, fw_mode, bw_mode, refframe, bw_ref);

                    direct    = (fw_mode == 0 && bw_mode == 0 && (img->type==B_IMG));

                    // !! start shenyanfei 
                    //if(((!IntraPrediction)&&/*cjw*/(!direct)&&(!skipped)&&(img->LumVarFlag == 1) && (img->mb_weighting_flag == 1)&&(img->weighting_prediction == 1))
                    //if(((!IntraPrediction)&&(!skipped)&&(!direct)&&(img->LumVarFlag == 1) && (img->mb_weighting_flag == 1)&&(img->weighting_prediction == 1)) //cjw 20051230
                    if(((!IntraPrediction)&& (!skipped) && (img->LumVarFlag == 1) && (img->mb_weighting_flag == 1)&&(img->weighting_prediction == 1))  //cjw 20060321
                        ||((img->LumVarFlag == 1) && (img->mb_weighting_flag == 0))){
                            for (j=0; j<4; j++)
                                for (i=0; i<4; i++)
                                    //cjw 20051219 Weighted Predition
                                    //img->mpr[i+block_x][j+block_y] = img->mpr[i+block_x][j+block_y];
                                    img->mpr[i+block_x][j+block_y] = img->mpr_weight[i+block_x][j+block_y]; 
                    }
                    // !! end shenyanfei 

                    if (skipped||img->NoResidueDirect)
                    {
                        for (j=0; j<4; j++)
                            for (i=block_x; i<block_x+4; i++)
                            {
                                imgUV[uv][img->pix_c_y+block_y+incr_y*j+off_y][img->pix_c_x+i] = img->mpr[i][block_y+j];
                            }
                    }
                    else
                        for (j=0; j<4; j++)
                            for (i=block_x; i<block_x+4; i++)
                            {
                                tmp_mpr[uv][i][block_y+j] = img->mpr[i][block_y+j];
                                tmp_block[uv][block_y+j][i] = imgUV_org[uv][img->pix_c_y+block_y+incr_y*j+off_y][img->pix_c_x+i] - img->mpr[i][block_y+j];
                                img->m7[i][block_y+j] = imgUV_org[uv][img->pix_c_y+block_y+incr_y*j+off_y][img->pix_c_x+i] - img->mpr[i][block_y+j];            
                            }
                }

                //===== DCT, Quantization, inverse Quantization, IDCT, and Reconstruction =====
                //===== Call function for skip mode in SP frames to properly process frame ====

                if ( !skipped&&!img->NoResidueDirect)
                {
                    for (j=0;j<8; j++)
                        for (i=0;i<8; i++)
                        {
                            tmp_block_88[j][i] = img->m7[i][j];
                        }
                        transform_B8(tmp_block_88);
                        //Lou 1013   scanquant_B8 (QP_SCALE_CR[ img->qp+QP_OFS-MIN_QP],4,4+uv,tmp_block_88,0,cr_cbp,&tmp_cbp_blk);
                        scanquant_B8   (QP_SCALE_CR[ img->qp-MIN_QP], 4, 4+uv, tmp_block_88,0,cr_cbp,&tmp_cbp_blk);          
                }
        }

        //===== update currMB->cbp =====
        img->mb_data[img->current_mb_nr].cbp +=(*cr_cbp);//((*cr_cbp)<<4); /*lgp*dct*/
    } //wzm,422
}

/* added by X ZHENG,422
*************************************************************************
* Function:A simplified implmention of 4:2:2 intra chroma 8x8 block's
prediction and mode decision.
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/
void IntraChromaPrediction8x8_422 (int *mb_up, int *mb_left, int*mb_up_left)
{
    Macroblock *currMB = &img->mb_data[img->current_mb_nr];
    unsigned char edgepixu[40]= {0};
#define EPU (edgepixu+20)
    unsigned char edgepixv[40]= {0};
#define EPV (edgepixv+20)
    int last_pix,new_pix;
    int bs_x=8;
    int bs_y=8;
    int x, y;

    int     i, j, k;
    pel_t** image;
    int     block_x, block_y, b4;
    int     img_cx            = img->pix_c_x;
    int     img_cy            = img->pix_c_y+8;
    int     img_cx_1          = img->pix_c_x-1;
    int     img_cx_4          = img->pix_c_x+4;
    int     img_cy_1          = img->pix_c_y-1+8;
    int     img_cy_4          = img->pix_c_y+4+8;
    int     b8_x              = img->pix_c_x/4;
    int     b8_y              = (img->pix_c_y+8)/4;
    int     mb_nr             = img->current_mb_nr;
    int     mb_width          = img->width/16;
    int  mb_available_up_right=0;
    int mb_available_left_down=((img_cx==0)||(b8_y>=(img->height_cr/BLOCK_SIZE-2))) ? 0 : (img->mb_data[img->current_mb_nr].slice_nr == img->mb_data[img->current_mb_nr+mb_width-1].slice_nr);
    int mb_available_up   = 1;
    int mb_available_left = (img_cx == 0) ? 0 : (img->mb_data[img->current_mb_nr].slice_nr == img->mb_data[img->current_mb_nr-1].slice_nr);
    int mb_available_up_left = (img_cx/BLOCK_SIZE == 0) ? 0 : (img->mb_data[img->current_mb_nr].slice_nr == img->mb_data[img->current_mb_nr-1].slice_nr);

    //changed by oliver 0512

    int     ih,iv;
    int     ib,ic,iaa;
    int     uv;
    int     hline[8], vline[8];
    int     mode;
    int     best_mode = DC_PRED_8;         //just an initilaization here, should always be overwritten
    int     cost;
    int     min_cost;
    int     diff[16];
    int     incr_y=1,off_y=0;/*lgp*/
    int     stage_block8x8_pos=0;/*lgp*/

    if (mb_up)
        *mb_up = mb_available_up;
    if (mb_left)
        *mb_left = mb_available_left;
    if( mb_up_left )
        *mb_up_left = mb_available_up_left;

    // compute all chroma intra prediction modes for both U and V

    uv=0;

    if(mb_available_up)
    {
        for(x=0;x<bs_x;x++)
            EPU[x+1]=imgUV[uv][img_cy-1][img_cx+x];

        /*
        for(x=0;x<bs_y;x++)
        EPU[1+x+bs_x]=EPU[bs_x];*/

        if(mb_available_up_right){
            for(x=0;x<bs_y;x++)
                EPU[1+x+bs_x]=imgUV[uv][img_cy-1][img_cx+bs_x+x];
        }
        else{
            for(x=0;x<bs_y;x++)  
                EPU[1+x+bs_x]=EPU[bs_x];  //bs_x=8; EPU[9~16]=r[8]  
        }
        //by oliver according to 1658

        EPU[0]=imgUV[uv][img_cy-1][img_cx];
    }
    if(mb_available_left)
    {
        for(y=0;y<bs_y;y++)
            EPU[-1-y]=imgUV[uv][img_cy+y][img_cx-1];

        for(y=0;y<bs_x;y++)
            EPU[-1-y-bs_y]=EPU[-bs_y];

        EPU[0]=imgUV[uv][img_cy][img_cx-1];
    }
    if(mb_available_up&&mb_available_left)
        EPU[0]=imgUV[uv][img_cy-1][img_cx-1];

    //lowpass (Those emlements that are not needed will not disturb)
    last_pix=EPU[-8];
    for(i=-8;i<=8;i++)
    {
        new_pix=( last_pix + (EPU[i]<<1) + EPU[i+1] + 2 )>>2;
        last_pix=EPU[i];
        EPU[i]=(unsigned char)new_pix;
    }


    uv=1;

    if(mb_available_up)
    {
        for(x=0;x<bs_x;x++)
            EPV[x+1]=imgUV[uv][img_cy-1][img_cx+x];

        /*
        for(x=0;x<bs_y;x++)
        EPV[1+x+bs_x]=EPV[bs_x];*/

        if(mb_available_up_right){
            for(x=0;x<bs_y;x++)
                EPV[1+x+bs_x]=imgUV[uv][img_cy-1][img_cx+bs_x+x];
        }
        else{
            for(x=0;x<bs_y;x++)  
                EPV[1+x+bs_x]=EPV[bs_x];  //bs_x=8; EPV[9~16]=r[8]  
        }
        //by oliver according to 1658

        EPV[0]=imgUV[uv][img_cy-1][img_cx];
    }
    if(mb_available_left)
    {
        for(y=0;y<bs_y;y++)
            EPV[-1-y]=imgUV[uv][img_cy+y][img_cx-1];

        for(y=0;y<bs_x;y++)
            EPV[-1-y-bs_y]=EPV[-bs_y];

        EPV[0]=imgUV[uv][img_cy][img_cx-1];
    }
    if(mb_available_up&&mb_available_left)
        EPV[0]=imgUV[uv][img_cy-1][img_cx-1];

    //lowpass (Those emlements that are not needed will not disturb)
    last_pix=EPV[-8];
    for(i=-8;i<=8;i++)
    {
        new_pix=( last_pix + (EPV[i]<<1) + EPV[i+1] + 2 )>>2;
        last_pix=EPV[i];
        EPV[i]=(unsigned char)new_pix;
    }

    // compute all chroma intra prediction modes for both U and V
    for (uv=0; uv<2; uv++)
    {
        image = imgUV[uv];

        // DC prediction
        for (block_y=/*0*/4*(stage_block8x8_pos/2)+8; block_y<16; block_y+=4)
            for (block_x=0; block_x<8; block_x+=4)
            {
                if (uv==0)
                {  
                    if (!mb_available_up && !mb_available_left)  
                        for (j=block_y; j<block_y+4; j++)
                            for (i=block_x; i<block_x+4; i++)
                            {
                                img->mprr_c[uv][DC_PRED_8][i][j] = 128;
                            }
                            if (mb_available_up && !mb_available_left)  
                                for (j=block_y; j<block_y+4; j++)
                                    for (i=block_x; i<block_x+4; i++)
                                    {
                                        img->mprr_c[uv][DC_PRED_8][i][j] = EPU[1+i];
                                    }
                                    if (!mb_available_up && mb_available_left)
                                        for (j=block_y; j<block_y+4; j++)
                                            for (i=block_x; i<block_x+4; i++)
                                            {
                                                img->mprr_c[uv][DC_PRED_8][i][j] = EPU[-1-j+8];
                                            }
                                            if (mb_available_up && mb_available_left)  
                                                for (j=block_y; j<block_y+4; j++)
                                                    for (i=block_x; i<block_x+4; i++)
                                                    {
                                                        img->mprr_c[uv][DC_PRED_8][i][j] = (EPU[1+i]+EPU[-1-j+8])>>1;
                                                    }
                }
                if (uv==1)
                {  
                    if (!mb_available_up && !mb_available_left)  
                        for (j=block_y; j<block_y+4; j++)
                            for (i=block_x; i<block_x+4; i++)
                            {
                                img->mprr_c[uv][DC_PRED_8][i][j] = 128;
                            }
                            if (mb_available_up && !mb_available_left)  
                                for (j=block_y; j<block_y+4; j++)
                                    for (i=block_x; i<block_x+4; i++)
                                    {
                                        img->mprr_c[uv][DC_PRED_8][i][j] = EPV[1+i];
                                    }
                                    if (!mb_available_up && mb_available_left)
                                        for (j=block_y; j<block_y+4; j++)
                                            for (i=block_x; i<block_x+4; i++)
                                            {
                                                img->mprr_c[uv][DC_PRED_8][i][j] = EPV[-1-j+8];
                                            }
                                            if (mb_available_up && mb_available_left)  
                                                for (j=block_y; j<block_y+4; j++)
                                                    for (i=block_x; i<block_x+4; i++)
                                                    {
                                                        img->mprr_c[uv][DC_PRED_8][i][j] = (EPV[1+i]+EPV[-1-j+8])>>1;
                                                    }
                }

                // vertical prediction
                if (mb_available_up)
                {
                    for (i=block_x; i<block_x+4; i++)
                        hline[i] = image[img_cy_1][img_cx+i];

                    for (j=block_y; j<block_y+4; j++)
                        for (i=block_x; i<block_x+4; i++)
                            img->mprr_c[uv][VERT_PRED_8][i][j] = hline[i];
                }

                // horizontal prediction
                if (mb_available_left)
                {
                    for (j=0; j<4; j++)
                        vline[block_y+j-8] = image[img_cy+block_y+incr_y*j+off_y/*lgp*/-8][img_cx_1];
                    for (j=block_y; j<block_y+4; j++)
                        for (i=block_x; i<block_x+4; i++)
                            img->mprr_c[uv][HOR_PRED_8][i][j] = vline[j-8];
                }        
            }

            // plane prediction
            if (mb_available_up_left)
            {
                ih = 4*(hline[7] - image[img_cy_1][img_cx_1]);
                iv = 4*(vline[7] - image[img_cy_1][img_cx_1]);
                for (i=1;i<4;i++)
                {
                    ih += i*(hline[3+i] - hline[3-i]);
                    iv += i*(vline[3+i] - vline[3-i]);
                }
                ib=(17*ih+16)>>5;
                ic=(17*iv+16)>>5;

                iaa=16*(hline[7]+vline[7]);

                for (j=0; j<8; j++)
                    for (i=0; i<8; i++)
                        img->mprr_c[uv][PLANE_8][i][j+8]=max(0,min(255,(iaa+(i-3)*ib +(j-3)*ic + 16)/32));// store plane prediction
            }
    }

    //The following is a simplified implementation of block 6&7's intra mode decision in the case of 422 coding
    //The implementation doesn't calculate block 6&7's coding bits. It will select the block 6&7's coding mode
    //before RD-Optimization. 
    min_cost = 1<<20;
    for (mode=DC_PRED_8; mode<=PLANE_8; mode++)
    {
        if ((mode==VERT_PRED_8 && !mb_available_up) ||
            (mode==HOR_PRED_8 && !mb_available_left) ||
            (mode==PLANE_8 && (!mb_available_left || !mb_available_up || !mb_available_up_left)))
            continue;

        cost = 0;
        for (uv=0; uv<2; uv++)
        {
            image = imgUV_org[uv];

            for (b4=0,block_y=/*0*/4*(stage_block8x8_pos/2); block_y<8; block_y+=4)
                for (block_x=0; block_x<8; block_x+=4,b4++)
                {
                    for (k=0,j=block_y; j<block_y+4; j++)
                        for (i=block_x; i<block_x+4; i++,k++)
                        {
                            diff[k] = image[img_cy+/*j*/incr_y*j + off_y/*lgp*/][img_cx+i] - img->mprr_c[uv][mode][i][j+8];
                        }
                        cost += SATD(diff, input->hadamard);
                }
        }

        if (cost < min_cost)
        {
            best_mode = mode;
            min_cost = cost;
        }
    }
    currMB->c_ipred_mode_2 = best_mode;  
}

/*
*************************************************************************
* Function:Predict an intra chroma 8x8 block
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void IntraChromaPrediction8x8 (int *mb_up, int *mb_left, int*mb_up_left)
{
    Macroblock *currMB = &img->mb_data[img->current_mb_nr];
    unsigned char edgepixu[40]= {0};
#define EPU (edgepixu+20)
    unsigned char edgepixv[40]= {0};
#define EPV (edgepixv+20)
    int last_pix,new_pix;
    int bs_x=8;
    int bs_y=8;
    int x, y;

    int     i, j, k;
    pel_t** image;
    int     block_x, block_y, b4;
    int     img_cx            = img->pix_c_x;
    int     img_cy            = img->pix_c_y;
    int     img_cx_1          = img->pix_c_x-1;
    int     img_cx_4          = img->pix_c_x+4;
    int     img_cy_1          = img->pix_c_y-1;
    int     img_cy_4          = img->pix_c_y+4;
    int     b8_x              = img->pix_c_x/4;
    int     b8_y              = img->pix_c_y/4;
    int     mb_nr             = img->current_mb_nr;
    int     mb_width          = img->width/16;
    /*
    int     mb_available_up   = (img_cy/BLOCK_SIZE == 0 || (img_cy/BLOCK_SIZE >0 && img->ipredmode[1+b8_x][1+b8_y-1]<0)) ? 0 : 1;
    int     mb_available_left = (img_cx/BLOCK_SIZE == 0 || (img_cx/BLOCK_SIZE >0 && img->ipredmode[1+b8_x - 1][1+b8_y]<0)) ? 0 : 1;
    int     mb_available_up_left = (img_cx/BLOCK_SIZE == 0 || img_cy/BLOCK_SIZE == 0 || (img_cy/BLOCK_SIZE >0 && img->ipredmode[1+b8_x][1+b8_y-1]<0) || 
    (img_cx/BLOCK_SIZE >0 && img->ipredmode[1+b8_x - 1][1+b8_y]<0)) ? 0 : 1;
    int     mb_available_up_right= (img_cy > 0)&&(b8_x<(img->width_cr/BLOCK_SIZE-2))&&(img->ipredmode[1+b8_x+1][1+b8_y-1]>=0);
    int    mb_available_left_down=(img_cx > 0)&&(b8_y<(img->height_cr/BLOCK_SIZE-2))&&(img->ipredmode[1+b8_x - 1][1+b8_y+1]>=0);
    //by oliver according to 1658*/
    /***********************************/
    int  mb_available_up_right=((img_cy==0)||(b8_x>=(img->width_cr/BLOCK_SIZE-2))) ? 0 : (img->mb_data[img->current_mb_nr].slice_nr == img->mb_data[img->current_mb_nr-mb_width+1].slice_nr);
    int  mb_available_left_down=((img_cx==0)||(b8_y>=(img->height_cr/BLOCK_SIZE-2))) ? 0 : (img->mb_data[img->current_mb_nr].slice_nr == img->mb_data[img->current_mb_nr+mb_width-1].slice_nr);
    int mb_available_up   = (img_cy == 0) ? 0 : (img->mb_data[img->current_mb_nr].slice_nr == img->mb_data[img->current_mb_nr-mb_width].slice_nr);
    int mb_available_left = (img_cx == 0) ? 0 : (img->mb_data[img->current_mb_nr].slice_nr == img->mb_data[img->current_mb_nr-1].slice_nr);
    int mb_available_up_left = (img_cx/BLOCK_SIZE == 0 || img_cy/BLOCK_SIZE == 0 ) ? 0 : (img->mb_data[img->current_mb_nr].slice_nr == img->mb_data[img->current_mb_nr-mb_width-1].slice_nr);

    //changed by oliver 0512

    int     ih,iv;
    int     ib,ic,iaa;
    int     uv;
    int     hline[8], vline[8];
    int     mode;
    int     best_mode = DC_PRED_8;         //just an initilaization here, should always be overwritten
    int     cost;
    int     min_cost;
    int     diff[16];
    int     incr_y=1,off_y=0;/*lgp*/
    int     stage_block8x8_pos=0;/*lgp*/



    if (mb_up)
        *mb_up = mb_available_up;
    if (mb_left)
        *mb_left = mb_available_left;
    if( mb_up_left )
        *mb_up_left = mb_available_up_left;

    // compute all chroma intra prediction modes for both U and V

    uv=0;

    if(mb_available_up)
    {
        for(x=0;x<bs_x;x++)
            EPU[x+1]=imgUV[uv][img_cy-1][img_cx+x];

        /*
        for(x=0;x<bs_y;x++)
        EPU[1+x+bs_x]=EPU[bs_x];*/

        if(mb_available_up_right){
            for(x=0;x<bs_y;x++)
                EPU[1+x+bs_x]=imgUV[uv][img_cy-1][img_cx+bs_x+x];
        }
        else{
            for(x=0;x<bs_y;x++)  
                EPU[1+x+bs_x]=EPU[bs_x];  //bs_x=8; EPU[9~16]=r[8]  
        }
        //by oliver according to 1658

        EPU[0]=imgUV[uv][img_cy-1][img_cx];
    }
    if(mb_available_left)
    {
        for(y=0;y<bs_y;y++)
            EPU[-1-y]=imgUV[uv][img_cy+y][img_cx-1];

        for(y=0;y<bs_x;y++)
            EPU[-1-y-bs_y]=EPU[-bs_y];

        EPU[0]=imgUV[uv][img_cy][img_cx-1];
    }
    if(mb_available_up&&mb_available_left)
        EPU[0]=imgUV[uv][img_cy-1][img_cx-1];

    //lowpass (Those emlements that are not needed will not disturb)
    last_pix=EPU[-8];
    for(i=-8;i<=8;i++)
    {
        new_pix=( last_pix + (EPU[i]<<1) + EPU[i+1] + 2 )>>2;
        last_pix=EPU[i];
        EPU[i]=(unsigned char)new_pix;
    }


    uv=1;

    if(mb_available_up)
    {
        for(x=0;x<bs_x;x++)
            EPV[x+1]=imgUV[uv][img_cy-1][img_cx+x];

        /*
        for(x=0;x<bs_y;x++)
        EPV[1+x+bs_x]=EPV[bs_x];*/

        if(mb_available_up_right){
            for(x=0;x<bs_y;x++)
                EPV[1+x+bs_x]=imgUV[uv][img_cy-1][img_cx+bs_x+x];
        }
        else{
            for(x=0;x<bs_y;x++)  
                EPV[1+x+bs_x]=EPV[bs_x];  //bs_x=8; EPV[9~16]=r[8]  
        }
        //by oliver according to 1658

        EPV[0]=imgUV[uv][img_cy-1][img_cx];
    }
    if(mb_available_left)
    {
        for(y=0;y<bs_y;y++)
            EPV[-1-y]=imgUV[uv][img_cy+y][img_cx-1];

        for(y=0;y<bs_x;y++)
            EPV[-1-y-bs_y]=EPV[-bs_y];

        EPV[0]=imgUV[uv][img_cy][img_cx-1];
    }
    if(mb_available_up&&mb_available_left)
        EPV[0]=imgUV[uv][img_cy-1][img_cx-1];

    //lowpass (Those emlements that are not needed will not disturb)
    last_pix=EPV[-8];
    for(i=-8;i<=8;i++)
    {
        new_pix=( last_pix + (EPV[i]<<1) + EPV[i+1] + 2 )>>2;
        last_pix=EPV[i];
        EPV[i]=(unsigned char)new_pix;
    }

    // compute all chroma intra prediction modes for both U and V
    for (uv=0; uv<2; uv++)
    {
        image = imgUV[uv];

        // DC prediction
        for (block_y=/*0*/4*(stage_block8x8_pos/2); block_y<8; block_y+=4)
            for (block_x=0; block_x<8; block_x+=4)
            {
                if (uv==0)
                {  
                    if (!mb_available_up && !mb_available_left)  
                        for (j=block_y; j<block_y+4; j++)
                            for (i=block_x; i<block_x+4; i++)
                            {
                                img->mprr_c[uv][DC_PRED_8][i][j] = 128;
                            }
                            if (mb_available_up && !mb_available_left)  
                                for (j=block_y; j<block_y+4; j++)
                                    for (i=block_x; i<block_x+4; i++)
                                    {
                                        img->mprr_c[uv][DC_PRED_8][i][j] = EPU[1+i];
                                    }
                                    if (!mb_available_up && mb_available_left)
                                        for (j=block_y; j<block_y+4; j++)
                                            for (i=block_x; i<block_x+4; i++)
                                            {
                                                img->mprr_c[uv][DC_PRED_8][i][j] = EPU[-1-j];
                                            }
                                            if (mb_available_up && mb_available_left)  
                                                for (j=block_y; j<block_y+4; j++)
                                                    for (i=block_x; i<block_x+4; i++)
                                                    {
                                                        img->mprr_c[uv][DC_PRED_8][i][j] = (EPU[1+i]+EPU[-1-j])>>1;
                                                    }
                }
                if (uv==1)
                {  
                    if (!mb_available_up && !mb_available_left)  
                        for (j=block_y; j<block_y+4; j++)
                            for (i=block_x; i<block_x+4; i++)
                            {
                                img->mprr_c[uv][DC_PRED_8][i][j] = 128;
                            }
                            if (mb_available_up && !mb_available_left)  
                                for (j=block_y; j<block_y+4; j++)
                                    for (i=block_x; i<block_x+4; i++)
                                    {
                                        img->mprr_c[uv][DC_PRED_8][i][j] = EPV[1+i];
                                    }
                                    if (!mb_available_up && mb_available_left)
                                        for (j=block_y; j<block_y+4; j++)
                                            for (i=block_x; i<block_x+4; i++)
                                            {
                                                img->mprr_c[uv][DC_PRED_8][i][j] = EPV[-1-j];
                                            }
                                            if (mb_available_up && mb_available_left)  
                                                for (j=block_y; j<block_y+4; j++)
                                                    for (i=block_x; i<block_x+4; i++)
                                                    {
                                                        img->mprr_c[uv][DC_PRED_8][i][j] = (EPV[1+i]+EPV[-1-j])>>1;
                                                    }
                }

                // vertical prediction
                if (mb_available_up)
                {
                    for (i=block_x; i<block_x+4; i++)
                        hline[i] = image[img_cy_1][img_cx+i];

                    for (j=block_y; j<block_y+4; j++)
                        for (i=block_x; i<block_x+4; i++)
                            img->mprr_c[uv][VERT_PRED_8][i][j] = hline[i];
                }

                // horizontal prediction
                if (mb_available_left)
                {
                    for (j=0; j<4; j++)
                        vline[block_y+j] = image[img_cy+block_y+incr_y*j+off_y/*lgp*/][img_cx_1];
                    for (j=block_y; j<block_y+4; j++)
                        for (i=block_x; i<block_x+4; i++)
                            img->mprr_c[uv][HOR_PRED_8][i][j] = vline[j];
                }        
            }

            // plane prediction
            if (mb_available_up_left)
            {
                ih = 4*(hline[7] - image[img_cy_1][img_cx_1]);
                iv = 4*(vline[7] - image[img_cy_1][img_cx_1]);
                for (i=1;i<4;i++)
                {
                    ih += i*(hline[3+i] - hline[3-i]);
                    iv += i*(vline[3+i] - vline[3-i]);
                }
                ib=(17*ih+16)>>5;
                ic=(17*iv+16)>>5;

                iaa=16*(hline[7]+vline[7]);

                for (j=0; j<8; j++)
                    for (i=0; i<8; i++)
                        img->mprr_c[uv][PLANE_8][i][j]=max(0,min(255,(iaa+(i-3)*ib +(j-3)*ic + 16)/32));// store plane prediction
            }
    }

    if (!input->rdopt) // the rd-opt part does not work correctly (see encode_one_macroblock)
    {                  // since ipredmodes could be overwritten => encoder-decoder-mismatches
        // pick lowest cost prediction mode
        min_cost = 1<<20;
        for (mode=DC_PRED_8; mode<=PLANE_8; mode++)
        {
            if ((mode==VERT_PRED_8 && !mb_available_up) ||
                (mode==HOR_PRED_8 && !mb_available_left) ||
                (mode==PLANE_8 && (!mb_available_left || !mb_available_up || !mb_available_up_left)))
                continue;

            cost = 0;
            for (uv=0; uv<2; uv++)
            {
                image = imgUV_org[uv];

                for (b4=0,block_y=/*0*/4*(stage_block8x8_pos/2); block_y<8; block_y+=4)
                    for (block_x=0; block_x<8; block_x+=4,b4++)
                    {
                        for (k=0,j=block_y; j<block_y+4; j++)
                            for (i=block_x; i<block_x+4; i++,k++)
                            {
                                diff[k] = image[img_cy+/*j*/incr_y*j + off_y/*lgp*/][img_cx+i] - img->mprr_c[uv][mode][i][j];
                            }
                            cost += SATD(diff, input->hadamard);
                    }
            }

            if (cost < min_cost)
            {
                best_mode = mode;
                min_cost = cost;
            }
        }
        currMB->c_ipred_mode = best_mode;
    }

}

/*lgp*/
/*
16x8_direct  0
16x8_fwd     1
16x8_bwd     2
16x8_sym     3
8x8          4
intra        5
*/
/*
*************************************************************************
* Function: Converts macroblock type to coding value
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int SubMBType2Value (Macroblock* currMB,int layer)
{
    int mbtype;

    if (img->type!=B_IMG)
    {
        if (currMB->mb_type==4)
            currMB->mb_type = P8x8;

        if (currMB->mb_type==I4MB)
            return (img->type==INTRA_IMG ? 0 : 5);

        else if (currMB->mb_type==P8x8)
        {       
            return 4;
        }
        else
            return currMB->mb_type;
    }
    else
    {
        if(currMB->mb_type==4)
            currMB->mb_type = P8x8;

        mbtype = currMB->mb_type;

        if      (mbtype==0)
            return 0;
        else if (mbtype==I4MB)
            return 5;
        else if (mbtype==P8x8)
            return 4;
        else if (mbtype==2)
            return 1 + currMB->b8pdir[2*layer];
        else 
            return 0;

    }
}
/*lgp*/

/*
*************************************************************************
* Function:Converts macroblock type to coding value
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int MBType2Value (Macroblock* currMB)
{
    static const int dir1offset[3]    =  { 1,  2, 3};
    static const int dir2offset[3][3] = {{ 0,  4,  8},   // 1. block forward
    { 6,  2, 10},   // 1. block backward
    {12, 14, 16}};  // 1. block bi-directional


    int mbtype, pdir0, pdir1;

    if (img->type!=B_IMG)
    {
        //this statment maybe have no use   xfwang 2004.7.29
        if (currMB->mb_type==4)
            currMB->mb_type = P8x8;
        //this sentence :img->type==INTRA_IMG  may have no use   //xfwang 2004.7.29
        if (currMB->mb_type==I4MB)
            return (img->type==INTRA_IMG ? 0 : 5)+NCBP[currMB->cbp][0];// qhg;  //modify by xfwang 2004.7.29

        else if (currMB->mb_type==P8x8)
        {         
            return 4;
        }
        else
            return currMB->mb_type;
    }
    else
    {
        if(currMB->mb_type==4)
            currMB->mb_type = P8x8;

        mbtype = currMB->mb_type;
        pdir0  = currMB->b8pdir[0];
        pdir1  = currMB->b8pdir[3];

        if      (mbtype==0)
            return 0;
        else if (mbtype==I4MB)
            return 23+NCBP[currMB->cbp][0];// qhg;
        else if (mbtype==P8x8)
            return 22;
        else if (mbtype==1)
            return dir1offset[pdir0];
        else if (mbtype==2)
            return 4 + dir2offset[pdir0][pdir1];
        else
            return 5 + dir2offset[pdir0][pdir1];

    }
}

/*
*************************************************************************
* Function:Writes intra prediction modes for an 8x8 block
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int writeIntra4x4Modes(int only_this_block)
{
    int i;
    int block8x8;
    int rate;
    int ipred_array[16],cont_array[16],ipred_number;
    Macroblock    *currMB     = &img->mb_data[img->current_mb_nr];
    SyntaxElement *currSE     = &img->MB_SyntaxElements[currMB->currSEnr];
    int           *bitCount   = currMB->bitcounter;

    ipred_number=0;

    for(block8x8=0;block8x8<4;block8x8++)
    {
        if( currMB->b8mode[block8x8]==IBLOCK && (only_this_block<0||only_this_block==block8x8) )
        {
            ipred_array[ipred_number]=currMB->intra_pred_modes[block8x8];
            cont_array[ipred_number]=block8x8;
            ipred_number++;
        }
    }

    rate=0;

    for(i=0;i<ipred_number;i++)
    {
        currMB->IntraChromaPredModeFlag = 1;
        currSE->context = cont_array[i];
        currSE->value1  = ipred_array[i];

#if TRACE
        snprintf(currSE->tracestring, TRACESTRING_SIZE, "Intra mode     = %3d %d",currSE->value1,currSE->context);
#endif

        /*--- set symbol type and function pointers ---*/
        currSE->type = SE_INTRAPREDMODE;

        /*--- encode and update rate ---*/
        writeSyntaxElement_Intra4x4PredictionMode(currSE, currBitStream);
        bitCount[BITS_COEFF_Y_MB]+=currSE->len;
        rate += currSE->len;
        currSE++;
        currMB->currSEnr++;
    }

    return rate;
}

/*
*************************************************************************
* Function:Converts 8x8 block tyoe to coding value
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int B8Mode2Value (int b8mode, int b8pdir)
{
    static const int b8start[8] = {0,0,0,0, 1, 4, 5, 10};
    static const int b8inc  [8] = {0,0,0,0, 1, 2, 2, 1};

    if (img->type!=B_IMG)
    {
        return (b8mode-4);
    }
    else
    {
        return b8start[b8mode] + b8inc[b8mode] * b8pdir;
    }

}

/*
*************************************************************************
* Function: Codes macroblock header
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int writeMBHeader (int rdopt)  // GB CHROMA !!!!!!!!
{
    int             i;
    int             mb_nr     = img->current_mb_nr;
    Macroblock*     currMB    = &img->mb_data[mb_nr];
    SyntaxElement  *currSE    = &img->MB_SyntaxElements[currMB->currSEnr];
    int*            bitCount  = currMB->bitcounter;
    int             no_bits   = 0;
    int             mb_x      = img->mb_x;
    int             mb_y      = img->mb_y;
    //  int             skip      = currMB->mb_type ? 0:((img->type==B_IMG) ? !currMB->cbp:1); //wzm,422
    int        skip;    //wzm,422
    int        tempcbp; //wzm,422

    //wzm,422
    if(input->chroma_format==2)
    {
        skip= currMB->mb_type ? 0:((img->type==B_IMG) ? (!currMB->cbp||!currMB->cbp01):1); //add by wuzhongmou 422
        if (currMB->cbp != 0||currMB->cbp01!=0)
            tempcbp = 1;
        else 
            tempcbp =  0;
    }
    else
    {
        skip= currMB->mb_type ? 0:((img->type==B_IMG) ? !currMB->cbp:1);
        if(tempcbp=(currMB->cbp != 0))
            tempcbp=1;
        else
            tempcbp=0;
    }
    //wzm,422

    currMB->IntraChromaPredModeFlag = IS_INTRA(currMB);
    currMB->mb_field = img->field_mode;

    //=====  BITS FOR MACROBLOCK MODE =====
    if(img->type == INTRA_IMG)//GB
    {
    }
    else if (input->skip_mode_flag)
    {    
        if (currMB->mb_type != 0 || ((img->type==B_IMG) && (tempcbp))) //wzm,422
        {
            //===== Run Length Coding: Non-Skipped macorblock =====
            //Ð´ÈëÌø¹ýºê¿é
            currSE->value1  = img->cod_counter;
            currSE->mapping = ue_linfo;
            currSE->type    = SE_MBTYPE;
            writeSyntaxElement_UVLC( currSE, currBitStream);

#if TRACE
            snprintf(currSE->tracestring, TRACESTRING_SIZE, "MB runlength = %3d",img->cod_counter);
#endif

            bitCount[BITS_MB_MODE] += currSE->len;
            no_bits                += currSE->len;
            currSE++;
            currMB->currSEnr++;

            // Reset cod counter
            img->cod_counter = 0;

            // Put out mb mode
            currSE->value1  = MBType2Value (currMB);

            if (img->type!=B_IMG)
            {
                currSE->value1--;
            }
            currSE->mapping = ue_linfo;
            currSE->type    = SE_MBTYPE;
            writeSyntaxElement_UVLC( currSE, currBitStream);

#if TRACE
            if (img->type==B_IMG)
                snprintf(currSE->tracestring, TRACESTRING_SIZE, "B_MB mode(%2d,%2d) = %3d",img->mb_x, img->mb_y, currMB->mb_type);
            else
                snprintf(currSE->tracestring, TRACESTRING_SIZE,   "MB mode(%2d,%2d) = %3d",img->mb_x, img->mb_y,currMB->mb_type);
#endif

            bitCount[BITS_MB_MODE] += currSE->len;
            no_bits                += currSE->len;
            currSE++;
            currMB->currSEnr++;
        }
        else
        {
            //Run Length Coding: Skipped macroblock
            img->cod_counter++;

            // if(img->current_mb_nr == img->total_number_mb) //Yulj 2004.0715
            if ( img->current_mb_nr == img->mb_no_currSliceLastMB )  //Yulj 2004.0715
            {
                // Put out run
                currSE->value1  = img->cod_counter;
                currSE->mapping = ue_linfo;
                currSE->type    = SE_MBTYPE;

                writeSyntaxElement_UVLC( currSE, currBitStream);

#if TRACE
                snprintf(currSE->tracestring, TRACESTRING_SIZE, "MB runlength = %3d",img->cod_counter);
#endif

                bitCount[BITS_MB_MODE] += currSE->len;
                no_bits                += currSE->len;
                currSE++;
                currMB->currSEnr++;

                // Reset cod counter
                img->cod_counter = 0;
            }
        }
    }
    else
    {
        // Put out mb mode
        currSE->value1  = MBType2Value (currMB);

        if (img->type!=B_IMG)
        {
            currSE->value1--;
        }

        if (currMB->mb_type == 0 && ((img->type!=B_IMG) || currMB->cbp == 0))
            currSE->value1 = 0;
        else
            currSE->value1++;

        currSE->mapping = ue_linfo;
        currSE->type    = SE_MBTYPE;
        writeSyntaxElement_UVLC( currSE, currBitStream);

#if TRACE
        if (img->type==B_IMG)
            snprintf(currSE->tracestring, TRACESTRING_SIZE, "B_MB mode(%2d,%2d) = %3d",img->mb_x, img->mb_y, currMB->mb_type);
        else
            snprintf(currSE->tracestring, TRACESTRING_SIZE,   "MB mode(%2d,%2d) = %3d",img->mb_x, img->mb_y,currMB->mb_type);
#endif

        bitCount[BITS_MB_MODE] += currSE->len;
        no_bits                += currSE->len;
        currSE++;
        currMB->currSEnr++;
    }

    //===== BITS FOR 8x8 SUB-PARTITION MODES =====
    if (IS_P8x8 (currMB))
    { 
        if(img->type!=INTER_IMG)
        {
            for (i=0; i<4; i++)
            {
                //mb_part_type is fix length coding(fix length equal 2)!!   jlzheng 7.22
                currSE->value1  =  currSE->bitpattern = B8Mode2Value (currMB->b8mode[i], currMB->b8pdir[i]);
                currSE->type    = SE_MBTYPE;
                currSE->len     = 2;
                writeSyntaxElement_fixed(currSE, currBitStream);
                // END

#if TRACE
                snprintf(currSE->tracestring, TRACESTRING_SIZE, "8x8 mode/pdir(%2d) = %3d/%d",
                    i,currMB->b8mode[i],currMB->b8pdir[i]);
#endif

                bitCount[BITS_MB_MODE]+= currSE->len;
                no_bits               += currSE->len;
                currSE++;
                currMB->currSEnr++;
            }
        }
    }

    if(!currMB->IntraChromaPredModeFlag&&!rdopt) //GB CHROMA !!!!!
        currMB->c_ipred_mode = DC_PRED_8; //setting c_ipred_mode to default is not the right place here
    //resetting in rdopt.c (but where ??)
    //with cabac and bframes maybe it could crash without this default
    //since cabac needs the right neighborhood for the later MBs

    if(IS_INTRA(currMB))
    {
        for (i=0; i<4; i++)
        {
            currSE->context = i;
            currSE->value1  = currMB->intra_pred_modes[i];

#if TRACE
            snprintf(currSE->tracestring, TRACESTRING_SIZE, "Intra mode     = %3d %d",currSE->value1,currSE->context);
#endif

            /*--- set symbol type and function pointers ---*/
            currSE->type = SE_INTRAPREDMODE;


            /*--- encode and update rate ---*/
            writeSyntaxElement_Intra4x4PredictionMode(currSE, currBitStream);
            bitCount[BITS_MB_MODE]+=currSE->len;
            no_bits += currSE->len;
            currSE++;
            currMB->currSEnr++;
        }

        currSE->mapping = ue_linfo;      
        currSE->value1 = currMB->c_ipred_mode;
        currSE->type = SE_INTRAPREDMODE;
        writeSyntaxElement_UVLC(currSE, currBitStream);
        bitCount[BITS_MB_MODE] += currSE->len;
        no_bits                    += currSE->len;
#if TRACE
        snprintf(currSE->tracestring, TRACESTRING_SIZE, "Chroma intra pred mode %d", currMB->c_ipred_mode);
#endif
        currSE++;
        currMB->currSEnr++;

        //X ZHENG, chroma 422
        if (input->chroma_format==2) {
            currSE->mapping = ue_linfo;      
            currSE->value1 = currMB->c_ipred_mode_2;
            currSE->type = SE_INTRAPREDMODE;
            writeSyntaxElement_UVLC(currSE, currBitStream);
            bitCount[BITS_MB_MODE] += currSE->len;
            no_bits                += currSE->len;
#if TRACE
            snprintf(currSE->tracestring, TRACESTRING_SIZE, "Chroma intra pred mode 422 %d", currMB->c_ipred_mode_2);
#endif
            currSE++;
            currMB->currSEnr++;
        }

    }

    return no_bits;
}

/*
*************************************************************************
* Function:Write chroma intra prediction mode.
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int writeChromaIntraPredMode()
{
    Macroblock*     currMB    = &img->mb_data[img->current_mb_nr];
    SyntaxElement*  currSE    = &img->MB_SyntaxElements[currMB->currSEnr];
    int*            bitCount  = currMB->bitcounter;
    int             rate      = 0;

    //===== BITS FOR CHROMA INTRA PREDICTION MODES
    currSE->mapping = ue_linfo;

    currSE->value1 = currMB->c_ipred_mode;
    currSE->type = SE_INTRAPREDMODE;
    writeSyntaxElement_UVLC(currSE, currBitStream);
    bitCount[BITS_COEFF_UV_MB] += currSE->len;
    rate                    += currSE->len;

#if TRACE
    snprintf(currSE->tracestring, TRACESTRING_SIZE, "Chroma intra pred mode");
#endif

    currSE++;
    currMB->currSEnr++;

    return rate;
}


/*
*************************************************************************
* Function:Sets context for reference frame parameter
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int BType2CtxRef (int btype)
{
    if (btype<4)   return 0;
    else           return 1;
}

/*
*************************************************************************
* Function:Writes motion vectors of an 8x8 block
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int writeMotionVector8x8 (int  i0,
                          int  j0,
                          int  i1,
                          int  j1,
                          int  refframe,
                          int  dmv_flag,
                          int  fwd_flag,
                          int  mv_mode)
{
    int            i, j, k, l, m;
    int            curr_mvd;
    int            bwflag      = ((refframe<0 || (!fwd_flag))?1:0);
    int            rate        = 0;
    int            step_h      = input->blc_size[mv_mode][0] >> 3;
    int            step_v      = input->blc_size[mv_mode][1] >> 3;
    Macroblock*    currMB      = &img->mb_data[img->current_mb_nr];
    SyntaxElement* currSE      = &img->MB_SyntaxElements[currMB->currSEnr];
    int*           bitCount    = currMB->bitcounter;
    int            refindex    = (refframe<0 ? 0 : refframe);
    int*****       all_mv      = (fwd_flag ? img->all_mv : img->all_bmv);
    int*****       pred_mv     = ((img->type!=B_IMG) ? img->mv : (fwd_flag ? img->p_fwMV : img->p_bwMV));

    if (!fwd_flag) bwflag = 1;

    for (j=j0; j<j1; j+=step_v)
        for (i=i0; i<i1; i+=step_h)
        {
            for (k=0; k<2; k++) 
            {
                curr_mvd = all_mv[i][j][refindex][mv_mode][k] - pred_mv[i][j][refindex][mv_mode][k];

                //--- store (oversampled) mvd ---
                for (l=0; l < step_v; l++) 
                    for (m=0; m < step_h; m++)    currMB->mvd[bwflag][j+l][i+m][k] = curr_mvd;

                currSE->value1 = curr_mvd;
                currSE->type   = ((img->type==B_IMG) ? SE_BFRAME : SE_MVD);
                currSE->mapping = se_linfo;

                writeSyntaxElement_UVLC(currSE, currBitStream);

#if TRACE
                {
                    if (fwd_flag)
                    {
                        snprintf(currSE->tracestring, TRACESTRING_SIZE, "FMVD(%d) = %3d  (org_mv %3d pred_mv %3d) %d",k, curr_mvd, all_mv[i][j][refindex][mv_mode][k], pred_mv[i][j][refindex][mv_mode][k],currSE->value2);
                    }
                    else
                    {
                        snprintf(currSE->tracestring, TRACESTRING_SIZE, "BMVD(%d) = %3d  (org_mv %3d pred_mv %3d)",k, curr_mvd, all_mv[i][j][refindex][mv_mode][k], pred_mv[i][j][refindex][mv_mode][k]);
                    }
                }
#endif

                bitCount[BITS_INTER_MB] += currSE->len;
                rate                    += currSE->len;
                currSE++;  
                currMB->currSEnr++;
            }
        }

        return rate;
}

/*
*************************************************************************
* Function:Writes motion vectors of an 8x8 block
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int
writeMotionVector8x8_bid (int  i0,
                          int  j0,
                          int  i1,
                          int  j1,
                          int  refframe,
                          int  dmv_flag,
                          int  fwd_flag,
                          int  mv_mode,
                          int  pdir)
{
    int            i, j, k, l, m;
    int            curr_mvd;
    int            bwflag     = ((refframe<0 || (!fwd_flag))?1:0);
    int            rate       = 0;
    int            step_h     = input->blc_size[mv_mode][0] >> 3;
    int            step_v     = input->blc_size[mv_mode][1] >> 3;
    Macroblock*    currMB     = &img->mb_data[img->current_mb_nr];
    SyntaxElement* currSE     = &img->MB_SyntaxElements[currMB->currSEnr];
    int*           bitCount   = currMB->bitcounter;
    int            refindex   = (refframe<0 ? 0 : refframe);
    int*****       all_mv     = (fwd_flag ? img->all_mv : img->all_bmv);
    int*****       pred_mv    = ((img->type!=B_IMG) ? img->mv : (fwd_flag ? img->p_fwMV : img->p_bwMV));
    if (!fwd_flag) bwflag = 1;

    if(pdir == 2 && mv_mode != 0)
    {
        //sw 10.1
        all_mv = img->all_omv;
        pred_mv = img->omv;
    }

    for (j=j0; j<j1; j+=step_v)
        for (i=i0; i<i1; i+=step_h)
        {
            if(img->nb_references>1 && img->type!=B_IMG)
            {
                currSE->value1 = refindex;
                currSE->type   = SE_REFFRAME;//Attention

                currSE->mapping = se_linfo;

                writeSyntaxElement_UVLC(currSE, currBitStream);
#if TRACE
                {
                    snprintf(currSE->tracestring, TRACESTRING_SIZE, "Reference Index = %d", refindex);
                }
#endif
                bitCount[BITS_INTER_MB] += currSE->len;
                rate                    += currSE->len;
                currSE++;  
                currMB->currSEnr++;
            }

            for (k=0; k<2; k++) 
            {

                curr_mvd = all_mv[i][j][refindex][mv_mode][k] - pred_mv[i][j][refindex][mv_mode][k];

                //--- store (oversampled) mvd ---
                for (l=0; l < step_v; l++) 
                    for (m=0; m < step_h; m++)    currMB->mvd[bwflag][j+l][i+m][k] = curr_mvd;

                currSE->value1 = curr_mvd;
                currSE->type   = (img->type==B_IMG ? SE_BFRAME : SE_MVD);

                currSE->mapping = se_linfo;
                writeSyntaxElement_UVLC(currSE, currBitStream);
#if TRACE
                if (fwd_flag)
                {
                    snprintf(currSE->tracestring, TRACESTRING_SIZE, "FMVD(%d) = %3d  (org_mv %3d pred_mv %3d) %d",k, curr_mvd, all_mv[i][j][refindex][mv_mode][k], pred_mv[i][j][refindex][mv_mode][k],currSE->value2);
                }
                else
                {
                    snprintf(currSE->tracestring, TRACESTRING_SIZE, "BMVD(%d) = %3d  (org_mv %3d pred_mv %3d)",k, curr_mvd, all_mv[i][j][refindex][mv_mode][k], pred_mv[i][j][refindex][mv_mode][k]);
                }
#endif
                bitCount[BITS_INTER_MB] += currSE->len;
                rate                    += currSE->len;
                currSE++;  
                currMB->currSEnr++;
            }
        }

        return rate;
}

/*
*************************************************************************
* Function:Writes Luma Coeff of an 8x8 block
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int writeLumaCoeff8x8 (int block8x8, int intra4x4mode)
{
    int  rate = 0;
    Macroblock*     currMB    = &img->mb_data[img->current_mb_nr];
    SyntaxElement*  currSE    = &img->MB_SyntaxElements[currMB->currSEnr];

    rate = writeLumaCoeffAVS_B8(block8x8,intra4x4mode);

    return rate;
}

/*
*************************************************************************
* Function:Writes CBP, DQUANT, and Luma Coefficients of an macroblock
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int writeCBPandLumaCoeff ()
{
    int             i;
    int             rate      = 0;
    Macroblock*     currMB    = &img->mb_data[img->current_mb_nr];
    int*            bitCount  = currMB->bitcounter;
    SyntaxElement*  currSE    = &img->MB_SyntaxElements[currMB->currSEnr];
    int             cbp       = currMB->cbp;//((((currMB->cbp)&0x300)>>4) + ((currMB->cbp)&0xf));

    int*  DCLevel = img->cofDC[0][0];
    int*  DCRun   = img->cofDC[0][1];
    int   mb_xpos = img->mb_x;
    int   mb_ypos = img->mb_y;

    //=====   C B P   =====
    //---------------------

    //=====  L U M I N A N C E   =====
    //--------------------------------
    for (i=0; i<4; i++) 
    {
        if( currMB->b8mode[i]==IBLOCK && i < 4)
        {
            currSE->context = i;
            currSE->value1  = currMB->intra_pred_modes[i];

#if TRACE
            snprintf(currSE->tracestring, TRACESTRING_SIZE, "Intra mode     = %3d %d",currSE->value1,currSE->context);
#endif

            /*--- set symbol type and function pointers ---*/
            currSE->type = SE_INTRAPREDMODE;


            /*--- encode and update rate ---*/
            writeSyntaxElement_Intra4x4PredictionMode(currSE, currBitStream);
            bitCount[BITS_COEFF_Y_MB]+=currSE->len;
            rate += currSE->len;
            currSE++;
            currMB->currSEnr++;
        }

        if (cbp & (1<<i))
        {  
            rate += writeLumaCoeff8x8 (i, (currMB->b8mode[i]==IBLOCK));
        }
    }

    return rate;
}

int
StoreMotionVector8x8 (int  i0,
                      int  j0,
                      int  i1,
                      int  j1,
                      int  refframe,
                      int  dmv_flag,
                      int  fwd_flag,
                      int  mv_mode)
{
    int            i, j, k, l, m;
    int            curr_mvd;
    int            bwflag     = ((refframe<0 || (!fwd_flag))?1:0);
    int            rate       = 0;
    int            step_h     = input->blc_size[mv_mode][0] >> 3;
    int            step_v     = input->blc_size[mv_mode][1] >> 3;
    Macroblock*    currMB     = &img->mb_data[img->current_mb_nr];
    SyntaxElement* currSE     = &img->MB_SyntaxElements[currMB->currSEnr];
    int*           bitCount   = currMB->bitcounter;

    int            refindex   = (refframe<0 ? 0 : refframe);
    int*****       all_mv     = (fwd_flag ? img->all_mv : img->all_bmv);
    int*****       pred_mv    = ((img->type!=B_IMG) ? img->mv : (fwd_flag ? img->p_fwMV : img->p_bwMV));

    if (!fwd_flag) bwflag = 1;

    for (j=j0; j<j1; j+=step_v)
        for (i=i0; i<i1; i+=step_h)
        {
            for (k=0; k<2; k++) 
            {
                curr_mvd = all_mv[i][j][refindex][mv_mode][k] - pred_mv[i][j][refindex][mv_mode][k];

                //--- store (oversampled) mvd ---
                for (l=0; l < step_v; l++) 
                    for (m=0; m < step_h; m++)    currMB->mvd[bwflag][j+l][i+m][k] = curr_mvd;
            }
        }

        return 0;
}

int
StoreMotionVector8x8_bid (int  i0,
                          int  j0,
                          int  i1,
                          int  j1,
                          int  refframe,
                          int  dmv_flag,
                          int  fwd_flag,
                          int  mv_mode,
                          int  pdir)
{
    int            i, j, k, l, m;
    int            curr_mvd;
    int            bwflag     = ((refframe<0 || (!fwd_flag))?1:0);
    int            rate       = 0;
    int            step_h     = input->blc_size[mv_mode][0] >> 3;
    int            step_v     = input->blc_size[mv_mode][1] >> 3;
    Macroblock*    currMB     = &img->mb_data[img->current_mb_nr];
    SyntaxElement* currSE     = &img->MB_SyntaxElements[currMB->currSEnr];
    int*           bitCount   = currMB->bitcounter;

    int            refindex   = (refframe<0 ? 0 : refframe);
    int*****       all_mv     = (fwd_flag ? img->all_mv : img->all_bmv);
    int*****       pred_mv    = ((img->type!=B_IMG) ? img->mv : (fwd_flag ? img->p_fwMV : img->p_bwMV));
    if (!fwd_flag) bwflag = 1;

    if(pdir == 2 && mv_mode != 0)
    {
        all_mv = img->all_omv;
        pred_mv = img->omv;
    }

    for (j=j0; j<j1; j+=step_v)
        for (i=i0; i<i1; i+=step_h)
        {
            for (k=0; k<2; k++) 
            {
                curr_mvd = all_mv[i][j][refindex][mv_mode][k] - pred_mv[i][j][refindex][mv_mode][k];

                //--- store (oversampled) mvd ---
                for (l=0; l < step_v; l++) 
                    for (m=0; m < step_h; m++) 
                        currMB->mvd[bwflag][j+l][i+m][k] = curr_mvd;
            }
        }

        return 0;
}

/*
*************************************************************************
* Function:Writes motion vectors of an 8x8 block
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/


int
writeMVD8x8 (int  i0,
             int  j0,
             int  i1,
             int  j1,
             int  refframe,
             int  dmv_flag,
             int  fwd_flag,
             int  mv_mode)
{
    int            i, j, k;
    int            curr_mvd;
    int            bwflag     = ((refframe<0 || (!fwd_flag))?1:0);
    int            rate       = 0;
    int            step_h     = input->blc_size[mv_mode][0] >> 3;
    int            step_v     = input->blc_size[mv_mode][1] >> 3;
    Macroblock*    currMB     = &img->mb_data[img->current_mb_nr];
    SyntaxElement* currSE     = &img->MB_SyntaxElements[currMB->currSEnr];
    int*           bitCount   = currMB->bitcounter;

    int            refindex   = (refframe<0 ? 0 : refframe);
    int*****       all_mv     = (fwd_flag ? img->all_mv : img->all_bmv);
    int*****       pred_mv    = ((img->type!=B_IMG) ? img->mv : (fwd_flag ? img->p_fwMV : img->p_bwMV));

    if (!fwd_flag) 
        bwflag = 1;


    for (j=j0; j<j1; j+=step_v)
        for (i=i0; i<i1; i+=step_h)
        {
            for (k=0; k<2; k++) 
            {
                curr_mvd = currMB->mvd[bwflag][j][i][k];

                currSE->value1 = curr_mvd;
                currSE->type   = ((img->type==B_IMG) ? SE_BFRAME : SE_MVD);
                currSE->mapping = se_linfo;

                writeSyntaxElement_UVLC(currSE, currBitStream);
#if TRACE
                {
                    if (fwd_flag)
                    {
                        snprintf(currSE->tracestring, TRACESTRING_SIZE, "FMVD(%d) = %3d  (org_mv %3d pred_mv %3d) %d",k, curr_mvd, all_mv[i][j][refindex][mv_mode][k], pred_mv[i][j][refindex][mv_mode][k],currSE->value2);
                    }
                    else
                    {
                        snprintf(currSE->tracestring, TRACESTRING_SIZE, "BMVD(%d) = %3d  (org_mv %3d pred_mv %3d)",k, curr_mvd, all_mv[i][j][refindex][mv_mode][k], pred_mv[i][j][refindex][mv_mode][k]);
                    }
                }
#endif
                bitCount[BITS_INTER_MB] += currSE->len;
                rate                    += currSE->len;
                currSE++;  
                currMB->currSEnr++;
            }
        }

        return rate;
}

int writeCBPandDqp (int pos,int *rate_top,int *rate_bot)
{
    int             rate      = 0;
    Macroblock*     currMB    = &img->mb_data[img->current_mb_nr];
    int*            bitCount  = currMB->bitcounter;
    SyntaxElement*  currSE    = &img->MB_SyntaxElements[currMB->currSEnr];
    int             tempcbp;       //wzm,422
    if(input->chroma_format==2) {  //wzm,422  
        if(currMB->cbp||currMB->cbp01)
            tempcbp=1;
        else
            tempcbp=0;
    }
    else if(input->chroma_format==1){
        if(currMB->cbp)
            tempcbp=1;
        else
            tempcbp=0;
    } //wzm,422 

    //=====   C B P   =====
    //---------------------

    currSE->value1 = currMB->cbp; //CBP

    if (IS_OLDINTRA (currMB) || currMB->mb_type == SI4MB)
    {
        currSE->mapping = cbp_linfo_intra;
        currSE->type = SE_CBP_INTRA;
    }
    else
    {
        currSE->mapping = cbp_linfo_inter;
        currSE->type = SE_CBP_INTER;
    }
    if(img->type==INTRA_IMG||IS_INTER(currMB)) // qhg
    {
        writeSyntaxElement_UVLC(currSE, currBitStream);
        bitCount[BITS_CBP_MB] += currSE->len;
        rate                   = currSE->len;

#if TRACE
        snprintf(currSE->tracestring, TRACESTRING_SIZE, "CBP (%2d,%2d) = %3d",img->mb_x, img->mb_y, currMB->cbp);
#endif


        currSE++;  
        currMB->currSEnr++;
    }

    //=====   C B P 422  =====
    //---------------------
    //added by wzm,422
    if(input->chroma_format==2)
    {
        currSE->value1 = currMB->cbp01; //MbCBP422                 
        currSE->mapping = se_linfo;                             
        currSE->type =SE_CBP01;                                 
        writeSyntaxElement_UVLC(currSE, currBitStream);         
        bitCount[BITS_CBP01_MB] += currSE->len;                
        rate                   = currSE->len;              
#if TRACE
        snprintf(currSE->tracestring, TRACESTRING_SIZE, "CBP422 (%2d,%2d) = %3d",img->mb_x, img->mb_y, currMB->cbp01);
#endif
        currSE++;
        currMB->currSEnr++;
    }
    //wzm,422

    //=====   DQUANT   =====
    //----------------------
    if ((tempcbp)&&!input->fixed_picture_qp)     //wzm,422
    {
        currSE->value1 = currMB->delta_qp;

        currSE->mapping = se_linfo;

        if (IS_INTER (currMB))  currSE->type = SE_DELTA_QUANT_INTER;
        else                    currSE->type = SE_DELTA_QUANT_INTRA;

        writeSyntaxElement_UVLC(  currSE, currBitStream);
        bitCount[BITS_DELTA_QUANT_MB] += currSE->len;
        rate                          += currSE->len;
#if TRACE
        snprintf(currSE->tracestring, TRACESTRING_SIZE, "Delta QP (%2d,%2d) = %3d",img->mb_x, img->mb_y, currMB->delta_qp);
#endif
        // proceed to next SE
        currSE++;
        currMB->currSEnr++;
    }


    *rate_top += rate/2;
    *rate_bot += rate/2;

    return 0;

}


/*lgp*/
/*
*************************************************************************
* Function:Writes motion info
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int storeMotionInfo (int pos)
{
    int k, j0, i0, refframe;

    Macroblock*     currMB    = &img->mb_data[img->current_mb_nr];
    int             no_bits   = 0;

    int   bframe          = (img->type==B_IMG);
    int** refframe_array  = ((img->type==B_IMG) ? fw_refFrArr : refFrArr);
    int** bw_refframe_array  = bw_refFrArr;
    int   multframe       = (input->no_multpred>1); 
    int   step_h0         = (input->blc_size[(IS_P8x8(currMB))? 4 : currMB->mb_type][0] >> 3);
    int   step_v0         = (input->blc_size[(IS_P8x8(currMB))? 4 : currMB->mb_type][1] >> 3);
    int   b8_y     = img->block8_y;
    int   b8_x     = img->block8_x;

    //===== write forward motion vectors =====
    if (IS_INTERMV (currMB))
    {
        for (j0=pos; j0<2; j0+=step_v0)
            for (i0=0; i0<2; i0+=step_h0)
            {
                k=j0*2+i0;
                if ((currMB->b8pdir[k]==0 || currMB->b8pdir[k]==2) && currMB->b8mode[k]!=0)//has forward vector
                {
                    refframe  = refframe_array[b8_y+j0][b8_x+i0];

                    if(currMB->b8pdir[k]==2)  {  
                        StoreMotionVector8x8_bid(i0, j0, i0+step_h0, j0+step_v0, refframe, 0, 1, currMB->b8mode[k],currMB->b8pdir[k]);        }
                    else
                        StoreMotionVector8x8(i0, j0, i0+step_h0, j0+step_v0, refframe, 0, 1, currMB->b8mode[k]);
                }
            }
    }

    //===== write backward motion vectors =====
    if (IS_INTERMV (currMB) && bframe)
    {
        for (j0=pos; j0<2; j0+=step_v0)
            for (i0=0; i0<2; i0+=step_h0)
            {
                k=j0*2+i0;
                if ((currMB->b8pdir[k]==1 || currMB->b8pdir[k]==2) && currMB->b8mode[k]!=0)//has backward vector
                {
                    refframe  = bw_refframe_array[b8_y+j0][b8_x+i0];
                    StoreMotionVector8x8(i0, j0, i0+step_h0, j0+step_v0, refframe, 0, 0, currMB->b8mode[k]);
                }
            }
    }
    return no_bits;
}

int
writeFrameRef (int  mode,
               int  i,
               int  j,
               int  fwd_flag,
               int  ref)
{
    Macroblock*     currMB    = &img->mb_data[img->current_mb_nr];
    SyntaxElement*  currSE    = &img->MB_SyntaxElements[currMB->currSEnr];
    int*            bitCount  = currMB->bitcounter;
    int             rate      = 0;
    int             num_ref   = img->ptype== INTER_IMG ? 2:1;

    if( num_ref == 1 && img->picture_structure)
    {
        return 0;
    }

    currSE->value1 = ref;

    currSE->type   = ((img->type==B_IMG) ? SE_BFRAME : SE_REFFRAME);

    currSE->bitpattern = currSE->value1;

    if (!img->picture_structure && (img->type!=B_IMG))
        currSE->len = 2;
    else
        currSE->len = 1;
    writeSyntaxElement2Buf_Fixed(currSE, currBitStream);

    bitCount[BITS_INTER_MB] += currSE->len;
    rate                    += currSE->len;

#if TRACE
    if (fwd_flag)
    {
        snprintf(currSE->tracestring, TRACESTRING_SIZE, "Fwd Ref frame no %d", currSE->bitpattern);
    }
    else
    {
        snprintf(currSE->tracestring, TRACESTRING_SIZE, "Bwd Ref frame no %d", currSE->bitpattern);
    }

#endif

    currSE++;
    currMB->currSEnr++;

    return rate;
}

/*
*************************************************************************
* Function:Writes motion info
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int writeReferenceIndex (int pos,int *rate_top,int *rate_bot)
{
    int k, j0, i0, refframe;

    Macroblock*     currMB    = &img->mb_data[img->current_mb_nr];
    int             no_bits   = 0;

    int   bframe          = (img->type==B_IMG);
    int** refframe_array  = ((img->type==B_IMG) ? fw_refFrArr : refFrArr);
    int** bw_refframe_array  = bw_refFrArr;
    int   multframe       = (input->no_multpred>1); 
    int   step_h0  =0       ;
    int   step_v0  =0       ;
    int   b8_y     = img->block8_y;
    int   b8_x     = img->block8_x;

    if(currMB->mb_type!=I4MB)
    {        
        step_h0         = (input->blc_size[IS_P8x8(currMB) ? 4 : currMB->mb_type][0] >> 3);
        step_v0         = (input->blc_size[IS_P8x8(currMB) ? 4 : currMB->mb_type][1] >> 3);
    }

    //forward reference 
    for (j0=pos; j0<2; )  
    {
        if((currMB->mb_type==I4MB&&j0==0)) { j0 += 1; continue;}

        for (i0=0; i0<2; )
        {
            k=j0*2+i0;
            no_bits = 0;
            if ((currMB->b8pdir[k]==0 || currMB->b8pdir[k]==2) && currMB->b8mode[k]!=0 && img->types!=INTRA_IMG)// &&!ZeroRef (currMB))//has forward vector//delete by xfwang 2004.7.29
            {
                refframe  = refframe_array[b8_y+j0][b8_x+i0];
                if ( img->nb_references>1)
                    no_bits   = writeFrameRef (currMB->b8mode[k], i0, j0, 1, refframe);  
            }

            if(j0<1)
                *rate_top += no_bits;
            else
                *rate_bot += no_bits;
            i0+=max(1,step_h0);
        }
        j0+=max(1,step_v0);
    }

    //backward reference 
    if (bframe)
    {
        for (j0=pos; j0<2; )  
        {
            if((currMB->mb_type==I4MB&&j0==0)) { 
                j0 += 1; continue;
            }

            for (i0=0; i0<2; )
            {
                k=j0*2+i0;
                no_bits = 0;

                if ((currMB->b8pdir[k]==1 ) && currMB->b8mode[k]!=0)//has backward vector
                {
                    refframe  = bw_refframe_array[b8_y+j0][b8_x+i0];
                    if ( img->nb_references>1)
                    {
                        if(!img->picture_structure)
                        {
                            if(img->type == B_IMG)
                                refframe = 1 - refframe;
                        }
                        no_bits   = writeFrameRef (currMB->b8mode[k], i0, j0, 0, refframe);    
                    }
                }

                if(j0<1)
                    *rate_top += no_bits;
                else
                    *rate_bot += no_bits;
                i0+=max(1,step_h0);
            }
            j0+=max(1,step_v0);
        }
    }

    return 0;
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

int writeMVD (int pos,int *rate_top,int *rate_bot)
{
    int k, j0, i0, refframe;

    Macroblock*     currMB    = &img->mb_data[img->current_mb_nr];
    int             no_bits   = 0;

    int   bframe          = (img->type==B_IMG);
    int** refframe_array  = ((img->type==B_IMG) ? fw_refFrArr : refFrArr);
    int** bw_refframe_array  = bw_refFrArr;
    int   multframe       = (input->no_multpred>1); 
    int   step_h0         = (input->blc_size[IS_P8x8(currMB) ? 4 : currMB->mb_type][0] >> 3);
    int   step_v0         = (input->blc_size[IS_P8x8(currMB) ? 4 : currMB->mb_type][1] >> 3);
    int   b8_y     = img->block8_y;
    int   b8_x     = img->block8_x;

    if(currMB->mb_type!=I4MB)
    {        
        step_h0         = (input->blc_size[IS_P8x8(currMB) ? 4 : currMB->mb_type][0] >> 3);
        step_v0         = (input->blc_size[IS_P8x8(currMB) ? 4 : currMB->mb_type][1] >> 3);
    }

    for (j0=pos; j0<2; )
    { 
        if((currMB->mb_type==I4MB&&j0==0)) { j0 += 1; continue;}

        for (i0=0; i0<2; )
        {
            k=j0*2+i0;

            no_bits = 0;
            if ((currMB->b8pdir[k]==0 || currMB->b8pdir[k]==2) && currMB->b8mode[k]!=0)//has forward vector
            {
                refframe  = refframe_array[b8_y+j0][b8_x+i0];

                if(currMB->b8pdir[k]==2 && input->InterlaceCodingOption==FRAME_CODING)
                {  
                    //??????10.25
                    no_bits  += writeMotionVector8x8_bid (i0, j0, i0+step_h0, j0+step_v0, refframe, 0, 1, currMB->b8mode[k],currMB->b8pdir[k]);      
                }
                else 
                    no_bits   = writeMVD8x8 (i0, j0, i0+step_h0, j0+step_v0, refframe, 0, 1, currMB->b8mode[k]);
            }

            if(j0<1)
                *rate_top += no_bits;
            else
                *rate_bot += no_bits;

            i0+=max(1,step_h0);
        }
        j0+=max(1,step_v0);

    }

    if (bframe)
    {
        for (j0=pos; j0<2; )
        {
            if((currMB->mb_type==I4MB&&j0==0)) { j0 += 1; continue;}

            for (i0=0; i0<2; )
            {
                k=j0*2+i0;
                /*lgp*/
                no_bits = 0;

                if ((currMB->b8pdir[k]==1) && currMB->b8mode[k]!=0)//has backward vector
                {
                    refframe  = bw_refframe_array[b8_y+j0][b8_x+i0];
                    no_bits   = writeMVD8x8 (i0, j0, i0+step_h0, j0+step_v0, refframe, 0, 0, currMB->b8mode[k]);            
                }

                if(j0<1)
                    *rate_top += no_bits;
                else
                    *rate_bot += no_bits;

                i0+=max(1,step_h0);
            }
            j0+=max(1,step_v0); 
        }
    }

    return 0;
}

/*
*************************************************************************
* Function:Writes CBP, DQUANT, and Luma Coefficients of an macroblock
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int
writeBlockCoeff (int block8x8)
{
    int             rate      = 0;
    Macroblock*     currMB    = &img->mb_data[img->current_mb_nr];
    int*            bitCount  = currMB->bitcounter;
    SyntaxElement*  currSE    = &img->MB_SyntaxElements[currMB->currSEnr];


    //=====  L U M I N A N C E   =====
    //--------------------------------
    if(block8x8<6)      //wzm 422  
    {
        if (currMB->cbp & (1<<block8x8))
        {
            /*lgp*dct*/
            if(block8x8 < 4)
            {
                rate += writeLumaCoeffAVS_B8 (block8x8, (currMB->b8mode[block8x8]==IBLOCK));
            }
            else
            {
                rate +=  writeChromaCoeffAVS_B8(block8x8,IS_INTRA(currMB));
            }
        }
    }
    else if(block8x8==6 ||block8x8==7 )  //wzm,422
    {
        if((currMB->cbp01>>(block8x8-6))&1)                     
            rate += writeChromaCoeffAVS_B8(block8x8,IS_INTRA(currMB)); 
    }

    return rate;
}

// !! shenyanfei 
void writeMBweightflag()  //cjw 20051230
{
    int             mb_nr     = img->current_mb_nr;
    Macroblock*     currMB    = &img->mb_data[mb_nr];
    Bitstream *bitstream      = currBitStream;

    //cjw 20060326
    SyntaxElement*  currSE    = &img->MB_SyntaxElements[currMB->currSEnr];

    if(!IS_INTRA(currMB)){
        //if((img->LumVarFlag == 1)&&(img->mb_weighting_flag == 1)&&(!IS_COPY(currMB))&&(!IS_DIRECT(currMB)))
        if((img->LumVarFlag == 1)&&(img->mb_weighting_flag == 1)&&(!IS_COPY(currMB))) //cjw 20060321 skip no WP, direct WP
        {
            //u_v(1, "MB weighting_prediction",img->weighting_prediction,bitstream); //cjw 20060326
            // should be modified as follows for the trace file 
            ///////////////////////cjw 20060326/////////////////////////////  
            currSE->bitpattern = img->weighting_prediction;
            currSE->len = 1;
            currSE->type = SE_HEADER;
            currSE->value1 = img->weighting_prediction;
            writeUVLC2buffer(currSE, bitstream); 

#if TRACE
            snprintf(currSE->tracestring, TRACESTRING_SIZE, "MB weighting_prediction = %d",img->weighting_prediction);
#endif

            currSE++;
            currMB->currSEnr++;      
            ///////end cjw ///////////////////////////////////////////////
        }
    }
    return ;
}


/*
*************************************************************************
* Function:Passes the chosen syntax elements to the NAL
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void write_one_macroblock (int eos_bit)
{
    Macroblock* currMB   = &img->mb_data[img->current_mb_nr];
    int*        bitCount = currMB->bitcounter;
    int i;
    int mb_x = img->mb_x;
    int mb_y = img->mb_y;
    int dummy;/*lgp*/

    //--- write header ---
    writeMBHeader (0);

    //  Do nothing more if copy and inter mode
    if(input->chroma_format==2) //wzm,422
    {
        if ((IS_INTERMV (currMB)  || IS_INTRA (currMB)  ) ||
            ((img->type==B_IMG)&& (currMB->cbp != 0 ||currMB->cbp01 != 0)) )
        {
            writeReferenceIndex(0,&dummy,&dummy);
            writeMVD(0,&dummy,&dummy);
            writeMBweightflag(); // !! shenyanfei cjw 20051230
            writeCBPandDqp(0,&dummy,&dummy);
            for (i=0; i < 8; i++) //add by wuzhongmou 422
                writeBlockCoeff (i);
        }
        //--- set total bit-counter ---
        bitCount[BITS_TOTAL_MB] = bitCount[BITS_MB_MODE] + bitCount[BITS_COEFF_Y_MB]     + bitCount[BITS_INTER_MB]
        + bitCount[BITS_CBP_MB] +bitCount[BITS_CBP01_MB] + bitCount[BITS_DELTA_QUANT_MB] + bitCount[BITS_COEFF_UV_MB];
        stats.bit_slice += bitCount[BITS_TOTAL_MB];

        //Rate control
        img->NumberofMBHeaderBits=bitCount[BITS_MB_MODE]   + bitCount[BITS_INTER_MB]
        + bitCount[BITS_CBP_MB]+ bitCount[BITS_CBP01_MB]  + bitCount[BITS_DELTA_QUANT_MB];
        img->NumberofMBTextureBits= bitCount[BITS_COEFF_Y_MB]+ bitCount[BITS_COEFF_UV_MB];
        img->NumberofTextureBits +=img->NumberofMBTextureBits;
        img->NumberofHeaderBits +=img->NumberofMBHeaderBits;
        /*basic unit layer rate control*/
        if(img->BasicUnit<img->Frame_Total_Number_MB)
        {
            img->NumberofBasicUnitHeaderBits +=img->NumberofMBHeaderBits;
            img->NumberofBasicUnitTextureBits +=img->NumberofMBTextureBits;
        }

        //Rate control
        img->NumberofCodedMacroBlocks++;
    }
    else if(input->chroma_format==1) //wzm,422
    {
        if ((IS_INTERMV (currMB)  || IS_INTRA (currMB)  ) ||
            ((img->type==B_IMG)     && currMB->cbp != 0)  )
        {
            writeReferenceIndex(0,&dummy,&dummy);
            writeMVD(0,&dummy,&dummy);
            writeMBweightflag(); // !! shenyanfei cjw 20051230
            writeCBPandDqp(0,&dummy,&dummy);
            for (i=0; i < 6; i++)
                writeBlockCoeff (i);
        }


        //--- set total bit-counter ---
        bitCount[BITS_TOTAL_MB] = bitCount[BITS_MB_MODE] + bitCount[BITS_COEFF_Y_MB]     + bitCount[BITS_INTER_MB]
        + bitCount[BITS_CBP_MB]  + bitCount[BITS_DELTA_QUANT_MB] + bitCount[BITS_COEFF_UV_MB];
        stats.bit_slice += bitCount[BITS_TOTAL_MB];

        //Rate control
        img->NumberofMBHeaderBits=bitCount[BITS_MB_MODE]   + bitCount[BITS_INTER_MB]
        + bitCount[BITS_CBP_MB]  + bitCount[BITS_DELTA_QUANT_MB];
        img->NumberofMBTextureBits= bitCount[BITS_COEFF_Y_MB]+ bitCount[BITS_COEFF_UV_MB];
        img->NumberofTextureBits +=img->NumberofMBTextureBits;
        img->NumberofHeaderBits +=img->NumberofMBHeaderBits;
        /*basic unit layer rate control*/
        if(img->BasicUnit<img->Frame_Total_Number_MB)
        {
            img->NumberofBasicUnitHeaderBits +=img->NumberofMBHeaderBits;
            img->NumberofBasicUnitTextureBits +=img->NumberofMBTextureBits;
        }

        //Rate control
        img->NumberofCodedMacroBlocks++;
    }
}
