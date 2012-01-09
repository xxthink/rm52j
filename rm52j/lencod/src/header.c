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
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "header.h"
#include "mbuffer.h"
#include "defines.h"
#include "vlc.h"

/*
*************************************************************************
* Function:
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/
int frametotc(int frame, int dropflag)
{
    int fps, pict, sec, minute, hour, tc;

    fps = (int)(frame_rate+0.5);
    pict = frame%fps;
    frame = (frame-pict)/fps;
    sec = frame%60;
    frame = (frame-sec)/60;
    minute = frame%60;
    frame = (frame-minute)/60;
    hour = frame%24;
    tc = (dropflag<<23) | (hour<<18) | (minute<<12) | (sec<<6) | pict;

    return tc;
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
int IPictureHeader(int frame)
{
    Bitstream *bitstream = currBitStream;
    int len = 0;
    int tc;
    int time_code_flag;
    marker_bit=1;
    time_code_flag = 0;

    len += u_v(32, "I picture start code",0x1B3,bitstream);

    //rate control
    if(input->RCEnable&&img->BasicUnit<=img->Frame_Total_Number_MB)
        input->fixed_picture_qp = 0;//  [5/8/2007 Leeswan]
    else
        input->fixed_picture_qp = 1;


    //xyji 12.23  
    len+=u_v(16,"bbv delay",0xffff/*img->bbv_delay*/,bitstream);

    len += u_v(1, "time_code_flag",0,bitstream);
    if (time_code_flag)
    {
        tc = frametotc(tc0+frame,img->dropflag);
        len += u_v(24, "time_code",tc,bitstream);
    }
    len+=u_1("marker_bit",1,bitstream);  //add by wuzhongmou 0610

    len+=u_v(8,"picture_distance",picture_distance,bitstream);

    if(input->low_delay)
    {
        len+=ue_v("bbv check times",bbv_check_times,bitstream);
    }

    len+=u_v(1,"progressive frame",img->progressive_frame,bitstream);
    if(!img->progressive_frame)
        len+=u_v(1,"picture_structure",img->picture_structure,bitstream);

    len+=u_v(1,"top field first",input->top_field_first,bitstream);
    len+=u_v(1,"repeat first field",input->repeat_first_field,bitstream);
    len+=u_v(1,"fixed picture qp",input->fixed_picture_qp,bitstream);
    //rate control
    if(input->RCEnable)
        len+=u_v(6,"I picture QP",img->qp,bitstream);
    else
    {
        len+=u_v(6,"I picture QP",input->qp0,bitstream);  
        img->qp = input->qp0;
    }

    //xyji 12.23
    if(img->progressive_frame==0)   //by oliver according to 1658
        if(img->picture_structure == 0)
        {
            len+=u_v(1,"skip mode flag",input->skip_mode_flag,bitstream);
        }

        len+=u_v(4,"reserved bits",0,bitstream);

        len+=u_v(1,"loop filter disable",input->loop_filter_disable,bitstream);
        if (!input->loop_filter_disable)
        {
            len+=u_v(1,"loop filter parameter flag",input->loop_filter_parameter_flag,bitstream);
            if (input->loop_filter_parameter_flag)
            {
                len+=se_v("alpha offset",input->alpha_c_offset,bitstream);
                len+=se_v("beta offset",input->beta_offset,bitstream);
            }
        }

        picture_reference_flag  = 0;

        return len;
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

