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
* Function: Decode a Macroblock
*
*************************************************************************************
*/



#include "contributors.h"

#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "global.h"
#include "mbuffer.h"
#include "elements.h"
#include "macroblock.h"
#include "vlc.h"

#include "defines.h"
#include "block.h"

void readReferenceIndex(struct img_par *img, struct inp_par *inp);
void readMotionVector(struct img_par *img, struct inp_par *inp);

#define MODE_IS_P8x8  (mbmode==4)
#define MODE_IS_I4x4  (mbmode==5)
#define I16OFFSET     (mbmode-7)
#define fwd_ref_idx_to_refframe(idx) ((idx)+fwd_refframe_offset)
#define bwd_ref_idx_to_refframe(idx) ((idx)+bwd_refframe_offset)

/*
******************************************************************************
*  Function: calculated field or frame distance between current field(frame)
*            and the reference field(frame).
*     Input: blkref
fw_bw
*    Output: distance for motion vector scale
*    Return:
* Attention:
*    Author: Yulejun // 2004.07.14
******************************************************************************
*/
// The funtion int calculate_distance(int blkref, int fw_bw ) is modified by Xiaozhen Zheng, HiSilicon, 20070327.
// At the modified version, 'img->tr' is replaced by 'picture_distance', which is used to calculate DistanceIndex and BlockDistance.
int calculate_distance(int blkref, int fw_bw )  //fw_bw>=0: forward ; fw_bw<0: backward
{
    int distance;
    if( img->picture_structure == 1 ) //frame coding
    {
        if ( img->type==P_IMG ) // P img
        {
            if(blkref==0)
                distance = picture_distance*2 - img->imgtr_last_P*2;
            else if(blkref==1)
                distance = picture_distance*2 - img->imgtr_last_prev_P*2;
            else
            {
                assert(0); //only two reference pictures for P frame
            }
        }
        else //B_IMG
        {
            if (fw_bw >=0 ) //forward
                distance = picture_distance*2 - img->imgtr_last_P*2;
            else
                distance = img->imgtr_next_P*2  - picture_distance*2;
        }
    }
    else if( ! img->picture_structure )  //field coding
    {
        if(img->type==P_IMG)
        {
            if(img->top_bot==0) //top field
            {
                switch ( blkref )
                {
                case 0:
                    distance = picture_distance*2 - img->imgtr_last_P*2 - 1 ;
                    break;
                case 1:
                    distance = picture_distance*2 - img->imgtr_last_P*2 ;
                    break;
                case 2:
                    distance = picture_distance*2 - img->imgtr_last_prev_P*2 - 1;
                    break;
                case 3:
                    distance = picture_distance*2 - img->imgtr_last_prev_P*2 ;
                    break;
                }
            }
            else if(img->top_bot==1) // bottom field.
            {
                switch ( blkref )
                {
                case 0:
                    distance = 1 ;
                    break;
                case 1:
                    distance = picture_distance*2 - img->imgtr_last_P*2 ;
                    break;
                case 2:
                    distance = picture_distance*2 - img->imgtr_last_P*2 + 1;
                    break;
                case 3:
                    distance = picture_distance*2 - img->imgtr_last_prev_P*2 ;
                    break;
                }
            }
            else
            {
                printf("Error. frame picture should not run into this branch.");
                exit(-1);
            }
        }
        else if(img->type==B_IMG)
        {
            assert(blkref==0 || blkref == 1);
            if (fw_bw >= 0 ) //forward
            {
                if(img->top_bot==0) //top field
                {
                    switch ( blkref )
                    {
                    case 0:
                        distance = picture_distance*2 - img->imgtr_last_P*2 - 1 ;
                        break;
                    case 1:
                        distance = picture_distance*2 - img->imgtr_last_P*2;
                        break;
                    }
                }
                else if(img->top_bot==1) // bottom field.
                {
                    switch ( blkref )
                    {
                    case 0:
                        distance = picture_distance*2 - img->imgtr_last_P*2 ;
                        break;
                    case 1:
                        distance = picture_distance*2 - img->imgtr_last_P*2 + 1;
                        break;
                    }
                }
                else
                {
                    printf("Error. frame picture should not run into this branch.");
                    exit(-1);
                }
            }
            else // backward
            {
                if(img->top_bot==0) //top field
                {
                    switch ( blkref )
                    {
                    case 0:
                        distance = img->imgtr_next_P*2 - picture_distance*2;
                        break;
                    case 1:
                        distance = img->imgtr_next_P*2 - picture_distance*2 + 1;
                        break;
                    }
                }
                else if(img->top_bot==1) // bottom field.
                {
                    switch ( blkref )
                    {
                    case 0:
                        distance = img->imgtr_next_P*2 - picture_distance*2 -  1;
                        break;
                    case 1:
                        distance = img->imgtr_next_P*2 - picture_distance*2 ;
                        break;
                    }
                }
                else
                {
                    printf("Error. frame picture should not run into this branch.");
                    exit(-1);
                }
            }

        }
    }
    distance = (distance+512)%512;
    return distance;
}

//The unit of time distance is calculated by field time
/*
*************************************************************************
* Function:
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/
int scale_motion_vector(int motion_vector, int currblkref, int neighbourblkref, int currsmbtype, int neighboursmbtype, int block_y_pos, int curr_block_y, int ref, int direct_mv)
{
    int sign = (motion_vector>0?1:-1);
    int mult_distance;
    int devide_distance;

    motion_vector = abs(motion_vector);

    if(motion_vector == 0)
        return 0;


    mult_distance = calculate_distance( currblkref, ref );
    devide_distance = calculate_distance(neighbourblkref, ref);

    motion_vector = sign*((motion_vector*mult_distance*(512/devide_distance)+256)>>9);

    return motion_vector;
}

/*
*************************************************************************
* Function:Set motion vector predictor
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/
static void SetMotionVectorPredictor (struct img_par  *img,
                                      int             *pmv_x,
                                      int             *pmv_y,
                                      int             ref_frame,
                                      int             **refFrArr,
                                      int             ***tmp_mv,
                                      int             block_x,
                                      int             block_y,
                                      int             blockshape_x,
                                      int             blockshape_y,
                                      int             ref,
                                      int             direct_mv)
{
    int mb_x                 = 8*block_x;
    int mb_y                 = 8*block_y;
    int pic_block_x          = img->block_x + block_x;
    int pic_block_y          = img->block_y + block_y;
    int mb_width             = img->width/16;
    int mb_nr = img->current_mb_nr;
    int mb_available_up   = (img->mb_y == 0 ) ? 0 : (mb_data[mb_nr].slice_nr == mb_data[mb_nr-mb_width  ].slice_nr);
    int mb_available_left = (img->mb_x == 0 ) ? 0 : (mb_data[mb_nr].slice_nr == mb_data[mb_nr-1         ].slice_nr);
    int mb_available_upleft  = (img->mb_x == 0) ? 0 : ((img->mb_y == 0) ? 0 :
        (mb_data[mb_nr].slice_nr == mb_data[mb_nr-mb_width-1].slice_nr));
    int mb_available_upright = (img->mb_y == 0) ? 0 : ((img->mb_x >= (mb_width-1)) ? 0 :
        (mb_data[mb_nr].slice_nr == mb_data[mb_nr-mb_width+1].slice_nr));

    int block_available_up, block_available_left, block_available_upright, block_available_upleft;
    int mv_a, mv_b, mv_c, mv_d, pred_vec=0;
    int mvPredType, rFrameL, rFrameU, rFrameUR;
    int hv;
    int mva[3] , mvb[3],mvc[3];
    int y_up = 1,y_upright=1,y_upleft=1,off_y=0;

    int rFrameUL;
    Macroblock*     currMB = &mb_data[img->current_mb_nr];
    int smbtypecurr, smbtypeL, smbtypeU, smbtypeUL, smbtypeUR;


    smbtypecurr = -2;
    smbtypeL = -2;
    smbtypeU = -2;
    smbtypeUL = -2;
    smbtypeUR = -2;


    /* D B C */
    /* A X   */

    /* 1 A, B, D are set to 0 if unavailable       */
    /* 2 If C is not available it is replaced by D */

    block_available_up   = mb_available_up   || (mb_y > 0);
    block_available_left = mb_available_left || (mb_x > 0);

    if (mb_y > 0)
    {
        if (mb_x < 8)  // first column of 8x8 blocks
        {
            if (mb_y==8)
            {
                if (blockshape_x == 16)      block_available_upright = 0;
                else                         block_available_upright = 1;
            }
            else
            {
                if (mb_x+blockshape_x != 8)  block_available_upright = 1;
                else                         block_available_upright = 0;
            }
        }
        else
        {
            if (mb_x+blockshape_x != 16)   block_available_upright = 1;
            else                           block_available_upright = 0;
        }
    }
    else if (mb_x+blockshape_x != MB_BLOCK_SIZE)
    {
        block_available_upright = block_available_up;
    }
    else
    {
        block_available_upright = mb_available_upright;
    }

    if (mb_x > 0)
    {
        block_available_upleft = (mb_y > 0 ? 1 : mb_available_up);
    }
    else if (mb_y > 0)
    {
        block_available_upleft = mb_available_left;
    }
    else
    {
        block_available_upleft = mb_available_upleft;
    }

    smbtypecurr = -2;
    smbtypeL = -2;
    smbtypeU = -2;
    smbtypeUL = -2;
    smbtypeUR = -2;

    mvPredType = MVPRED_MEDIAN;

    rFrameL    = block_available_left    ? refFrArr[pic_block_y]  [pic_block_x-1] : -1;
    rFrameU    = block_available_up      ? refFrArr[pic_block_y-1][pic_block_x]   : -1;
    rFrameUR   = block_available_upright ? refFrArr[pic_block_y-1][pic_block_x+blockshape_x/8] :
        block_available_upleft  ? refFrArr[pic_block_y-1][pic_block_x-1] : -1;
    rFrameUL   = block_available_upleft  ? refFrArr[pic_block_y-1][pic_block_x-1] : -1;

    if((rFrameL != -1)&&(rFrameU == -1)&&(rFrameUR == -1))
        mvPredType = MVPRED_L;
    else if((rFrameL == -1)&&(rFrameU != -1)&&(rFrameUR == -1))
        mvPredType = MVPRED_U;
    else if((rFrameL == -1)&&(rFrameU == -1)&&(rFrameUR != -1))
        mvPredType = MVPRED_UR;

    // Directional predictions
    else if(blockshape_x == 8 && blockshape_y == 16)
    {
        if(mb_x == 0)
        {
            if(rFrameL == ref_frame)
                mvPredType = MVPRED_L;
        }
        else
        {
            if( rFrameUR == ref_frame)
                mvPredType = MVPRED_UR;
        }
    }
    else if(blockshape_x == 16 && blockshape_y == 8)
    {
        if(mb_y == 0)
        {
            if(rFrameU == ref_frame)
                mvPredType = MVPRED_U;
        }
        else
        {
            if(rFrameL == ref_frame)
                mvPredType = MVPRED_L;
        }
    }

#define MEDIAN(a,b,c)  (a + b + c - min(a, min(b, c)) - max(a, max(b, c)));

    for (hv=0; hv < 2; hv++)
    {
        mva[hv] = mv_a = block_available_left    ? tmp_mv[4+pic_block_x-1             ][pic_block_y][hv] : 0;
        mvb[hv] = mv_b = block_available_up      ? tmp_mv[4+pic_block_x               ][pic_block_y-1][hv] : 0;
        mv_d = block_available_upleft  ? tmp_mv[4+pic_block_x-1][pic_block_y-1][hv] : 0;
        mvc[hv] = mv_c = block_available_upright ? tmp_mv[4+pic_block_x+blockshape_x/8][pic_block_y-1][hv] : mv_d;
        //--- Yulj 2004.07.14
        // mv_a, mv_b... are not scaled.
        mva[hv] = scale_motion_vector(mva[hv], ref_frame, rFrameL, smbtypecurr, smbtypeL, pic_block_y-off_y, pic_block_y, ref, direct_mv);
        mvb[hv] = scale_motion_vector(mvb[hv], ref_frame, rFrameU, smbtypecurr, smbtypeU, pic_block_y-y_up, pic_block_y, ref, direct_mv);
        mv_d = scale_motion_vector(mv_d, ref_frame, rFrameUL, smbtypecurr, smbtypeUL, pic_block_y-y_upleft, pic_block_y, ref, direct_mv);
        mvc[hv] = block_available_upright ? scale_motion_vector(mvc[hv], ref_frame, rFrameUR, smbtypecurr, smbtypeUR, pic_block_y-y_upright, pic_block_y, ref, direct_mv): mv_d;


        switch (mvPredType)
        {
        case MVPRED_MEDIAN:

            if(hv == 1){

                // !! for A

                mva[2] = abs(mva[0] - mvb[0])  + abs(mva[1] - mvb[1]);
                // !! for B

                mvb[2] = abs(mvb[0] - mvc[0]) + abs(mvb[1] - mvc[1]);
                // !! for C

                mvc[2] = abs(mvc[0] - mva[0])  + abs(mvc[1] - mva[1]);

                pred_vec = MEDIAN(mva[2],mvb[2],mvc[2]);

                if(pred_vec == mva[2]){
                    *pmv_x = mvc[0];
                    *pmv_y = mvc[1];
                }
                else if(pred_vec == mvb[2]){
                    *pmv_x = mva[0];
                    *pmv_y = mva[1];
                }
                else{
                    *pmv_x = mvb[0];
                    *pmv_y = mvb[1];
                }   //  END


            }
            break;
        case MVPRED_L:
            pred_vec = mv_a;
            break;
        case MVPRED_U:
            pred_vec = mv_b;
            break;
        case MVPRED_UR:
            pred_vec = mv_c;
            break;
        default:
            break;
        }


        if(mvPredType != MVPRED_MEDIAN){
            if (hv==0)
                *pmv_x = pred_vec;
            else
                *pmv_y = pred_vec;
        }
    }

#undef MEDIAN
}

/*
*************************************************************************
* Function:Checks the availability of neighboring macroblocks of
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

void CheckAvailabilityOfNeighbors(struct img_par *img)
{
    int i,j;
    const int mb_width = img->width/MB_BLOCK_SIZE;
    const int mb_nr = img->current_mb_nr;
    Macroblock *currMB = &mb_data[mb_nr];
    int check_value;

    // mark all neighbors as unavailable
    for (i=0; i<3; i++)
        for (j=0; j<3; j++)
            mb_data[mb_nr].mb_available[i][j]=NULL;

    mb_data[mb_nr].mb_available[1][1]=currMB; // current MB

    // Check MB to the left
    if(img->pix_x >= MB_BLOCK_SIZE)
    {
        int remove_prediction = currMB->slice_nr != mb_data[mb_nr-1].slice_nr;
        // upper blocks
        if (remove_prediction)
        {
            img->ipredmode[img->block_x][img->block_y+1] = -1;
            img->ipredmode[img->block_x][img->block_y+2] = -1;
        }
        if (!remove_prediction)
        {
            currMB->mb_available[1][0]=&(mb_data[mb_nr-1]);
        }
    }

    // Check MB above
    check_value =  (img->pix_y >= MB_BLOCK_SIZE);
    if(check_value)
    {
        int remove_prediction = currMB->slice_nr != mb_data[mb_nr-mb_width].slice_nr;
        // upper blocks
        if (remove_prediction)
        {
            img->ipredmode[img->block_x+1][img->block_y] = -1;
            img->ipredmode[img->block_x+2][img->block_y] = -1;
        }

        if (!remove_prediction)
        {
            currMB->mb_available[0][1]=&(mb_data[mb_nr-mb_width]);
        }
    }

    // Check MB left above
    if(img->pix_y >= MB_BLOCK_SIZE && img->pix_x >= MB_BLOCK_SIZE)
    {
        int remove_prediction = currMB->slice_nr != mb_data[mb_nr-mb_width-1].slice_nr;

        if (remove_prediction)
        {
            img->ipredmode[img->block_x][img->block_y] = -1;
        }
        if (!remove_prediction)
        {
            currMB->mb_available[0][0]=&(mb_data[mb_nr-mb_width-1]);
        }
    }

    // Check MB right above
    if(img->pix_y >= MB_BLOCK_SIZE && img->pix_x < (img->width-MB_BLOCK_SIZE ))
    {
        if(currMB->slice_nr == mb_data[mb_nr-mb_width+1].slice_nr)
            currMB->mb_available[0][2]=&(mb_data[mb_nr-mb_width+1]);
    }

}

void set_MB_parameters (struct img_par *img,struct inp_par *inp, int mb)
{
    const int number_mb_per_row = img->width / MB_BLOCK_SIZE ;
    const int mb_nr = img->current_mb_nr;
    Macroblock *currMB = &mb_data[mb_nr];

    img->mb_x = mb % number_mb_per_row;
    img->mb_y = mb / number_mb_per_row;

    // Define vertical positions
    img->block8_y= img->mb_y * BLOCK_SIZE/2;
    img->block_y = img->mb_y * BLOCK_SIZE/2;      // vertical luma block position
    img->pix_y   = img->mb_y * MB_BLOCK_SIZE;   // vertical luma macroblock position
    if(chroma_format==2)
        img->pix_c_y = img->mb_y * MB_BLOCK_SIZE; // vertical chroma macroblock position
    else
        img->pix_c_y = img->mb_y * MB_BLOCK_SIZE/2; // vertical chroma macroblock position

    // Define horizontal positions
    img->block8_x= img->mb_x * BLOCK_SIZE/2;
    img->block_x = img->mb_x * BLOCK_SIZE/2;        // luma block
    img->pix_x   = img->mb_x * MB_BLOCK_SIZE;     // luma pixel
    img->pix_c_x   = img->mb_x * MB_BLOCK_SIZE/2; // chroma pixel

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


void start_macroblock(struct img_par *img,struct inp_par *inp)
{
    int i,j,k,l;
    Macroblock *currMB;   // intialization code deleted, see below, StW

    assert (img->current_mb_nr >=0 && img->current_mb_nr < img->max_mb_nr);

    currMB = &mb_data[img->current_mb_nr];//GB

    /* Update coordinates of the current macroblock */
    img->mb_x = (img->current_mb_nr)%(img->width/MB_BLOCK_SIZE);
    img->mb_y = (img->current_mb_nr)/(img->width/MB_BLOCK_SIZE);

    /* Define vertical positions */
    img->block_y = img->mb_y * BLOCK_SIZE/2;      /* luma block position */
    img->block8_y = img->mb_y * BLOCK_SIZE/2;
    img->pix_y   = img->mb_y * MB_BLOCK_SIZE;   /* luma macroblock position */
    if(chroma_format==2)
        img->pix_c_y = img->mb_y * MB_BLOCK_SIZE; /* chroma macroblock position */
    else
        img->pix_c_y = img->mb_y * MB_BLOCK_SIZE/2; /* chroma macroblock position */

    /* Define horizontal positions */
    img->block_x = img->mb_x * BLOCK_SIZE/2;      /* luma block position */
    img->block8_x = img->mb_x * BLOCK_SIZE/2;
    img->pix_x   = img->mb_x * MB_BLOCK_SIZE;   /* luma pixel position */
    img->pix_c_x = img->mb_x * MB_BLOCK_SIZE/2; /* chroma pixel position */

    // If MB is next to a slice boundary, mark neighboring blocks unavailable for prediction
    CheckAvailabilityOfNeighbors(img);      // support only slice mode 0 in MBINTLC1 at this time

    // Reset syntax element entries in MB struct
    currMB->qp          = img->qp ;
    currMB->mb_type     = 0;
    currMB->delta_quant = 0;
    currMB->cbp         = 0;
    currMB->cbp_blk     = 0;
    currMB->c_ipred_mode= DC_PRED_8; //GB
    currMB->c_ipred_mode_2= DC_PRED_8;

    for (l=0; l < 2; l++)
        for (j=0; j < BLOCK_MULTIPLE; j++)
            for (i=0; i < BLOCK_MULTIPLE; i++)
                for (k=0; k < 2; k++)
                    currMB->mvd[l][j][i][k] = 0;

    currMB->cbp_bits   = 0;

    // initialize img->m7 for ABT//Lou
    for (j=0; j<MB_BLOCK_SIZE; j++)
        for (i=0; i<MB_BLOCK_SIZE; i++)
            img->m7[i][j] = 0;

    for (j=0; j<2*BLOCK_SIZE; j++)
        for (i=0; i<2*BLOCK_SIZE; i++)
        {
            img->m8[0][i][j] = 0;
            img->m8[1][i][j] = 0;
            img->m8[2][i][j] = 0;
            img->m8[3][i][j] = 0;
        }

        currMB->lf_disable = loop_filter_disable;

        img->weighting_prediction=0;
}

