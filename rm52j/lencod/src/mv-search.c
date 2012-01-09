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
#include "contributors.h"

#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

#include "global.h"
#include "mv-search.h"
#include "refbuf.h"
#include "memalloc.h"
#include "block.h"

#ifdef FastME
#include "fast_me.h"
#endif

// These procedure pointers are used by motion_search() and one_eigthpel()
static pel_t (*PelY_14) (pel_t**, int, int);
static pel_t *(*PelYline_11) (pel_t *, int, int);

// Statistics, temporary
int    max_mvd;
int*   spiral_search_x;
int*   spiral_search_y;
int*   mvbits;
int*   refbits;
int*   byte_abs;
int*** motion_cost;
int*** motion_cost_bid;

#define MEDIAN(a,b,c)  (a + b + c - min(a, min(b, c)) - max(a, max(b, c)));  //jlzheng 6.30


/*
******************************************************************************
*  Function: Determine the MVD's value (1/4 pixel) is legal or not.
*  Input: 
*  Output: 
*  Return: 0: out of the legal mv range; 1: in the legal mv range
*  Attention:
*  Author: xiaozhen zheng, 20071009
******************************************************************************
*/
int check_mvd(int mvd_x, int mvd_y)
{

    if(mvd_x>4095 || mvd_x<-4096 || mvd_y>4095 || mvd_y<-4096)
        return 0;

    return 1;
}


/*
******************************************************************************
*  Function: Determine the mv's value (1/4 pixel) is legal or not.
*  Input: 
*  Output: 
*  Return: 0: out of the legal mv range; 1: in the legal mv range
*  Attention:
*  Author: xiaozhen zheng, 20071009
******************************************************************************
*/
int check_mv_range(int mv_x, int mv_y, int pix_x, int pix_y, int blocktype)
{
    int curr_max_x, curr_min_x, curr_max_y, curr_min_y;
    int bx[6] = {8, 16, 16,  8, 8, 4};   // blocktype=0,1,2,3,P8x8, 4x4
    int by[6] = {8, 16,  8, 16, 8, 4};

    curr_max_x = (img->width -  (pix_x+bx[blocktype]))*4 + 16*4;
    curr_min_x = pix_x*4 + 16*4;
    curr_max_y = (img->height - (pix_y+by[blocktype]))*4 + 16*4;
    curr_min_y = pix_y*4 + 16*4;


    if(mv_x>curr_max_x || mv_x<-curr_min_x || mv_x>Max_H_MV || mv_x<Min_H_MV)
        return 0;
    if(mv_y>curr_max_y || mv_y<-curr_min_y || mv_y>Max_V_MV || mv_y<Min_V_MV)
        return 0;

    return 1;
}


/*
******************************************************************************
*  Function: Determine the forward and backward mvs' value (1/4 pixel) is legal or not.
*  Input: 
*  Output: 
*  Return: 0: out of the legal mv range; 1: in the legal mv range
*  Attention:
*  Author: xiaozhen zheng, 20071009
******************************************************************************
*/
int check_mv_range_bid(int mv_x, int mv_y, int pix_x, int pix_y, int blocktype, int ref)
{
    int bw_mvx, bw_mvy;
    int delta_P,TRp,DistanceIndexFw,DistanceIndexBw,refframe ,delta_PB;   
    int curr_max_x, curr_min_x, curr_max_y, curr_min_y;
    int bx[5] = {8, 16, 16,  8, 8};
    int by[5] = {8, 16,  8, 16, 8};

    refframe = ref;
    delta_P = 2*(img->imgtr_next_P_frm - img->imgtr_last_P_frm);
    delta_P = (delta_P + 512)%512;  
    TRp = delta_P;
    delta_PB = 2*(picture_distance - img->imgtr_last_P_frm);  
    TRp  = (TRp + 512)%512;
    delta_PB = (delta_PB + 512)%512;  
    if(!img->picture_structure)
    {
        if(img->current_mb_nr_fld < img->total_number_mb) //top field
            DistanceIndexFw =  refframe == 0 ? delta_PB-1:delta_PB;
        else
            DistanceIndexFw =  refframe == 0 ? delta_PB:delta_PB+1;
    }
    else
        DistanceIndexFw = delta_PB;  

    DistanceIndexBw    = (TRp - DistanceIndexFw+512)%512;

    bw_mvx = - ((mv_x*DistanceIndexBw*(512/DistanceIndexFw)+256)>>9);
    bw_mvy = - ((mv_y*DistanceIndexBw*(512/DistanceIndexFw)+256)>>9);


    curr_max_x = (img->width -  (pix_x+bx[blocktype]))*4 + 16*4;
    curr_min_x = pix_x*4 + 16*4;
    curr_max_y = (img->height - (pix_y+by[blocktype]))*4 + 16*4;
    curr_min_y = pix_y*4 + 16*4;


    if(mv_x>curr_max_x || mv_x<-curr_min_x || mv_x>Max_H_MV || mv_x<Min_H_MV)
        return 0;
    if(mv_y>curr_max_y || mv_y<-curr_min_y || mv_y>Max_V_MV || mv_y<Min_V_MV)
        return 0;

    if(bw_mvx>curr_max_x || bw_mvx<-curr_min_x || bw_mvx>Max_H_MV || bw_mvx<Min_H_MV)
        return 0;
    if(bw_mvy>curr_max_y || bw_mvy<-curr_min_y || bw_mvy>Max_V_MV || bw_mvy<Min_V_MV)
        return 0;

    return 1;
}


