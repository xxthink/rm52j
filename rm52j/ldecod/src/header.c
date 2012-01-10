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
* File name: header.c
* Function: AVS Slice headers
*
*************************************************************************************
*/


#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "memalloc.h"
#include "global.h"
#include "elements.h"
#include "defines.h"
#include "vlc.h"
#include "header.h"
CameraParamters CameraParameter, *camera=&CameraParameter;
/*
*************************************************************************
* Function:sequence header
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

void SequenceHeader (char *buf,int startcodepos, int length)
{

    memcpy (currStream->streamBuffer, buf, length);
    currStream->code_len = currStream->bitstream_length = length;
    currStream->read_len = currStream->frame_bitoffset = (startcodepos+1)*8;

    profile_id                  = u_v  (8, "profile_id"  );
    level_id                    = u_v  (8, "level_id"    );
    progressive_sequence        = u_v  (1, "progressive_sequence"  );
    horizontal_size             = u_v  (14, "horizontal_size"  );
    vertical_size               = u_v  (14, "vertical_size"  );
    chroma_format               = u_v  (2, "chroma_format"  );
    sample_precision            = u_v  (3, "sample_precision"  );
    aspect_ratio_information    = u_v  (4, "aspect_ratio_information"  );
    frame_rate_code             = u_v  (4, "frame_rate_code"  );

    bit_rate_lower              = u_v  (18, "bit_rate_lower"  );
    u_v  (1, "marker bit"  );
    bit_rate_upper              = u_v  (12, "bit_rate_upper"  );
    low_delay                   = u_v  (1, "low_delay"  );
    u_v  (1, "marker bit"  );
    bbv_buffer_size = u_v(18,"bbv buffer size");

    u_v  (3,"reseved bits"  );

    img->width          = horizontal_size;
    img->height         = vertical_size;
    img->width_cr       = (img->width>>1);
    if(chroma_format==2)
        img->height_cr  = (img->height);
    else
        img->height_cr      = (img->height>>1);
    img->PicWidthInMbs  = img->width/MB_BLOCK_SIZE;
    img->PicHeightInMbs = img->height/MB_BLOCK_SIZE;
    img->PicSizeInMbs   = img->PicWidthInMbs * img->PicHeightInMbs;
    img->buf_cycle      = input->buf_cycle+1;
    img->max_mb_nr      =(img->width * img->height) / (MB_BLOCK_SIZE * MB_BLOCK_SIZE);

}


void video_edit_code_data(char *buf,int startcodepos, int length)
{
    currStream->frame_bitoffset = currStream->read_len = (startcodepos+1)*8;
    currStream->code_len = currStream->bitstream_length = length;
    memcpy (currStream->streamBuffer, buf, length);
    vec_flag = 1;
}
/*
*************************************************************************
* Function:I picture header  //sw
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

void I_Picture_Header(char *buf,int startcodepos, int length)
{
    currStream->frame_bitoffset = currStream->read_len = (startcodepos+1)*8;
    currStream->code_len = currStream->bitstream_length = length;
    memcpy (currStream->streamBuffer, buf, length);

    bbv_delay = u_v(16,"bbv delay");

    time_code_flag       = u_v(1,"time_code_flag");
    if (time_code_flag)
        time_code        =u_v(24,"time_code");

    marker_bit            =u_v(1,"marker_bit");
    picture_distance      = u_v(8,"picture_distance");

    if(low_delay)
    {
        bbv_check_times = ue_v("bbv check times");
    }

    progressive_frame    = u_v(1,"progressive_frame");
    if (!progressive_frame)
    {
        img->picture_structure   = u_v(1,"picture_structure");
    }else
    {
        img->picture_structure = 1;
    }

    top_field_first      = u_v(1,"top_field_first");
    repeat_first_field   = u_v(1,"repeat_first_field");
    fixed_picture_qp     = u_v(1,"fixed_picture_qp");
    picture_qp           = u_v(6,"picture_qp");


    if(progressive_frame==0)
    {
        if(img->picture_structure == 0)
        {
            skip_mode_flag =u_v(1,"skip mode flag");
        }
    }

    u_v(4,"reserved bits");

    loop_filter_disable = u_v(1,"loop_filter_disable");
    if (!loop_filter_disable)
    {
        loop_filter_parameter_flag = u_v(1,"loop_filter_parameter_flag");
        if (loop_filter_parameter_flag)
        {
            alpha_offset = se_v("alpha_offset");
            beta_offset  = se_v("beta_offset");
        }
        else
        {
            alpha_offset = 0;
            beta_offset  = 0;
        }
    }

    img->qp                = picture_qp;
    img->pic_distance      = picture_distance;
    img->type              = I_IMG;
}

/*
*************************************************************************
* Function:pb picture header
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/
void PB_Picture_Header(char *buf,int startcodepos, int length)
{
    currStream->frame_bitoffset = currStream->read_len = (startcodepos+1)*8;
    currStream->code_len = currStream->bitstream_length = length;
    memcpy (currStream->streamBuffer, buf, length);

    bbv_delay = u_v(16,"bbv delay");
    picture_coding_type       = u_v(2,"picture_coding_type");

    picture_distance         = u_v(8,"picture_distance");


    if(low_delay)
    {
        bbv_check_times = ue_v("bbv check times");
    }

    progressive_frame        = u_v(1,"progressive_frame");
    if (!progressive_frame)
    {
        img->picture_structure = u_v(1,"picture_structure");
        if (!img->picture_structure)
            img->advanced_pred_mode_disable = u_v(1,"advanced_pred_mode_disable");
    }else
        img->picture_structure   = 1;
    top_field_first        = u_v(1,"top_field_first");
    repeat_first_field     = u_v(1,"repeat_first_field");

    fixed_picture_qp       = u_v(1,"fixed_picture_qp");
    picture_qp             = u_v(6,"picture_qp");


    if (!(picture_coding_type==2 && img->picture_structure==1))
    {
        picture_reference_flag = u_v(1,"picture_reference_flag");
    }
    img->no_forward_reference = u_v(1, "no_forward_reference_flag");
    u_v(3,"reserved bits");

    skip_mode_flag      = u_v(1,"skip_mode_flag");
    loop_filter_disable = u_v(1,"loop_filter_disable");
    if (!loop_filter_disable)
    {
        loop_filter_parameter_flag = u_v(1,"loop_filter_parameter_flag");
        if (loop_filter_parameter_flag)
        {
            alpha_offset = se_v("alpha_offset");
            beta_offset  = se_v("beta_offset");
        }
        else  // 20071009
        {
            alpha_offset = 0;
            beta_offset  = 0;
        }
    }

    img->qp = picture_qp;

    if(picture_coding_type==1) {
        img->type = P_IMG;
        vec_flag = 0;
    } else {
        img->type = B_IMG;
    }
    img->pic_distance = picture_distance;
}
/*
*************************************************************************
* Function:decode extension and user data
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

void extension_data(char *buf,int startcodepos, int length)
{
    int ext_ID;

    memcpy (currStream->streamBuffer, buf, length);
    currStream->code_len = currStream->bitstream_length = length;
    currStream->read_len = currStream->frame_bitoffset = (startcodepos+1)*8;

    ext_ID = u_v(4,"extension ID");

    switch (ext_ID)
    {
    case SEQUENCE_DISPLAY_EXTENSION_ID:
        sequence_display_extension();
        break;
    case COPYRIGHT_EXTENSION_ID:
        copyright_extension();
        break;
    case PICTURE_DISPLAY_EXTENSION_ID:
        picture_display_extension();
    case CAMERAPARAMETERS_EXTENSION_ID:
        cameraparameters_extension();
        break;
    default:
        printf("reserved extension start code ID %d\n",ext_ID);
        break;
    }

}
/*
*************************************************************************
* Function: decode sequence display extension
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/
void sequence_display_extension()
{
    video_format      = u_v(3,"video format ID");
    video_range       = u_v(1,"video range");
    color_description = u_v(1,"color description");

    if (color_description)
    {
        color_primaries          = u_v(8,"color primaries");
        transfer_characteristics = u_v(8,"transfer characteristics");
        matrix_coefficients      = u_v(8,"matrix coefficients");
    }

    display_horizontal_size = u_v(14,"display_horizontaol_size");
    u_v  (1, "marker bit"  );
    display_vertical_size   = u_v(14,"display_vertical_size");
    u_v  (2, "reserved bits"  );
}

/*
*************************************************************************
* Function: decode picture display extension
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/
void picture_display_extension()
{
    int NumberOfFrameCentreOffsets;
    int i;

    if (progressive_sequence == 1) {
        if (repeat_first_field == 1){
            if (top_field_first == 1)
                NumberOfFrameCentreOffsets = 3;
            else
                NumberOfFrameCentreOffsets = 2;
        } else {
            NumberOfFrameCentreOffsets = 1;
        }
    } else {
        if (img->picture_structure == 0){
            NumberOfFrameCentreOffsets = 1;
        } else {
            if (repeat_first_field == 1)
                NumberOfFrameCentreOffsets = 3;
            else
                NumberOfFrameCentreOffsets = 2;
        }
    }

    for(i = 0; i < NumberOfFrameCentreOffsets; i++){
        frame_centre_horizontal_offset[i] = u_v(16, "frame_centre_horizontal_offset");
        u_v(1,"marker_bit");
        frame_centre_vertical_offset[i]   = u_v(16, "frame_centre_vertical_offset");
        u_v(1,"marker_bit");
    }

}

/*
*************************************************************************
* Function:user data //sw
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

void user_data(char *buf,int startcodepos, int length)
{
    int user_data;
    int i;

    memcpy (currStream->streamBuffer, buf, length);

    currStream->code_len = currStream->bitstream_length = length;
    currStream->read_len = currStream->frame_bitoffset = (startcodepos+1)*8;

    for(i=0; i<length-4; i++)
        user_data = u_v (8, "user data");
}

/*
*************************************************************************
* Function:Copyright extension //sw
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

void copyright_extension()
{
    int reserved_data;
    int marker_bit;

    copyright_flag       =  u_v(1,"copyright_flag");
    copyright_identifier =  u_v(8,"copyright_identifier");
    original_or_copy     =  u_v(1,"original_or_copy");

    /* reserved */
    reserved_data      = u_v(7,"reserved_data");
    marker_bit         = u_v(1,"marker_bit");
    copyright_number_1 = u_v(20,"copyright_number_1");
    marker_bit         = u_v(1,"marker_bit");
    copyright_number_2 = u_v(22,"copyright_number_2");
    marker_bit         = u_v(1,"marker_bit");
    copyright_number_3 = u_v(22,"copyright_number_3");

}