/*
*************************************************************************
* Function:Interpret the mb mode for P-Frames
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

void interpret_mb_mode_P(struct img_par *img)
{
    int i;
    const int ICBPTAB[6] = {0,16,32,15,31,47};
    Macroblock *currMB = &mb_data[img->current_mb_nr];//GB current_mb_nr];
    int         mbmode = currMB->mb_type;

    if(mbmode <4)
    {
        currMB->mb_type = mbmode;
        for (i=0;i<4;i++)
        {
            currMB->b8mode[i]   = mbmode;
            currMB->b8pdir[i]   = 0;
        }
    }
    else if(MODE_IS_P8x8)
    {
        currMB->mb_type = P8x8;
    }
    else if(/* MODE_IS_I4x4 qhg */mbmode>=5)//modefy by xfwang 2004.7.29
    {
        currMB->cbp=NCBP[currMB->mb_type-5][0]; // qhg  //modefy by xfwang 2004.7.29
        currMB->mb_type = I4MB;
        for (i=0;i<4;i++)
        {
            currMB->b8mode[i] = IBLOCK;
            currMB->b8pdir[i] = -1;
        }
    }
    else
    {
        currMB->mb_type = I16MB;
        for (i=0;i<4;i++) {currMB->b8mode[i]=0; currMB->b8pdir[i]=-1; }
        currMB->cbp= ICBPTAB[(I16OFFSET)>>2];
    }
}

/*
*************************************************************************
* Function:Interpret the mb mode for I-Frames
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

void interpret_mb_mode_I(struct img_par *img)
{
    int i;
    const int ICBPTAB[6] = {0,16,32,15,31,47};
    Macroblock *currMB   = &mb_data[img->current_mb_nr];
    int         num      =4;

    currMB->mb_type = I4MB;

    for (i=0;i<4;i++)
    {
        currMB->b8mode[i]=IBLOCK;
        currMB->b8pdir[i]=-1;
    }

    for (i=num;i<4;i++)
    {
        currMB->b8mode[i]=currMB->mb_type_2==P8x8? 4 : currMB->mb_type_2; currMB->b8pdir[i]=0;
    }
}

/*
*************************************************************************
* Function:Interpret the mb mode for B-Frames
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

void interpret_mb_mode_B(struct img_par *img)
{
    static const int offset2pdir16x16[12]   = {0, 0, 1, 2, 0,0,0,0,0,0,0,0};
    static const int offset2pdir16x8[22][2] = {{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{1,1},{0,0},{0,1},{0,0},{1,0},
    {0,0},{0,2},{0,0},{1,2},{0,0},{2,0},{0,0},{2,1},{0,0},{2,2},{0,0}};
    static const int offset2pdir8x16[22][2] = {{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{1,1},{0,0},{0,1},{0,0},
    {1,0},{0,0},{0,2},{0,0},{1,2},{0,0},{2,0},{0,0},{2,1},{0,0},{2,2}};

    const int ICBPTAB[6] = {0,16,32,15,31,47};
    Macroblock *currMB = &mb_data[img->current_mb_nr];//GB current_mb_nr];

    int i, mbmode;
    int mbtype  = currMB->mb_type;
    int *b8mode = currMB->b8mode;
    int *b8pdir = currMB->b8pdir;

    //--- set mbtype, b8type, and b8pdir ---
    if (mbtype==0)       // direct
    {
        mbmode=0;       for(i=0;i<4;i++) {b8mode[i]=0;          b8pdir[i]=2; }
    }
    else if (/*mbtype==23 qhg */ mbtype>=23) // intra4x4
    {
        currMB->cbp=NCBP[mbtype-23][0]; // qhg
        mbmode=I4MB;    for(i=0;i<4;i++) {b8mode[i]=IBLOCK;     b8pdir[i]=-1; }
    }
    else if (mbtype==22) // 8x8(+split)
    {
        mbmode=P8x8;       // b8mode and pdir is transmitted in additional codewords
    }
    else if (mbtype<4)   // 16x16
    {
        mbmode=1;
        for(i=0;i<4;i++) {b8mode[i]=1;          b8pdir[i]=offset2pdir16x16[mbtype]; }
    }
    else if (mbtype%2==0) // 16x8
    {
        mbmode=2;
        for(i=0;i<4;i++) {b8mode[i]=2;          b8pdir[i]=offset2pdir16x8 [mbtype][i/2]; }
    }
    else
    {
        mbmode=3;
        for(i=0;i<4;i++) {b8mode[i]=3;          b8pdir[i]=offset2pdir8x16 [mbtype][i%2]; }
    }


    currMB->mb_type = mbmode;
}

/*
*************************************************************************
* Function:init macroblock I and P frames
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

void init_macroblock(struct img_par *img)
{
    int i,j;
    Macroblock *currMB = &mb_data[img->current_mb_nr];//GB current_mb_nr];

    img->mv[img->block_x+4][img->block_y][2]=img->number;

    for (i=0;i<2;i++)
    {                           // reset vectors and pred. modes
        for(j=0;j<2;j++)
        {
            img->mv[img->block_x+i+4][img->block_y+j][0] = 0;
            img->mv[img->block_x+i+4][img->block_y+j][1] = 0;
        }
    }

    for (i=0;i<2;i++)
    {                           // reset vectors and pred. modes
        for(j=0;j<2;j++)
        {
            img->ipredmode[img->block_x+i+1][img->block_y+j+1] = -1;            //by oliver 0512
        }
    }

    // Set the reference frame information for motion vector prediction
    if (IS_INTRA (currMB))
    {
        for (j=0; j<2; j++)
            for (i=0; i<2; i++)
            {
                refFrArr[img->block_y+j][img->block_x+i] = -1;
            }
    }
    else if (!IS_P8x8 (currMB))
    {
        for (j=0; j<2; j++)
            for (i=0; i<2; i++)
            {
                refFrArr[img->block_y+j][img->block_x+i] = 0;
            }
    }
    else
    {
        for (j=0; j<2; j++)
            for (i=0; i<2; i++)
            {
                refFrArr[img->block_y+j][img->block_x+i] = (currMB->b8mode[2*j+i]==IBLOCK ? -1 : 0);
            }
    }
}

/*
*************************************************************************
* Function:Sets mode for 8x8 block
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

void SetB8Mode (struct img_par* img, Macroblock* currMB, int value, int i)
{
    static const int p_v2b8 [ 5] = {4, 5, 6, 7, IBLOCK};
    static const int p_v2pd [ 5] = {0, 0, 0, 0, -1};
    static const int b_v2b8 [14] = {0, 4, 4, 4, 5, 6, 5, 6, 5, 6, 7, 7, 7, IBLOCK};
    static const int b_v2pd [14] = {2, 0, 1, 2, 0, 0, 1, 1, 2, 2, 0, 1, 2, -1};

    if (img->type==B_IMG)
    {
        currMB->b8mode[i]   = b_v2b8[value];
        currMB->b8pdir[i]   = b_v2pd[value];
    }
    else
    {
        currMB->b8mode[i]   = p_v2b8[value];
        currMB->b8pdir[i]   = p_v2pd[value];
    }

}

/*
*************************************************************************
* Function:Get the syntax elements from the NAL
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

int read_one_macroblock(struct img_par *img,struct inp_par *inp)
{
    int i,j;

    SyntaxElement currSE;
    Macroblock *currMB = &mb_data[img->current_mb_nr];//GB current_mb_nr];
    int  img_block_y;
    int  real_mb_type;
    int  tempcbp;
    int fixqp;
    fixqp = (fixed_picture_qp|| fixed_slice_qp);

    for(i=0;i<8;i++)
        for(j=0;j<8;j++)
        {
            img->m8[0][i][j]=0;
            img->m8[1][i][j]=0;
            img->m8[2][i][j]=0;
            img->m8[3][i][j]=0;
        }

        currMB->qp = img->qp ;
        currSE.type = SE_MBTYPE;
        currSE.mapping = linfo_ue;

        currMB->mb_type_2= 0;

        if(img->type == I_IMG)
        {
            currMB->mb_type = 0;
        }
        else if(skip_mode_flag)
        {
            if(img->cod_counter == -1)
            {

#if TRACE
                strncpy(currSE.tracestring, "MB runlength", TRACESTRING_SIZE);
#endif

                readSyntaxElement_UVLC(&currSE,inp);
                img->cod_counter = currSE.value1;
            }

            if (img->cod_counter==0)
            {
#if TRACE
                strncpy(currSE.tracestring, "MB Type", TRACESTRING_SIZE);
#endif

                readSyntaxElement_UVLC(&currSE,inp);

                if(img->type == P_IMG)
                    currSE.value1++;

                currMB->mb_type = currSE.value1;
                img->cod_counter--;
            }
            else
            {
                img->cod_counter--;
                currMB->mb_type = 0;  // !! skip mode shenyanfei
            }
        }
        else
        {

#if TRACE
            strncpy(currSE.tracestring, "MB Type", TRACESTRING_SIZE);
#endif

            readSyntaxElement_UVLC(&currSE,inp);

            if(img->type == P_IMG)
                currSE.value1++;

            currSE.value1--;
            real_mb_type = currSE.value1;
            if(currSE.value1<0)
                currSE.value1 = 0;
            currMB->mb_type = currSE.value1;
            img->cod_counter--;
        }

        if ((img->type==P_IMG ))      // inter frame
            interpret_mb_mode_P(img);
        else if (img->type==I_IMG)                                  // intra frame
            interpret_mb_mode_I(img);
        else if ((img->type==B_IMG))  // B frame
            interpret_mb_mode_B(img);

        //====== READ 8x8 SUB-PARTITION MODES (modes of 8x8 blocks) and Intra VBST block modes ======
        if (IS_P8x8 (currMB))
        {
            currSE.type    = SE_MBTYPE;

            if(img->type!=P_IMG)
            {
                for (i=0; i<4; i++)
                {
                    currSE.mapping = linfo_ue;

#if TRACE
                    strncpy(currSE.tracestring, "8x8 mode", TRACESTRING_SIZE);
#endif

                    //mb_part_type is fix length coding(fix length equal 2)!!   jlzheng    7.22
                    assert (currStream->streamBuffer != NULL);
                    currSE.len = 2;
                    readSyntaxElement_FLC (&currSE);
                    //  END
                    SetB8Mode (img, currMB, currSE.value1, i);
                }
            }
            else
            {
                currSE.value1 = 0;
                for (i=0; i<4; i++)
                {
                    SetB8Mode (img, currMB, currSE.value1, i);
                }
            }
        }



        //! TO for Error Concelament
        //! If we have an INTRA Macroblock and we lost the partition
        //! which contains the intra coefficients Copy MB would be better
        //! than just a grey block.
        //! Seems to be a bit at the wrong place to do this right here, but for this case
        //! up to now there is no other way.//Lou

        //--- init macroblock data ---
        if (img->type==B_IMG)
            init_macroblock_Bframe(img);
        else
            init_macroblock       (img);

        if (IS_INTRA(currMB))
        {
            for (i=0;i</*5*/(chroma_format==1?5:6);i++)
                read_ipred_block_modes(img,inp,i);
        }

        if (skip_mode_flag)
        {
            if (IS_DIRECT (currMB) && img->cod_counter >= 0)
            {
                int i, j, iii, jjj;
                currMB->cbp = 0;

                for (i=0;i<BLOCK_SIZE;i++)
                { // reset luma coeffs
                    for (j=0;j<BLOCK_SIZE;j++)
                        for(iii=0;iii<BLOCK_SIZE;iii++)
                            for(jjj=0;jjj<BLOCK_SIZE;jjj++)
                                img->cof[i][j][iii][jjj]=0;
                }

                for (j=4;j<8;j++)
                { // reset chroma coeffs
                    for (i=0;i<4;i++)
                        for (iii=0;iii<4;iii++)
                            for (jjj=0;jjj<4;jjj++)
                                img->cof[i][j][iii][jjj]=0;
                }

                return DECODE_MB;
            }
        }
        else
        {
            if (img->type==B_IMG&& real_mb_type<0)
            {
                int i, j, iii, jjj;
                currMB->cbp = 0;

                for (i=0;i<BLOCK_SIZE;i++)
                { // reset luma coeffs
                    for (j=0;j<BLOCK_SIZE;j++)
                        for(iii=0;iii<BLOCK_SIZE;iii++)
                            for(jjj=0;jjj<BLOCK_SIZE;jjj++)
                                img->cof[i][j][iii][jjj]=0;
                }

                for (j=4;j</*6*/8;j++)
                { // reset chroma coeffs
                    for (i=0;i<4;i++)
                        for (iii=0;iii<4;iii++)
                            for (jjj=0;jjj<4;jjj++)
                                img->cof[i][j][iii][jjj]=0;
                }

                return DECODE_MB;
            }
        }

        if (IS_COPY (currMB)) //keep last macroblock
        {
            int i, j, iii, jjj, pmv[2];
            int ***tmp_mv         = img->mv;
            int mb_available_up   = (img->mb_y == 0)  ? 0 : (currMB->slice_nr == mb_data[img->current_mb_nr-img->width/16].slice_nr);
            int mb_available_left = (img->mb_x == 0)  ? 0 : (currMB->slice_nr == mb_data[img->current_mb_nr-1].slice_nr);
            int zeroMotionAbove   = !mb_available_up  ? 1 : refFrArr[img->block_y-1][img->block_x]  == 0 && tmp_mv[4+img->block_x  ][img->block_y-1][0] == 0 && tmp_mv[4+img->block_x  ][img->block_y-1][1] == 0 ? 1 : 0;
            int zeroMotionLeft    = !mb_available_left? 1 : refFrArr[img->block_y][img->block_x-1]  == 0 && tmp_mv[4+img->block_x-1][img->block_y  ][0] == 0 && tmp_mv[4+img->block_x-1][img->block_y  ][1] == 0 ? 1 : 0;

            currMB->cbp = 0;

            for (i=0;i<BLOCK_SIZE;i++)
            { // reset luma coeffs
                for (j=0;j<BLOCK_SIZE;j++)
                    for(iii=0;iii<BLOCK_SIZE;iii++)
                        for(jjj=0;jjj<BLOCK_SIZE;jjj++)
                            img->cof[i][j][iii][jjj]=0;
            }
            for (j=4;j</*6*/8;j++)
            { // reset chroma coeffs
                for (i=0;i<4;i++)
                    for (iii=0;iii<4;iii++)
                        for (jjj=0;jjj<4;jjj++)
                            img->cof[i][j][iii][jjj]=0;
            }


            img_block_y   = img->block_y;

            if (zeroMotionAbove || zeroMotionLeft)
            {
                for(i=0;i<2;i++)
                    for(j=0;j<2;j++)
                    {
                        img->mv[img->block_x+i+BLOCK_SIZE][img->block_y+j][0] = 0;
                        img->mv[img->block_x+i+BLOCK_SIZE][img->block_y+j][1] = 0;
                    }
            }
            else
            {
                SetMotionVectorPredictor (img, pmv, pmv+1, 0, refFrArr, img->mv, 0, 0, 16, 16, 0, 0);
                for(i=0;i<2;i++)
                    for(j=0;j<2;j++)
                    {
                        img->mv[img->block_x+i+BLOCK_SIZE][img_block_y+j][0] = pmv[0];
                        img->mv[img->block_x+i+BLOCK_SIZE][img_block_y+j][1] = pmv[1];
                    }
            }

            for (j=0; j<2;j++)
                for (i=0; i<2;i++)
                {
                    refFrArr_frm[img->block_y+j][img->block_x+i] = 0;
                }

                return DECODE_MB;
        }

        readReferenceIndex(img, inp);
        readMotionVector (img, inp);

        // !! start shenyanfei cjw
        //if((!IS_INTRA(currMB)) && (img->slice_weighting_flag == 1) && (img->mb_weighting_flag == 0)){ //cjw 20051219
        //if((!IS_INTRA(currMB)) && (img->slice_weighting_flag == 1) && (img->mb_weighting_flag == 1)&&(!IS_COPY(currMB))&&(!IS_DIRECT(currMB))){ //cjw20051230 for skip MB
        if((!IS_INTRA(currMB)) && (img->slice_weighting_flag == 1) && (img->mb_weighting_flag == 1)&&(!IS_COPY(currMB))){ //cjw20060321 skip no WP, direct WP
            img->weighting_prediction = u_v(1,"MB weighting_prediction");  //cjw 20051219
        }
        // !! end shenyanfei cjw

        // read CBP if not new intra mode
        if (!(IS_COPY (currMB) || (IS_DIRECT (currMB) && img->cod_counter >= 0)))
        {

            if (IS_OLDINTRA (currMB) || currMB->mb_type == SI4MB )   currSE.type = SE_CBP_INTRA;
            else                        currSE.type = SE_CBP_INTER;

            if (IS_OLDINTRA (currMB) || currMB->mb_type == SI4MB)
                currSE.mapping = linfo_cbp_intra;
            else
                currSE.mapping = linfo_cbp_inter;


#if TRACE
            snprintf(currSE.tracestring, TRACESTRING_SIZE, "CBP");
#endif

            if(img->type==I_IMG||IS_INTER(currMB)) // qhg
            {
                currSE.golomb_maxlevels = 0;
                readSyntaxElement_UVLC(&currSE,inp);
                currMB->cbp = currSE.value1;
            }

            //added by wzm,422
            if(chroma_format==2)
            {
                currSE.mapping = linfo_se;
                currSE.type =SE_CBP_INTRA;
                readSyntaxElement_UVLC(&currSE,inp);
                currMB->cbp01 = currSE.value1;
#if TRACE
                snprintf(currSE.tracestring, TRACESTRING_SIZE, "CBP01");
#endif
            }


            //added by wzm,422
            if(chroma_format==2)
            {
                if(currMB->cbp !=0||currMB->cbp01 !=0)
                    tempcbp=1;
                else
                    tempcbp=0;

            }
            else if(chroma_format==1)
            {
                if(currMB->cbp !=0)
                    tempcbp=1;
                else
                    tempcbp=0;
            }


            // Delta quant only if nonzero coeffs
            if (!fixqp&&(tempcbp))
            {
                if (IS_INTER (currMB))  currSE.type = SE_DELTA_QUANT_INTER;
                else                    currSE.type = SE_DELTA_QUANT_INTRA;

                currSE.mapping = linfo_se;

#if TRACE
                snprintf(currSE.tracestring, TRACESTRING_SIZE, "Delta quant ");
#endif
                readSyntaxElement_UVLC(&currSE,inp);
                currMB->delta_quant = currSE.value1;
                img->qp= (img->qp-MIN_QP+currMB->delta_quant+(MAX_QP-MIN_QP+1))%(MAX_QP-MIN_QP+1)+MIN_QP; //discussed by cjw AVS Zhuhai 20070123
            }

            if (fixqp)
            {
                currMB->delta_quant = 0;
                img->qp= (img->qp-MIN_QP+currMB->delta_quant+(MAX_QP-MIN_QP+1))%(MAX_QP-MIN_QP+1)+MIN_QP;

            }

        }