/*
******************************************************************************
*  Function: calculated field or frame distance between current field(frame)
*            and the reference field(frame).
*     Input:
*    Output:
*    Return: 
* Attention:
*    Author: Yulj 2004.07.14
******************************************************************************
*/
int calculate_distance(int blkref, int fw_bw )  //fw_bw>=: forward prediction.
{
    int distance=1;
    //if( img->picture_structure == 1 ) // frame
    if ( img->top_bot == -1 )   // frame
    {
        if ( img->type == INTER_IMG ) // P img
        {
            if(blkref==0)
                distance = picture_distance*2 - img->imgtr_last_P_frm*2 ;       // Tsinghua 200701
            else if(blkref==1)
                distance = picture_distance*2 - img->imgtr_last_prev_P_frm*2;    // Tsinghua 200701
            else
            {
                assert(0); //only two reference pictures for P frame
            }
        }
        else //B_IMG
        {
            if (fw_bw >=0 ) //forward
                distance = picture_distance*2 - img->imgtr_last_P_frm*2;       // Tsinghua 200701
            else
                distance = img->imgtr_next_P_frm*2  - picture_distance*2;      // Tsinghua 200701
        } 
    }  
    //else if( !progressive_sequence )
    //else if( ! img->picture_structure )
    else  // field
    {
        if(img->type==INTER_IMG)
        {
            if(img->top_bot==0) //top field
            {
                switch ( blkref )
                {
                case 0:
                    distance = picture_distance*2 - img->imgtr_last_P_frm*2 - 1 ;       // Tsinghua 200701
                    break;
                case 1:
                    distance = picture_distance*2 - img->imgtr_last_P_frm*2 ;            // Tsinghua 200701
                    break;
                case 2:
                    distance = picture_distance*2 - img->imgtr_last_prev_P_frm*2 - 1;    // Tsinghua 200701
                    break;
                case 3:
                    distance = picture_distance*2 - img->imgtr_last_prev_P_frm*2 ;       // Tsinghua 200701
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
                    distance = picture_distance*2 - img->imgtr_last_P_frm*2 ;            // Tsinghua 200701
                    break;
                case 2:
                    distance = picture_distance*2 - img->imgtr_last_P_frm*2 + 1;        // Tsinghua 200701
                    break;
                case 3:
                    distance = picture_distance*2 - img->imgtr_last_prev_P_frm*2 ;    // Tsinghua 200701
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
                        distance = picture_distance*2 - img->imgtr_last_P_frm*2 - 1 ;  // Tsinghua 200701
                        break;
                    case 1:
                        distance = picture_distance*2 - img->imgtr_last_P_frm*2;    // Tsinghua 200701
                        break;
                    }
                }
                else if(img->top_bot==1) // bottom field.
                {
                    switch ( blkref )
                    {
                    case 0:
                        distance = picture_distance*2 - img->imgtr_last_P_frm*2 ;  // Tsinghua 200701
                        break;
                    case 1:
                        distance = picture_distance*2 - img->imgtr_last_P_frm*2 + 1;    // Tsinghua 200701
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
                        distance = img->imgtr_next_P_frm*2 - picture_distance*2;    // Tsinghua 200701
                        break;
                    case 1:
                        distance = img->imgtr_next_P_frm*2 - picture_distance*2 + 1;    // Tsinghua 200701
                        break;
                    }
                }
                else if(img->top_bot==1) // bottom field.
                {
                    switch ( blkref )
                    {
                    case 0:
                        distance = img->imgtr_next_P_frm*2 - picture_distance*2 -  1;  // Tsinghua 200701
                        break;
                    case 1:
                        distance = img->imgtr_next_P_frm*2 - picture_distance*2 ;    // Tsinghua 200701
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
    distance = (distance+512)%512;    // Added by Xiaozhen ZHENG, 20070413, HiSilicon
    return distance;
}
/*Lou 1016 End*/
/*Lou 1016 Start*/
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
    int neighbour_coding_stage = -2;
    int current_coding_stage = -2;
    int sign = (motion_vector>0?1:-1);
    int mult_distance;
    int devide_distance;

    motion_vector = abs(motion_vector);

    if(motion_vector == 0)
        return 0;

    //!!!--- Yulj 2004.07.15
    // The better way is to deinfe ref_frame same as index in motion search function.
    // ref_frame is different from index when it is B field back ward prediction.
    // change ref_frame to index for backwward prediction of B field.
    // ref_frame :   1  |  0  | -2  | -1
    //      index:   1  |  0  |  0  |  1
    //  direction:   f  |  f  |  b  |  b
    if ( img->type == B_IMG && !img->picture_structure && ref < 0 ) 
    {
        currblkref = 1 - currblkref;
        neighbourblkref = 1 - neighbourblkref;
    }
    //---end


    mult_distance = calculate_distance(currblkref, ref);
    devide_distance = calculate_distance(neighbourblkref, ref);

    motion_vector = sign*((motion_vector*mult_distance*(512/devide_distance)+256)>>9);
    //   motion_vector = ((motion_vector * mult_distance * (512 / devide_distance) + 256) >> 9 );

    return motion_vector;
}
/*Lou 1016 End*/

/*
*************************************************************************
* Function:setting the motion vector predictor
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void SetMotionVectorPredictor (int  pmv[2],
                               int  **refFrArr,
                               int  ***tmp_mv,
                               int  ref_frame,
                               int  mb_pix_x,
                               int  mb_pix_y,
                               int  blockshape_x,
                               int  blockshape_y,//Lou 1016
                               int  ref,
                               int  direct_mv)//Lou 1016
{
    int pic_block_x          = (img->block_x>>1) + (mb_pix_x>>3);
    int pic_block_y          = (img->block_y>>1) + (mb_pix_y>>3);
    int mb_nr                = img->current_mb_nr;
    int mb_width             = img->width/16;

    int mb_available_up      = (img->mb_y == 0          ) ? 0 : (img->mb_data[mb_nr].slice_nr == img->mb_data[mb_nr-mb_width  ].slice_nr);  //jlzheng 6.23
    int mb_available_left    = (img->mb_x == 0          ) ? 0 : (img->mb_data[mb_nr].slice_nr == img->mb_data[mb_nr-1         ].slice_nr);  // jlzheng 6.23


    int mb_available_upleft  = (img->mb_x == 0 ||
        img->mb_y == 0          ) ? 0 : (img->mb_data[mb_nr].slice_nr == img->mb_data[mb_nr-mb_width-1].slice_nr);

    int mb_available_upright = (img->mb_x >= mb_width-1 ||
        img->mb_y == 0          ) ? 0 : (img->mb_data[mb_nr].slice_nr == img->mb_data[mb_nr-mb_width+1].slice_nr);  


    int block_available_up, block_available_left, block_available_upright, block_available_upleft;
    int mv_a, mv_b, mv_c, mv_d, pred_vec=0;
    int mvPredType, rFrameL, rFrameU, rFrameUR;
    int hv;
    int mva[3] , mvb[3],mvc[3];
    
    int y_up = 1,y_upright=1,y_upleft=1,off_y=0;

    /*Lou 1016 Start*/
    int rFrameUL;
    Macroblock*     currMB = &img->mb_data[img->current_mb_nr];
    int smbtypecurr, smbtypeL, smbtypeU, smbtypeUL, smbtypeUR;

    smbtypecurr = -2;
    smbtypeL = -2;
    smbtypeU = -2;
    smbtypeUL = -2;
    smbtypeUR = -2;
    /*Lou 1016 End*/

    /* D B C */
    /* A X   */

    /* 1 A, B, D are set to 0 if unavailable       */
    /* 2 If C is not available it is replaced by D */

    block_available_up   = mb_available_up   || (mb_pix_y > 0);
    block_available_left = mb_available_left || (mb_pix_x > 0);


    if (mb_pix_y > 0)
    {
        if (mb_pix_x < 8)  // first column of 8x8 blocks
        {
            if (mb_pix_y==8)
            {
                if (blockshape_x == 16)
                    block_available_upright = 0;
                else
                    block_available_upright = 1;
            }
            else
            {
                if (mb_pix_x+blockshape_x != 8)
                    block_available_upright = 1;
                else
                    block_available_upright = 0;
            }
        }
        else
        {
            if (mb_pix_x+blockshape_x != 16)
                block_available_upright = 1;
            else
                block_available_upright = 0;
        }
    }
    else if (mb_pix_x+blockshape_x != MB_BLOCK_SIZE)
    {
        block_available_upright = block_available_up;
    }
    else
    {
        block_available_upright = mb_available_upright;
    }

    if (mb_pix_x > 0)
    {
        block_available_upleft = (mb_pix_y > 0 ? 1 : block_available_up);
    }
    else if (mb_pix_y > 0)
    {
        block_available_upleft = block_available_left;
    }
    else
    {
        block_available_upleft = mb_available_upleft;
    }

    /*Lou 1016 Start*/
    mvPredType = MVPRED_MEDIAN;

    rFrameL    = block_available_left    ? refFrArr[pic_block_y  -off_y]  [pic_block_x-1] : -1;
    rFrameU    = block_available_up      ? refFrArr[pic_block_y-/*1*/y_up][pic_block_x]   : -1;
    rFrameUR   = block_available_upright ? refFrArr[pic_block_y-/*1*/y_upright][pic_block_x+blockshape_x/8] :
        block_available_upleft  ? refFrArr[pic_block_y-/*1*/y_upleft][pic_block_x-1] : -1;
    rFrameUL   = block_available_upleft  ? refFrArr[pic_block_y-/*1*/y_upleft][pic_block_x-1] : -1;

    if((rFrameL != -1)&&(rFrameU == -1)&&(rFrameUR == -1))
        mvPredType = MVPRED_L;
    else if((rFrameL == -1)&&(rFrameU != -1)&&(rFrameUR == -1))
        mvPredType = MVPRED_U;
    else if((rFrameL == -1)&&(rFrameU == -1)&&(rFrameUR != -1))
        mvPredType = MVPRED_UR;
    /*Lou 1016 End*/

    else if(blockshape_x == 8 && blockshape_y == 16)
    {
        if(mb_pix_x == 0)
        {
            if(rFrameL == ref_frame)
                mvPredType = MVPRED_L;
        }
        else
        {
            if(rFrameUR == ref_frame)
                mvPredType = MVPRED_UR;
        }
    }
    //#endif
    else if(blockshape_x == 16 && blockshape_y == 8)
    {
        if(mb_pix_y == 0)
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

    for (hv=0; hv < 2; hv++)
    {
        mva[hv] = mv_a = block_available_left    ? tmp_mv[hv][pic_block_y - off_y][4+pic_block_x-1]              : 0;
        mvb[hv] = mv_b = block_available_up      ? tmp_mv[hv][pic_block_y-/*1*/y_up][4+pic_block_x]                : 0;
        mv_d = block_available_upleft  ? tmp_mv[hv][pic_block_y-/*1*/y_upleft][4+pic_block_x-1]              : 0;
        mvc[hv] = mv_c = block_available_upright ? tmp_mv[hv][pic_block_y-/*1*/y_upright][4+pic_block_x+blockshape_x/8] : mv_d;

        //--- Yulj 2004.07.04
        // mv_a, mv_b... are not scaled.
        mva[hv] = scale_motion_vector(mva[hv], ref_frame, rFrameL, smbtypecurr, smbtypeL, pic_block_y-off_y, pic_block_y, ref, direct_mv);
        mvb[hv] = scale_motion_vector(mvb[hv], ref_frame, rFrameU, smbtypecurr, smbtypeU, pic_block_y-y_up, pic_block_y, ref, direct_mv);
        mv_d = scale_motion_vector(mv_d, ref_frame, rFrameUL, smbtypecurr, smbtypeUL, pic_block_y-y_upleft, pic_block_y, ref, direct_mv);
        mvc[hv] = block_available_upright ? scale_motion_vector(mvc[hv], ref_frame, rFrameUR, smbtypecurr, smbtypeUR, pic_block_y-y_upright, pic_block_y, ref, direct_mv): mv_d;    


        switch (mvPredType)
        {
        case MVPRED_MEDIAN:


            if(hv == 1){
                // jlzheng 7.2
                // !! for A 
                //       
                mva[2] = abs(mva[0] - mvb[0])  + abs(mva[1] - mvb[1]) ;
                // !! for B
                //       
                mvb[2] = abs(mvb[0] - mvc[0]) + abs(mvb[1] - mvc[1]);
                // !! for C
                //      
                mvc[2] = abs(mvc[0] - mva[0])  + abs(mvc[1] - mva[1]) ;

                pred_vec = MEDIAN(mva[2],mvb[2],mvc[2]);

                if(pred_vec == mva[2]){
                    pmv[0] = mvc[0];
                    pmv[1] = mvc[1];
                }

                else if(pred_vec == mvb[2]){
                    pmv[0] = mva[0];
                    pmv[1] = mva[1];
                }
                else {
                    pmv[0] = mvb[0];
                    pmv[1] = mvb[1];
                }// END

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
        if(mvPredType != MVPRED_MEDIAN)
            pmv[hv] = pred_vec;
    }
}

/*
*************************************************************************
* Function:Initialize the motion search
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void Init_Motion_Search_Module ()
{
    int bits, i, imin, imax, k, l;
    int search_range               = input->search_range;
    int number_of_reference_frames = img->buf_cycle;
    int max_search_points          = (2*search_range+1)*(2*search_range+1);
    int max_ref_bits               = 1 + 2 * (int)floor(log(max(16,number_of_reference_frames+1)) / log(2) + 1e-10);
    int max_ref                    = (1<<((max_ref_bits>>1)+1))-1;
    int number_of_subpel_positions = 4 * (2*search_range+3);
    int max_mv_bits                = 3 + 2 * (int)ceil (log(number_of_subpel_positions+1) / log(2) + 1e-10);

    max_mvd                        = (1<<(( max_mv_bits >>1))   )-1;

    //=====   CREATE ARRAYS   =====
    //-----------------------------
    if ((spiral_search_x = (int*)calloc(max_search_points, sizeof(int))) == NULL)
        no_mem_exit("Init_Motion_Search_Module: spiral_search_x");
    if ((spiral_search_y = (int*)calloc(max_search_points, sizeof(int))) == NULL)
        no_mem_exit("Init_Motion_Search_Module: spiral_search_y");
    if ((mvbits = (int*)calloc(2*max_mvd+1, sizeof(int))) == NULL)
        no_mem_exit("Init_Motion_Search_Module: mvbits");
    if ((refbits = (int*)calloc(max_ref, sizeof(int))) == NULL)
        no_mem_exit("Init_Motion_Search_Module: refbits");
    if ((byte_abs = (int*)calloc(512, sizeof(int))) == NULL)
        no_mem_exit("Init_Motion_Search_Module: byte_abs");

    get_mem3Dint (&motion_cost, 8, 2*(img->buf_cycle+1), 4);

    get_mem3Dint (&motion_cost_bid, 8, 2*(img->buf_cycle+1), 4);

    //--- set array offsets ---
    mvbits   += max_mvd;
    byte_abs += 256;

    //=====   INIT ARRAYS   =====
    //---------------------------
    //--- init array: motion vector bits ---
    mvbits[0] = 1;

    for (bits=3; bits<=max_mv_bits; bits+=2)
    {
        imax = 1    << (bits >> 1);
        imin = imax >> 1;

        for (i = imin; i < imax; i++)
            mvbits[-i] = mvbits[i] = bits;
    }
    //--- init array: reference frame bits ---
    refbits[0] = 1;
    for (bits=3; bits<=max_ref_bits; bits+=2)
    {
        imax = (1   << ((bits >> 1) + 1)) - 1;
        imin = imax >> 1;

        for (i = imin; i < imax; i++)
            refbits[i] = bits;
    }
    //--- init array: absolute value ---
    byte_abs[0] = 0;

    for (i=1; i<256; i++)
        byte_abs[i] = byte_abs[-i] = i;
    //--- init array: search pattern ---
    spiral_search_x[0] = spiral_search_y[0] = 0;
    for (k=1, l=1; l<=max(1,search_range); l++)
    {
        for (i=-l+1; i< l; i++)
        {
            spiral_search_x[k] =  i;  spiral_search_y[k++] = -l;
            spiral_search_x[k] =  i;  spiral_search_y[k++] =  l;
        }
        for (i=-l;   i<=l; i++)
        {
            spiral_search_x[k] = -l;  spiral_search_y[k++] =  i;
            spiral_search_x[k] =  l;  spiral_search_y[k++] =  i;
        }
    }

}

/*
*************************************************************************
* Function:Free memory used by motion search
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void Clear_Motion_Search_Module ()
{
    //--- correct array offset ---
    mvbits   -= max_mvd;
    byte_abs -= 256;

    //--- delete arrays ---
    free (spiral_search_x);
    free (spiral_search_y);
    free (mvbits);
    free (refbits);
    free (byte_abs);
    free_mem3Dint (motion_cost, 8);

}

/*
*************************************************************************
* Function:Full pixel block motion search
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int                                               //  ==> minimum motion cost after search
FullPelBlockMotionSearch_c (pel_t**   orig_pic,     // <--  original pixel values for the AxB block
                            int       ref,          // <--  reference frame (0... or -1 (backward))
                            int       pic_pix_x,    // <--  absolute x-coordinate of regarded AxB block
                            int       pic_pix_y,    // <--  absolute y-coordinate of regarded AxB block
                            int       blocktype,    // <--  block type (1-16x16 ... 7-4x4)
                            int       pred_mv_x,    // <--  motion vector predictor (x) in sub-pel units
                            int       pred_mv_y,    // <--  motion vector predictor (y) in sub-pel units
                            int*      mv_x,         // <--> in: search center (x) / out: motion vector (x) - in pel units
                            int*      mv_y,         // <--> in: search center (y) / out: motion vector (y) - in pel units
                            int       search_range, // <--  1-d search range in pel units
                            int       min_mcost,    // <--  minimum motion cost (cost for center or huge value)
                            double    lambda)       // <--  lagrangian parameter for determining motion cost
{
    int   pos, cand_x, cand_y, y, x8, mcost;
    pel_t *orig_line, *ref_line;
    pel_t *(*get_ref_line)(int, pel_t*, int, int);
    pel_t*  ref_pic       = img->type==B_IMG? Refbuf11 [/*ref+((mref==mref_fld)) +1*/ref+(!img->picture_structure) +1/*by oliver 0511*/] : Refbuf11[ref];

    int   best_pos      = 0;                                        // position with minimum motion cost
    int   max_pos       = (2*search_range+1)*(2*search_range+1);    // number of search positions
    int   lambda_factor = LAMBDA_FACTOR (lambda);                   // factor for determining lagragian motion cost
    int   blocksize_y   = input->blc_size[blocktype][1];            // vertical block size
    int   blocksize_x   = input->blc_size[blocktype][0];            // horizontal block size
    int   blocksize_x8  = blocksize_x >> 3;                         // horizontal block size in 4-pel units
    int   pred_x        = (pic_pix_x << 2) + pred_mv_x;       // predicted position x (in sub-pel units)
    int   pred_y        = (pic_pix_y << 2) + pred_mv_y;       // predicted position y (in sub-pel units)
    int   center_x      = pic_pix_x + *mv_x;                        // center position x (in pel units)
    int   center_y      = pic_pix_y + *mv_y;                        // center position y (in pel units)
    int   check_for_00  = (blocktype==1 && !input->rdopt && img->type!=B_IMG && ref==0);

    int   height        = img->height;

    //===== set function for getting reference picture lines =====
    if ((center_x > search_range) && (center_x < img->width -1-search_range-blocksize_x) &&
        (center_y > search_range) && (center_y < height-1-search_range-blocksize_y)   )
    {
        get_ref_line = FastLineX; //add by wuzhongmou 0610
    }
    else   //add by wuzhongmou 0610
    {
        get_ref_line = UMVLineX;
    }



    //===== loop over all search positions =====
    for (pos=0; pos<max_pos; pos++)
    {
        //--- set candidate position (absolute position in pel units) ---
        cand_x = center_x + spiral_search_x[pos];
        cand_y = center_y + spiral_search_y[pos];

        //--- initialize motion cost (cost for motion vector) and check ---
        mcost = MV_COST (lambda_factor, 2, cand_x, cand_y, pred_x, pred_y);
        if (check_for_00 && cand_x==pic_pix_x && cand_y==pic_pix_y)
        {
            mcost -= WEIGHTED_COST (lambda_factor, 16);
        }

        if (mcost >= min_mcost)
            continue;

        //--- add residual cost to motion cost ---
        for (y=0; y<blocksize_y; y++)
        {
            ref_line  = get_ref_line (blocksize_x, ref_pic, cand_y+y, cand_x);
            orig_line = orig_pic [y];

            for (x8=0; x8<blocksize_x8; x8++)
            {
                mcost += byte_abs[ *orig_line++ - *ref_line++ ];
                mcost += byte_abs[ *orig_line++ - *ref_line++ ];
                mcost += byte_abs[ *orig_line++ - *ref_line++ ];
                mcost += byte_abs[ *orig_line++ - *ref_line++ ];
                mcost += byte_abs[ *orig_line++ - *ref_line++ ];
                mcost += byte_abs[ *orig_line++ - *ref_line++ ];
                mcost += byte_abs[ *orig_line++ - *ref_line++ ];
                mcost += byte_abs[ *orig_line++ - *ref_line++ ];
            }

            if (mcost >= min_mcost)
            {
                break;
            }
        }

        //--- check if motion cost is less than minimum cost ---
        if (mcost < min_mcost)
        {
            best_pos  = pos;
            min_mcost = mcost;
        }
    }

    //===== set best motion vector and return minimum motion cost =====
    if (best_pos)
    {
        *mv_x += spiral_search_x[best_pos];
        *mv_y += spiral_search_y[best_pos];
    }

    return min_mcost;
}


int                                               //  ==> minimum motion cost after search
FullPelBlockMotionSearch_sse (pel_t**   orig_pic,     // <--  original pixel values for the AxB block
                              int       ref,          // <--  reference frame (0... or -1 (backward))
                              int       pic_pix_x,    // <--  absolute x-coordinate of regarded AxB block
                              int       pic_pix_y,    // <--  absolute y-coordinate of regarded AxB block
                              int       blocktype,    // <--  block type (1-16x16 ... 7-4x4)
                              int       pred_mv_x,    // <--  motion vector predictor (x) in sub-pel units
                              int       pred_mv_y,    // <--  motion vector predictor (y) in sub-pel units
                              int*      mv_x,         // <--> in: search center (x) / out: motion vector (x) - in pel units
                              int*      mv_y,         // <--> in: search center (y) / out: motion vector (y) - in pel units
                              int       search_range, // <--  1-d search range in pel units
                              int       min_mcost,    // <--  minimum motion cost (cost for center or huge value)
                              double    lambda)       // <--  lagrangian parameter for determining motion cost
{
    int   pos, cand_x, cand_y, y, x8, mcost;
    pel_t *orig_line, *ref_line;
    pel_t *(*get_ref_line)(int, pel_t*, int, int);
    pel_t*  ref_pic       = img->type==B_IMG? Refbuf11 [/*ref+((mref==mref_fld)) +1*/ref+(!img->picture_structure) +1/*by oliver 0511*/] : Refbuf11[ref];

    int   best_pos      = 0;                                        // position with minimum motion cost
    int   max_pos       = (2*search_range+1)*(2*search_range+1);    // number of search positions
    int   lambda_factor = LAMBDA_FACTOR (lambda);                   // factor for determining lagragian motion cost
    int   blocksize_y   = input->blc_size[blocktype][1];            // vertical block size
    int   blocksize_x   = input->blc_size[blocktype][0];            // horizontal block size
    int   blocksize_x8  = blocksize_x >> 3;                         // horizontal block size in 4-pel units
    int   pred_x        = (pic_pix_x << 2) + pred_mv_x;       // predicted position x (in sub-pel units)
    int   pred_y        = (pic_pix_y << 2) + pred_mv_y;       // predicted position y (in sub-pel units)
    int   center_x      = pic_pix_x + *mv_x;                        // center position x (in pel units)
    int   center_y      = pic_pix_y + *mv_y;                        // center position y (in pel units)
    int   check_for_00  = (blocktype==1 && !input->rdopt && img->type!=B_IMG && ref==0);

    int   height        = img->height;

    //===== set function for getting reference picture lines =====
    if ((center_x > search_range) && (center_x < img->width -1-search_range-blocksize_x) &&
        (center_y > search_range) && (center_y < height-1-search_range-blocksize_y)   )
    {
        get_ref_line = FastLineX; //add by wuzhongmou 0610
    }
    else   //add by wuzhongmou 0610
    {
        get_ref_line = UMVLineX;
    }



    //===== loop over all search positions =====
    for (pos=0; pos<max_pos; pos++)
    {
        //--- set candidate position (absolute position in pel units) ---
        cand_x = center_x + spiral_search_x[pos];
        cand_y = center_y + spiral_search_y[pos];

        //--- initialize motion cost (cost for motion vector) and check ---
        mcost = MV_COST (lambda_factor, 2, cand_x, cand_y, pred_x, pred_y);
        if (check_for_00 && cand_x==pic_pix_x && cand_y==pic_pix_y)
        {
            mcost -= WEIGHTED_COST (lambda_factor, 16);
        }

        if (mcost >= min_mcost)
            continue;

        //--- add residual cost to motion cost ---
        for (y=0; y<blocksize_y; y++)
        {
            ref_line  = get_ref_line (blocksize_x, ref_pic, cand_y+y, cand_x);
            orig_line = orig_pic [y];

            for (x8=0; x8<blocksize_x8; x8++)
            {
                mcost += byte_abs[ *orig_line++ - *ref_line++ ];
                mcost += byte_abs[ *orig_line++ - *ref_line++ ];
                mcost += byte_abs[ *orig_line++ - *ref_line++ ];
                mcost += byte_abs[ *orig_line++ - *ref_line++ ];
                mcost += byte_abs[ *orig_line++ - *ref_line++ ];
                mcost += byte_abs[ *orig_line++ - *ref_line++ ];
                mcost += byte_abs[ *orig_line++ - *ref_line++ ];
                mcost += byte_abs[ *orig_line++ - *ref_line++ ];
            }

            if (mcost >= min_mcost)
            {
                break;
            }
        }

        //--- check if motion cost is less than minimum cost ---
        if (mcost < min_mcost)
        {
            best_pos  = pos;
            min_mcost = mcost;
        }
    }

    //===== set best motion vector and return minimum motion cost =====
    if (best_pos)
    {
        *mv_x += spiral_search_x[best_pos];
        *mv_y += spiral_search_y[best_pos];
    }

    return min_mcost;
}


/*
*************************************************************************
* Function:Calculate SA(T)D
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/
int SATD (int* diff, int use_hadamard)
{
    int k, satd = 0, m[16], dd, *d=diff;

    if (use_hadamard)
    {
        /*===== hadamard transform =====*/
        m[ 0] = d[ 0] + d[12];
        m[ 4] = d[ 4] + d[ 8];
        m[ 8] = d[ 4] - d[ 8];
        m[12] = d[ 0] - d[12];
        m[ 1] = d[ 1] + d[13];
        m[ 5] = d[ 5] + d[ 9];
        m[ 9] = d[ 5] - d[ 9];
        m[13] = d[ 1] - d[13];
        m[ 2] = d[ 2] + d[14];
        m[ 6] = d[ 6] + d[10];
        m[10] = d[ 6] - d[10];
        m[14] = d[ 2] - d[14];
        m[ 3] = d[ 3] + d[15];
        m[ 7] = d[ 7] + d[11];
        m[11] = d[ 7] - d[11];
        m[15] = d[ 3] - d[15];

        d[ 0] = m[ 0] + m[ 4];
        d[ 8] = m[ 0] - m[ 4];
        d[ 4] = m[ 8] + m[12];
        d[12] = m[12] - m[ 8];
        d[ 1] = m[ 1] + m[ 5];
        d[ 9] = m[ 1] - m[ 5];
        d[ 5] = m[ 9] + m[13];
        d[13] = m[13] - m[ 9];
        d[ 2] = m[ 2] + m[ 6];
        d[10] = m[ 2] - m[ 6];
        d[ 6] = m[10] + m[14];
        d[14] = m[14] - m[10];
        d[ 3] = m[ 3] + m[ 7];
        d[11] = m[ 3] - m[ 7];
        d[ 7] = m[11] + m[15];
        d[15] = m[15] - m[11];

        m[ 0] = d[ 0] + d[ 3];
        m[ 1] = d[ 1] + d[ 2];
        m[ 2] = d[ 1] - d[ 2];
        m[ 3] = d[ 0] - d[ 3];
        m[ 4] = d[ 4] + d[ 7];
        m[ 5] = d[ 5] + d[ 6];
        m[ 6] = d[ 5] - d[ 6];
        m[ 7] = d[ 4] - d[ 7];
        m[ 8] = d[ 8] + d[11];
        m[ 9] = d[ 9] + d[10];
        m[10] = d[ 9] - d[10];
        m[11] = d[ 8] - d[11];
        m[12] = d[12] + d[15];
        m[13] = d[13] + d[14];
        m[14] = d[13] - d[14];
        m[15] = d[12] - d[15];

        d[ 0] = m[ 0] + m[ 1];
        d[ 1] = m[ 0] - m[ 1];
        d[ 2] = m[ 2] + m[ 3];
        d[ 3] = m[ 3] - m[ 2];
        d[ 4] = m[ 4] + m[ 5];
        d[ 5] = m[ 4] - m[ 5];
        d[ 6] = m[ 6] + m[ 7];
        d[ 7] = m[ 7] - m[ 6];
        d[ 8] = m[ 8] + m[ 9];
        d[ 9] = m[ 8] - m[ 9];
        d[10] = m[10] + m[11];
        d[11] = m[11] - m[10];
        d[12] = m[12] + m[13];
        d[13] = m[12] - m[13];
        d[14] = m[14] + m[15];
        d[15] = m[15] - m[14];

        /*===== sum up =====*/
        for (dd=diff[k=0]; k<16; dd=diff[++k])
        {
            satd += (dd < 0 ? -dd : dd);
        }
        satd >>= 1;
    }
    else
    {
        /*===== sum up =====*/
        for (k = 0; k < 16; k++)
        {
            satd += byte_abs [diff [k]];
        }
    }

    return satd;
}

/*
*************************************************************************
* Function:Sub pixel block motion search
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int                                               //  ==> minimum motion cost after search
SubPelBlockMotionSearch (pel_t**   orig_pic,      // <--  original pixel values for the AxB block
                         int       ref,           // <--  reference frame (0... or -1 (backward))
                         int       pic_pix_x,     // <--  absolute x-coordinate of regarded AxB block
                         int       pic_pix_y,     // <--  absolute y-coordinate of regarded AxB block
                         int       blocktype,     // <--  block type (1-16x16 ... 7-4x4)
                         int       pred_mv_x,     // <--  motion vector predictor (x) in sub-pel units
                         int       pred_mv_y,     // <--  motion vector predictor (y) in sub-pel units
                         int*      mv_x,          // <--> in: search center (x) / out: motion vector (x) - in pel units
                         int*      mv_y,          // <--> in: search center (y) / out: motion vector (y) - in pel units
                         int       search_pos2,   // <--  search positions for    half-pel search  (default: 9)
                         int       search_pos4,   // <--  search positions for quarter-pel search  (default: 9)
                         int       min_mcost,     // <--  minimum motion cost (cost for center or huge value)
                         double    lambda         // <--  lagrangian parameter for determining motion cost
                         )
{
    int   diff[16], *d;
    int   pos, best_pos, mcost, abort_search;
    int   y0, x0, ry0, rx0, ry;
    int   cand_mv_x, cand_mv_y;
    pel_t *orig_line;
    int   incr            = ref==-1 ? ((!img->fld_type)&&/*(!img->picture_structure)&&*/(img->type==B_IMG)) : (mref==mref_fld)&&(img->type==B_IMG) ;  //by oliver 0511
    pel_t **ref_pic;      
    int   lambda_factor   = LAMBDA_FACTOR (lambda);
    int   mv_shift        = 0;
    int   check_position0 = (blocktype==1 && *mv_x==0 && *mv_y==0 && input->hadamard && !input->rdopt && img->type!=B_IMG && ref==0);
    int   blocksize_x     = input->blc_size[blocktype][0];
    int   blocksize_y     = input->blc_size[blocktype][1];
    int   pic4_pix_x      = (pic_pix_x << 2);
    int   pic4_pix_y      = (pic_pix_y << 2);
    int   max_pos_x4      = ((img->width -blocksize_x+1)<<2);
    int   max_pos_y4      = ((img->height-blocksize_y+1)<<2);
    int   min_pos2        = (input->hadamard ? 0 : 1);
    int   max_pos2        = (input->hadamard ? max(1,search_pos2) : search_pos2);

    int   curr_diff[MB_BLOCK_SIZE][MB_BLOCK_SIZE]; // for AVS 8x8 SATD calculation
    int   xx,yy,kk;                                // indicees for curr_diff

    if (!img->picture_structure)
    {
        if (img->type==B_IMG)
        {
            incr = 2;
        }
    }else
    {
        if(img->type==B_IMG)
            incr = 1;
    }

    ref_pic = img->type==B_IMG? mref [ref+incr] : mref [ref];

    /*********************************
    *****                       *****
    *****  HALF-PEL REFINEMENT  *****
    *****                       *****
    *********************************/
    //===== convert search center to quarter-pel units =====
    *mv_x <<= 2;
    *mv_y <<= 2;