/*
*************************************************************************
* Function:Copyright extension //sw
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

void cameraparameters_extension()
{
    //  int marker_bit1,marker_bit,marker_bit2;
    // float reserved_data;
    /* reserved */
    u_v(1,"reserved");
    u_v(7,"camera id");
    u_v(1,"marker bit");
    u_v(22,"height_of_image_device");
    u_v(1,"marker bit");
    u_v(22,"focal_length");
    u_v(1,"marker bit");
    u_v(22,"f_number");
    u_v(1,"marker bit");
    u_v(22,"vertical_angle_of_view");
    u_v(1,"marker bit");

    u_v(16,"camera_position_x_upper");
    u_v(1,"marker bit");
    u_v(16,"camera_position_x_lower");
    u_v(1,"marker bit");

    u_v(16,"camera_position_y_upper");
    u_v(1,"marker bit");
    u_v(16,"camera_position_y_lower");
    u_v(1,"marker bit");

    u_v(16,"camera_position_z_upper");
    u_v(1,"marker bit");
    u_v(16,"camera_position_z_lower");
    u_v(1,"marker bit");


    u_v(22,"camera_direction_x");
    u_v(1,"marker bit");
    u_v(22,"camera_direction_y");
    u_v(1,"marker bit");
    u_v(22,"camera_direction_z");
    u_v(1,"marker bit");

    u_v(22,"image_plane_vertical_x");
    u_v(1,"marker bit");
    u_v(22,"image_plane_vertical_y");
    u_v(1,"marker bit");
    u_v(22,"image_plane_vertical_z");
    u_v(1,"marker bit");

    u_v(32,"reserved data");
}
/*
*************************************************************************
* Function:To calculate the poc values
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

void calc_picture_distance(struct img_par *img)
{
    static int flag = 0;
    // for POC mode 0:
    unsigned int        MaxPicDistanceLsb = (1<<8);
    flag ++;
    // for POC mode 1:

    // Calculate the MSBs of current picture
    if( img->pic_distance  <  img->PrevPicDistanceLsb  &&
        ( img->PrevPicDistanceLsb - img->pic_distance )  >=  ( MaxPicDistanceLsb / 2 ) )
        img->CurrPicDistanceMsb = img->PicDistanceMsb + MaxPicDistanceLsb;
    else if ( img->pic_distance  >  img->PrevPicDistanceLsb  &&
        ( img->pic_distance - img->PrevPicDistanceLsb )  >  ( MaxPicDistanceLsb / 2 ) )
        img->CurrPicDistanceMsb = img->PicDistanceMsb - MaxPicDistanceLsb;
    else
        img->CurrPicDistanceMsb = img->PicDistanceMsb;

    // 2nd
    img->toppoc = img->CurrPicDistanceMsb + img->pic_distance;
    img->bottompoc = img->toppoc + img->delta_pic_order_cnt_bottom;

    // last: some post-processing.
    if ( img->toppoc <= img->bottompoc )
        img->ThisPOC = img->framepoc = img->toppoc;
    else
        img->ThisPOC = img->framepoc = img->bottompoc;

    img->tr_frm = img->ThisPOC/2;
    img->tr = img->ThisPOC;
    img->tr_fld = img->ThisPOC;
    //moved from above for stuff that still uses img->tr

    if(img->type != B_IMG)
    {
        img->imgtr_last_prev_P = img->imgtr_last_P;
        img->imgtr_last_P = img->imgtr_next_P;
        img->imgtr_next_P = picture_distance;
        img->Bframe_number=0;
    }
    if(img->type==B_IMG)
    {
        img->Bframe_number++;
    }
#ifdef _OUTPUT_DISTANCE_
    fprintf(pf_distance, "img->pic_distance:%4d\timg->toppoc:%4d\n", img->pic_distance, img->toppoc);
    fflush(pf_distance);
#endif
}

/*
*************************************************************************
* Function:slice header
* Input:
* Output:
* Return:
* Attention:          // CJW Spec 9.4.3  6.30
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

void SliceHeader (char *buf,int startcodepos, int length)
{
    int i;


    int weight_para_num;
    int mb_row;
    int mb_index;
    int mb_width, mb_height;

    memcpy (currStream->streamBuffer, buf, length);
    currStream->code_len = currStream->bitstream_length = length;

    currStream->read_len = currStream->frame_bitoffset = (startcodepos)*8;
    slice_vertical_position              = u_v  (8, "slice vertical position");

    if(vertical_size > 2800)
        slice_vertical_position_extension  = u_v  (3, "slice vertical position extension");

    if (vertical_size > 2800)
        mb_row = (slice_vertical_position_extension << 7) + slice_vertical_position;
    else
        mb_row = slice_vertical_position;

    mb_width  = (horizontal_size + 15) / 16;
    if(!progressive_sequence)
        mb_height = 2 * ((vertical_size + 31) / 32);
    else
        mb_height = (vertical_size + 15) / 16;

    mb_index = mb_row * mb_width;

    if (!img->picture_structure && img->type==I_IMG && (mb_index >= mb_width*mb_height/2) ) {
        second_IField = 1;
        img->type = P_IMG;
        pre_img_type = P_IMG;
    }

    if (!fixed_picture_qp)
    {
        fixed_slice_qp   = u_v  (1,"fixed_slice_qp");
        slice_qp         = u_v  (6,"slice_qp");

        img->qp=slice_qp;
    }

    if(img->type != I_IMG){
        img->slice_weighting_flag = u_v(1,"slice weighting flag");
        if(img->slice_weighting_flag)
        {
            if(second_IField && !img->picture_structure)  //I bottom
                weight_para_num=1;
            else if(img->type==P_IMG && img->picture_structure)  //P frame coding
                weight_para_num=2;
            else if(img->type==P_IMG && !img->picture_structure) //P field coding
                weight_para_num=4;
            else if(img->type==B_IMG && img->picture_structure)  //B frame coding
                weight_para_num=2;
            else if(img->type==B_IMG && !img->picture_structure) //B field coding
                weight_para_num=4;

            for(i=0;i<weight_para_num;i++)
            {
                img->lum_scale[i] = u_v(8,"luma scale");
                img->lum_shift[i] =i_8("luma shift");
                u_1 ("insert bit");
                img->chroma_scale[i] = u_v(8,"chroma scale");
                img->chroma_shift[i] = i_8("chroma shift");
                u_1 ("insert bit");
            }
            img->mb_weighting_flag    =u_v(1,"MB weighting flag");
        }
    }


}

/*
*************************************************************************
* Function:Error handling procedure. Print error message to stderr and exit
with supplied code.
* Input:text
Error message
* Output:
* Return:
* Attention:
*************************************************************************
*/

void error(char *text, int code)
{
    fprintf(stderr, "%s\n", text);
    exit(code);
}

int sign(int a , int b)
{
    int x;

    x=abs(a);

    if (b>0)
        return(x);
    else
        return(-x);
}