#ifdef _OUTPUT_PRED_
        if (img->tr == _FRAME_NUM_ && img->current_mb_nr == _MBNR_ )
            img->tr = _FRAME_NUM_;
#endif
        // read CBP and Coeffs  ***************************************************************
        readCBPandCoeffsFromNAL (img,inp);

        return DECODE_MB;
}

void read_ipred_block_modes(struct img_par *img,struct inp_par *inp,int b8)
{
    int bi,bj,dec;
    SyntaxElement currSE;
    Macroblock *currMB;
    int j2;
    int mostProbableIntraPredMode;
    int upIntraPredMode;
    int leftIntraPredMode;
    int IntraChromaPredModeFlag;
    int MBRowSize = img->width / MB_BLOCK_SIZE;

    currMB=mb_data+img->current_mb_nr;//current_mb_nr;
    IntraChromaPredModeFlag = IS_INTRA(currMB);

    currSE.type = SE_INTRAPREDMODE;
#if TRACE
    strncpy(currSE.tracestring, "Ipred Mode", TRACESTRING_SIZE);
#endif

    if(b8<4)
    {
        if( currMB->b8mode[b8]==IBLOCK )
        {
            IntraChromaPredModeFlag = 1;
            //get from stream
            readSyntaxElement_Intra8x8PredictionMode(&currSE);

            //get from array and decode
            bi = img->block_x + (b8&1);
            bj = img->block_y + (b8/2);

            upIntraPredMode            = img->ipredmode[bi+1][bj];
            leftIntraPredMode          = img->ipredmode[bi][bj+1];
            mostProbableIntraPredMode  = (upIntraPredMode < 0 || leftIntraPredMode < 0) ? DC_PRED : upIntraPredMode < leftIntraPredMode ? upIntraPredMode : leftIntraPredMode;

            dec = (currSE.value1 == -1) ? mostProbableIntraPredMode : currSE.value1 + (currSE.value1 >= mostProbableIntraPredMode);

            //set
            img->ipredmode[1+bi][1+bj]=dec;
            j2 = bj;
        }
    }
    else if( b8==4&&currMB->b8mode[b8-3]==IBLOCK)
    {

        currSE.type = SE_INTRAPREDMODE;
#if TRACE
        strncpy(currSE.tracestring, "Chroma intra pred mode", TRACESTRING_SIZE);
#endif

        currSE.mapping = linfo_ue;

        readSyntaxElement_UVLC(&currSE,inp);
        currMB->c_ipred_mode = currSE.value1;

        if (currSE.value1 < DC_PRED_8 || currSE.value1 > PLANE_8)
        {
            printf("%d\n", img->current_mb_nr);
            error("illegal chroma intra pred mode!\n", 600);
        }
    }
    else if( b8==5&&currMB->b8mode[b8-3]==IBLOCK&&chroma_format==2)
    {

        currSE.type = SE_INTRAPREDMODE;
#if TRACE
        strncpy(currSE.tracestring, "Chroma intra pred mode 422", TRACESTRING_SIZE);
#endif

        currSE.mapping = linfo_ue;

        readSyntaxElement_UVLC(&currSE,inp);
        currMB->c_ipred_mode_2 = currSE.value1;

        if (currSE.value1 < DC_PRED_8 || currSE.value1 > PLANE_8)
        {
            printf("%d\n", img->current_mb_nr);
            error("illegal chroma intra pred mode!\n", 600);
        }
    }
}