#ifdef _DISABLE_SUB_ME_
    return min_mcost;
#endif
    //===== set function for getting pixel values =====
    if ((pic4_pix_x + *mv_x > 1) && (pic4_pix_x + *mv_x < max_pos_x4 - 2) &&
        (pic4_pix_y + *mv_y > 1) && (pic4_pix_y + *mv_y < max_pos_y4 - 2)   )
    {
        PelY_14 = FastPelY_14;
    }
    else
    {
        PelY_14 = UMVPelY_14;
    }
    //===== loop over search positions =====
    for (best_pos = 0, pos = min_pos2; pos < max_pos2; pos++)
    {
        cand_mv_x = *mv_x + (spiral_search_x[pos] << 1);    // quarter-pel units
        cand_mv_y = *mv_y + (spiral_search_y[pos] << 1);    // quarter-pel units

        //----- set motion vector cost -----
        mcost = MV_COST (lambda_factor, mv_shift, cand_mv_x, cand_mv_y, pred_mv_x, pred_mv_y);
        if (check_position0 && pos==0)
        {
            mcost -= WEIGHTED_COST (lambda_factor, 16);
        }

        //----- add up SATD -----
        for (y0=0, abort_search=0; y0<blocksize_y && !abort_search; y0+=4)
        {
            ry0 = ((pic_pix_y+y0)<<2) + cand_mv_y;

            for (x0=0; x0<blocksize_x; x0+=4)
            {
                rx0 = ((pic_pix_x+x0)<<2) + cand_mv_x;
                d   = diff;

                orig_line = orig_pic [y0  ];    ry=ry0;
                *d++      = orig_line[x0  ]  -  PelY_14 (ref_pic, ry, rx0   );
                *d++      = orig_line[x0+1]  -  PelY_14 (ref_pic, ry, rx0+ 4);
                *d++      = orig_line[x0+2]  -  PelY_14 (ref_pic, ry, rx0+ 8);
                *d++      = orig_line[x0+3]  -  PelY_14 (ref_pic, ry, rx0+12);

                orig_line = orig_pic [y0+1];    ry=ry0+4;
                *d++      = orig_line[x0  ]  -  PelY_14 (ref_pic, ry, rx0   );
                *d++      = orig_line[x0+1]  -  PelY_14 (ref_pic, ry, rx0+ 4);
                *d++      = orig_line[x0+2]  -  PelY_14 (ref_pic, ry, rx0+ 8);
                *d++      = orig_line[x0+3]  -  PelY_14 (ref_pic, ry, rx0+12);

                orig_line = orig_pic [y0+2];    ry=ry0+8;
                *d++      = orig_line[x0  ]  -  PelY_14 (ref_pic, ry, rx0   );
                *d++      = orig_line[x0+1]  -  PelY_14 (ref_pic, ry, rx0+ 4);
                *d++      = orig_line[x0+2]  -  PelY_14 (ref_pic, ry, rx0+ 8);
                *d++      = orig_line[x0+3]  -  PelY_14 (ref_pic, ry, rx0+12);

                orig_line = orig_pic [y0+3];    ry=ry0+12;
                *d++      = orig_line[x0  ]  -  PelY_14 (ref_pic, ry, rx0   );
                *d++      = orig_line[x0+1]  -  PelY_14 (ref_pic, ry, rx0+ 4);
                *d++      = orig_line[x0+2]  -  PelY_14 (ref_pic, ry, rx0+ 8);
                *d        = orig_line[x0+3]  -  PelY_14 (ref_pic, ry, rx0+12);

                for (yy=y0,kk=0; yy<y0+4; yy++)
                    for (xx=x0; xx<x0+4; xx++, kk++)
                        curr_diff[yy][xx] = diff[kk];         
            }
        }

        mcost += find_sad_8x8(input->hadamard, blocksize_x, blocksize_y, 0, 0, curr_diff);   
        if (mcost < min_mcost)
        {
            min_mcost = mcost;
            best_pos  = pos;
        }
    }

    if (best_pos)
    {
        *mv_x += (spiral_search_x [best_pos] << 1);
        *mv_y += (spiral_search_y [best_pos] << 1);
    }

    /************************************
    *****                          *****
    *****  QUARTER-PEL REFINEMENT  *****
    *****                          *****
    ************************************/
    //===== set function for getting pixel values =====
    if ((pic4_pix_x + *mv_x > 1) && (pic4_pix_x + *mv_x < max_pos_x4 - 1) &&
        (pic4_pix_y + *mv_y > 1) && (pic4_pix_y + *mv_y < max_pos_y4 - 1)   )
    {
        PelY_14 = FastPelY_14;
    }
    else
    {
        PelY_14 = UMVPelY_14;
    }

    //===== loop over search positions =====
    for (best_pos = 0, pos = 1; pos < search_pos4; pos++)
    {
        cand_mv_x = *mv_x + spiral_search_x[pos];    // quarter-pel units
        cand_mv_y = *mv_y + spiral_search_y[pos];    // quarter-pel units

        // xiaozhen zheng, mv_rang, 20071009
        img->mv_range_flag = check_mv_range(cand_mv_x, cand_mv_y, pic_pix_x, pic_pix_y, blocktype);
        if (!img->mv_range_flag) {
            img->mv_range_flag = 1;
            continue;
        }
        // xiaozhen zheng, mv_rang, 20071009

        //----- set motion vector cost -----
        mcost = MV_COST (lambda_factor, mv_shift, cand_mv_x, cand_mv_y, pred_mv_x, pred_mv_y);

        //----- add up SATD -----
        for (y0=0, abort_search=0; y0<blocksize_y && !abort_search; y0+=4)
        {
            ry0 = ((pic_pix_y+y0)<<2) + cand_mv_y;

            for (x0=0; x0<blocksize_x; x0+=4)
            {
                rx0 = ((pic_pix_x+x0)<<2) + cand_mv_x;
                d   = diff;

                orig_line = orig_pic [y0  ];    ry=ry0;
                *d++      = orig_line[x0  ]  -  PelY_14 (ref_pic, ry, rx0   );
                *d++      = orig_line[x0+1]  -  PelY_14 (ref_pic, ry, rx0+ 4);
                *d++      = orig_line[x0+2]  -  PelY_14 (ref_pic, ry, rx0+ 8);
                *d++      = orig_line[x0+3]  -  PelY_14 (ref_pic, ry, rx0+12);

                orig_line = orig_pic [y0+1];    ry=ry0+4;
                *d++      = orig_line[x0  ]  -  PelY_14 (ref_pic, ry, rx0   );
                *d++      = orig_line[x0+1]  -  PelY_14 (ref_pic, ry, rx0+ 4);
                *d++      = orig_line[x0+2]  -  PelY_14 (ref_pic, ry, rx0+ 8);
                *d++      = orig_line[x0+3]  -  PelY_14 (ref_pic, ry, rx0+12);

                orig_line = orig_pic [y0+2];    ry=ry0+8;
                *d++      = orig_line[x0  ]  -  PelY_14 (ref_pic, ry, rx0   );
                *d++      = orig_line[x0+1]  -  PelY_14 (ref_pic, ry, rx0+ 4);
                *d++      = orig_line[x0+2]  -  PelY_14 (ref_pic, ry, rx0+ 8);
                *d++      = orig_line[x0+3]  -  PelY_14 (ref_pic, ry, rx0+12);

                orig_line = orig_pic [y0+3];    ry=ry0+12;
                *d++      = orig_line[x0  ]  -  PelY_14 (ref_pic, ry, rx0   );
                *d++      = orig_line[x0+1]  -  PelY_14 (ref_pic, ry, rx0+ 4);
                *d++      = orig_line[x0+2]  -  PelY_14 (ref_pic, ry, rx0+ 8);
                *d        = orig_line[x0+3]  -  PelY_14 (ref_pic, ry, rx0+12);

                for (yy=y0,kk=0; yy<y0+4; yy++)
                    for (xx=x0; xx<x0+4; xx++, kk++)
                        curr_diff[yy][xx] = diff[kk];
            }
        }

        mcost += find_sad_8x8(input->hadamard, blocksize_x, blocksize_y, 0, 0, curr_diff);
        if (mcost < min_mcost)
        {
            min_mcost = mcost;
            best_pos  = pos;
        }
    }

    if (best_pos)
    {
        *mv_x += spiral_search_x [best_pos];
        *mv_y += spiral_search_y [best_pos];
    }

    //===== return minimum motion cost =====
    return min_mcost;
}