int PBPictureHeader()
{
    Bitstream *bitstream = currBitStream;
    int len = 0;

    if (img->type == INTER_IMG)
    {  
        img->count_PAFF=img->count_PAFF+1;
        picture_coding_type = 1;
        if((input->InterlaceCodingOption == FRAME_CODING)||(input->InterlaceCodingOption == FIELD_CODING))   //add by wuzhongmou
        {
            // Add by cjw, 20070327, Begin
            if(img->Seqheader_flag==1)   
            {
                //                img->buf_cycle=1;    // restrict ref_num after the seq_header, 20071009
                img->Seqheader_flag=0;
            } 
            // Add by cjw, 20070327, End
        } 
        if(input->InterlaceCodingOption == PAFF_CODING)
        {
            // Add by cjw, 20070327, Begin
            if(img->Seqheader_flag==1)     
            {
                //                img->buf_cycle=1;    // restrict ref_num after the seq_header, 20071009
                if(img->count_PAFF==2)
                    img->Seqheader_flag=0;
            } 
            // Add by cjw, 20070327, End
        }//add by wuzhongmou

    }
    else
        picture_coding_type = 2; //add by wuzhongmou 

    //rate control
    if(input->RCEnable&&img->BasicUnit<=img->Frame_Total_Number_MB)
        input->fixed_picture_qp = 0;//  [5/8/2007 Leeswan]
    else
        input->fixed_picture_qp = 1;

    if (img->nb_references==1)
        picture_reference_flag = 1;
    else if (img->nb_references>1)
        picture_reference_flag = 0;

    len+=u_v(24,"start code prefix",1,bitstream);
    len+=u_v(8, "PB picture start code",0xB6,bitstream);
    //xyji 12.23  
    len+=u_v(16,"bbv delay",0xffff/*bbv_delay*/,bitstream);
    len+=u_v(2,"picture coding type",picture_coding_type,bitstream);

    len+=u_v(8,"picture_distance",picture_distance,bitstream);

    //if(low_delay == 1)
    if(input->low_delay) //cjw 20070414
    {
        len+=ue_v("bbv check times",bbv_check_times,bitstream);
    }

    len+=u_v(1,"progressive frame",img->progressive_frame,bitstream);
    if (!img->progressive_frame)
    {
        len+=u_v(1,"picture_structure",img->picture_structure,bitstream);
        if (!img->picture_structure)
        {   
            img->advanced_pred_mode_disable=1;  //add by wuzhongmou 06.10   
            len+=u_v(1,"advanced_pred_mode_disable",img->advanced_pred_mode_disable,bitstream);
        }
    }
    len+=u_v(1,"top field first",input->top_field_first,bitstream);
    len+=u_v(1,"repeat first field",input->repeat_first_field,bitstream);
    len+=u_v(1,"fixed qp",input->fixed_picture_qp,bitstream);
    //rate control
    if(img->type==INTER_IMG)
    {
        if(input->RCEnable)
            len+=u_v(6,"I picture QP",img->qp,bitstream);
        else
        {
            len+=u_v(6,"I picture QP",input->qpN,bitstream);
            img->qp=input->qpN;
        }
    }
    else if(img->type==B_IMG)
    {
        if(input->RCEnable)
            len+=u_v(6,"I picture QP",img->qp,bitstream);
        else
        {
            len+=u_v(6,"I picture QP",input->qpB,bitstream);
            img->qp=input->qpB;
        }
    }

    if (!(picture_coding_type == 2 && img->picture_structure==1))
    {
        len+=u_v(1,"piture reference flag",picture_reference_flag,bitstream);
    }

    len+=u_v(1, "no_forward_reference_flag", 0, bitstream);    // Added by cjw, 20070327

    //  len+=u_v(4,"reserved bits",0,bitstream);          // Commented by cjw, 20070327
    len+=u_v(3,"reserved bits",0,bitstream);          // Added by cjw, 20070327
    len+=u_v(1,"skip mode flag",input->skip_mode_flag, bitstream);

    len+=u_v(1,"loop filter disable",input->loop_filter_disable,bitstream);
    if (!input->loop_filter_disable)
    {
        len+=u_v(1,"loop filter parameter flag",input->loop_filter_parameter_flag,bitstream);
        if (input->loop_filter_parameter_flag)
        {
            len+=se_v("alpha offset",input->alpha_c_offset,bitstream);
            len+=se_v("beta offset",input->beta_offset,bitstream);
        }
    }


    return len;
}

/*
*************************************************************************
* Function:
* Input:
* Output:
* Return: 
* Attention:    cjw 20060112  Spec 9.4.3
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
*************************************************************************
*/                                                                

int SliceHeader(int slice_nr, int slice_qp)
{
    Bitstream *bitstream = currBitStream;
    int i;
    int len = 0;

    //cjw 20060321
    int weight_para_num;

    len+=u_v(24,"start code prefix",1,bitstream);
    //  len+=u_v(8, "slice vertical position",slice_nr,bitstream);  //cjw 20060327
    //  len+=u_v(8, "slice vertical position",img->current_mb_nr/(img->width>>4),bitstream);   //add by wuzhongmou 200612  //commented by Xiaozhen Zheng, 20070327
    len+=u_v(8, "slice vertical position",(img->current_mb_nr+bot_field_mb_nr+1)/(img->width>>4),bitstream);  //Added by Xiaozhen Zheng, HiSilicon, 20070327
    if(img->height > 2800)     //add by wuzhongmou 200612
        len+=u_v(3, "slice vertical position extension",slice_vertical_position_extension,bitstream);

    if (!input->fixed_picture_qp)
    {
        len += u_v(1,"fixed_slice_qp",0,bitstream);//  [5/8/2007 Leeswan]
        len += u_v(6,"slice_qp",slice_qp,bitstream);
        img->qp=slice_qp;
    }

    // 2004/08
    if(img->type != INTRA_IMG){
        len += u_v(1,"slice weighting flag",img->LumVarFlag,bitstream);

        if(img->LumVarFlag)  
        {
            //cjw 20060321  Spec 9.4.3
            if(second_IField && !img->picture_structure)  //I bottom
                weight_para_num=1;
            else if(img->type==INTER_IMG && img->picture_structure)  //P frame coding
                weight_para_num=2;
            else if(img->type==INTER_IMG && !img->picture_structure) //P field coding
                weight_para_num=4;
            else if(img->type==B_IMG && img->picture_structure)  //B frame coding
                weight_para_num=2;
            else if(img->type==B_IMG && !img->picture_structure) //B field coding
                weight_para_num=4;

            //for(i=0;i<img->buf_cycle;i++) //cjw20051230
            for(i=0;i<weight_para_num;i++) //cjw20050321
            {
                len+=u_v(8,"luma scale",img->lum_scale[i],bitstream);      //cjw 20051230
                //  len+=u_v(8,"luma shift",img->lum_shift[i]+127,bitstream); 
                //  img->lum_shift[i]=-1;
                len+=u_v(8,"luma shift",img->lum_shift[i],bitstream);     //cjw 20051230

                u_1 ("insert bit", 1, bitstream);

                len+=u_v(8,"chroma scale",img->chroma_scale[i],bitstream); //cjw 20051230
                //  len+=u_v(8,"chroma shift",img->chroma_shift[i]+127,bitstream);
                len+=u_v(8,"chroma shift",img->chroma_shift[i],bitstream); //cjw 20051230

                u_1 ("insert bit", 1, bitstream); //cjw 20060321
            }

            len+=u_v(1,"mb weighting flag",img->mb_weighting_flag,bitstream);
        }
    }
    // end for 2004/08

    return len;
}