/*
*************************************************************************
* Function:Set context for reference frames
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

int BType2CtxRef (int btype)
{
    if (btype<4)
        return 0;
    else
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
void readReferenceIndex(struct img_par *img, struct inp_par *inp)
{
    int i,j,k;
    // int mb_nr           = img->current_mb_nr; //GB Falsch
    Macroblock *currMB  = &mb_data[img->current_mb_nr];
    SyntaxElement currSE;
    int bframe          = (img->type==B_IMG);
    int partmode        = (IS_P8x8(currMB)?4:currMB->mb_type);
    int step_h0         = BLOCK_STEP [partmode][0];
    int step_v0         = BLOCK_STEP [partmode][1];

    int i0, j0, refframe;
    int **fwRefFrArr = img->fw_refFrArr;
    int **bwRefFrArr = img->bw_refFrArr;
    int  ***fw_mv = img->fw_mv;
    int  ***bw_mv = img->bw_mv;
    //int  **moving_block_dir = moving_block;
    int flag_mode;

    //  If multiple ref. frames, read reference frame for the MB *********************************
    flag_mode = 0;

    currSE.type = SE_REFFRAME;
    currSE.mapping = linfo_ue;

    if(currMB->mb_type!=I4MB)
    {
        step_h0         = (BLOCK_STEP[IS_P8x8(currMB) ? 4 : currMB->mb_type][0]);
        step_v0         = (BLOCK_STEP[IS_P8x8(currMB) ? 4 : currMB->mb_type][1]);
    }

    // !! shenyanfei
    for (j0=0; j0<2; )
    {
        if((currMB->mb_type==I4MB&&j0==0))
        { j0 += 1; continue;}

        for (i0=0; i0<2; )
        {
            k=2*j0+i0;
            if ((currMB->b8pdir[k]==0 || currMB->b8pdir[k]==2) && currMB->b8mode[k]!=0)
            {
                img->subblock_x = i0;
                img->subblock_y = j0;

                if ((!picture_reference_flag&&img->type==P_IMG && img->types!=I_IMG) || (!picture_reference_flag&&!img->picture_structure && img->types!=I_IMG))
                {
#if TRACE
                    strncpy(currSE.tracestring,  "Fwd ref frame no ", TRACESTRING_SIZE);
#endif
                    currSE.context = BType2CtxRef (currMB->b8mode[k]);

                    if(img->picture_structure || bframe)
                        currSE.len = 1;
                    else
                        currSE.len = 2;

                    readSyntaxElement_FLC(&currSE);
                    refframe = currSE.value1;
                }
                else
                {
                    refframe = 0;
                }

                if (!bframe)
                {
                    for (j=j0; j<j0+step_v0;j++)
                        for (i=i0; i<i0+step_h0;i++)
                            refFrArr[img->block_y+j][img->block_x+i] = refframe;
                }
                else // !! for B frame shenyanfei
                {
                    for (j=j0; j<j0+step_v0;j++)
                        for (i=i0; i<i0+step_h0;i++)
                            img->fw_refFrArr[img->block_y+j][img->block_x+i] = refframe;

                    if (currMB->b8pdir[k]==2 && !img->picture_structure)
                    {
                        for (j=j0; j<j0+step_v0;j++)
                            for (i=i0; i<i0+step_h0;i++)
                                img->bw_refFrArr[img->block_y+j][img->block_x+i] = 1-refframe;
                    }
                }
            }
            i0+=max(1,step_h0);
        }
        j0+=max(1,step_v0);
    }

    for (j0=0; j0<2; )
    {
        if((currMB->mb_type==I4MB&&j0==0))
        { j0 += 1; continue;}

        for (i0=0; i0<2; )
        {
            k=2*j0+i0;
            if ((currMB->b8pdir[k]==1) && currMB->b8mode[k]!=0)
            {
                img->subblock_x = i0;
                img->subblock_y = j0;

                if (img->type==B_IMG && !img->picture_structure&&!picture_reference_flag)
                {
#if TRACE
                    strncpy(currSE.tracestring,  "Bwd ref frame no ", TRACESTRING_SIZE);
#endif
                    currSE.context = BType2CtxRef (currMB->b8mode[k]);

                    if(img->picture_structure || bframe)
                        currSE.len = 1;
                    else
                        currSE.len = 2;

                    readSyntaxElement_FLC(&currSE);
                    refframe = currSE.value1;
                }
                else
                {
                    refframe = 0;
                }

                for (j=j0; j<j0+step_v0;j++)
                    for (i=i0; i<i0+step_h0;i++)
                        img->bw_refFrArr[img->block_y+j][img->block_x+i] = refframe;
            }

            i0+=max(1,step_h0);
        }
        j0+=max(1,step_v0);
    }
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
void readMotionVector(struct img_par *img, struct inp_par *inp)
{
    int i,j,k,l,m,n;
    int step_h,step_v;
    int curr_mvd;
    // int mb_nr           = img->current_mb_nr; //GB Falsch
    Macroblock *currMB  = &mb_data[img->current_mb_nr];
    SyntaxElement currSE;
    int bframe          = (img->type==B_IMG);
    int partmode        = (IS_P8x8(currMB)?4:currMB->mb_type);
    int step_h0         = BLOCK_STEP [partmode][0];
    int step_v0         = BLOCK_STEP [partmode][1];

    int mv_mode, i0, j0, refframe;
    int pmv[2];
    int j4, i4, ii,jj;
    int vec;

    int iTRb,iTRp,iTRd;
    int frame_no_next_P, frame_no_B, delta_P, delta_P_scale;  // 20071009
    int ref;
    int img_block_y;
    int use_scaled_mv;
    int fw_refframe,current_tr;

    int **fwRefFrArr = img->fw_refFrArr;
    int **bwRefFrArr = img->bw_refFrArr;
    int  ***fw_mv = img->fw_mv;
    int  ***bw_mv = img->bw_mv;
    int  scale_refframe,iTRp1,bw_ref;

    if (bframe && IS_P8x8 (currMB))
    {
        for (i=0;i<4;i++)
        {
            if (currMB->b8mode[i] == 0)
            {
                for(j=i/2;j<i/2+1;j++)
                {
                    for(k=(i%2);k<(i%2)+1;k++)
                    {
                        //sw
                        img->fw_refFrArr[img->block_y + j][img->block_x + k] = 0;
                        img->bw_refFrArr[img->block_y + j][img->block_x + k] = 0;
                    }
                }
            }
        }
    }


    //=====  READ FORWARD MOTION VECTORS =====
    currSE.type = SE_MVD;

    currSE.mapping = linfo_se;

    for (j0=0; j0<2; )
    {
        if(currMB->mb_type!=I4MB)
        {
            step_h0         = (BLOCK_STEP[(currMB->mb_type==P8x8)? 4 : currMB->mb_type][0]);
            step_v0         = (BLOCK_STEP[(currMB->mb_type==P8x8)? 4 : currMB->mb_type][1]);
        }

        if((currMB->mb_type==I4MB&&j0==0)) // for  by jhzheng [2004/08/02]
        { j0 += 1; continue;}
        //{break;}


        for (i0=0; i0<2; )
        {
            k=2*j0+i0;
            if ((currMB->b8pdir[k]==0 || currMB->b8pdir[k]==2) && (currMB->b8mode[k] !=0))//has forward vector
            {
                mv_mode  = currMB->b8mode[k];
                step_h   = BLOCK_STEP [mv_mode][0];
                step_v   = BLOCK_STEP [mv_mode][1];

                if (!bframe)  refframe = refFrArr        [img->block_y+j0][img->block_x+i0];
                else                                     refframe = img->fw_refFrArr[img->block_y+j0][img->block_x+i0];


                for (j=j0; j<j0+step_v0; j+=step_v)
                    for (i=i0; i<i0+step_h0; i+=step_h)
                    {
                        j4 = img->block_y+j;
                        i4 = img->block_x+i;

                        // first make mv-prediction
                        if (!bframe)
                            SetMotionVectorPredictor (img, pmv, pmv+1, refframe, refFrArr,         img->mv,    i, j, 8*step_h, 8*step_v, 0, 0);
                        else
                            SetMotionVectorPredictor (img, pmv, pmv+1, refframe, img->fw_refFrArr, img->fw_mv, i, j, 8*step_h, 8*step_v, 0, 0);


                        for (n=0; n < 2; n++)
                        {
#if TRACE
                            snprintf(currSE.tracestring, TRACESTRING_SIZE, "FMVD (pred %d)",pmv[n]);
#endif
                            img->subblock_x = i; // position used for context determination
                            img->subblock_y = j; // position used for context determination
                            currSE.value2 = (!bframe ? n : 2*n); // identifies the component; only used for context determination
                            readSyntaxElement_UVLC(&currSE,inp);
                            curr_mvd = currSE.value1;

                            vec=curr_mvd+pmv[n];           /* find motion vector */

                            // need B support
                            if (!bframe)
                            {
                                for(ii=0;ii<step_h;ii++)
                                    for(jj=0;jj<step_v;jj++)
                                        img->mv[i4+ii+BLOCK_SIZE][j4+jj][n]=vec;
                            }
                            else      // B frame
                            {
                                for(ii=0;ii<step_h;ii++)
                                    for(jj=0;jj<step_v;jj++)
                                        img->fw_mv[i4+ii+BLOCK_SIZE][j4+jj][n]=vec;
                            }

                            /* store (oversampled) mvd */
                            for (l=0; l < step_v; l++)
                                for (m=0; m < step_h; m++)
                                    currMB->mvd[0][j+l][i+m][n] =  curr_mvd;
                        }
                    }
            }
            else if (currMB->b8mode[k=2*j0+i0]==0)
            {// direct mode
                // by Junhao Zheng 2004-08-04 22:00:51
                //step_v0 = step_h0 = 1;
                for (j=j0; j<j0+step_v0; j++)
                    for (i=i0; i<i0+step_h0; i++)
                    {

                        ref= refFrArr[img->block_y+j][img->block_x+i];
                        img_block_y = (img->current_mb_nr%2) ? (img->block_y-4)/2:img->block_y/2;

                        if(ref == -1)
                        {

                            //sw
                            img->fw_refFrArr[img->block_y+j][img->block_x+i]=0;
                            img->bw_refFrArr[img->block_y+j][img->block_x+i]=0;
                            j4 = img->block_y+j;
                            i4 = img->block_x+i;

                            for (ii=0; ii < 2; ii++)
                            {
                                img->fw_mv[i4+BLOCK_SIZE][j4][ii]=0;
                                img->bw_mv[i4+BLOCK_SIZE][j4][ii]=0;
                            }

                            SetMotionVectorPredictor(img,&(img->fw_mv[i4+BLOCK_SIZE][j4][0]),
                                &(img->fw_mv[i4+BLOCK_SIZE][j4][1]),0,img->fw_refFrArr,
                                img->fw_mv,0,0,16,16, 0, 1);
                            SetMotionVectorPredictor(img,&(img->bw_mv[i4+BLOCK_SIZE][j4][0]),
                                &(img->bw_mv[i4+BLOCK_SIZE][j4][1]),0,img->bw_refFrArr,
                                img->bw_mv,0,0,16,16, -1, 1);
                        }
                        else
                        {
                            frame_no_next_P =2*img->imgtr_next_P;
                            //                            frame_no_B = 2*img->tr;
                            frame_no_B = 2*img->pic_distance;  // Added by Xiaozhen ZHENG, 2007.05.01

                            delta_P = 2* (img->imgtr_next_P - img->imgtr_last_P);
                            delta_P = (delta_P + 512)%512;            // Added by Xiaozhen ZHENG, 2007.05.01
                            delta_P_scale = 2*(img->imgtr_next_P - img->imgtr_last_prev_P);    // 20071009
                            delta_P_scale = (delta_P_scale + 512)%512;              // 20071009

                            if(!img->picture_structure)
                            {

                                if (img->current_mb_nr_fld < img->PicSizeInMbs) //top field
                                    scale_refframe =   ref == 0 ? 0 : 1;
                                else
                                    scale_refframe =   ref == 1 ? 1 : 2;
                            }
                            else
                                scale_refframe = 0;


                            if(img->picture_structure)
                            {
                                //                                iTRp = (ref+1)*delta_P;        // 20071009
                                //                                iTRp1 = (scale_refframe+1)*delta_P;  // 20071009
                                iTRp  = ref==0 ? delta_P : delta_P_scale;        // 20071009
                                iTRp1 = scale_refframe==0 ? delta_P : delta_P_scale;  // 20071009
                            }else
                            {
                                if (img->current_mb_nr_fld < img->PicSizeInMbs) //top field
                                {
                                    //                                    iTRp = delta_P*(ref/2+1)-(ref+1)%2;  //the lates backward reference  // 20071009
                                    //                                    iTRp1 = delta_P*(scale_refframe/2+1)-(scale_refframe+1)%2;  //the lates backward reference    // 20071009
                                    iTRp  = ref<2 ? delta_P-(ref+1)%2 : delta_P_scale-(ref+1)%2;                  // 20071009
                                    iTRp1 = scale_refframe<2 ? delta_P-(scale_refframe+1)%2 : delta_P_scale-(scale_refframe+1)%2;  // 20071009
                                    bw_ref = 0;
                                }
                                else
                                {
                                    //                                    iTRp = 1 + delta_P*((ref+1)/2)-ref%2;                      // 20071009
                                    //                                    iTRp1 = 1 + delta_P*((scale_refframe+1)/2)-scale_refframe%2;          // 20071009
                                    iTRp  = ref==0 ? 1 : ref<3 ? 1 + delta_P - ref%2 : 1 + delta_P_scale - ref%2;  // 20071009
                                    iTRp1 = scale_refframe==0 ? 1 : scale_refframe<3 ? 1 + delta_P - scale_refframe%2 : 1 + delta_P_scale - scale_refframe%2;  // 20071009
                                    bw_ref = 1;
                                }
                            }

                            iTRd = frame_no_next_P - frame_no_B;
                            iTRb = iTRp1 - iTRd;


                            // Added by Xiaozhen ZHENG, 2007.05.01
                            iTRp  = (iTRp  + 512)%512;
                            iTRp1 = (iTRp1 + 512)%512;
                            iTRd  = (iTRd  + 512)%512;
                            iTRb  = (iTRb  + 512)%512;
                            // Added by Xiaozhen ZHENG, 2007.05.01

                            if(!img->picture_structure)
                            {
                                if (img->current_mb_nr_fld >= img->PicSizeInMbs)
                                    scale_refframe --;
                                img->fw_refFrArr[img->block_y+j][img->block_x+i]=scale_refframe; // PLUS2, Krit, 7/06 (used to be + 1)
                                img->bw_refFrArr[img->block_y+j][img->block_x+i]=bw_ref;
                            }
                            else
                            {
                                img->fw_refFrArr[img->block_y+j][img->block_x+i]=0; // PLUS2, Krit, 7/06 (used to be + 1)
                                // by Junhao Zheng 2004-08-04 21:50:38
                                //img->bw_refFrArr[img->block_y+j][img->block_x+i]=1;
                                img->bw_refFrArr[img->block_y+j][img->block_x+i]=0;
                            }

                            j4 = img->block_y+j;
                            i4 = img->block_x+i;

                            for (ii=0; ii < 2; ii++)
                            {
                                if(img->mv[img->block_x+i+4][img->block_y+j][ii] < 0)
                                {
                                    img->fw_mv[i4+BLOCK_SIZE][j4][ii] =  -(((16384/iTRp)*(1-iTRb*img->mv[img->block_x+i+4][img->block_y+j][ii])-1)>>14);
                                    img->bw_mv[i4+BLOCK_SIZE][j4][ii] = ((16384/iTRp)*(1-iTRd*img->mv[img->block_x+i+4][img->block_y+j][ii])-1)>>14;
                                }
                                else
                                {
                                    img->fw_mv[i4+BLOCK_SIZE][j4][ii] = ((16384/iTRp)*(1+iTRb*img->mv[img->block_x+i+4][img->block_y+j][ii])-1)>>14;
                                    img->bw_mv[i4+BLOCK_SIZE][j4][ii] = -(((16384/iTRp)*(1+iTRd*img->mv[img->block_x+i+4][img->block_y+j][ii])-1)>>14);
                                }

                            }
                        }
                    }
            }

            img_block_y = img->block_y;

            i0+=max(1,step_h0);
        }
        j0+=max(1,step_v0);
    }


    //=====  READ BACKWARD MOTION VECTORS =====
    currSE.type = SE_MVD;

    currSE.mapping = linfo_se;

    img_block_y = img->block_y;

    for (j0=0; j0<2; )
    {
        if(currMB->mb_type!=I4MB)
        {
            step_h0         = (BLOCK_STEP[(currMB->mb_type==P8x8)? 4 : currMB->mb_type][0]);
            step_v0         = (BLOCK_STEP[(currMB->mb_type==P8x8)? 4 : currMB->mb_type][1]);
        }

        if((currMB->mb_type==I4MB&&j0==0))
        { j0 += 1; continue;}


        for (i0=0; i0<2; )
        {
            k=2*j0+i0;

            if ((currMB->b8pdir[k]==1 || currMB->b8pdir[k]==2) && (currMB->b8mode[k]!=0))//has backward vector
            {
                mv_mode  = currMB->b8mode[k];
                step_h   = BLOCK_STEP [mv_mode][0];
                step_v   = BLOCK_STEP [mv_mode][1];

                refframe = img->bw_refFrArr[img->block_y+j0][img->block_x+i0]; // always 0

                use_scaled_mv = 0;

                if(currMB->b8pdir[k]==2)
                {
                    fw_refframe = img->fw_refFrArr[img->block_y+j0][img->block_x+i0];
                    //          current_tr  = 2*img->tr_frm;   // Commented by Xiaozhen ZHENG, 2007.05.01
                    current_tr  = 2*(img->pic_distance/2);

                }

                for (j=j0; j<j0+step_v0; j+=step_v)
                    for (i=i0; i<i0+step_h0; i+=step_h)
                    {
                        j4 = img->block_y+j;
                        i4 = img->block_x+i;

                        SetMotionVectorPredictor (img, pmv, pmv+1, refframe, img->bw_refFrArr, img->bw_mv, i, j, 8*step_h, 8*step_v, -1, 0);

                        for (k=0; k < 2; k++)
                        {
#if TRACE
                            snprintf(currSE.tracestring, TRACESTRING_SIZE, "BMVD (pred %d)",pmv[k]);
#endif
                            img->subblock_x = i; // position used for context determination
                            img->subblock_y = j; // position used for context determination
                            currSE.value2   = 2*k+1; // identifies the component; only used for context determination

                            if (img->pic_distance == 10 && currMB->b8pdir[2*j0+i0] == 2)
                            {
                                img->pic_distance = img->pic_distance;
                            }
                            if(currMB->b8pdir[2*j0+i0] == 2)
                            {
                                int delta_P,iTRp,DistanceIndexFw,DistanceIndexBw,refframe,delta_PB;
                                refframe = fw_refframe;
                                delta_P = 2*(img->imgtr_next_P - img->imgtr_last_P);
                                delta_P = (delta_P + 512)%512;    // Added by Xiaozhen ZHENG, 2007.05.01
                                if(img->picture_structure)
                                    iTRp = (refframe+1)*delta_P;  //the lates backward reference
                                else
                                {
                                    iTRp = delta_P;//refframe == 0 ? delta_P-1 : delta_P+1;
                                }
                                //                delta_PB = 2*(img->tr - img->imgtr_last_P);            // Commented by Xiaozhen ZHENG, 2007.05.01
                                delta_PB = 2*(img->pic_distance - img->imgtr_last_P);  // Added by Xiaozhen ZHENG, 2007.05.01
                                delta_PB = (delta_PB + 512)%512;                       // Added by Xiaozhen ZHENG, 2007.05.01
                                iTRp     = (iTRp + 512)%512;                           // Added by Xiaozhen ZHENG, 2007.05.01
                                if(!img->picture_structure)
                                {
                                    if(img->current_mb_nr_fld < img->PicSizeInMbs) //top field
                                        DistanceIndexFw =  refframe == 0 ? delta_PB-1:delta_PB;
                                    else
                                        DistanceIndexFw =  refframe == 0 ? delta_PB:delta_PB+1;
                                }
                                else
                                    DistanceIndexFw = delta_PB;

                                //DistanceIndexBw  = iTRp - DistanceIndexFw;
                                DistanceIndexBw    = (iTRp - DistanceIndexFw+512)%512; // Added by Zhijie Yang, 20070419, Broadcom
                                curr_mvd = - ((img->fw_mv[i4+BLOCK_SIZE][j4][k]*DistanceIndexBw*(512/DistanceIndexFw)+256)>>9);
                                vec=curr_mvd;           /* find motion vector */
                            }
                            else
                            {
                                readSyntaxElement_UVLC(&currSE,inp);
                                curr_mvd = currSE.value1;
                                vec=curr_mvd+pmv[k];           /* find motion vector */
                            }

                            for(ii=0;ii<step_h;ii++)
                                for(jj=0;jj<step_v;jj++)
                                    img->bw_mv[i4+ii+BLOCK_SIZE][j4+jj][k]=vec;

                            /* store (oversampled) mvd */
                            for (l=0; l < step_v; l++)
                                for (m=0; m < step_h; m++)
                                    currMB->mvd[1][j+l][i+m][k] =  curr_mvd;
                        }
                    }
            }
            i0+=max(1,step_h0);
        }
        j0+=max(1,step_v0);

    }
}

/*
*************************************************************************
* Function:Get coded block pattern and coefficients (run/level)
from the NAL
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

void readCBPandCoeffsFromNAL(struct img_par *img,struct inp_par *inp)
{
    int i,j;
    int mb_nr = img->current_mb_nr; //GBimg->current_mb_nr;

    int  m2,jg2;
    Macroblock *currMB = &mb_data[mb_nr];
    int iii,jjj;
    int b8;

    int block_x,block_y;

    int qp_per    = (img->qp-MIN_QP)/6;
    int qp_rem    = (img->qp-MIN_QP)%6;
    int qp_per_uv = QP_SCALE_CR[img->qp-MIN_QP]/6;
    int qp_rem_uv = QP_SCALE_CR[img->qp-MIN_QP]%6;

    for (i=0;i<BLOCK_SIZE;i++)
        for (j=0;j<BLOCK_SIZE;j++)
            for(iii=0;iii<BLOCK_SIZE;iii++)
                for(jjj=0;jjj<BLOCK_SIZE;jjj++)
                    img->cof[i][j][iii][jjj]=0;// reset luma coeffs

    qp_per    = (img->qp-MIN_QP)/6;
    qp_rem    = (img->qp-MIN_QP)%6;
    qp_per_uv = QP_SCALE_CR[img->qp-MIN_QP]/6;
    qp_rem_uv = QP_SCALE_CR[img->qp-MIN_QP]%6;
    currMB->qp = img->qp;

    // luma coefficients
    for (block_y=0; block_y < 4; block_y += 2) /* all modes */
    {
        for (block_x=0; block_x < 4; block_x += 2)
        {
            b8 = 2*(block_y/2) + block_x/2;
#ifdef _OUTPUT_PRED_
            if (img->tr == _FRAME_NUM_ && img->current_mb_nr == _MBNR_ && b8 == _B8_)
                img->tr = _FRAME_NUM_;
#endif

            if (currMB->cbp&(1<<b8))
            {
                readLumaCoeff_B8(b8, inp, img);
            }
        }
    }

    for (j=4;j<6;j++) // reset all chroma coeffs before read
        for (i=0;i<4;i++)
            for (iii=0;iii<4;iii++)
                for (jjj=0;jjj<4;jjj++)
                    img->cof[i][j][iii][jjj]=0;

    m2 =img->mb_x*2;
    jg2=img->mb_y*2;

    if ((currMB->cbp>>4)&1)
    {
        readChromaCoeff_B8(4, inp, img);
    }


    if ((currMB->cbp>>4)&2)
    {
        readChromaCoeff_B8(5, inp, img);
    }

    //added by wzm,422
    if(chroma_format==2)
    {
        if(currMB->cbp01&1)
        {
            readChromaCoeff_B8(6, inp, img);
        }
        if((currMB->cbp01>>1)&1)
        {
            readChromaCoeff_B8(7, inp, img);
        }
    }

}

/*
*************************************************************************
* Function:decode one macroblock
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

int decode_one_macroblock(struct img_par *img,struct inp_par *inp)
{
    unsigned char edgepixu[40];
#define EPU (edgepixu+20)
    unsigned char edgepixv[40];
#define EPV (edgepixv+20)
    int x,y,last_pix,new_pix;
    int bs_x=8;
    int bs_y=8;

    int tmp_block[8][8];
    int tmp_blockbw[8][8];
    int i=0,j=0,ii=0,jj=0,i1=0,j1=0,j4=0,i4=0;
    int js0=0,js1=0,js2=0,js3=0,jf=0;
    int uv, hv;
    int vec1_x=0,vec1_y=0,vec2_x=0,vec2_y=0;
    int ioff,joff;
    short curr_blk[B8_SIZE][B8_SIZE];  //SW for AVS
    int tmp;
    int block8x8,b88=0;   // needed for ABT,422
    int bw_pred, fw_pred, ifx;
    int ii0,jj0,ii1,jj1,if1,jf1,if0,jf0;
    int mv_mul,f1,f2,f3,f4;
    //added by wzm,422
    int     img_cx            = img->pix_c_x;
    int     img_cy            = img->pix_c_y;
    int     img_cx_1          = img->pix_c_x-1;
    int     img_cx_4          = img->pix_c_x+4;
    int     img_cy_1          = img->pix_c_y-1;
    int     img_cy_4          = img->pix_c_y+4;
    int   block8x8_idx[4][4] =
    { {0, 1, 0, 0},
    {0, 1, 0, 0},
    {2, 3, 0, 0},
    {2, 3, 0, 0}
    };


    const byte decode_block_scan[16] = {0,1,4,5,2,3,6,7,8,9,12,13,10,11,14,15};

    Macroblock *currMB   = &mb_data[img->current_mb_nr];//GB current_mb_nr];
    int refframe, fw_refframe, bw_refframe, mv_mode, pred_dir, intra_prediction; // = currMB->ref_frame;
    int fw_ref_idx, bw_ref_idx;
    int*** mv_array, ***fw_mv_array, ***bw_mv_array;
    int bframe = (img->type==B_IMG);
    int  b8_s=0,b8_e=4,incr_y=1,off_y=0,even_y=4,add_y=0;
    int b8_x              = img->pix_c_x/4;
    int b8_y              = img->pix_c_y/8;
    int frame_no_next_P, frame_no_B, delta_P, delta_P_scale; // 20071224
    int iTRb, iTRp,iTRd;

    int mb_nr             = img->current_mb_nr;//GBimg->current_mb_nr;
    int mb_width          = img->width/16;
    int mb_available_up;
    int mb_available_left;
    /*
    int mb_available_up_right;
    int mb_available_left_down;
    int mb_available_up_left;*/

    int fwd_refframe_offset,bwd_refframe_offset;
    int direct_pdir;
    int  scale_refframe,iTRp1,bw_ref;

    // !! shenyanfei
    int fw_lum_scale , fw_lum_shift ;
    int bw_lum_scale , bw_lum_shift ;
    //cjw 20051219 Weighted Predition
    int fw_chrom_scale , fw_chrom_shift ;
    int bw_chrom_scale , bw_chrom_shift ;

    int  mb_available_up_right=((img->mb_y==0)||(img->mb_x==img->width/MB_BLOCK_SIZE-1)) ? 0 : (mb_data[mb_nr].slice_nr == mb_data[mb_nr-mb_width+1].slice_nr);
    int  mb_available_left_down=((img->mb_x==0)||(img->mb_y==img->height/MB_BLOCK_SIZE-1)) ? 0 : (mb_data[mb_nr].slice_nr == mb_data[mb_nr+mb_width-1].slice_nr);
    //by oliver according to 1658
    mb_available_up   = (img->mb_y == 0) ? 0 : (mb_data[mb_nr].slice_nr == mb_data[mb_nr-mb_width].slice_nr);
    mb_available_left = (img->mb_x == 0) ? 0 : (mb_data[mb_nr].slice_nr == mb_data[mb_nr-1].slice_nr);

    ifx = if1 = 0;

    /*
    //cjw weighted prediction parameter map 20060112
    ///frame coding/////////
    P    img->lum_scale[0]  fw[0]
    img->lum_scale[1]  fw[1]
    B      img->lum_scale[0]  fw[0]
    img->lum_scale[1]  bw[0]
    ///field coding////////
    P    img->lum_scale[0]  fw[0] ; img->lum_scale[1]  fw[1]
    img->lum_scale[2]  fw[2] ; img->lum_scale[3]  fw[3]
    B    img->lum_scale[0]  fw[0] ; img->lum_scale[1]  bw[0]
    img->lum_scale[2]  fw[1] ; img->lum_scale[3]  bw[1]
    //For B framecoding
    fw  [fw_ref]
    bw  [bw_ref+1]
    //For B fieldcoding
    fw  [fw_ref*2]
    bw  [bw_ref*2+1]
    */

    if(bframe)
    {
        //    int current_tr  = 2*img->tr_frm;   // Commented by Xiaozhen ZHENG, 2007.05.01
        int current_tr = 2*(img->pic_distance/2);
        if(img->imgtr_next_P <= current_tr)
            fwd_refframe_offset = 0;
        else
            fwd_refframe_offset = 1;
    }
    else
    {
        fwd_refframe_offset = 0;
    }

    if (bframe)
    {
        bwd_refframe_offset = 0;
    }

    mv_mul=4;
    f1=8;
    f2=7;

    f3=f1*f1;
    f4=f3/2;