/*
*************************************************************************
* Function:Sub pixel block motion search
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int                                               //  ==> minimum motion cost after search
SubPelBlockMotionSearch_bid (pel_t**   orig_pic,      // <--  original pixel values for the AxB block
                             int       ref,           // <--  reference frame (0... or -1 (backward))
                             int       pic_pix_x,     // <--  absolute x-coordinate of regarded AxB block
                             int       pic_pix_y,     // <--  absolute y-coordinate of regarded AxB block
                             int       blocktype,     // <--  block type (1-16x16 ... 7-4x4)
                             int       pred_mv_x,     // <--  motion vector predictor (x) in sub-pel units
                             int       pred_mv_y,     // <--  motion vector predictor (y) in sub-pel units
                             int*      mv_x,          // <--> in: search center (x) / out: motion vector (x) - in pel units
                             int*      mv_y,          // <--> in: search center (y) / out: motion vector (y) - in pel units
                             int       search_pos2,   // <--  search positions for    half-pel search  (default: 9)
                             int       search_pos4,   // <--  search positions for quarter-pel search  (default: 9)
                             int       min_mcost,     // <--  minimum motion cost (cost for center or huge value)
                             double    lambda         // <--  lagrangian parameter for determining motion cost
                             )
{
    int   diff[16], *d;
    int   pos, best_pos, mcost, abort_search;
    int   y0, x0, ry0, rx0, ry;
    int    ry0_bid, rx0_bid, ry_bid;
    int   cand_mv_x, cand_mv_y;
    pel_t *orig_line;
    int   incr            = ref==-1 ? ((!img->fld_type)&&(mref==mref_fld)&&(img->type==B_IMG)) : (mref==mref_fld)&&(img->type==B_IMG) ;
    pel_t **ref_pic,**ref_pic_bid;      
    int   lambda_factor   = LAMBDA_FACTOR (lambda);
    int   mv_shift        = 0;
    int   check_position0 = (blocktype==1 && *mv_x==0 && *mv_y==0 && input->hadamard && !input->rdopt && img->type!=B_IMG && ref==0);
    int   blocksize_x     = input->blc_size[blocktype][0];
    int   blocksize_y     = input->blc_size[blocktype][1];
    int   pic4_pix_x      = (pic_pix_x << 2);
    int   pic4_pix_y      = (pic_pix_y << 2);
    int   max_pos_x4      = ((img->width -blocksize_x+1)<<2);
    int   max_pos_y4      = ((img->height-blocksize_y+1)<<2);
    int   min_pos2        = (input->hadamard ? 0 : 1);
    int   max_pos2        = (input->hadamard ? max(1,search_pos2) : search_pos2);
    int apply_weights = 0;
    int delta_P,TRp,DistanceIndexFw,DistanceIndexBw,refframe ,delta_PB;   
    refframe = ref;
    delta_P = 2*(img->imgtr_next_P_frm - img->imgtr_last_P_frm);
    delta_P = (delta_P + 512)%512;  // Added by Xiaozhen ZHENG, 2007.05.05
    if(img->picture_structure)
        TRp = (refframe+1)*delta_P;
    else
        TRp = delta_P;//ref == 0 ? delta_P-1 : delta_P+1;
    delta_PB = 2*(picture_distance - img->imgtr_last_P_frm);  // Tsinghua 200701
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

    //xyji 11.23
    if (!img->picture_structure)
    {
        if (img->type==B_IMG)
        {
            incr = 2;
        }
    }else
    {
        if(img->type==B_IMG)
            incr = 1;
    }

    ref_pic = img->type==B_IMG? mref [ref+incr] : mref [ref];  

    ref_pic_bid = img->type==B_IMG? mref [img->picture_structure ? 0 : ref/*2 - (ref+incr)*/] : mref [ref];

    /*********************************
    *****                       *****
    *****  HALF-PEL REFINEMENT  *****
    *****                       *****
    *********************************/
    //===== convert search center to quarter-pel units =====
    *mv_x <<= 2;
    *mv_y <<= 2;
#ifdef _DISABLE_SUB_ME_
    return min_mcost;
#endif
    //===== set function for getting pixel values =====
    if ((pic4_pix_x + *mv_x > 1) && (pic4_pix_x + *mv_x < max_pos_x4 - 2) &&
        (pic4_pix_y + *mv_y > 1) && (pic4_pix_y + *mv_y < max_pos_y4 - 2)   )
    {
        PelY_14 = UMVPelY_14;//FastPelY_14;//xyji
    }
    else
    {
        PelY_14 = UMVPelY_14;
    }
    //===== loop over search positions =====
    for (best_pos = 0, pos = min_pos2; pos < max_pos2; pos++)
    {
        cand_mv_x = *mv_x + (spiral_search_x[pos] << 1);    // quarter-pel units
        cand_mv_y = *mv_y + (spiral_search_y[pos] << 1);    // quarter-pel units

        //----- set motion vector cost -----
        mcost = MV_COST (lambda_factor, mv_shift, cand_mv_x, cand_mv_y, pred_mv_x, pred_mv_y);
        if (check_position0 && pos==0)
        {
            mcost -= WEIGHTED_COST (lambda_factor, 16);
        }

        //----- add up SATD -----
        for (y0=0, abort_search=0; y0<blocksize_y && !abort_search; y0+=4)
        {
            ry0 = ((pic_pix_y+y0)<<2) + cand_mv_y;

            ry0_bid = ((pic_pix_y+y0)<<2) - ((cand_mv_y*DistanceIndexBw*(512/DistanceIndexFw)+256)>>9);
            for (x0=0; x0<blocksize_x; x0+=4)
            {
                rx0 = ((pic_pix_x+x0)<<2) + cand_mv_x;
                rx0_bid = ((pic_pix_x+x0)<<2) - ((cand_mv_x*DistanceIndexBw*(512/DistanceIndexFw)+256)>>9);
                d   = diff;

                orig_line = orig_pic [y0  ];    ry=ry0; ry_bid = ry0_bid;

                *d++      = orig_line[x0  ]  -  (PelY_14 (ref_pic, ry, rx0   ) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid   ))/2;
                *d++      = orig_line[x0+1]  -  (PelY_14 (ref_pic, ry, rx0+ 4) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid+ 4))/2;
                *d++      = orig_line[x0+2]  -  (PelY_14 (ref_pic, ry, rx0+ 8) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid+ 8))/2;
                *d++      = orig_line[x0+3]  -  (PelY_14 (ref_pic, ry, rx0+12) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid+12))/2;

                orig_line = orig_pic [y0+1];    ry=ry0+4;ry_bid=ry0_bid+4;
                *d++      = orig_line[x0  ]  -  (PelY_14 (ref_pic, ry, rx0   ) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid   ))/2;
                *d++      = orig_line[x0+1]  -  (PelY_14 (ref_pic, ry, rx0+ 4) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid+ 4))/2;
                *d++      = orig_line[x0+2]  -  (PelY_14 (ref_pic, ry, rx0+ 8) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid+ 8))/2;
                *d++      = orig_line[x0+3]  -  (PelY_14 (ref_pic, ry, rx0+12) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid+12))/2;

                orig_line = orig_pic [y0+2];    ry=ry0+8;ry_bid=ry0_bid+8;
                *d++      = orig_line[x0  ]  -  (PelY_14 (ref_pic, ry, rx0   ) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid   ))/2;
                *d++      = orig_line[x0+1]  -  (PelY_14 (ref_pic, ry, rx0+ 4) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid+ 4))/2;
                *d++      = orig_line[x0+2]  -  (PelY_14 (ref_pic, ry, rx0+ 8) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid+ 8))/2;
                *d++      = orig_line[x0+3]  -  (PelY_14 (ref_pic, ry, rx0+12) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid+12))/2;

                orig_line = orig_pic [y0+3];    ry=ry0+12;ry_bid=ry0_bid+12;
                *d++      = orig_line[x0  ]  -  (PelY_14 (ref_pic, ry, rx0   ) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid   ))/2;
                *d++      = orig_line[x0+1]  -  (PelY_14 (ref_pic, ry, rx0+ 4) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid+ 4))/2;
                *d++      = orig_line[x0+2]  -  (PelY_14 (ref_pic, ry, rx0+ 8) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid+ 8))/2;
                *d        = orig_line[x0+3]  -  (PelY_14 (ref_pic, ry, rx0+12) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid+12))/2;

                if ((mcost += SATD (diff, input->hadamard)) > min_mcost)
                {
                    abort_search = 1;
                    break;
                }
            }
        }

        if (mcost < min_mcost)
        {
            min_mcost = mcost;
            best_pos  = pos;
        }
    }
    if (best_pos)
    {
        *mv_x += (spiral_search_x [best_pos] << 1);
        *mv_y += (spiral_search_y [best_pos] << 1);
    }


    /************************************
    *****                          *****
    *****  QUARTER-PEL REFINEMENT  *****
    *****                          *****
    ************************************/
    //===== set function for getting pixel values =====
    if ((pic4_pix_x + *mv_x > 1) && (pic4_pix_x + *mv_x < max_pos_x4 - 1) &&
        (pic4_pix_y + *mv_y > 1) && (pic4_pix_y + *mv_y < max_pos_y4 - 1)   )
    {
        PelY_14 = UMVPelY_14;//FastPelY_14;//xyji
    }
    else
    {
        PelY_14 = UMVPelY_14;
    }
    //===== loop over search positions =====
    for (best_pos = 0, pos = 1; pos < search_pos4; pos++)
    {
        cand_mv_x = *mv_x + spiral_search_x[pos];    // quarter-pel units
        cand_mv_y = *mv_y + spiral_search_y[pos];    // quarter-pel units

        // xiaozhen zheng, mv_rang, 20071009
        img->mv_range_flag = check_mv_range_bid(cand_mv_x, cand_mv_y, pic_pix_x, pic_pix_y, blocktype, ref);
        if (!img->mv_range_flag) {
            img->mv_range_flag = 1;
            continue;
        }
        // xiaozhen zheng, mv_rang, 20071009

        //----- set motion vector cost -----
        mcost = MV_COST (lambda_factor, mv_shift, cand_mv_x, cand_mv_y, pred_mv_x, pred_mv_y);

        //----- add up SATD -----
        for (y0=0, abort_search=0; y0<blocksize_y && !abort_search; y0+=4)
        {
            ry0 = ((pic_pix_y+y0)<<2) + cand_mv_y;

            ry0_bid = ((pic_pix_y+y0)<<2) - ((cand_mv_y*DistanceIndexBw*(512/DistanceIndexFw)+256)>>9);
            for (x0=0; x0<blocksize_x; x0+=4)
            {
                rx0 = ((pic_pix_x+x0)<<2) + cand_mv_x;

                rx0_bid = ((pic_pix_x+x0)<<2) - ((cand_mv_x*DistanceIndexBw*(512/DistanceIndexFw)+256)>>9);
                d   = diff;

                orig_line = orig_pic [y0  ];    ry=ry0; ry_bid=ry0_bid;
                *d++      = orig_line[x0  ]  -  (PelY_14 (ref_pic, ry, rx0   ) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid   ))/2;
                *d++      = orig_line[x0+1]  -  (PelY_14 (ref_pic, ry, rx0+ 4) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid+ 4))/2;
                *d++      = orig_line[x0+2]  -  (PelY_14 (ref_pic, ry, rx0+ 8) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid+ 8))/2;
                *d++      = orig_line[x0+3]  -  (PelY_14 (ref_pic, ry, rx0+12) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid+12))/2;

                orig_line = orig_pic [y0+1];    ry=ry0+4;ry_bid=ry0_bid+4;
                *d++      = orig_line[x0  ]  -  (PelY_14 (ref_pic, ry, rx0   ) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid   ))/2;
                *d++      = orig_line[x0+1]  -  (PelY_14 (ref_pic, ry, rx0+ 4) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid+ 4))/2;
                *d++      = orig_line[x0+2]  -  (PelY_14 (ref_pic, ry, rx0+ 8) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid+ 8))/2;
                *d++      = orig_line[x0+3]  -  (PelY_14 (ref_pic, ry, rx0+12) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid+12))/2;

                orig_line = orig_pic [y0+2];    ry=ry0+8;ry_bid=ry0_bid+8;
                *d++      = orig_line[x0  ]  -  (PelY_14 (ref_pic, ry, rx0   ) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid   ))/2;
                *d++      = orig_line[x0+1]  -  (PelY_14 (ref_pic, ry, rx0+ 4) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid+ 4))/2;
                *d++      = orig_line[x0+2]  -  (PelY_14 (ref_pic, ry, rx0+ 8) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid+ 8))/2;
                *d++      = orig_line[x0+3]  -  (PelY_14 (ref_pic, ry, rx0+12) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid+12))/2;

                orig_line = orig_pic [y0+3];    ry=ry0+12;ry_bid=ry0_bid+12;
                *d++      = orig_line[x0  ]  -  (PelY_14 (ref_pic, ry, rx0   ) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid   ))/2;
                *d++      = orig_line[x0+1]  -  (PelY_14 (ref_pic, ry, rx0+ 4) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid+ 4))/2;
                *d++      = orig_line[x0+2]  -  (PelY_14 (ref_pic, ry, rx0+ 8) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid+ 8))/2;
                *d        = orig_line[x0+3]  -  (PelY_14 (ref_pic, ry, rx0+12) + PelY_14 (ref_pic_bid, ry_bid, rx0_bid+12))/2;

                if ((mcost += SATD (diff, input->hadamard)) > min_mcost)
                {
                    abort_search = 1;
                    break;
                }
            }
        }

        if (mcost < min_mcost)
        {
            min_mcost = mcost;
            best_pos  = pos;
        }
    }
    if (best_pos)
    {
        *mv_x += spiral_search_x [best_pos];
        *mv_y += spiral_search_y [best_pos];
    }

    //===== return minimum motion cost =====
    return min_mcost;
}

/*
*************************************************************************
* Function:Block motion search
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int                                         //  ==> minimum motion cost after search
BlockMotionSearch (int       ref,           // <--  reference frame (0... or -1 (backward))
                   int       pic_pix_x,     // <--  absolute x-coordinate of regarded AxB block
                   int       pic_pix_y,     // <--  absolute y-coordinate of regarded AxB block
                   int       blocktype,     // <--  block type (1-16x16 ... 7-4x4)
                   int       search_range,  // <--  1-d search range for integer-position search
                   double    lambda         // <--  lagrangian parameter for determining motion cost
                   )
{
    static pel_t   orig_val [256];
    static pel_t  *orig_pic  [16] = {orig_val,     orig_val+ 16, orig_val+ 32, orig_val+ 48,
        orig_val+ 64, orig_val+ 80, orig_val+ 96, orig_val+112,
        orig_val+128, orig_val+144, orig_val+160, orig_val+176,
        orig_val+192, orig_val+208, orig_val+224, orig_val+240};

    int       pred_mv_x, pred_mv_y, mv_x, mv_y, i, j;
    int       max_value     = (1<<20);
    int       min_mcost     = max_value;
    int       mb_pix_x      = pic_pix_x-img->pix_x;
    int       mb_pix_y      = pic_pix_y-img->pix_y;
    int       b8_x          = (mb_pix_x>>3);
    int       b8_y          = (mb_pix_y>>3);
    int       bsx           = input->blc_size[blocktype][0];
    int       bsy           = input->blc_size[blocktype][1];
    int       refframe      = (ref==-1 ? 0 : ref);
    int*      pred_mv;
    int**     ref_array     = ((img->type!=B_IMG) ? refFrArr : ref>=0 ? fw_refFrArr : bw_refFrArr);
    int***    mv_array      = ((img->type!=B_IMG) ? tmp_mv   : ref>=0 ? tmp_fwMV    : tmp_bwMV);
    int*****  all_bmv       = img->all_bmv;
    int*****  all_mv        = (ref/*==-1*/ <0/*lgp13*/? img->all_bmv : img->all_mv);
    byte**    imgY_org_pic  = imgY_org;
    int       incr_y=1,off_y=0;
    int       center_x = pic_pix_x;
    int       center_y = pic_pix_y;
    //  int       temp_x,temp_y;      // commented by xiaozhen zheng, 20071009
    //  int MaxMVHRange, MaxMVVRange;    // commented by xiaozhen zheng, 20071009
    int   blocksize_y   = input->blc_size[blocktype][1];            // vertical block size
    int   blocksize_x   = input->blc_size[blocktype][0];  
    if (!img->picture_structure) // field coding
    {
        if (img->type==B_IMG)
            refframe = ref<0 ? ref+2 : ref;
    }

    pred_mv = ((img->type!=B_IMG) ? img->mv  : ref>=0 ? img->p_fwMV : img->p_bwMV)[mb_pix_x>>3][mb_pix_y>>3][refframe][blocktype];

    //==================================
    //=====   GET ORIGINAL BLOCK   =====
    //==================================
    for (j = 0; j < bsy; j++)
    {
        for (i = 0; i < bsx; i++)
        {
            orig_pic[j][i] = imgY_org_pic[pic_pix_y+/*j*/incr_y*j+off_y][pic_pix_x+i];
        }
    }

    //===========================================
    //=====   GET MOTION VECTOR PREDICTOR   =====
    //===========================================
    SetMotionVectorPredictor (pred_mv, ref_array, mv_array, refframe, mb_pix_x, mb_pix_y, bsx, bsy, ref, 0);//Lou 1016
    pred_mv_x = pred_mv[0];
    pred_mv_y = pred_mv[1];


    //==================================
    //=====   INTEGER-PEL SEARCH   =====
    //==================================
    //--- set search center ---
    mv_x = pred_mv_x / 4;
    mv_y = pred_mv_y / 4;

    if (!input->rdopt)
    {
        //--- adjust search center so that the (0,0)-vector is inside ---
        mv_x = max (-search_range, min (search_range, mv_x));
        mv_y = max (-search_range, min (search_range, mv_y));
    }

    //--- perform motion search ---
    min_mcost = FullPelBlockMotionSearch     (orig_pic, ref, center_x, center_y, blocktype,
        pred_mv_x, pred_mv_y, &mv_x, &mv_y, search_range,
        min_mcost, lambda);



    //add by wuzhongmou 200612
    //==============================
    //=====   SUB-PEL SEARCH   =====
    //==============================
    if (input->hadamard)
    {
        min_mcost = max_value;
    }
    min_mcost =  SubPelBlockMotionSearch (orig_pic, ref, center_x, center_y, blocktype,
        pred_mv_x, pred_mv_y, &mv_x, &mv_y, 9, 9,
        min_mcost, lambda);

    if (!input->rdopt)
    {
        // Get the skip mode cost
        if (blocktype == 1 && img->type == INTER_IMG)
        {
            int cost;

            FindSkipModeMotionVector ();

            cost  = GetSkipCostMB (lambda);
            cost -= (int)floor(8*lambda+0.4999);

            if (cost < min_mcost)
            {
                min_mcost = cost;
                mv_x      = img->all_mv [0][0][0][0][0];
                mv_y      = img->all_mv [0][0][0][0][1];
            }
        }
    }

    /* // commented by xiaozhen zheng, 20071009
    temp_x = 4*pic_pix_x + pred_mv_x+mv_x;                   //add by wuzhongmou 200612  
    temp_y = 4*pic_pix_y + pred_mv_y+mv_y;                   //add by wuzhongmou 200612
    if(temp_x<=-64)                 //add by wuzhongmou  200612
    {
    mv_x=-63-4*pic_pix_x-pred_mv_x;
    }
    if(temp_x >= 4*(img->width -1-blocksize_x+16))
    {
    mv_x=4*(img->width -1-blocksize_x+15-pic_pix_x)-pred_mv_x;
    }
    if(temp_y <=-64)
    {
    mv_y=-63-4*pic_pix_y-pred_mv_y;
    }
    if(temp_y >= 4*(img->height-1-blocksize_y+16))
    {
    mv_y=4*(img->height -1-blocksize_y+15-pic_pix_y)-pred_mv_y;
    }                                               //add by wuzhongmou 200612
    MaxMVHRange= MAX_H_SEARCH_RANGE;      //add by wuzhongmou 
    MaxMVVRange= MAX_V_SEARCH_RANGE;
    if(!img->picture_structure) //field coding
    {
    MaxMVVRange=MAX_V_SEARCH_RANGE_FIELD;
    }
    if(mv_x < -MaxMVHRange )
    mv_x = -MaxMVHRange;
    if( mv_x> MaxMVHRange-1)
    mv_x=MaxMVHRange-1;
    if( mv_y < -MaxMVVRange)
    mv_y = -MaxMVVRange;
    if( mv_y > MaxMVVRange-1)
    mv_y = MaxMVVRange-1;        //add by wuzhongmou 
    */
    //===============================================
    //=====   SET MV'S AND RETURN MOTION COST   =====
    //===============================================
    for (i=0; i < (bsx>>3); i++)
    {
        for (j=0; j < (bsy>>3); j++)
        {
            all_mv[b8_x+i][b8_y+j][refframe][blocktype][0] = mv_x;
            all_mv[b8_x+i][b8_y+j][refframe][blocktype][1] = mv_y;
        }
    }

    // xiaozhen zheng, mv_rang, 20071009
    img->mv_range_flag = check_mv_range(mv_x, mv_y, pic_pix_x, pic_pix_y, blocktype);
    img->mv_range_flag *= check_mvd((mv_x-pred_mv_x), (mv_y-pred_mv_y));
    if (!img->mv_range_flag) {
        min_mcost = 1<<24;
        img->mv_range_flag = 1;
    }
    // xiaozhen zheng, mv_rang, 20071009

    return min_mcost;
}