#ifdef _OUTPUT_MV
    if (img->type == P_IMG)
    {
        for (block8x8=0; block8x8<4; block8x8++)
        {
            i = block8x8%2;
            j = block8x8/2;
            i4=img->block_x+i;
            j4=img->block_y+j;
            if (!bframe)
            {
                mv_array = img->mv;
            }
            vec1_x = mv_array[i4+BLOCK_SIZE][j4][0];
            vec1_y = mv_array[i4+BLOCK_SIZE][j4][1];
            fprintf(pf_mv, "(%4d,%4d)(mode:%4d, mvy:%4d, mvx:%4d)", img->mb_y, img->mb_x, currMB->b8mode[block8x8], vec1_y, vec1_x);
        }
        fprintf(pf_mv, "\n");
    }
#endif
    // luma decoding **************************************************
    for (block8x8=0; block8x8<4; block8x8++)
    {
#ifdef _OUTPUT_PRED_
        if (FrameNum==_FRAME_NUM_ && img->type == B_IMG && img->current_mb_nr == _MBNR_ && block8x8 == _B8_ )
        {
            block8x8 = block8x8;
        }
#endif
        if (currMB->b8mode[block8x8] != IBLOCK)
        {
            i = block8x8%2;
            j = block8x8/2;

            ioff=i*8;
            i4=img->block_x+i;

            joff=j*8;
            j4=img->block_y+j;

            mv_mode  = currMB->b8mode[block8x8];
            pred_dir = currMB->b8pdir[block8x8];

            if (pred_dir != 2)
            {
                //===== FORWARD/BACKWARD PREDICTION =====
                if (!bframe)  // !! P MB shenyanfei
                {
                    refframe = refFrArr[j4][i4];
                    fw_ref_idx = refframe;
                    mv_array = img->mv;
                }
                else if (!pred_dir) // !! B forward shenyanfei
                {
                    refframe = img->fw_refFrArr[j4][i4];// fwd_ref_idx_to_refframe(img->fw_refFrArr[j4][i4]);
                    fw_ref_idx = img->fw_refFrArr[j4][i4];
                    mv_array = img->fw_mv;
                }
                else // !! B backward shenyanfei
                {
                    refframe = img->bw_refFrArr[j4][i4];// bwd_ref_idx_to_refframe(img->bw_refFrArr[j4][i4]);
                    bw_ref_idx = img->bw_refFrArr[j4][i4];
                    bw_ref_idx = bw_ref_idx;
                    mv_array = img->bw_mv;
                }

                if((progressive_sequence||progressive_frame)&&bframe&&img->picture_structure)
                    refframe = 0;
                vec1_x = i4*8*mv_mul + mv_array[i4+BLOCK_SIZE][j4][0];
                vec1_y = j4*8*mv_mul + mv_array[i4+BLOCK_SIZE][j4][1];

                if(!bframe){ // !! for P MB shenyanfei cjw

                    get_block(refframe, vec1_x, vec1_y, img, tmp_block,mref[refframe]);

                    //if(((currMB->mb_type != 0)&&(img->slice_weighting_flag == 1) &&
                    if(((img->slice_weighting_flag == 1)&&      //cjw20060321
                        (img->mb_weighting_flag == 1)&&(img->weighting_prediction == 1))
                        ||((img->slice_weighting_flag == 1) && (img->mb_weighting_flag == 0))){
                            fw_lum_scale = img->lum_scale[refframe] ;
                            fw_lum_shift = img->lum_shift[refframe] ;
                            for(ii=0;ii<8;ii++){
                                for(jj=0;jj<8;jj++){
                                    tmp_block[ii][jj] = Clip1(((tmp_block[ii][jj]*fw_lum_scale+16)>>5) + fw_lum_shift);
                                }
                            }
                    }
                }
                else // !! for B MB one direction  shenyanfei cjw
                {
                    if(!pred_dir){  // !! forward shenyanfei
                        get_block (refframe, vec1_x, vec1_y, img, tmp_block,mref_fref[refframe]);

                        if(((img->slice_weighting_flag == 1) && (img->mb_weighting_flag == 1)&&(img->weighting_prediction == 1))
                            ||((img->slice_weighting_flag == 1) && (img->mb_weighting_flag == 0))){

                                refframe=(img->picture_structure)?(refframe):(2*refframe);  //cjw 20060112 fw

                                fw_lum_scale = img->lum_scale[refframe] ;
                                fw_lum_shift = img->lum_shift[refframe] ;

                                for(ii=0;ii<8;ii++){
                                    for(jj=0;jj<8;jj++){
                                        tmp_block[ii][jj] = Clip1(((tmp_block[ii][jj]*fw_lum_scale+16)>>5) + fw_lum_shift);
                                    }
                                }
                        }
                    }
                    else{ // !! backward shenyanfei
                        get_block (refframe, vec1_x, vec1_y, img, tmp_block,mref_bref[refframe]);

                        if(((img->slice_weighting_flag == 1) && (img->mb_weighting_flag == 1)&&(img->weighting_prediction == 1))
                            ||((img->slice_weighting_flag == 1) && (img->mb_weighting_flag == 0))){

                                refframe=(img->picture_structure)?(refframe+1):(2*refframe+1);  //cjw 20060112 bw

                                bw_lum_scale = img->lum_scale[refframe] ;
                                bw_lum_shift = img->lum_shift[refframe] ;
                                for(ii=0;ii<8;ii++){
                                    for(jj=0;jj<8;jj++){
                                        tmp_block[ii][jj] = Clip1(((tmp_block[ii][jj]*bw_lum_scale+16)>>5) + bw_lum_shift);
                                    }
                                }
                        }
                    }
                }

                for(ii=0;ii<8;ii++)
                    for(jj=0;jj<8;jj++)
                        img->mpr[ii+ioff][jj+joff] = tmp_block[ii][jj];
            }
            else  // !! pred_dir == 2
            {
                if (mv_mode != 0)
                {
                    //===== BI-DIRECTIONAL PREDICTION =====

                    fw_mv_array = img->fw_mv;
                    bw_mv_array = img->bw_mv;

                    fw_refframe = img->fw_refFrArr[j4][i4];// fwd_ref_idx_to_refframe(img->fw_refFrArr[j4][i4]);
                    bw_refframe = img->bw_refFrArr[j4][i4];// bwd_ref_idx_to_refframe(img->bw_refFrArr[j4][i4]);
                    fw_ref_idx = img->fw_refFrArr[j4][i4];
                    bw_ref_idx = img->bw_refFrArr[j4][i4];
                    bw_ref_idx = bw_ref_idx;
                }
                else
                {
                    //===== DIRECT PREDICTION =====
                    fw_mv_array = img->dfMV;
                    bw_mv_array = img->dbMV;
                    bw_refframe = 0;


                    if(refFrArr[j4][i4] == -1) // next P is intra mode
                    {

                        for(hv=0; hv<2; hv++)
                        {
                            img->dfMV[i4+BLOCK_SIZE][j4][hv]=img->dbMV[i4+BLOCK_SIZE][j4][hv]=0;
                            img->fw_mv[i4+BLOCK_SIZE][j4][hv]=img->bw_mv[i4+BLOCK_SIZE][j4][hv]=0;
                        }
                        //sw
                        img->fw_refFrArr[j4][i4]=0;
                        img->bw_refFrArr[j4][i4]=0;

                        SetMotionVectorPredictor(img,&(img->fw_mv[i4+BLOCK_SIZE][j4][0]),
                            &(img->fw_mv[i4+BLOCK_SIZE][j4][1]),0,img->fw_refFrArr,
                            img->fw_mv,0,0,16,16, 0, 1);
                        SetMotionVectorPredictor(img,&(img->bw_mv[i4+BLOCK_SIZE][j4][0]),
                            &(img->bw_mv[i4+BLOCK_SIZE][j4][1]),0,img->bw_refFrArr,
                            img->bw_mv,0,0,16,16, -1, 1);
                        for(hv=0; hv<2; hv++)
                        {
                            img->dfMV[i4+BLOCK_SIZE][j4][hv]=img->fw_mv[i4+BLOCK_SIZE][j4][hv];
                            img->dbMV[i4+BLOCK_SIZE][j4][hv]=img->bw_mv[i4+BLOCK_SIZE][j4][hv];
                        }

                        fw_refframe = 0;
                        fw_ref_idx = 0;
                    }
                    else // next P is skip or inter mode
                    {
                        refframe = refFrArr[j4][i4];

                        fw_ref_idx = refframe;
                        frame_no_next_P =2*img->imgtr_next_P;

                        //            frame_no_B = 2*img->tr;            // Commented by Xiaozhen ZHENG, 2007.05.01
                        frame_no_B = 2*img->pic_distance;  // Added by Xiaozhen ZHENG, 2007.05.01
                        delta_P = 2*(img->imgtr_next_P - img->imgtr_last_P);
                        delta_P = (delta_P + 512)%512;            // Added by Xiaozhen ZHENG,2007.05.01
                        delta_P_scale = 2*(img->imgtr_next_P - img->imgtr_last_prev_P);    // direct, 20071224
                        delta_P_scale = (delta_P_scale + 512)%512;              // direct, 20071224

                        iTRp = (refframe+1)*delta_P;
                        iTRb = iTRp - (frame_no_next_P - frame_no_B);

                        if(!img->picture_structure)
                        {
                            if (img->current_mb_nr_fld < img->PicSizeInMbs) //top field
                                scale_refframe =   refframe == 0 ? 0 : 1;
                            else
                                scale_refframe =   refframe == 1 ? 1 : 2;
                        }
                        else
                            scale_refframe = 0;

                        if(img->picture_structure)
                        {
                            //              iTRp = (refframe+1)*delta_P;                // direct, 20071224
                            //              iTRp1 = (scale_refframe+1)*delta_P;              // direct, 20071224
                            iTRp  = refframe==0 ? delta_P : delta_P_scale;        // direct, 20071224
                            iTRp1 = scale_refframe==0 ? delta_P : delta_P_scale;    // direct, 20071224
                        }else
                        {
                            if (img->current_mb_nr_fld < img->PicSizeInMbs) //top field
                            {
                                //                iTRp = delta_P*(refframe/2+1)-(refframe+1)%2;  //the lates backward reference                // direct, 20071224
                                //                iTRp1 = delta_P*(scale_refframe/2+1)-(scale_refframe+1)%2;  //the lates backward reference          // direct, 20071224
                                iTRp  = refframe<2 ? delta_P-(refframe+1)%2 : delta_P_scale-(refframe+1)%2;                  // direct, 20071224
                                iTRp1 = scale_refframe<2 ? delta_P-(scale_refframe+1)%2 : delta_P_scale-(scale_refframe+1)%2;        // direct, 20071224
                                bw_ref = 0;
                            }
                            else
                            {
                                //                iTRp = 1 + delta_P*((refframe+1)/2)-refframe%2;                            // direct, 20071224
                                //                iTRp1 = 1 + delta_P*((scale_refframe+1)/2)-scale_refframe%2;                    // direct, 20071224
                                iTRp  = refframe==0 ? 1 : refframe<3 ? 1 + delta_P - refframe%2 : 1 + delta_P_scale - refframe%2;  // direct, 20071224
                                iTRp1 = scale_refframe==0 ? 1 : scale_refframe<3 ? 1 + delta_P - scale_refframe%2 : 1 + delta_P_scale - scale_refframe%2;  // direct, 20071224
                                bw_ref = 1;
                            }
                        }
                        iTRd = frame_no_next_P - frame_no_B;
                        if(img->picture_structure)
                        {
                            iTRb = iTRp1 - (frame_no_next_P - frame_no_B);
                        }else
                        {
                            iTRb = iTRp1 - (frame_no_next_P - frame_no_B);
                            if (img->current_mb_nr_fld >= img->PicSizeInMbs) //top field
                                scale_refframe --;
                        }
                        // Added by Xiaozhen ZHENG, 2007.05.01
                        iTRp  = (iTRp  + 512)%512;
                        iTRp1 = (iTRp1 + 512)%512;
                        iTRd  = (iTRd  + 512)%512;
                        iTRb  = (iTRb  + 512)%512;
                        // Added by Xiaozhen ZHENG, 2007.05.01

                        if(img->mv[i4+4][j4][0] < 0)
                        {
                            img->dfMV[i4+BLOCK_SIZE][j4][0] = -(((16384/iTRp)*(1-iTRb*img->mv[i4+4][j4][0])-1)>>14);
                            img->dbMV[i4+BLOCK_SIZE][j4][0] = ((16384/iTRp)*(1-iTRd*img->mv[i4+4][j4][0])-1)>>14;
                        }
                        else
                        {
                            img->dfMV[i4+BLOCK_SIZE][j4][0] = ((16384/iTRp)*(1+iTRb*img->mv[i4+4][j4][0])-1)>>14;
                            img->dbMV[i4+BLOCK_SIZE][j4][0] = -(((16384/iTRp)*(1+iTRd*img->mv[i4+4][j4][0])-1)>>14);
                        }

                        if(img->mv[i4+4][j4][1] < 0)
                        {
                            img->dfMV[i4+BLOCK_SIZE][j4][1] = -(((16384/iTRp)*(1-iTRb*img->mv[i4+4][j4][1])-1)>>14);
                            img->dbMV[i4+BLOCK_SIZE][j4][1] = ((16384/iTRp)*(1-iTRd*img->mv[i4+4][j4][1])-1)>>14;
                        }
                        else
                        {
                            img->dfMV[i4+BLOCK_SIZE][j4][1] = ((16384/iTRp)*(1+iTRb*img->mv[i4+4][j4][1])-1)>>14;
                            img->dbMV[i4+BLOCK_SIZE][j4][1] = -(((16384/iTRp)*(1+iTRd*img->mv[i4+4][j4][1])-1)>>14);
                        }

                        if(!img->picture_structure)
                        {
                            fw_refframe = scale_refframe;  // DIRECT
                            fw_ref_idx = max(0,scale_refframe);
                            img->fw_refFrArr[j4][i4]=scale_refframe;
                        }
                        else
                        {
                            fw_refframe = 0;  // DIRECT
                            fw_ref_idx = max(0,refFrArr[j4][i4]);
                            img->fw_refFrArr[j4][i4]=0;
                        }


                        img->fw_mv[i4+BLOCK_SIZE][j4][0]=img->dfMV[i4+BLOCK_SIZE][j4][0];
                        img->fw_mv[i4+BLOCK_SIZE][j4][1]=img->dfMV[i4+BLOCK_SIZE][j4][1];
                        img->bw_mv[i4+BLOCK_SIZE][j4][0]=img->dbMV[i4+BLOCK_SIZE][j4][0];
                        img->bw_mv[i4+BLOCK_SIZE][j4][1]=img->dbMV[i4+BLOCK_SIZE][j4][1];

                        if(img->picture_structure)
                            img->bw_refFrArr[j4][i4]=0;
                        else
                            bw_refframe = img->bw_refFrArr[j4][i4]=bw_ref;
                    }
                }
                //if ((progressive_sequence||progressive_frame)&&img->picture_structure)

                vec1_x = i4*8*mv_mul + fw_mv_array[i4+BLOCK_SIZE][j4][0];
                vec1_y = j4*8*mv_mul + fw_mv_array[i4+BLOCK_SIZE][j4][1];
                vec2_x = i4*8*mv_mul + bw_mv_array[i4+BLOCK_SIZE][j4][0];
                vec2_y = j4*8*mv_mul + bw_mv_array[i4+BLOCK_SIZE][j4][1];

                // !! bidirection prediction shenyanfei
                get_block (fw_refframe, vec1_x, vec1_y, img, tmp_block,mref_fref[fw_refframe]);
                get_block (bw_refframe, vec2_x, vec2_y, img, tmp_blockbw,mref_bref[bw_refframe]);
                //get_block (bw_refframe, vec2_x, vec2_y, img, tmp_blockbw,mref_bref[bw_refframe+1]);
                //by oliver 0502

                //if(((img->slice_weighting_flag == 1) && (img->mb_weighting_flag == 1)&&(img->weighting_prediction == 1)&&(mv_mode != 0))
                if(((img->slice_weighting_flag == 1) && (img->mb_weighting_flag == 1)&&(img->weighting_prediction == 1)) //cjw20060321
                    ||((img->slice_weighting_flag == 1) && (img->mb_weighting_flag == 0))){

                        fw_refframe=(img->picture_structure)?(fw_refframe):(2*fw_refframe);  //cjw 20060112 fw
                        bw_refframe=(img->picture_structure)?(bw_refframe+1):(2*bw_refframe+1);  //cjw 20060112 bw

                        fw_lum_scale = img->lum_scale[fw_refframe] ;
                        fw_lum_shift = img->lum_shift[fw_refframe] ;
                        bw_lum_scale = img->lum_scale[bw_refframe] ;
                        bw_lum_shift = img->lum_shift[bw_refframe] ;

                        for(ii=0;ii<8;ii++){
                            for(jj=0;jj<8;jj++){
                                tmp_block[ii][jj] =Clip1(((tmp_block[ii][jj]*fw_lum_scale+16)>>5) + fw_lum_shift);
                            }
                        }

                        for(ii=0;ii<8;ii++){
                            for(jj=0;jj<8;jj++){
                                tmp_blockbw[ii][jj] =Clip1(((tmp_blockbw[ii][jj]*bw_lum_scale+16)>>5) + bw_lum_shift);
                            }
                        }
                }

                for(ii=0;ii<8;ii++)
                    for(jj=0;jj<8;jj++)
                        img->mpr[ii+ioff][jj+joff] = (tmp_block[ii][jj]+tmp_blockbw[ii][jj]+1)/2;

            }
        }

        get_curr_blk (block8x8, img, curr_blk);

        if (currMB->b8mode[block8x8]!=IBLOCK)//((IS_INTER (currMB) && mv_mode!=IBLOCK))
        {
            idct_dequant_B8 (block8x8, currMB->qp-MIN_QP, curr_blk, img);
        }
        else
        {
            //intrapred creates the intraprediction in the array img->mpr[x][y] at the location corresponding to the position within the 16x16 MB.
            tmp = intrapred( img, (img->mb_x<<4)+((block8x8&1)<<3),
                (img->mb_y<<4)+((block8x8&2)<<2));
            if ( tmp == SEARCH_SYNC)  /* make 4x4 prediction block mpr from given prediction img->mb_mode */
                return SEARCH_SYNC;                   /* bit error */

            idct_dequant_B8 (block8x8, currMB->qp-MIN_QP, curr_blk, img);

        }
    }

    // chroma decoding *******************************************************
    if(chroma_format==2)
    {
        for(uv=0;uv<2;uv++)
        {
            intra_prediction = IS_INTRA (currMB);

            if (intra_prediction)
            {
                if (uv==0)
                {
                    if(mb_available_up)
                    {
                        for(x=0;x<bs_x;x++)
                            EPU[x+1]=imgUV[uv][img->pix_c_y-1][img->pix_c_x+x];

                        /*
                        for(x=0;x<bs_y;x++)
                        EPU[1+x+bs_x]=EPU[bs_x];*/
                        if(mb_available_up_right){
                            for(x=0;x<bs_y;x++)
                                EPU[1+x+bs_x]=imgUV[uv][img->pix_c_y-1][img->pix_c_x+bs_x+x];
                        }
                        else{
                            for(x=0;x<bs_y;x++)
                                EPU[1+x+bs_x]=EPU[bs_x];  //bs_x=8; EPU[9~16]=r[8]
                        }
                        //by oliver according to 1658


                        EPU[0]=imgUV[uv][img->pix_c_y-1][img->pix_c_x];
                    }
                    if(mb_available_left)
                    {
                        for(y=0;y<bs_y;y++)
                            EPU[-1-y]=imgUV[uv][img->pix_c_y+y][img->pix_c_x-1];

                        for(y=0;y<bs_x;y++)
                            EPU[-1-y-bs_y]=EPU[-bs_y];

                        EPU[0]=imgUV[uv][img->pix_c_y][img->pix_c_x-1];
                    }
                    if(mb_available_up&&mb_available_left)
                        EPU[0]=imgUV[uv][img->pix_c_y-1][img->pix_c_x-1];

                    //lowpass (Those emlements that are not needed will not disturb)
                    last_pix=EPU[-8];
                    for(i=-8;i<=8;i++)
                    {
                        new_pix=( last_pix + (EPU[i]<<1) + EPU[i+1] + 2 )>>2;
                        last_pix=EPU[i];
                        EPU[i]=(unsigned char)new_pix;
                    }
                }


                if (uv==1)
                {
                    if(mb_available_up)
                    {
                        for(x=0;x<bs_x;x++)
                            EPV[x+1]=imgUV[uv][img->pix_c_y-1][img->pix_c_x+x];

                        /*          for(x=0;x<bs_y;x++)
                        EPV[1+x+bs_x]=EPV[bs_x];*/

                        if(mb_available_up_right){
                            for(x=0;x<bs_y;x++)
                                EPV[1+x+bs_x]=imgUV[uv][img->pix_c_y-1][img->pix_c_x+bs_x+x];
                        }
                        else{
                            for(x=0;x<bs_y;x++)
                                EPV[1+x+bs_x]=EPV[bs_x];  //bs_x=8; EPV[9~16]=r[8]
                        }
                        //by oliver according to 1658
                        EPV[0]=imgUV[uv][img->pix_c_y-1][img->pix_c_x];
                    }
                    if(mb_available_left)
                    {
                        for(y=0;y<bs_y;y++)
                            EPV[-1-y]=imgUV[uv][img->pix_c_y+y][img->pix_c_x-1];

                        for(y=0;y<bs_x;y++)
                            EPV[-1-y-bs_y]=EPV[-bs_y];

                        EPV[0]=imgUV[uv][img->pix_c_y][img->pix_c_x-1];
                    }
                    if(mb_available_up&&mb_available_left)
                        EPV[0]=imgUV[uv][img->pix_c_y-1][img->pix_c_x-1];

                    //lowpass (Those emlements that are not needed will not disturb)
                    last_pix=EPV[-8];
                    for(i=-8;i<=8;i++)
                    {
                        new_pix=( last_pix + (EPV[i]<<1) + EPV[i+1] + 2 )>>2;
                        last_pix=EPV[i];
                        EPV[i]=(unsigned char)new_pix;
                    }
                }
            }

            for (j=4;j<8;j++)
            {
                joff=(j-4)*4;


                for(i=0;i<2;i++)
                {
                    ioff=i*4;

                    b88 = block8x8_idx[joff>>2][ioff>>2];

                    mv_mode  = currMB->b8mode[b88];
                    pred_dir = currMB->b8pdir[b88];
                    jf=img->block_y+(b88/2);
                    ifx=img->block_x+(b88%2);


                    if (currMB->mb_type==I4MB)
                    {
                        //--- INTRA PREDICTION ---
                        int pred;
                        int ih, iv, ib, ic, iaa;

                        switch (currMB->c_ipred_mode)
                        {
                        case DC_PRED_8:
                            if (uv==0)
                            {
                                if      (!mb_available_up && !mb_available_left)
                                    for (ii=0; ii<4; ii++)
                                        for (jj=0; jj<4; jj++)
                                        {
                                            img->mpr[ii+ioff][jj+joff]=128;
                                        }
                                        if      (mb_available_up && !mb_available_left)
                                            for (ii=0; ii<4; ii++)
                                                for (jj=0; jj<4; jj++)
                                                {
                                                    img->mpr[ii+ioff][jj+joff]=EPU[1+ii+ioff];
                                                }
                                                if      (!mb_available_up && mb_available_left)
                                                    for (ii=0; ii<4; ii++)
                                                        for (jj=0; jj<4; jj++)
                                                        {
                                                            img->mpr[ii+ioff][jj+joff]=EPU[-1-jj-joff];
                                                        }
                                                        if      (mb_available_up && mb_available_left)
                                                            for (ii=0; ii<4; ii++)
                                                                for (jj=0; jj<4; jj++)
                                                                {
                                                                    img->mpr[ii+ioff][jj+joff]=(EPU[1+ii+ioff]+EPU[-1-jj-joff])>>1;
                                                                }
                            }
                            if (uv==1)
                            {
                                if      (!mb_available_up && !mb_available_left)
                                    for (ii=0; ii<4; ii++)
                                        for (jj=0; jj<4; jj++)
                                        {
                                            img->mpr[ii+ioff][jj+joff]=128;
                                        }
                                        if      (mb_available_up && !mb_available_left)
                                            for (ii=0; ii<4; ii++)
                                                for (jj=0; jj<4; jj++)
                                                {
                                                    img->mpr[ii+ioff][jj+joff]=EPV[1+ii+ioff];
                                                }
                                                if      (!mb_available_up && mb_available_left)
                                                    for (ii=0; ii<4; ii++)
                                                        for (jj=0; jj<4; jj++)
                                                        {
                                                            img->mpr[ii+ioff][jj+joff]=EPV[-1-jj-joff];
                                                        }
                                                        if      (mb_available_up && mb_available_left)
                                                            for (ii=0; ii<4; ii++)
                                                                for (jj=0; jj<4; jj++)
                                                                {
                                                                    img->mpr[ii+ioff][jj+joff]=(EPV[1+ii+ioff]+EPV[-1-jj-joff])>>1;
                                                                }
                            }
                            break;
                        case HOR_PRED_8:
                            if (!mb_available_left)
                                error("unexpected HOR_PRED_8 chroma intra prediction mode",-1);
                            for (jj=0; jj<4; jj++)
                            {
                                pred = imgUV[uv][img->pix_c_y+jj+joff][img->pix_c_x-1];
                                for (ii=0; ii<4; ii++)
                                    img->mpr[ii+ioff][jj+joff]=pred;
                            }
                            break;
                        case VERT_PRED_8:
                            if (!mb_available_up)
                                error("unexpected VERT_PRED_8 chroma intra prediction mode",-1);
                            for (ii=0; ii<4; ii++)
                            {
                                pred = imgUV[uv][img->pix_c_y-1][img->pix_c_x+ii+ioff];
                                for (jj=0; jj<4; jj++)
                                    img->mpr[ii+ioff][jj+joff]=pred;
                            }
                            break;
                        case PLANE_8:
                            if (!mb_available_left || !mb_available_up)
                                error("unexpected PLANE_8 chroma intra prediction mode",-1);
                            ih=iv=0;
                            for (ii=1;ii<5;ii++)
                            {
                                ih += ii*(imgUV[uv][img->pix_c_y-1][img->pix_c_x+3+ii] - imgUV[uv][img->pix_c_y-1][img->pix_c_x+3-ii]);
                                iv += ii*(imgUV[uv][img->pix_c_y+3+ii][img->pix_c_x-1] - imgUV[uv][img->pix_c_y+3-ii][img->pix_c_x-1]);
                            }
                            ib=(17*ih+16)>>5;
                            ic=(17*iv+16)>>5;
                            iaa=16*(imgUV[uv][img->pix_c_y-1][img->pix_c_x+7]+imgUV[uv][img->pix_c_y+7][img->pix_c_x-1]);
                            for (ii=0; ii<4; ii++)
                                for (jj=0; jj<4; jj++)
                                    img->mpr[ii+ioff][jj+joff]=max(0,min(255,(iaa+(ii+ioff-3)*ib +(jj+joff-3)*ic + 16)/32));
                            break;
                        default:
                            error("illegal chroma intra prediction mode", 600);
                            break;
                        }
                    }
                    else if (pred_dir != 2)
                    {
                        //--- FORWARD/BACKWARD PREDICTION ---
                        if (!bframe)
                        {
                            mv_array = img->mv;
                        }
                        else if (!pred_dir)
                        {
                            mv_array = img->fw_mv;
                        }
                        else
                        {
                            mv_array = img->bw_mv;
                        }

                        for(jj=0;jj<4;jj++)
                        {

                            for(ii=0;ii<4;ii++)
                            {
                                if (!bframe)
                                {
                                    refframe = refFrArr[jf][ifx];
                                    fw_ref_idx = refFrArr[jf][ifx];
                                }
                                else if (!pred_dir)
                                {
                                    refframe = img->fw_refFrArr[jf][ifx];// fwd_ref_idx_to_refframe(img->fw_refFrArr[jf][if1]);
                                    fw_ref_idx = img->fw_refFrArr[jf][ifx];
                                }
                                else
                                {
                                    refframe = img->bw_refFrArr[jf][ifx];// bwd_ref_idx_to_refframe(img->bw_refFrArr[jf][if1]);
                                    bw_ref_idx = img->bw_refFrArr[jf][ifx];
                                    bw_ref_idx = bw_ref_idx;
                                }

                                i1=(img->pix_c_x+ii+ioff)*f1+mv_array[ifx+4][jf][0];
                                j1=(img->pix_c_y+jj+joff)*f1+2*mv_array[ifx+4][jf][1];

                                ii0=max (0, min (i1/f1, img->width_cr-1));
                                jj0=max (0, min (j1/f1, img->height_cr-1));
                                ii1=max (0, min ((i1+f2)/f1, img->width_cr-1));
                                jj1=max (0, min ((j1+f2)/f1, img->height_cr-1));

                                if1=(i1 & f2);
                                jf1=(j1 & f2);
                                if0=f1-if1;
                                jf0=f1-jf1;

                                if(!bframe)
                                {
                                    img->mpr[ii+ioff][jj+joff]=
                                        (if0*jf0*mcef[refframe][uv][jj0][ii0]+
                                        if1*jf0*mcef[refframe][uv][jj0][ii1]+
                                        if0*jf1*mcef[refframe][uv][jj1][ii0]+
                                        if1*jf1*mcef[refframe][uv][jj1][ii1]+f4)/f3;

                                    //cjw 20051219 Weighted Predition
                                    // if(((currMB->mb_type != 0 )&&(img->slice_weighting_flag == 1) &&
                                    //          if(((img->slice_weighting_flag == 1) &&    //cjw20060321
                                    //            (img->allframeweight == 1)&&(img->mbweightflag == 1))
                                    //            ||((img->slice_weighting_flag == 1) && (img->allframeweight == 0)))
                                    //          {
                                    //            fw_chrom_scale = img->chroma_scale[refframe];
                                    //            fw_chrom_shift = img->chroma_shift[refframe];
                                    //
                                    //            img->mpr[ii+ioff][jj+joff]=Clip1(((img->mpr[ii+ioff][jj+joff]*fw_chrom_scale+16)>>5)+fw_chrom_shift);
                                    //          }
                                }
                                else
                                {
                                    if(img->picture_structure)
                                        refframe = 0;

                                    if(!pred_dir) //cjw 20051219 forward
                                    {
                                        img->mpr[ii+ioff][jj+joff]=
                                            (if0*jf0*mcef_fref[refframe][uv][jj0][ii0]+
                                            if1*jf0*mcef_fref[refframe][uv][jj0][ii1]+
                                            if0*jf1*mcef_fref[refframe][uv][jj1][ii0]+
                                            if1*jf1*mcef_fref[refframe][uv][jj1][ii1]+f4)/f3;

                                        //        //cjw 20051219 Weighted Predition
                                        //             if(((img->slice_weighting_flag == 1) &&(img->allframeweight == 1)&&(img->mbweightflag == 1))
                                        //            ||((img->slice_weighting_flag == 1) && (img->allframeweight == 0)))
                                        //        {
                                        //
                                        //              refframe=(img->picture_structure)?(refframe):(2*refframe);  //cjw 20060112 fw
                                        //
                                        //            fw_chrom_scale = img->chroma_scale[refframe];
                                        //            fw_chrom_shift = img->chroma_shift[refframe];
                                        //
                                        //              img->mpr[ii+ioff][jj+joff]=Clip1(((img->mpr[ii+ioff][jj+joff]*fw_chrom_scale+16)>>5)+fw_chrom_shift);
                                        //          }
                                    }
                                    else   //cjw 20051219 backward
                                    {
                                        img->mpr[ii+ioff][jj+joff]=
                                            (if0*jf0*mcef_bref[refframe][uv][jj0][ii0]+
                                            if1*jf0*mcef_bref[refframe][uv][jj0][ii1]+
                                            if0*jf1*mcef_bref[refframe][uv][jj1][ii0]+
                                            if1*jf1*mcef_bref[refframe][uv][jj1][ii1]+f4)/f3;

                                        //cjw 20051219 Weighted Predition
                                        //             if(((img->slice_weighting_flag == 1) &&(img->allframeweight == 1)&&(img->mbweightflag == 1))
                                        //            ||((img->slice_weighting_flag == 1) && (img->allframeweight == 0)))
                                        //          {
                                        //
                                        //              refframe=(img->picture_structure)?(refframe+1):(2*refframe+1);  //cjw 20060112 bw
                                        //
                                        //              fw_chrom_scale = img->chroma_scale[refframe];
                                        //            fw_chrom_shift = img->chroma_shift[refframe];
                                        //
                                        //            img->mpr[ii+ioff][jj+joff]=Clip1(((img->mpr[ii+ioff][jj+joff]*fw_chrom_scale+16)>>5)+fw_chrom_shift);
                                        //          }
                                    }
                                }
                            }
                        }
                    }
                    else // !! bidirection prediction
                    {
                        if (mv_mode != 0)
                        {
                            //===== BI-DIRECTIONAL PREDICTION =====
                            fw_mv_array = img->fw_mv;
                            bw_mv_array = img->bw_mv;
                        }
                        else
                        {
                            //===== DIRECT PREDICTION =====
                            fw_mv_array = img->dfMV;
                            bw_mv_array = img->dbMV;
                        }


                        for(jj=0;jj<4;jj++)
                        {

                            for(ii=0;ii<4;ii++)
                            {
                                direct_pdir = 2;

                                if (mv_mode != 0)
                                {
                                    fw_refframe = img->fw_refFrArr[jf][ifx];// fwd_ref_idx_to_refframe(img->fw_refFrArr[jf][ifx]);
                                    bw_refframe = img->bw_refFrArr[jf][ifx];//bwd_ref_idx_to_refframe(img->bw_refFrArr[jf][ifx]);

                                    fw_ref_idx = img->fw_refFrArr[jf][ifx];
                                    bw_ref_idx = img->bw_refFrArr[jf][ifx];
                                    bw_ref_idx = bw_ref_idx;
                                }
                                else
                                {
                                    fw_refframe = 0;
                                    bw_refframe = 0;

                                    if(!img->picture_structure)
                                    {
                                        fw_refframe = img->fw_refFrArr[jf][ifx];
                                        bw_refframe = img->bw_refFrArr[jf][ifx];
                                    }
                                }

                                i1=(img->pix_c_x+ii+ioff)*f1+fw_mv_array[ifx+4][jf][0];
                                j1=(img->pix_c_y+jj+joff)*f1+2*fw_mv_array[ifx+4][jf][1];

                                ii0=max (0, min (i1/f1, img->width_cr-1));
                                jj0=max (0, min (j1/f1, img->height_cr-1));
                                ii1=max (0, min ((i1+f2)/f1, img->width_cr-1));
                                jj1=max (0, min ((j1+f2)/f1, img->height_cr-1));

                                if1=(i1 & f2);
                                jf1=(j1 & f2);
                                if0=f1-if1;
                                jf0=f1-jf1;

                                fw_pred=(if0*jf0*mcef_fref[fw_refframe][uv][jj0][ii0]+
                                    if1*jf0*mcef_fref[fw_refframe][uv][jj0][ii1]+
                                    if0*jf1*mcef_fref[fw_refframe][uv][jj1][ii0]+
                                    if1*jf1*mcef_fref[fw_refframe][uv][jj1][ii1]+f4)/f3;

                                i1=(img->pix_c_x+ii+ioff)*f1+bw_mv_array[ifx+4][jf][0];
                                j1=(img->pix_c_y+jj+joff)*f1+2*bw_mv_array[ifx+4][jf][1];

                                ii0=max (0, min (i1/f1, img->width_cr-1));
                                jj0=max (0, min (j1/f1, img->height_cr-1));
                                ii1=max (0, min ((i1+f2)/f1, img->width_cr-1));
                                jj1=max (0, min ((j1+f2)/f1, img->height_cr-1));

                                if1=(i1 & f2);
                                jf1=(j1 & f2);
                                if0=f1-if1;
                                jf0=f1-jf1;

                                bw_pred=(if0*jf0*mcef_bref[bw_refframe][uv][jj0][ii0]+
                                    if1*jf0*mcef_bref[bw_refframe][uv][jj0][ii1]+
                                    if0*jf1*mcef_bref[bw_refframe][uv][jj1][ii0]+
                                    if1*jf1*mcef_bref[bw_refframe][uv][jj1][ii1]+f4)/f3;

                                img->mpr[ii+ioff][jj+joff]=(fw_pred + bw_pred + 1 )/2;   //<! Replaced with integer only operations

                                //cjw 20051219 Weighted Predition
                                //         if(((img->slice_weighting_flag == 1) &&(img->allframeweight == 1)&&(img->mbweightflag == 1)&&(mv_mode != 0))
                                //          ||((img->slice_weighting_flag == 1) && (img->allframeweight == 0)))
                                //        {
                                //
                                //            fw_refframe=(img->picture_structure)?(fw_refframe):(2*fw_refframe);  //cjw 20060112 fw
                                //          bw_refframe=(img->picture_structure)?(bw_refframe+1):(2*bw_refframe+1);  //cjw 20060112 bw
                                //
                                //            fw_chrom_scale = img->chroma_scale[fw_refframe];
                                //          fw_chrom_shift = img->chroma_shift[fw_refframe];
                                //
                                //          bw_chrom_scale = img->chroma_scale[bw_refframe];
                                //          bw_chrom_shift = img->chroma_shift[bw_refframe];
                                //
                                //          img->mpr[ii+ioff][jj+joff]=((Clip1(((fw_pred*fw_chrom_scale+16)>>5)+fw_chrom_shift)
                                //                 +Clip1(((bw_pred*bw_chrom_scale+16)>>5)+bw_chrom_shift))+1)/2;
                                //
                                //        }
                            }
                        }
                    }
                }
            }






            get_curr_blk (4+uv, img, curr_blk);
            idct_dequant_B8 (4+uv, QP_SCALE_CR[currMB->qp-MIN_QP], curr_blk, img);

            if (currMB->mb_type!=I4MB) {
                get_curr_blk (6+uv, img, curr_blk);
                idct_dequant_B8 (6+uv, QP_SCALE_CR[currMB->qp-MIN_QP], curr_blk, img);
            }

            //block 6&7's intra decoding, added by X ZHENG,422
            {
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
                int   mb_available_up_right=0;
                int mb_available_left_down=((img_cx==0)||(b8_y>=(img->height_cr/BLOCK_SIZE-2))) ? 0 : (mb_data[img->current_mb_nr].slice_nr == mb_data[img->current_mb_nr+mb_width-1].slice_nr);
                int mb_available_up   = 1;
                int mb_available_left = (img_cx == 0) ? 0 : (mb_data[img->current_mb_nr].slice_nr == mb_data[img->current_mb_nr-1].slice_nr);
                int mb_available_up_left = (img_cx/BLOCK_SIZE == 0) ? 0 : (mb_data[img->current_mb_nr].slice_nr == mb_data[img->current_mb_nr-1].slice_nr);

                int pred;
                int ih, iv, ib, ic, iaa;

                if (currMB->mb_type!=I4MB)
                    continue;

                if (uv==0)
                {
                    if(mb_available_up)
                    {
                        for(x=0;x<bs_x;x++)
                            EPU[x+1]=imgUV[uv][img_cy_1][img_cx+x];

                        if(mb_available_up_right){
                            for(x=0;x<bs_y;x++)
                                EPU[1+x+bs_x]=imgUV[uv][img_cy_1][img_cx+bs_x+x];
                        }
                        else{
                            for(x=0;x<bs_y;x++)
                                EPU[1+x+bs_x]=EPU[bs_x];  //bs_x=8; EPU[9~16]=r[8]
                        }
                        //by oliver according to 1658


                        EPU[0]=imgUV[uv][img_cy_1][img_cx];
                    }
                    if(mb_available_left)
                    {
                        for(y=0;y<bs_y;y++)
                            EPU[-1-y]=imgUV[uv][img_cy+y][img_cx_1];

                        for(y=0;y<bs_x;y++)
                            EPU[-1-y-bs_y]=EPU[-bs_y];

                        EPU[0]=imgUV[uv][img_cy][img_cx_1];
                    }
                    if(mb_available_up&&mb_available_left)
                        EPU[0]=imgUV[uv][img_cy_1][img_cx_1];

                    //lowpass (Those emlements that are not needed will not disturb)
                    last_pix=EPU[-8];
                    for(i=-8;i<=8;i++)
                    {
                        new_pix=( last_pix + (EPU[i]<<1) + EPU[i+1] + 2 )>>2;
                        last_pix=EPU[i];
                        EPU[i]=(unsigned char)new_pix;
                    }
                }

                if (uv==1)
                {
                    if(mb_available_up)
                    {
                        for(x=0;x<bs_x;x++)
                            EPV[x+1]=imgUV[uv][img_cy_1][img_cx+x];

                        if(mb_available_up_right){
                            for(x=0;x<bs_y;x++)
                                EPV[1+x+bs_x]=imgUV[uv][img_cy_1][img_cx+bs_x+x];
                        }
                        else{
                            for(x=0;x<bs_y;x++)
                                EPV[1+x+bs_x]=EPV[bs_x];  //bs_x=8; EPV[9~16]=r[8]
                        }
                        //by oliver according to 1658
                        EPV[0]=imgUV[uv][img_cy_1][img_cx];
                    }
                    if(mb_available_left)
                    {
                        for(y=0;y<bs_y;y++)
                            EPV[-1-y]=imgUV[uv][img_cy+y][img_cx_1];

                        for(y=0;y<bs_x;y++)
                            EPV[-1-y-bs_y]=EPV[-bs_y];

                        EPV[0]=imgUV[uv][img_cy][img_cx_1];
                    }
                    if(mb_available_up&&mb_available_left)
                        EPV[0]=imgUV[uv][img_cy_1][img_cx_1];

                    //lowpass (Those emlements that are not needed will not disturb)
                    last_pix=EPV[-8];
                    for(i=-8;i<=8;i++)
                    {
                        new_pix=( last_pix + (EPV[i]<<1) + EPV[i+1] + 2 )>>2;
                        last_pix=EPV[i];
                        EPV[i]=(unsigned char)new_pix;
                    }
                }

                for (j=6;j<8;j++)
                {
                    joff=(j-4)*4;
                    j4=img->pix_c_y+joff;

                    for(i=0;i<2;i++)
                    {
                        ioff=i*4;
                        i4=img->pix_c_x+ioff;

                        switch (currMB->c_ipred_mode_2)
                        {
                        case DC_PRED_8:
                            if (uv==0)
                            {
                                if      (!mb_available_up && !mb_available_left)
                                    for (ii=0; ii<4; ii++)
                                        for (jj=0; jj<4; jj++)
                                        {
                                            img->mpr[ii+ioff][jj+joff]=128;
                                        }
                                        if      (mb_available_up && !mb_available_left)
                                            for (ii=0; ii<4; ii++)
                                                for (jj=0; jj<4; jj++)
                                                {
                                                    img->mpr[ii+ioff][jj+joff]=EPU[1+ii+ioff];
                                                }
                                                if      (!mb_available_up && mb_available_left)
                                                    for (ii=0; ii<4; ii++)
                                                        for (jj=0; jj<4; jj++)
                                                        {
                                                            img->mpr[ii+ioff][jj+joff]=EPU[-1-jj-joff+8];
                                                        }
                                                        if      (mb_available_up && mb_available_left)
                                                            for (ii=0; ii<4; ii++)
                                                                for (jj=0; jj<4; jj++)
                                                                {
                                                                    img->mpr[ii+ioff][jj+joff]=(EPU[1+ii+ioff]+EPU[-1-jj-joff+8])>>1;
                                                                }
                            }
                            if (uv==1)
                            {
                                if      (!mb_available_up && !mb_available_left)
                                    for (ii=0; ii<4; ii++)
                                        for (jj=0; jj<4; jj++)
                                        {
                                            img->mpr[ii+ioff][jj+joff]=128;
                                        }
                                        if      (mb_available_up && !mb_available_left)
                                            for (ii=0; ii<4; ii++)
                                                for (jj=0; jj<4; jj++)
                                                {
                                                    img->mpr[ii+ioff][jj+joff]=EPV[1+ii+ioff];
                                                }
                                                if      (!mb_available_up && mb_available_left)
                                                    for (ii=0; ii<4; ii++)
                                                        for (jj=0; jj<4; jj++)
                                                        {
                                                            img->mpr[ii+ioff][jj+joff]=EPV[-1-jj-joff+8];
                                                        }
                                                        if      (mb_available_up && mb_available_left)
                                                            for (ii=0; ii<4; ii++)
                                                                for (jj=0; jj<4; jj++)
                                                                {
                                                                    img->mpr[ii+ioff][jj+joff]=(EPV[1+ii+ioff]+EPV[-1-jj-joff+8])>>1;
                                                                }
                            }
                            break;
                        case HOR_PRED_8:
                            if (!mb_available_left)
                                error("unexpected HOR_PRED_8 chroma intra prediction mode",-1);
                            for (jj=0; jj<4; jj++)
                            {
                                pred = imgUV[uv][img->pix_c_y+jj+joff][img->pix_c_x-1];
                                for (ii=0; ii<4; ii++)
                                    img->mpr[ii+ioff][jj+joff]=pred;
                            }
                            break;
                        case VERT_PRED_8:
                            if (!mb_available_up)
                                error("unexpected VERT_PRED_8 chroma intra prediction mode",-1);
                            for (ii=0; ii<4; ii++)
                            {
                                pred = imgUV[uv][img->pix_c_y-1+8][img->pix_c_x+ii+ioff];
                                for (jj=0; jj<4; jj++)
                                    img->mpr[ii+ioff][jj+joff]=pred;
                            }
                            break;
                        case PLANE_8:
                            if (!mb_available_left || !mb_available_up)
                                error("unexpected PLANE_8 chroma intra prediction mode",-1);
                            ih=iv=0;
                            for (ii=1;ii<5;ii++)
                            {
                                ih += ii*(imgUV[uv][img->pix_c_y-1+8][img->pix_c_x+3+ii] - imgUV[uv][img->pix_c_y-1+8][img->pix_c_x+3-ii]);
                                iv += ii*(imgUV[uv][img->pix_c_y+3+ii+8][img->pix_c_x-1] - imgUV[uv][img->pix_c_y+3-ii+8][img->pix_c_x-1]);
                            }
                            ib=(17*ih+16)>>5;
                            ic=(17*iv+16)>>5;
                            iaa=16*(imgUV[uv][img->pix_c_y-1+8][img->pix_c_x+7]+imgUV[uv][img->pix_c_y+7+8][img->pix_c_x-1]);
                            for (ii=0; ii<4; ii++)
                                for (jj=0; jj<4; jj++)
                                    img->mpr[ii+ioff][jj+joff]=max(0,min(255,(iaa+(ii+ioff-3)*ib +(jj+joff-3-8)*ic + 16)/32));
                            break;
                        default:
                            error("illegal chroma intra prediction mode", 600);
                            break;
                        }
                    }
                }
                get_curr_blk (6+uv, img, curr_blk);
                idct_dequant_B8 (6+uv, QP_SCALE_CR[currMB->qp-MIN_QP], curr_blk, img);
            }

        }
    }
    else if(chroma_format==1)
    {
        for(uv=0;uv<2;uv++)
        {
            intra_prediction = IS_INTRA (currMB);

            if (intra_prediction)
            {
                if (uv==0)
                {
                    if(mb_available_up)
                    {
                        for(x=0;x<bs_x;x++)
                            EPU[x+1]=imgUV[uv][img->pix_c_y-1][img->pix_c_x+x];

                        /*
                        for(x=0;x<bs_y;x++)
                        EPU[1+x+bs_x]=EPU[bs_x];*/
                        if(mb_available_up_right){
                            for(x=0;x<bs_y;x++)
                                EPU[1+x+bs_x]=imgUV[uv][img->pix_c_y-1][img->pix_c_x+bs_x+x];
                        }
                        else{
                            for(x=0;x<bs_y;x++)
                                EPU[1+x+bs_x]=EPU[bs_x];  //bs_x=8; EPU[9~16]=r[8]
                        }
                        //by oliver according to 1658


                        EPU[0]=imgUV[uv][img->pix_c_y-1][img->pix_c_x];
                    }
                    if(mb_available_left)
                    {
                        for(y=0;y<bs_y;y++)
                            EPU[-1-y]=imgUV[uv][img->pix_c_y+y][img->pix_c_x-1];

                        for(y=0;y<bs_x;y++)
                            EPU[-1-y-bs_y]=EPU[-bs_y];

                        EPU[0]=imgUV[uv][img->pix_c_y][img->pix_c_x-1];
                    }
                    if(mb_available_up&&mb_available_left)
                        EPU[0]=imgUV[uv][img->pix_c_y-1][img->pix_c_x-1];

                    //lowpass (Those emlements that are not needed will not disturb)
                    last_pix=EPU[-8];
                    for(i=-8;i<=8;i++)
                    {
                        new_pix=( last_pix + (EPU[i]<<1) + EPU[i+1] + 2 )>>2;
                        last_pix=EPU[i];
                        EPU[i]=(unsigned char)new_pix;
                    }
                }


                if (uv==1)
                {
                    if(mb_available_up)
                    {
                        for(x=0;x<bs_x;x++)
                            EPV[x+1]=imgUV[uv][img->pix_c_y-1][img->pix_c_x+x];

                        /*          for(x=0;x<bs_y;x++)
                        EPV[1+x+bs_x]=EPV[bs_x];*/

                        if(mb_available_up_right){
                            for(x=0;x<bs_y;x++)
                                EPV[1+x+bs_x]=imgUV[uv][img->pix_c_y-1][img->pix_c_x+bs_x+x];
                        }
                        else{
                            for(x=0;x<bs_y;x++)
                                EPV[1+x+bs_x]=EPV[bs_x];  //bs_x=8; EPV[9~16]=r[8]
                        }
                        //by oliver according to 1658
                        EPV[0]=imgUV[uv][img->pix_c_y-1][img->pix_c_x];
                    }
                    if(mb_available_left)
                    {
                        for(y=0;y<bs_y;y++)
                            EPV[-1-y]=imgUV[uv][img->pix_c_y+y][img->pix_c_x-1];

                        for(y=0;y<bs_x;y++)
                            EPV[-1-y-bs_y]=EPV[-bs_y];

                        EPV[0]=imgUV[uv][img->pix_c_y][img->pix_c_x-1];
                    }
                    if(mb_available_up&&mb_available_left)
                        EPV[0]=imgUV[uv][img->pix_c_y-1][img->pix_c_x-1];

                    //lowpass (Those emlements that are not needed will not disturb)
                    last_pix=EPV[-8];
                    for(i=-8;i<=8;i++)
                    {
                        new_pix=( last_pix + (EPV[i]<<1) + EPV[i+1] + 2 )>>2;
                        last_pix=EPV[i];
                        EPV[i]=(unsigned char)new_pix;
                    }
                }
            }

            for (j=4;j<6;j++)
            {
                joff=(j-4)*4;
                j4=img->pix_c_y+joff;

                for(i=0;i<2;i++)
                {
                    ioff=i*4;
                    i4=img->pix_c_x+ioff;

                    mv_mode  = currMB->b8mode[2*(j-4)+i];
                    pred_dir = currMB->b8pdir[2*(j-4)+i];

                    // PREDICTION
                    if (currMB->mb_type==I4MB)
                    {
                        //--- INTRA PREDICTION ---
                        int pred;
                        int ih, iv, ib, ic, iaa;

                        switch (currMB->c_ipred_mode)
                        {
                        case DC_PRED_8:
                            if (uv==0)
                            {
                                if      (!mb_available_up && !mb_available_left)
                                    for (ii=0; ii<4; ii++)
                                        for (jj=0; jj<4; jj++)
                                        {
                                            img->mpr[ii+ioff][jj+joff]=128;
                                        }
                                        if      (mb_available_up && !mb_available_left)
                                            for (ii=0; ii<4; ii++)
                                                for (jj=0; jj<4; jj++)
                                                {
                                                    img->mpr[ii+ioff][jj+joff]=EPU[1+ii+ioff];
                                                }
                                                if      (!mb_available_up && mb_available_left)
                                                    for (ii=0; ii<4; ii++)
                                                        for (jj=0; jj<4; jj++)
                                                        {
                                                            img->mpr[ii+ioff][jj+joff]=EPU[-1-jj-joff];
                                                        }
                                                        if      (mb_available_up && mb_available_left)
                                                            for (ii=0; ii<4; ii++)
                                                                for (jj=0; jj<4; jj++)
                                                                {
                                                                    img->mpr[ii+ioff][jj+joff]=(EPU[1+ii+ioff]+EPU[-1-jj-joff])>>1;
                                                                }
                            }
                            if (uv==1)
                            {
                                if      (!mb_available_up && !mb_available_left)
                                    for (ii=0; ii<4; ii++)
                                        for (jj=0; jj<4; jj++)
                                        {
                                            img->mpr[ii+ioff][jj+joff]=128;
                                        }
                                        if      (mb_available_up && !mb_available_left)
                                            for (ii=0; ii<4; ii++)
                                                for (jj=0; jj<4; jj++)
                                                {
                                                    img->mpr[ii+ioff][jj+joff]=EPV[1+ii+ioff];
                                                }
                                                if      (!mb_available_up && mb_available_left)
                                                    for (ii=0; ii<4; ii++)
                                                        for (jj=0; jj<4; jj++)
                                                        {
                                                            img->mpr[ii+ioff][jj+joff]=EPV[-1-jj-joff];
                                                        }
                                                        if      (mb_available_up && mb_available_left)
                                                            for (ii=0; ii<4; ii++)
                                                                for (jj=0; jj<4; jj++)
                                                                {
                                                                    img->mpr[ii+ioff][jj+joff]=(EPV[1+ii+ioff]+EPV[-1-jj-joff])>>1;
                                                                }
                            }
                            break;
                        case HOR_PRED_8:
                            if (!mb_available_left)
                                error("unexpected HOR_PRED_8 chroma intra prediction mode",-1);
                            for (jj=0; jj<4; jj++)
                            {
                                pred = imgUV[uv][img->pix_c_y+jj+joff][img->pix_c_x-1];
                                for (ii=0; ii<4; ii++)
                                    img->mpr[ii+ioff][jj+joff]=pred;
                            }
                            break;
                        case VERT_PRED_8:
                            if (!mb_available_up)
                                error("unexpected VERT_PRED_8 chroma intra prediction mode",-1);
                            for (ii=0; ii<4; ii++)
                            {
                                pred = imgUV[uv][img->pix_c_y-1][img->pix_c_x+ii+ioff];
                                for (jj=0; jj<4; jj++)
                                    img->mpr[ii+ioff][jj+joff]=pred;
                            }
                            break;
                        case PLANE_8:
                            if (!mb_available_left || !mb_available_up)
                                error("unexpected PLANE_8 chroma intra prediction mode",-1);
                            ih=iv=0;
                            for (ii=1;ii<5;ii++)
                            {
                                ih += ii*(imgUV[uv][img->pix_c_y-1][img->pix_c_x+3+ii] - imgUV[uv][img->pix_c_y-1][img->pix_c_x+3-ii]);
                                iv += ii*(imgUV[uv][img->pix_c_y+3+ii][img->pix_c_x-1] - imgUV[uv][img->pix_c_y+3-ii][img->pix_c_x-1]);
                            }
                            ib=(17*ih+16)>>5;
                            ic=(17*iv+16)>>5;
                            iaa=16*(imgUV[uv][img->pix_c_y-1][img->pix_c_x+7]+imgUV[uv][img->pix_c_y+7][img->pix_c_x-1]);
                            for (ii=0; ii<4; ii++)
                                for (jj=0; jj<4; jj++)
                                    img->mpr[ii+ioff][jj+joff]=max(0,min(255,(iaa+(ii+ioff-3)*ib +(jj+joff-3)*ic + 16)/32));
                            break;
                        default:
                            error("illegal chroma intra prediction mode", 600);
                            break;
                        }
                    }
                    else if (pred_dir != 2)
                    {
                        //--- FORWARD/BACKWARD PREDICTION ---
                        if (!bframe)
                        {
                            mv_array = img->mv;
                        }
                        else if (!pred_dir)
                        {
                            mv_array = img->fw_mv;
                        }
                        else
                        {
                            mv_array = img->bw_mv;
                        }

                        for(jj=0;jj<4;jj++)
                        {
                            jf=(j4+jj)/4;
                            for(ii=0;ii<4;ii++)
                            {
                                if1=(i4+ii)/4;

                                if (!bframe)
                                {
                                    refframe = refFrArr[jf][if1];
                                    fw_ref_idx = refFrArr[jf][if1];
                                }
                                else if (!pred_dir)
                                {
                                    refframe = img->fw_refFrArr[jf][if1];// fwd_ref_idx_to_refframe(img->fw_refFrArr[jf][if1]);
                                    fw_ref_idx = img->fw_refFrArr[jf][if1];
                                }
                                else
                                {
                                    refframe = img->bw_refFrArr[jf][if1];// bwd_ref_idx_to_refframe(img->bw_refFrArr[jf][if1]);
                                    bw_ref_idx = img->bw_refFrArr[jf][if1];
                                    bw_ref_idx = bw_ref_idx;
                                }

                                i1=(img->pix_c_x+ii+ioff)*f1+mv_array[if1+4][jf][0];
                                j1=(img->pix_c_y+jj+joff)*f1+mv_array[if1+4][jf][1];

                                ii0=max (0, min (i1/f1, img->width_cr-1));
                                jj0=max (0, min (j1/f1, img->height_cr-1));
                                ii1=max (0, min ((i1+f2)/f1, img->width_cr-1));
                                jj1=max (0, min ((j1+f2)/f1, img->height_cr-1));

                                if1=(i1 & f2);
                                jf1=(j1 & f2);
                                if0=f1-if1;
                                jf0=f1-jf1;

                                if(!bframe)
                                {
                                    img->mpr[ii+ioff][jj+joff]=(if0*jf0*mcef[refframe][uv][jj0][ii0]+
                                        if1*jf0*mcef[refframe][uv][jj0][ii1]+
                                        if0*jf1*mcef[refframe][uv][jj1][ii0]+
                                        if1*jf1*mcef[refframe][uv][jj1][ii1]+f4)/f3;

                                    //cjw 20051219 Weighted Predition
                                    // if(((currMB->mb_type != 0 )&&(img->slice_weighting_flag == 1) &&
                                    if(((img->slice_weighting_flag == 1) && //cjw20060321
                                        (img->mb_weighting_flag == 1)&&(img->weighting_prediction == 1))
                                        ||((img->slice_weighting_flag == 1) && (img->mb_weighting_flag == 0)))
                                    {
                                        fw_chrom_scale = img->chroma_scale[refframe];
                                        fw_chrom_shift = img->chroma_shift[refframe];

                                        img->mpr[ii+ioff][jj+joff]=Clip1(((img->mpr[ii+ioff][jj+joff]*fw_chrom_scale+16)>>5)+fw_chrom_shift);
                                    }
                                }
                                else
                                {
                                    if(img->picture_structure)
                                        refframe = 0;

                                    if(!pred_dir) //cjw 20051219 forward
                                    {
                                        img->mpr[ii+ioff][jj+joff]=(if0*jf0*mcef_fref[refframe][uv][jj0][ii0]+
                                            if1*jf0*mcef_fref[refframe][uv][jj0][ii1]+
                                            if0*jf1*mcef_fref[refframe][uv][jj1][ii0]+
                                            if1*jf1*mcef_fref[refframe][uv][jj1][ii1]+f4)/f3;

                                        //cjw 20051219 Weighted Predition
                                        if(((img->slice_weighting_flag == 1) &&(img->mb_weighting_flag == 1)&&(img->weighting_prediction == 1))  //cjw20060321
                                            ||((img->slice_weighting_flag == 1) && (img->mb_weighting_flag == 0)))
                                        {

                                            refframe=(img->picture_structure)?(refframe):(2*refframe);  //cjw 20060112 fw

                                            fw_chrom_scale = img->chroma_scale[refframe];
                                            fw_chrom_shift = img->chroma_shift[refframe];

                                            img->mpr[ii+ioff][jj+joff]=Clip1(((img->mpr[ii+ioff][jj+joff]*fw_chrom_scale+16)>>5)+fw_chrom_shift);
                                        }
                                    }
                                    else   //cjw 20051219 backward
                                    {
                                        img->mpr[ii+ioff][jj+joff]=(if0*jf0*mcef_bref[refframe][uv][jj0][ii0]+
                                            if1*jf0*mcef_bref[refframe][uv][jj0][ii1]+
                                            if0*jf1*mcef_bref[refframe][uv][jj1][ii0]+
                                            if1*jf1*mcef_bref[refframe][uv][jj1][ii1]+f4)/f3;

                                        //cjw 20051219 Weighted Predition
                                        if(((img->slice_weighting_flag == 1) &&(img->mb_weighting_flag == 1)&&(img->weighting_prediction == 1))
                                            ||((img->slice_weighting_flag == 1) && (img->mb_weighting_flag == 0)))
                                        {

                                            refframe=(img->picture_structure)?(refframe+1):(2*refframe+1);  //cjw 20060112 bw

                                            fw_chrom_scale = img->chroma_scale[refframe];
                                            fw_chrom_shift = img->chroma_shift[refframe];

                                            img->mpr[ii+ioff][jj+joff]=Clip1(((img->mpr[ii+ioff][jj+joff]*fw_chrom_scale+16)>>5)+fw_chrom_shift);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else // !! bidirection prediction
                    {
                        if (mv_mode != 0)
                        {
                            //===== BI-DIRECTIONAL PREDICTION =====
                            fw_mv_array = img->fw_mv;
                            bw_mv_array = img->bw_mv;
                        }
                        else
                        {
                            //===== DIRECT PREDICTION =====
                            fw_mv_array = img->dfMV;
                            bw_mv_array = img->dbMV;
                        }

                        for(jj=0;jj<4;jj++)
                        {
                            jf=(j4+jj)/4;
                            for(ii=0;ii<4;ii++)
                            {
                                ifx=(i4+ii)/4;

                                direct_pdir = 2;

                                if (mv_mode != 0)
                                {
                                    fw_refframe = img->fw_refFrArr[jf][ifx];// fwd_ref_idx_to_refframe(img->fw_refFrArr[jf][ifx]);
                                    bw_refframe = img->bw_refFrArr[jf][ifx];//bwd_ref_idx_to_refframe(img->bw_refFrArr[jf][ifx]);

                                    fw_ref_idx = img->fw_refFrArr[jf][ifx];
                                    bw_ref_idx = img->bw_refFrArr[jf][ifx];
                                    bw_ref_idx = bw_ref_idx;
                                }
                                else
                                {
                                    fw_refframe = 0;
                                    bw_refframe = 0;

                                    if(!img->picture_structure)
                                    {
                                        fw_refframe = img->fw_refFrArr[jf][ifx];
                                        bw_refframe = img->bw_refFrArr[jf][ifx];
                                    }
                                }

                                i1=(img->pix_c_x+ii+ioff)*f1+fw_mv_array[ifx+4][jf][0];
                                j1=(img->pix_c_y+jj+joff)*f1+fw_mv_array[ifx+4][jf][1];

                                ii0=max (0, min (i1/f1, img->width_cr-1));
                                jj0=max (0, min (j1/f1, img->height_cr-1));
                                ii1=max (0, min ((i1+f2)/f1, img->width_cr-1));
                                jj1=max (0, min ((j1+f2)/f1, img->height_cr-1));

                                if1=(i1 & f2);
                                jf1=(j1 & f2);
                                if0=f1-if1;
                                jf0=f1-jf1;

                                fw_pred=(if0*jf0*mcef_fref[fw_refframe][uv][jj0][ii0]+
                                    if1*jf0*mcef_fref[fw_refframe][uv][jj0][ii1]+
                                    if0*jf1*mcef_fref[fw_refframe][uv][jj1][ii0]+
                                    if1*jf1*mcef_fref[fw_refframe][uv][jj1][ii1]+f4)/f3;

                                i1=(img->pix_c_x+ii+ioff)*f1+bw_mv_array[ifx+4][jf][0];
                                j1=(img->pix_c_y+jj+joff)*f1+bw_mv_array[ifx+4][jf][1];

                                ii0=max (0, min (i1/f1, img->width_cr-1));
                                jj0=max (0, min (j1/f1, img->height_cr-1));
                                ii1=max (0, min ((i1+f2)/f1, img->width_cr-1));
                                jj1=max (0, min ((j1+f2)/f1, img->height_cr-1));

                                if1=(i1 & f2);
                                jf1=(j1 & f2);
                                if0=f1-if1;
                                jf0=f1-jf1;

                                bw_pred=(if0*jf0*mcef_bref[bw_refframe][uv][jj0][ii0]+
                                    if1*jf0*mcef_bref[bw_refframe][uv][jj0][ii1]+
                                    if0*jf1*mcef_bref[bw_refframe][uv][jj1][ii0]+
                                    if1*jf1*mcef_bref[bw_refframe][uv][jj1][ii1]+f4)/f3;

                                img->mpr[ii+ioff][jj+joff]=(fw_pred + bw_pred + 1 )/2;   //<! Replaced with integer only operations

                                //cjw 20051219 Weighted Predition
                                //if(((img->slice_weighting_flag == 1) &&(img->mb_weighting_flag == 1)&&(img->weighting_prediction == 1)&&(mv_mode != 0))
                                if(((img->slice_weighting_flag == 1) &&(img->mb_weighting_flag == 1)&&(img->weighting_prediction == 1))
                                    ||((img->slice_weighting_flag == 1) && (img->mb_weighting_flag == 0)))
                                {

                                    fw_refframe=(img->picture_structure)?(fw_refframe):(2*fw_refframe);  //cjw 20060112 fw
                                    bw_refframe=(img->picture_structure)?(bw_refframe+1):(2*bw_refframe+1);  //cjw 20060112 bw

                                    fw_chrom_scale = img->chroma_scale[fw_refframe];
                                    fw_chrom_shift = img->chroma_shift[fw_refframe];

                                    bw_chrom_scale = img->chroma_scale[bw_refframe];
                                    bw_chrom_shift = img->chroma_shift[bw_refframe];

                                    img->mpr[ii+ioff][jj+joff]=((Clip1(((fw_pred*fw_chrom_scale+16)>>5)+fw_chrom_shift)
                                        +Clip1(((bw_pred*bw_chrom_scale+16)>>5)+bw_chrom_shift))+1)/2;

                                }
                            }
                        }
                    }
                }
            }

            get_curr_blk (4+uv, img, curr_blk);

            idct_dequant_B8 (4+uv, QP_SCALE_CR[currMB->qp-MIN_QP], curr_blk, img);
        }
    }

    return 0;
}