/*
*************************************************************************
* Function:Block motion search
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int                                         //  ==> minimum motion cost after search
BlockMotionSearch_bid (int       ref,           // <--  reference frame (0... or -1 (backward))
                       int       pic_pix_x,     // <--  absolute x-coordinate of regarded AxB block
                       int       pic_pix_y,     // <--  absolute y-coordinate of regarded AxB block
                       int       blocktype,     // <--  block type (1-16x16 ... 7-4x4)
                       int       search_range,  // <--  1-d search range for integer-position search
                       double    lambda         // <--  lagrangian parameter for determining motion cost
                       )
{
    static pel_t   orig_val [256];
    static pel_t  *orig_pic  [16] = {orig_val,     orig_val+ 16, orig_val+ 32, orig_val+ 48,
        orig_val+ 64, orig_val+ 80, orig_val+ 96, orig_val+112,
        orig_val+128, orig_val+144, orig_val+160, orig_val+176,
        orig_val+192, orig_val+208, orig_val+224, orig_val+240};

    int       pred_mv_x, pred_mv_y, mv_x, mv_y, i, j;

    int       max_value = (1<<20);
    int       min_mcost = max_value;
    int       mb_x      = pic_pix_x-img->pix_x;
    int       mb_y      = pic_pix_y-img->pix_y;
    //sw 9.30
    int       block_x   = (mb_x>>3);
    int       block_y   = (mb_y>>3);
    int       bsx       = input->blc_size[blocktype][0];
    int       bsy       = input->blc_size[blocktype][1];
    int       refframe  = (ref==-1 ? 0 : ref);
    int*      pred_mv;
    int**     ref_array = ((img->type!=B_IMG) ? refFrArr : ref>=0 ? fw_refFrArr : bw_refFrArr);
    int***    mv_array  = ((img->type!=B_IMG) ? tmp_mv   : ref>=0 ? tmp_fwMV    : tmp_bwMV);
    int*****  all_bmv   = img->all_bmv;
    int*****  all_mv    = (ref==-1 ? img->all_bmv : img->all_omv);
    //sw 10.1
    //int*****  all_mv    = (ref==-1 ? img->all_bmv : img->all_mv);
    byte**    imgY_org_pic = imgY_org;
    int       incr_y=1,off_y=0;
    int       center_x = pic_pix_x;
    int       center_y = pic_pix_y;
    //   int MaxMVHRange, MaxMVVRange;      // commented by xiaozhen zheng, 20071009
    //   int  temp_x,temp_y;          // commented by xiaozhen zheng, 20071009
    int   blocksize_y   = input->blc_size[blocktype][1];            // vertical block size
    int   blocksize_x   = input->blc_size[blocktype][0]; 
    if (!img->picture_structure) // field coding
    {
        if (img->type==B_IMG)
            refframe = ref<0 ? ref+2 : ref;
    }

    //sw 10.1
    pred_mv = ((img->type!=B_IMG) ? img->mv  : ref>=0 ? img->omv/*img->p_fwMV*/ : img->p_bwMV)[mb_x>>3][mb_y>>3][refframe][blocktype];
    // pred_mv = ((img->type!=B_IMG) ? img->mv  : ref>=0 ? img->p_fwMV: img->p_bwMV)[mb_x>>3][mb_y>>3][refframe][blocktype];


    //==================================
    //=====   GET ORIGINAL BLOCK   =====
    //==================================
    for (j = 0; j < bsy; j++)
    {
        for (i = 0; i < bsx; i++)
        {
            orig_pic[j][i] = imgY_org_pic[pic_pix_y+/*j*/incr_y*j+off_y][pic_pix_x+i];
        }
    }

    //===========================================
    //=====   GET MOTION VECTOR PREDICTOR   =====
    //===========================================
    //SetMotionVectorPredictor (pred_mv, ref_array, mv_array, refframe, mb_x, mb_y, bsx, bsy);
    SetMotionVectorPredictor (pred_mv, ref_array, mv_array, refframe, mb_x, mb_y, bsx, bsy, ref, 0);//Lou 1016
    pred_mv_x = pred_mv[0];
    pred_mv_y = pred_mv[1];


    //==================================
    //=====   INTEGER-PEL SEARCH   =====
    //==================================

    //--- set search center ---
    mv_x = pred_mv_x / 4;
    mv_y = pred_mv_y / 4;
    if (!input->rdopt)
    {
        //--- adjust search center so that the (0,0)-vector is inside ---
        mv_x = max (-search_range, min (search_range, mv_x));
        mv_y = max (-search_range, min (search_range, mv_y));
    }

    //--- perform motion search ---
    min_mcost = FullPelBlockMotionSearch     (orig_pic, ref, center_x, center_y, blocktype,
        pred_mv_x, pred_mv_y, &mv_x, &mv_y, search_range,
        min_mcost, lambda);

    //==============================
    //=====   SUB-PEL SEARCH   =====
    //==============================
    if (input->hadamard)
    {
        min_mcost = max_value;
    }
    min_mcost =  SubPelBlockMotionSearch_bid (orig_pic, ref, center_x, center_y, blocktype,
        pred_mv_x, pred_mv_y, &mv_x, &mv_y, 9, 9,
        min_mcost, lambda);


    if (!input->rdopt)
    {
        // Get the skip mode cost
        if (blocktype == 1 && img->type == INTER_IMG)
        {
            int cost;

            FindSkipModeMotionVector ();

            cost  = GetSkipCostMB (lambda);
            cost -= (int)floor(8*lambda+0.4999);

            if (cost < min_mcost)
            {
                min_mcost = cost;
                mv_x      = img->all_mv [0][0][0][0][0];
                mv_y      = img->all_mv [0][0][0][0][1];
            }
        }
    }

    /*  // commented by xiaozhen zheng, 20071009
    temp_x = 4*pic_pix_x + pred_mv_x+mv_x;                   //add by wuzhongmou 200612  
    temp_y = 4*pic_pix_y + pred_mv_y+mv_y;                   //add by wuzhongmou 200612
    if(temp_x<=-64)                 //add by wuzhongmou  200612
    {
    mv_x=-63-4*pic_pix_x-pred_mv_x;
    }
    if(temp_x >= 4*(img->width -1-blocksize_x+16))
    {
    mv_x=4*(img->width -1-blocksize_x+15-pic_pix_x)-pred_mv_x;
    }
    if(temp_y <=-64)
    {
    mv_y=-63-4*pic_pix_y-pred_mv_y;
    }
    if(temp_y >= 4*(img->height-1-blocksize_y+16))
    {
    mv_y=4*(img->height -1-blocksize_y+15-pic_pix_y)-pred_mv_y;
    }  
    MaxMVHRange= MAX_H_SEARCH_RANGE;      //add by wuzhongmou 
    MaxMVVRange= MAX_V_SEARCH_RANGE;
    if(!img->picture_structure) //field coding
    {
    MaxMVVRange=MAX_V_SEARCH_RANGE_FIELD;
    }
    if(mv_x < -MaxMVHRange )
    mv_x = -MaxMVHRange;
    if( mv_x> MaxMVHRange-1)
    mv_x=MaxMVHRange-1;
    if( mv_y < -MaxMVVRange)
    mv_y = -MaxMVVRange;
    if( mv_y > MaxMVVRange-1)
    mv_y = MaxMVVRange-1;        //add by wuzhongmou 
    */
    //===============================================
    //=====   SET MV'S AND RETURN MOTION COST   =====
    //===============================================
    for (i=0; i < (bsx>>3); i++)
    {
        for (j=0; j < (bsy>>3); j++)
        {
            all_mv[block_x+i][block_y+j][refframe][blocktype][0] = mv_x;
            all_mv[block_x+i][block_y+j][refframe][blocktype][1] = mv_y;
        }
    }

    // xiaozhen zheng, mv_rang, 20071009
    img->mv_range_flag = check_mv_range_bid(mv_x, mv_y, pic_pix_x, pic_pix_y, blocktype, ref);
    img->mv_range_flag *= check_mvd((mv_x-pred_mv_x), (mv_y-pred_mv_y));
    if (!img->mv_range_flag) {
        min_mcost = 1<<24;
        img->mv_range_flag = 1;
    }
    // xiaozhen zheng, mv_rang, 20071009


    return min_mcost;
}

/*
*************************************************************************
* Function:Motion Cost for Bidirectional modes
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/


int
BIDPartitionCost (int   blocktype,
                  int   block8x8,
                  int   fw_ref,
                  int   bw_ref,/*lgp*13*/
                  int   lambda_factor)
{
    static int  bx0[5][4] = {{0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,2,0,0}, {0,2,0,2}};
    static int  by0[5][4] = {{0,0,0,0}, {0,0,0,0}, {0,2,0,0}, {0,0,0,0}, {0,0,2,2}};

    int   diff[16];
    int   pic_pix_x, block_x, block_y;
    int   v, h, mcost, i, j, k;
    int   mvd_bits  = 0;
    int   parttype  = (blocktype<4?blocktype:4);
    int   step_h0   = (input->blc_size[ parttype][0]>>2);
    int   step_v0   = (input->blc_size[ parttype][1]>>2);
    int   step_h    = (input->blc_size[blocktype][0]>>2);
    int   step_v    = (input->blc_size[blocktype][1]>>2);
    int   curr_blk[MB_BLOCK_SIZE][MB_BLOCK_SIZE]; // pred.error buffer for AVS

    int   bsx       = min(input->blc_size[blocktype][0],8);
    int   bsy       = min(input->blc_size[blocktype][1],8);
    int   incr_y=1,off_y=0;
    int   pic_pix_y = img->pix_y + ((block8x8 / 2) << 3);//b

    int   bxx, byy;                               // indexing curr_blk
    byte** imgY_original  = imgY_org;
    int    *****all_mv = img->all_mv;
    int   *****all_bmv = img->all_bmv;
    int   *****p_fwMV  = img->p_fwMV;
    int   *****p_bwMV  = img->p_bwMV;

    int    *****all_omv = img->all_omv;  

    for (v=by0[parttype][block8x8]; v<by0[parttype][block8x8]+step_v0; v+=step_v)
        for (h=bx0[parttype][block8x8]; h<bx0[parttype][block8x8]+step_h0; h+=step_h)
        {
            //sw 10.1
            all_omv [h/2][v/2][fw_ref][blocktype][0] = all_mv [h/2][v/2][fw_ref][blocktype][0];
            all_omv [h/2][v/2][fw_ref][blocktype][1] = all_mv [h/2][v/2][fw_ref][blocktype][1];
        }

        //----- cost for motion vector bits -----
        for (v=by0[parttype][block8x8]; v<by0[parttype][block8x8]+step_v0; v+=step_v)
            for (h=bx0[parttype][block8x8]; h<bx0[parttype][block8x8]+step_h0; h+=step_h)
            {

                mvd_bits += mvbits[ all_omv [h/2][v/2][fw_ref][blocktype][0] - p_fwMV[h/2][v/2][fw_ref][blocktype][0] ];
                mvd_bits += mvbits[ all_omv [h/2][v/2][fw_ref][blocktype][1] - p_fwMV[h/2][v/2][fw_ref][blocktype][1] ];

            }
            mcost = WEIGHTED_COST (lambda_factor, mvd_bits);

            //----- cost of residual signal -----
            for (byy=0, v=by0[parttype][block8x8]; v<by0[parttype][block8x8]+step_v0; byy+=4, v++)
            {
                //pic_pix_y = pix_y + (block_y = (v<<2));//b
                block_y = (v<<2);//b

                for (bxx=0, h=bx0[parttype][block8x8]; h<bx0[parttype][block8x8]+step_h0; bxx+=4, h++)
                {
                    pic_pix_x = img->pix_x + (block_x = (h<<2));

                    LumaPrediction4x4 (block_x, block_y, blocktype, blocktype, fw_ref, bw_ref);/*lgp*13*/

                    for (k=j=0; j<4; j++)
                        for (  i=0; i<4; i++, k++)
                        {            
                            diff[k] = curr_blk[byy+j][bxx+i] = 
                                imgY_original[pic_pix_y+incr_y*(j+byy)+off_y][pic_pix_x+i] - img->mpr[i+block_x][j+block_y];//b
                        }
                }
            }

            mcost += find_sad_8x8(input->hadamard,8,8,0,0,curr_blk);

            return mcost;
}

/*
*************************************************************************
* Function:Get cost for skip mode for an macroblock
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int GetSkipCostMB (double lambda)
{
    int block_y, block_x, pic_pix_y, pic_pix_x, i, j, k;
    int diff[16];
    int cost = 0;
    int pix_x = img->pix_x;
    int pix_y = img->pix_y;
    byte**    imgY_org_pic = imgY_org;
    //int   incr_y=1,off_y=0;//b

    for (block_y=0; block_y<16; block_y+=4)
    {
        pic_pix_y = pix_y + block_y;

        //b

        for (block_x=0; block_x<16; block_x+=4)
        {
            pic_pix_x = pix_x + block_x;

            //===== prediction of 4x4 block =====
            LumaPrediction4x4 (block_x, block_y, 0, 0, 0, 0);

            //===== get displaced frame difference ======                
            for (k=j=0; j<4; j++)
                for (i=0; i<4; i++, k++)
                {
                    diff[k] = imgY_org_pic[pic_pix_y+j][pic_pix_x+i] - img->mpr[i+block_x][j+block_y];//b
                }

                cost += SATD (diff, input->hadamard);
        }
    }

    return cost;
}

/*
*************************************************************************
* Function:Find motion vector for the Skip mode
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void FindSkipModeMotionVector ()
{
    int bx, by;
    int mb_nr = img->current_mb_nr;
    int mb_width = img->width/16;
    //int mb_available_up   = (img->mb_y == 0)  ? 0 : 1;
    int mb_available_up      = (img->mb_y == 0          ) ? 0 : (img->mb_data[mb_nr].slice_nr == img->mb_data[mb_nr-mb_width  ].slice_nr);  //jlzheng 6.23
    //int mb_available_left = (img->mb_x == 0)  ? 0 : 1;
    int mb_available_left    = (img->mb_x == 0          ) ? 0 : (img->mb_data[mb_nr].slice_nr == img->mb_data[mb_nr-1         ].slice_nr);
    int zeroMotionAbove   = !mb_available_up  ? 1 : refFrArr[(img->block_y>>1)-1][(img->block_x>>1)] == 0 && tmp_mv[0][(img->block_y>>1)-1][4+(img->block_x>>1)]   == 0 && tmp_mv[1][(img->block_y>>1)-1][4+(img->block_x>>1)]   == 0 ? 1 : 0;
    int zeroMotionLeft    = !mb_available_left? 1 : refFrArr[(img->block_y>>1)][(img->block_x>>1)-1] == 0 && tmp_mv[0][(img->block_y>>1)  ][4+(img->block_x>>1)-1] == 0 && tmp_mv[1][(img->block_y>>1)  ][4+(img->block_x>>1)-1] == 0 ? 1 : 0;
    int mb_x = img->mb_x;
    int mb_y = img->mb_y;
    int block_x = img->block_x;
    int block_y = img->block_y;
    int **refar = refFrArr;
    int ***tmpmv = tmp_mv;
    int *****all_mv = img->all_mv;
    //int *****mv  = img->mv;
    int *mv  = img->mv[0][0][0][0];

    if (zeroMotionAbove || zeroMotionLeft)
    {
        for (by = 0;by < 2;by++)
            for (bx = 0;bx < 2;bx++)
            {
                all_mv [bx][by][0][0][0] = 0;
                all_mv [bx][by][0][0][1] = 0;
            }
    }
    else
    {
        //    for (by = 0;by < 2;by++)
        //      for (bx = 0;bx < 2;bx++)
        //      {
        //        all_mv [bx][by][0][0][0] = mv[0][0][0][1][0];
        //        all_mv [bx][by][0][0][1] = mv[0][0][0][1][1];
        //      }

        //for skip and 16x16 use the same block size if 16x16 is disable 
        //the skip should not be disable
        //cjw 20060327  qhg   when  16x16 is disable, skip and 16x16 should use the different
        //motion vectorpredictor
        SetMotionVectorPredictor(mv,refar,tmpmv,0,0,0,16,16, 0, 0);
        for (by = 0;by < 2;by++)
            for (bx = 0;bx < 2;bx++)
            {
                all_mv [bx][by][0][0][0] = mv[0];
                all_mv [bx][by][0][0][1] = mv[1];
            }
    }

    // xiaozhen zheng, mv_range, 20071009
    img->mv_range_flag = check_mv_range(mv[0], mv[1], block_x*4, block_y*4, 1);

}

/*
*************************************************************************
* Function:Get cost for direct mode for an 8x8 block
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int Get_Direct_Cost8x8 (int block, double lambda)
{
    int block_y, block_x, pic_pix_y, pic_pix_x, i, j, k;
    int diff[16];
    int cost  = 0;
    int mb_y  = (block/2)<<3;
    int mb_x  = (block%2)<<3;
    int curr_blk[MB_BLOCK_SIZE][MB_BLOCK_SIZE]; // pred.error buffer for 8x8
    int bxx, byy;                               // indexing curr_blk
    byte **imgY_original = imgY_org;
    int pix_y = img->pix_y;

    //b


    for (byy=0, block_y=mb_y; block_y<mb_y+8; byy+=4, block_y+=4)
    {
        pic_pix_y = pix_y + block_y;

        for (bxx=0, block_x=mb_x; block_x<mb_x+8; bxx+=4, block_x+=4)
        {
            pic_pix_x = img->pix_x + block_x;

            //===== prediction of 4x4 block =====
            LumaPrediction4x4 (block_x, block_y, 0, 0, max(0,refFrArr[pic_pix_y>>2][pic_pix_x>>2]), 0);

            //===== get displaced frame difference ======                
            for (k=j=0; j<4; j++)
                for (i=0; i<4; i++, k++)
                {
                    diff[k] =  curr_blk[byy+j][bxx+i] =
                        imgY_original[pic_pix_y+j][pic_pix_x+i] - img->mpr[i+block_x][j+block_y];
                }
        }
    }

    cost += find_sad_8x8(input->hadamard,8,8,0,0,curr_blk);

    return cost;
}

/*
*************************************************************************
* Function:Get cost for direct mode for an macroblock
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int Get_Direct_CostMB (double lambda)
{
    int i;
    int cost = 0;

    //b

    mv_out_of_range = 1;  // mv_range, 20071009

    for (i=0; i<4; i++)
    {
        cost += Get_Direct_Cost8x8 (i, lambda);
    }

    // mv_range, 20071009
    if (!mv_out_of_range)
        cost = 1<<30;

    return cost;
}

/*
*************************************************************************
* Function:Motion search for a partition
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void PartitionMotionSearch (int    blocktype,
                            int    block8x8,
                            double lambda)
{
    static int  bx0[5][4] = {{0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,1,0,0}, {0,1,0,1}};
    static int  by0[5][4] = {{0,0,0,0}, {0,0,0,0}, {0,1,0,0}, {0,0,0,0}, {0,0,1,1}};

    int   **ref_array, ***mv_array, *****all_mv;
    int   ref, refinx, refframe, v, h, mcost, search_range, i, j;
    int   pic_block_x, pic_block_y;
    int   bframe    = (img->type==B_IMG);
    int   max_ref   = bframe? img->nb_references-1 : img->nb_references;
    int   parttype  = (blocktype<4?blocktype:4);

    int   step_h0   = (input->blc_size[ parttype][0]>>3);
    int   step_v0   = (input->blc_size[ parttype][1]>>3);
    int   step_h    = (input->blc_size[blocktype][0]>>3);
    int   step_v    = (input->blc_size[blocktype][1]>>3);
    int   block_x   = img->block_x;
    int   block_y   = img->block_y;
    int   min_ref   = (bframe?-1:0);/*lgp13*/

    if(img->type==B_IMG)  max_ref =1;  

    if (max_ref > img->buf_cycle)
        max_ref = img->buf_cycle;
    if (mref[0] == mref_fld[0])
    {
        if (img->type == B_IMG)
        {
            max_ref = 2;
            min_ref = -2;
        }
    }

    //===== LOOP OVER REFERENCE FRAMES =====
    for (ref=min_ref/*lgp13*/; ref<max_ref; ref++)
    {
        refinx    = ref+1;
        refframe  = (ref<0?0:ref);
        if (mref[0] == mref_fld[0])
        {
            if (img->type == B_IMG)
            {
                refinx    = ref+2;
                refframe  = (ref<0?ref+2:ref);  // Yulj 2004.07.15
            }
        }

        //   if(img->type == B_IMG && img->VEC_FLAG==1 && input->vec_ext==3 && ref>=0) // M1956 by Grandview 2006.12.12
        //   {
        //     motion_cost[blocktype][refinx][block8x8] = 1<<30;
        //     continue;
        //   }

        //----- set search range ---
        search_range = input->search_range;

        //----- set arrays -----
        ref_array = ((img->type!=B_IMG) ? refFrArr : ref<0 ? bw_refFrArr : fw_refFrArr);
        mv_array  = ((img->type!=B_IMG) ?   tmp_mv : ref<0 ?    tmp_bwMV :    tmp_fwMV);
        all_mv    = (ref<0 ? img->all_bmv : img->all_mv);

        //----- init motion cost -----
        motion_cost[blocktype][refinx][block8x8] = 0;

        //===== LOOP OVER BLOCKS =====
        for (v=by0[parttype][block8x8]; v<by0[parttype][block8x8]+step_v0; v+=step_v)
        {
            pic_block_y = (block_y>>1) + v;

            for (h=bx0[parttype][block8x8]; h<bx0[parttype][block8x8]+step_h0; h+=step_h)
            {
                pic_block_x = (block_x>>1) + h;

                //--- motion search for block ---
#ifdef FastME
                mcost = FME_BlockMotionSearch (ref, 8*pic_block_x, 8*pic_block_y, blocktype, search_range, lambda);//should modify the constant?
#else
                mcost = BlockMotionSearch (ref, 8*pic_block_x, 8*pic_block_y, blocktype, search_range, lambda);
#endif
                motion_cost[blocktype][refinx][block8x8] += mcost;

                //--- set motion vectors and reference frame (for motion vector prediction) ---
                for (j=0; j<step_v; j++)
                    for (i=0; i<step_h; i++)
                    {
                        mv_array[0][pic_block_y+j][pic_block_x+i+4] = all_mv[h][v][refframe][blocktype][0];
                        mv_array[1][pic_block_y+j][pic_block_x+i+4] = all_mv[h][v][refframe][blocktype][1];
                        ref_array  [pic_block_y+j][pic_block_x+i  ] = refframe;
                    }
            }
        }
    }

}

/*
*************************************************************************
* Function:Motion search for a partition
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void
PartitionMotionSearch_bid (int    blocktype,
                           int    block8x8,
                           double lambda)
{
    static int  bx0[5][4] = {{0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,1,0,0}, {0,1,0,1}};
    static int  by0[5][4] = {{0,0,0,0}, {0,0,0,0}, {0,1,0,0}, {0,0,0,0}, {0,0,1,1}};

    int   **ref_array, ***mv_array, *****all_mv;
    int   ref, refinx, refframe, v, h, mcost, search_range, i, j;
    int   pic_block_x, pic_block_y;
    int   bframe    = (img->type==B_IMG);
    int   max_ref   = bframe? img->nb_references-1 : img->nb_references;
    int   parttype  = (blocktype<4?blocktype:4);
    int   step_h0   = (input->blc_size[ parttype][0]>>3);
    int   step_v0   = (input->blc_size[ parttype][1]>>3);
    int   step_h    = (input->blc_size[blocktype][0]>>3);
    int   step_v    = (input->blc_size[blocktype][1]>>3);
    int   block_y   = img->block_y;
    int  min_ref;

    if(img->type==B_IMG)  max_ref = 1;

    if (max_ref > img->buf_cycle) max_ref = img->buf_cycle;
    if (mref[0] == mref_fld[0])
    {
        if (img->type == B_IMG)
        {
            max_ref = 2;
            min_ref = -2;
        }
    }

    //===== LOOP OVER REFERENCE FRAMES =====
    for (ref=0; ref<max_ref; ref++)
    {
        refinx    = ref+1;
        refframe  = (ref<0?0:ref);
        if (mref[0] == mref_fld[0])
        {
            if (img->type == B_IMG)
            {
                refinx    = ref+2;
                refframe  = (ref<0?ref+2:ref);
            }
        }

        //   if(img->type == B_IMG && img->VEC_FLAG==1 && input->vec_ext==3 && ref>=0) // M1956 by Grandview 2006.12.12
        //   {
        //     motion_cost_bid[blocktype][refinx][block8x8] = 1<<30;
        //     continue;
        //   }

        search_range = input->search_range;
        //----- set arrays -----
        ref_array = ((img->type!=B_IMG) ? refFrArr : ref<0 ? bw_refFrArr : fw_refFrArr);
        mv_array  = ((img->type!=B_IMG) ?   tmp_mv : ref<0 ?    tmp_bwMV :    tmp_fwMV);
        //sw 10.1
        //all_mv    = (ref<0 ? img->all_bmv : img->all_mv);
        all_mv    = (ref<0 ? img->all_bmv : img->all_omv);

        //----- init motion cost -----
        motion_cost_bid[blocktype][refinx][block8x8] = 0;

        //===== LOOP OVER BLOCKS =====
        for (v=by0[parttype][block8x8]; v<by0[parttype][block8x8]+step_v0; v+=step_v)
        {
            pic_block_y = (block_y>>1) + v;

            for (h=bx0[parttype][block8x8]; h<bx0[parttype][block8x8]+step_h0; h+=step_h)
            {
                pic_block_x = (img->block_x>>1) + h;

                //--- motion search for block ---
#ifdef FastME
                mcost = FME_BlockMotionSearch_bid (ref, 8*pic_block_x, 8*pic_block_y, blocktype, search_range, lambda);//modify?
#else
                mcost = BlockMotionSearch_bid (ref, 8*pic_block_x, 8*pic_block_y, blocktype, search_range, lambda);
#endif
                motion_cost_bid[blocktype][refinx][block8x8] += mcost;

                //--- set motion vectors and reference frame (for motion vector prediction) ---
                for (j=0; j<step_v; j++)
                    for (i=0; i<step_h; i++)
                    {
                        mv_array[0][pic_block_y+j][pic_block_x+i+4] = all_mv[h][v][refframe][blocktype][0];
                        mv_array[1][pic_block_y+j][pic_block_x+i+4] = all_mv[h][v][refframe][blocktype][1];
                        ref_array  [pic_block_y+j][pic_block_x+i  ] = refframe;
                    }
            }
        }
    }
}


extern int* last_P_no;
/*********************************************
*****                                   *****
*****  Calculate Direct Motion Vectors  *****
*****                                   *****
*********************************************/

/*
*************************************************************************
* Function:
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void Get_IP_direct()
{
    int  block_x, block_y, pic_block_x, pic_block_y;
    int  refframe, bw_ref, /*refP_tr,*/ TRb, TRp, TRd, TRp1;   // 20071009
    int  frame_no_next_P, frame_no_B, delta_P, delta_P_scale;  // 20071009
    int  pix_y            = img->pix_y;
    int  **refarr         = refFrArr;
    int  ***tmpmvs        = tmp_mv;
    int  *****all_mvs     = img->all_mv;
    int  *****all_bmvs    = img->all_bmv;
    int  prev_mb_is_field = 0; 
    int  mv_scale, scale_refframe;

    int  **fwrefarr         = fw_refFrArr;
    int  **bwrefarr         = bw_refFrArr;   
    int  ***tmpmvfw         = tmp_fwMV;
    int  ***tmpmvbw         = tmp_bwMV;

    pic_block_x = img->block_x;
    pic_block_y = img->block_y;

    for (block_y=0; block_y<2; block_y++)
    {
        pic_block_y = (pix_y>>3) + block_y;

        for (block_x=0; block_x<2; block_x++)
        {
            pic_block_x = (img->pix_x>>3) + block_x;

            if((refframe=refarr[pic_block_y][pic_block_x]) == -1)
            {
                all_mvs [block_x][block_y][0][0][0] = 0;
                all_mvs[block_x][block_y][0][0][1]  = 0;
                all_bmvs[block_x][block_y][0][0][0] = 0;
                all_bmvs[block_x][block_y][0][0][1] = 0;        
                SetMotionVectorPredictor(all_mvs [block_x][block_y][0][0],fwrefarr,tmpmvfw,0,0,0,16,16, 0, 1);//Lou 1016
                SetMotionVectorPredictor(all_bmvs [block_x][block_y][img->picture_structure?0:1][0],bwrefarr,tmpmvbw,img->picture_structure?0:1,0,0,16,16, -1, 1);//Lou 1016

                // xiaozhen zheng, mv_range, 20071009 // change img->block_x>>2 and img->block_y>>2 to img->block_x>>1 and img->block_y>>1  20071224
                img->mv_range_flag = check_mv_range(all_mvs [block_x][block_y][0][0][0], all_mvs [block_x][block_y][0][0][1], 
                    8*((img->block_x>>1/*2*/) + block_x), 8*((img->block_y>>1/*2*/) + block_y), 0);
                img->mv_range_flag *= check_mv_range(all_bmvs [block_x][block_y][img->picture_structure?0:1][0][0], all_bmvs [block_x][block_y][img->picture_structure?0:1][0][1],
                    8*((img->block_x>>1/*2*/) + block_x), 8*((img->block_y>>1/*2*/) + block_y), 0);
                // xiaozhen zheng, mv_range, 20071009
            }
            else
            {
                //        refP_tr = nextP_tr - ((refframe+1)*img->p_interval);    // 20071009
                //        refP_tr = (refP_tr+256)%256;              // 20071009
                frame_no_next_P = 2*img->imgtr_next_P_frm;
                //        frame_no_B = 2*img->tr;          // Commented by Xiaozhen ZHENG, 20070413, HiSilicon 
                frame_no_B = 2*picture_distance;    // Added by Xiaozhen ZHENG, 20070413, HiSilicon 
                delta_P = 2*(img->imgtr_next_P_frm - img->imgtr_last_P_frm);
                delta_P = (delta_P + 512)%512;            // Xiaozhen ZHENG, 2007.05.01
                delta_P_scale = 2*(img->imgtr_next_P_frm - img->imgtr_last_prev_P_frm);  // 20071009
                delta_P_scale = (delta_P_scale + 512)%512;    // 20071009

                if(!img->picture_structure)
                {

                    if (img->current_mb_nr_fld < img->total_number_mb) //top field
                        scale_refframe =   refframe == 0  ? 0 : 1;
                    else
                        scale_refframe =   refframe == 1  ? 1 : 2; 
                }
                else
                    scale_refframe = 0;

                if(!img->picture_structure) 
                {
                    if (img->current_mb_nr_fld < img->total_number_mb) //top field
                    {
                        //            TRp = delta_P*(refframe/2+1)-(refframe+1)%2;  //the lates backward reference        // 20071009
                        //            TRp1 = delta_P*(scale_refframe/2+1)-(scale_refframe+1)%2;  //the lates backward reference  // 20071009
                        TRp = refframe<2 ? delta_P-(refframe+1)%2 : delta_P_scale-(refframe+1)%2;            // 20071009
                        TRp1 = scale_refframe<2 ? delta_P-(scale_refframe+1)%2 : delta_P_scale-(scale_refframe+1)%2;  // 20071009
                        bw_ref = 1;
                    }
                    else
                    {          
                        //            TRp = 1 + delta_P*((refframe+1)/2)-refframe%2;          // 20071009
                        //            TRp1 = 1 + delta_P*((scale_refframe+1)/2)-scale_refframe%2;    // 20071009
                        TRp  = refframe==0 ? 1 : refframe<3 ? 1 + delta_P - refframe%2 : 1 + delta_P_scale - refframe%2;              // 20071009
                        TRp1 = scale_refframe==0 ? 1 : scale_refframe<3 ? 1 + delta_P - scale_refframe%2 : 1 + delta_P_scale - scale_refframe%2;  // 20071009
                        bw_ref = 0;
                    }
                }
                else
                {
                    //          TRp  = (refframe+1)*delta_P;      // 20071009
                    //          TRp1  = (scale_refframe+1)*delta_P;    // 20071009
                    TRp  = refframe==0 ? delta_P : delta_P_scale;      // 20071009
                    TRp1 = scale_refframe==0 ? delta_P : delta_P_scale;    // 20071009
                }

                TRd = frame_no_next_P - frame_no_B;

                TRb = TRp1 - TRd;
                // Xiaozhen ZHENG, 2007.05.01
                TRp  = (TRp + 512)%512;
                TRp1 = (TRp1 + 512)%512;
                TRd  = (TRd + 512)%512;
                TRb  = (TRb + 512)%512;
                // Xiaozhen ZHENG, 2007.05.01

                mv_scale = (TRb * 256) / TRp;       //! Note that this could be precomputed at the frame/slice level. 

                refframe = 0;      

                if(!img->picture_structure)
                {
                    if (img->current_mb_nr_fld >= img->total_number_mb) //top field
                        scale_refframe --;
                    refframe = scale_refframe;
                }else
                {
                    refframe = 0;
                    bw_ref = 0;
                }

                if(tmpmvs[0][pic_block_y][pic_block_x+4] < 0)
                {
                    all_mvs [block_x][block_y][refframe][0][0] = -(((16384/TRp)*(1-TRb*tmpmvs[0][pic_block_y][pic_block_x+4])-1)>>14);
                    all_bmvs [block_x][block_y][bw_ref][0][0] =  ((16384/TRp)*(1-TRd*tmpmvs[0][pic_block_y][pic_block_x+4])-1)>>14;
                }
                else
                {
                    all_mvs [block_x][block_y][refframe][0][0] = ((16384/TRp)*(1+TRb*tmpmvs[0][pic_block_y][pic_block_x+4])-1)>>14;
                    all_bmvs [block_x][block_y][bw_ref][0][0] =  -(((16384/TRp)*(1+TRd*tmpmvs[0][pic_block_y][pic_block_x+4])-1)>>14);
                }

                if(tmpmvs[1][pic_block_y][pic_block_x+4] < 0)
                {
                    all_mvs [block_x][block_y][refframe][0][1] = -(((16384/TRp)*(1-TRb*tmpmvs[1][pic_block_y][pic_block_x+4])-1)>>14);
                    all_bmvs [block_x][block_y][bw_ref][0][1] =    ((16384/TRp)*(1-TRd*tmpmvs[1][pic_block_y][pic_block_x+4])-1)>>14;
                }
                else
                {
                    all_mvs [block_x][block_y][refframe][0][1] = ((16384/TRp)*(1+TRb*tmpmvs[1][pic_block_y][pic_block_x+4])-1)>>14;
                    all_bmvs [block_x][block_y][bw_ref][0][1] =  -(((16384/TRp)*(1+TRd*tmpmvs[1][pic_block_y][pic_block_x+4])-1)>>14);
                }

                // xiaozhen zheng, mv_range, 20071009 // change img->block_x>>2 and img->block_y>>2 to img->block_x>>1 and img->block_y>>1  20071224
                img->mv_range_flag *= check_mv_range(all_mvs [block_x][block_y][refframe][0][0], all_mvs [block_x][block_y][refframe][0][1], 
                    8*((img->block_x>>1/*2*/) + block_x), 8*((img->block_y>>1/*2*/) + block_y), 0);
                img->mv_range_flag *= check_mv_range(all_bmvs [block_x][block_y][bw_ref][0][0], all_bmvs [block_x][block_y][bw_ref][0][1],
                    8*((img->block_x>>1/*2*/) + block_x), 8*((img->block_y>>1/*2*/) + block_y), 0);
                // xiaozhen zheng, mv_range, 20071009
            }
        }
    } 
}

/*
*************************************************************************
* Function:control the sign of a with b
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int sign(int a,int b)
{
    int x;
    x=absm(a);

    if (b >= 0)
        return x;
    else
        return -x;
}
