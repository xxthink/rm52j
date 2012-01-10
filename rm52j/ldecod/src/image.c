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
* File name: image.c
* Function: Decode a Slice
*
*************************************************************************************
*/


#include "contributors.h"

#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <sys/timeb.h>
#include <string.h>
#include <assert.h>

#include "global.h"
#include "mbuffer.h"
#include "header.h"
#include "annexb.h"
#include "vlc.h"
#include "memalloc.h"


static void rotate_buffer ();
void store_field_MV(struct img_par *img);
void copy2buffer(struct img_par *img, struct inp_par *inp,int bot);
void replace2buffer(struct img_par *img, struct inp_par *inp,int bot);


extern struct img_par *erc_img;

int *last_P_no;
int *last_P_no_frm;
/* 08.16.2007--for user data after pic header */
unsigned char *temp_slice_buf;
int first_slice_length;
int first_slice_startpos;
/* 08.16.2007--for user data after pic header */

/*
*************************************************************************
* Function:decodes one I- or P-frame
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

int decode_one_frame(struct img_par *img,struct inp_par *inp, struct snr_par *snr)
{
    int current_header;

    time_t ltime1;                  // for time measurement
    time_t ltime2;

#ifdef WIN32
    struct _timeb tstruct1;
    struct _timeb tstruct2;
#else
    struct timeb tstruct1;
    struct timeb tstruct2;
#endif

    int tmp_time;                   // time used by decoding the last frame
    int i;

#ifdef WIN32
    _ftime (&tstruct1);             // start time ms
#else
    ftime (&tstruct1);              // start time ms
#endif
    time( &ltime1 );                // start time s

    singlefactor =(float)((Bframe_ctr & 0x01) ? 0.5 : 2);

    if(!img->picture_structure)
        singlefactor = 1;

    img->current_mb_nr = -4711; // initialized to an impossible value for debugging -- correct value is taken from slice header

    second_IField=0;

    //u_v(8, "stuffying bits"); //added by cjw AVS Zhuhai 20070122 //deleted by cjw AVS 20070204
    current_header = Header();


    if (current_header == EOS)
        return EOS;

    //############################################################
    //Carmen, 2007/12/20, Bug Fix: correct picture size for outputted reconstructed pictures
    {
        if (horizontal_size% 16 != 0)
        {
            img->auto_crop_right = 16-(horizontal_size % 16);
        }
        else
        {
            img->auto_crop_right=0;
        }
        //  if (img->picture_structure != FRAME) //20080515
        if (!progressive_sequence) //X ZHENG,M2356,20080515
        {
            if (vertical_size % 32 != 0)
            {
                img->auto_crop_bottom = 32-(vertical_size % 32);
            }
            else
            {
                img->auto_crop_bottom=0;
            }
        }
        else
        {
            if (vertical_size % 16 != 0)
            {
                img->auto_crop_bottom = 16-(vertical_size % 16);
            }
            else
            {
                img->auto_crop_bottom=0;
            }
        }
        // Reinit parameters (NOTE: need to do before init_frame //
        img->width          = (horizontal_size+img->auto_crop_right);
        img->height         = (vertical_size+img->auto_crop_bottom);
        img->width_cr       = (img->width>>1);
        if(chroma_format==2)  // XZHENG,422
            img->height_cr      = (img->height);
        else
            img->height_cr      = (img->height>>1);
        img->PicWidthInMbs  = img->width/MB_BLOCK_SIZE;
        img->PicHeightInMbs = img->height/MB_BLOCK_SIZE;
        img->PicSizeInMbs   = img->PicWidthInMbs * img->PicHeightInMbs;
        img->max_mb_nr      =(img->width * img->height) / (MB_BLOCK_SIZE * MB_BLOCK_SIZE);

    }
    //Carmen, 2007/12/20, Bug Fix: correct picture size for outputted reconstructed pictures
    //############################################################

    img->current_mb_nr = 0;

    init_frame(img, inp);
    if (img->picture_structure)
    {
        img->types = img->type;   // jlzheng 7.15

        if(img->type!=B_IMG){    //HiSilicon, 2007.03.21
            pre_img_type  = img->type;
            pre_img_types = img->types;
        }

        picture_data(img, inp);

        if (img->type != B_IMG&&!progressive_sequence)
        {
            img->height >>= 1;
            img->height_cr >>= 1;
            init_top_buffer();
            split_field_top();
            Update_Picture_top_field();
            init_bot_buffer();
            split_field_bot();
            Update_Picture_bot_field();
            img->height <<= 1;
            img->height_cr <<= 1;
        }
        imgY = imgY_frm;
        imgUV = imgUV_frm;
    }
    else
    {
        img->height         = (vertical_size+img->auto_crop_bottom)/2; //Carmen, 2007/12/24 Fix picture boundary
        if(chroma_format==2)
            img->height_cr      = (img->height);
        else
            img->height_cr      = (img->height>>1);
        img->PicWidthInMbs  = img->width/MB_BLOCK_SIZE;
        img->PicHeightInMbs = img->height/MB_BLOCK_SIZE;
        img->PicSizeInMbs   = img->PicWidthInMbs * img->PicHeightInMbs;
        img->types = img->type;

        if(img->type!=B_IMG){     //HiSilicon, 2007.03.21
            pre_img_type  = img->type;
            pre_img_types = img->types;
        }

        init_top(img,inp);

        top_field(img,inp);
        if(img->type!=B_IMG)
        {
            Update_Picture_top_field();
        }
        else
        {
            for (i=0; i<img->height; i++)
            {
                memcpy(imgY_frm[i*2], imgY_top[i], img->width);     // top field
            }

            for (i=0; i<img->height_cr; i++)
            {
                memcpy(imgUV_frm[0][i*2], imgUV_top[0][i], img->width_cr);
                memcpy(imgUV_frm[1][i*2], imgUV_top[1][i], img->width_cr);
            }
        }

        /* Commented by Xiaozhen Zheng, HiSilicon, 20070327
        if(img->type == I_IMG)
        {
        img->type = P_IMG;
        pre_img_type = P_IMG;    //HiSilicon, 2007.03.21
        second_IField=1;
        }
        */

        init_bot(img,inp);
        bot_field(img,inp);

        if(img->type!=B_IMG)
        {
            Update_Picture_bot_field();
        }
        else
        {
            for (i=0; i<img->height; i++)
            {
                memcpy(imgY_frm[i*2 + 1], imgY_bot[i], img->width); // bottom field
            }

            for (i=0; i<img->height_cr; i++)
            {
                memcpy(imgUV_frm[0][i*2 + 1], imgUV_bot[0][i], img->width_cr);
                memcpy(imgUV_frm[1][i*2 + 1], imgUV_bot[1][i], img->width_cr);
            }
        }
        init_frame_buffer();
        if(img->type!=B_IMG)
            combine_field(img);

        imgY = imgY_frm;
        imgUV = imgUV_frm;
    }

    img->height = (vertical_size+img->auto_crop_bottom);  //Carmen, 2007/12/24, Fix picture boundary
    img->height_cr = /*vertical_size*/ img->height/(chroma_format==2?1:2);    //Carmen, 2007/12/24, Fix picture boundary,422
    img->PicWidthInMbs  = img->width/MB_BLOCK_SIZE;
    img->PicHeightInMbs = img->height/MB_BLOCK_SIZE;
    img->PicSizeInMbs   = img->PicWidthInMbs * img->PicHeightInMbs;

    if(!progressive_sequence)
    {
        store_field_MV(img);
    }

    frame_postprocessing(img);


    /* //commented by Zheng Xiaozhen, HiSilicon, 2007.03.21
    if (p_ref)
    find_snr(snr,img,p_ref);      // if ref sequence exist
    */

    //added by Zheng Xiaozhen, HiSilicon, 2007.03.21
    if (img->type==B_IMG) {
        if (p_ref)
            find_snr(snr,img,p_ref);      // if ref sequence exist
    }



#ifdef WIN32
    _ftime (&tstruct2);   // end time ms
#else
    ftime (&tstruct2);    // end time ms
#endif

    time( &ltime2 );                                // end time sec
    tmp_time=(ltime2*1000+tstruct2.millitm) - (ltime1*1000+tstruct1.millitm);
    tot_time=tot_time + tmp_time;

    //added by Zheng Xiaozhen, HiSilicon, 2007.03.21
    if (img->type!=B_IMG) {
        pre_tmp_time = tmp_time;
        pre_img_tr   = img->tr;
        pre_img_qp   = img->qp;
    }

    /* commented by Zheng Xiaozhe, HiSilicon, 2007.03.21
    if (vec_pr_flag) {
    sprintf(str_vec, "v%d  ", vec_ext);
    } else {
    sprintf(str_vec, "    ");
    }

    if(img->type == I_IMG) // I picture
    fprintf(stdout,"%s%3d(I)  %3d %5d %7.4f %7.4f %7.4f %5d\n",
    str_vec, frame_no, img->tr, img->qp,snr->snr_y,snr->snr_u,snr->snr_v,tmp_time);
    else if(img->type == P_IMG) // P pictures
    {
    //if(frame_no == 0)
    if (img->types == I_IMG)
    fprintf(stdout,"%s%3d(I)  %3d %5d %7.4f %7.4f %7.4f %5d\n",
    str_vec, frame_no, img->tr, img->qp,snr->snr_y,snr->snr_u,snr->snr_v,tmp_time);   //jlzheng 7.3
    else
    fprintf(stdout,"%s%3d(P)  %3d %5d %7.4f %7.4f %7.4f %5d\n",
    str_vec, frame_no, img->tr, img->qp,snr->snr_y,snr->snr_u,snr->snr_v,tmp_time);
    }
    else // B pictures
    fprintf(stdout,"%s%3d(B)  %3d %5d %7.4f %7.4f %7.4f %5d\n",
    str_vec, frame_no, img->tr, img->qp,snr->snr_y,snr->snr_u,snr->snr_v,tmp_time);
    */

    if(img->type==B_IMG)
        fprintf(stdout,"%3d(B)  %3d %5d %7.4f %7.4f %7.4f %5d\n",
        FrameNum, img->tr, img->qp,snr->snr_y,snr->snr_u,snr->snr_v,tmp_time);

    fflush(stdout);

    if(img->type==B_IMG) //added by Xiaozhen Zheng, Hisilicon, 20070327
        report_frame(snr,tmp_time);

    if(img->type == I_IMG || img->type == P_IMG) // I or P pictures
    {
        copy_Pframe(img);  // imgY-->imgY_prev, imgUV-->imgUV_prev
        Update_Picture_Buffers();
    }
    else // B pictures
    {
#ifdef _OUTPUT_DEC_REC
        write_frame(img,p_out);         // write image to output YUV file
#endif
    }
    //! TO 19.11.2001 Known Problem: for init_frame we have to know the picture type of the actual frame
    //! in case the first slice of the P-Frame following the I-Frame was lost we decode this P-Frame but//! do not write it because it was assumed to be an I-Frame in init_frame.So we force the decoder to
    //! guess the right picture type. This is a hack a should be removed by the time there is a clean
    //! solution where we do not have to know the picture type for the function init_frame.
    //! End TO 19.11.2001//Lou

    if(img->type == I_IMG || img->type == P_IMG)   // I or P pictures
        img->number++;
    else
        Bframe_ctr++;    // B pictures

    return (SOP);
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
void report_frame(struct snr_par    *snr,int tmp_time)
{
    FILE *file;
    file = fopen("stat.dat","at");

    if(img->type == I_IMG) // I picture
        fprintf(file,"%3d(I)  %3d %5d %7.4f %7.4f %7.4f %5d\n",
        FrameNum, pre_img_tr, pre_img_qp,snr->snr_y,snr->snr_u,snr->snr_v,pre_tmp_time);
    /* Commented by HiSilicon, 2007.03.21
    fprintf(file,"%3d(I)  %3d %5d %7.4f %7.4f %7.4f %5d\n",
    frame_no, img->tr, img->qp,snr->snr_y,snr->snr_u,snr->snr_v,tmp_time);*/

    else if(img->type == P_IMG) // P pictures
        fprintf(file,"%3d(P)  %3d %5d %7.4f %7.4f %7.4f %5d\n",
        FrameNum, pre_img_tr, pre_img_qp,snr->snr_y,snr->snr_u,snr->snr_v,pre_tmp_time);
    /* Commented by HiSilicon, 2007.03.21
    fprintf(file,"%3d(P)  %3d %5d %7.4f %7.4f %7.4f %5d\n",
    frame_no, img->tr, img->qp,snr->snr_y,snr->snr_u,snr->snr_v,tmp_time);*/

    else // B pictures
        fprintf(file,"%3d(B)  %3d %5d %7.4f %7.4f %7.4f %5d\n",
        FrameNum, img->tr, img->qp,snr->snr_y,snr->snr_u,snr->snr_v,tmp_time);
    /* Commented by HiSilicon, 2007.03.21
    fprintf(file,"%3d(B)  %3d %5d %7.4f %7.4f %7.4f %5d\n",
    frame_no, img->tr, img->qp,snr->snr_y,snr->snr_u,snr->snr_v,tmp_time);
    */

    FrameNum++;  //HiSilicon

    fclose(file);
}

/*
*************************************************************************
* Function:Find PSNR for all three components.Compare decoded frame with
the original sequence. Read inp->jumpd frames to reflect frame skipping.
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

void find_snr(
struct snr_par *snr,   //!< pointer to snr parameters
struct img_par *img,   //!< pointer to image parameters
    FILE *p_ref)           //!< filestream to reference YUV file
{
    int i,j;
    int diff_y,diff_u,diff_v;
    int uv;
    int status;
    int uvformat=chroma_format==2?2:4;

    //Carmen, 2007/12/20, Bug Fix: correct picture size for outputted reconstructed pictures, 20080515
    int img_width = (img->width-img->auto_crop_right);
    int img_height = (img->height-img->auto_crop_bottom);
    int img_width_cr = (img_width/2);
    int img_height_cr = (img_height/(chroma_format==2?1:2));
    //Carmen, 2007/12/20, Bug Fix: correct picture size for outputted reconstructed pictures, 20080515


    snr->snr_y=0.0;
    snr->snr_u=0.0;
    snr->snr_v=0.0;
    /* HiSilicon, 2007.03.21
    if(img->type==I_IMG ) // I, P pictures
    {
    frame_no=img->tr;
    }else if(img->type==P_IMG)
    {
    frame_no=img->tr;
    }else// B pictures
    {
    diff=nextP_tr-img->tr;
    frame_no=(img->number-1)*P_interval-diff;
    }
    */

    rewind(p_ref);
    // 20071224
    if(RefPicExist)
    {
        if(chroma_format==1)
            status = fseek (p_ref, FrameNum*img_height*img_width*3/2, 0);
        else if(chroma_format==2)
            status = fseek (p_ref, FrameNum*img_height*img_width*2, 0);  // 20080515,422

        if (status != 0)
        {
            snprintf(errortext, ET_SIZE, "Error in seeking img->tr: %d", img->tr);
            RefPicExist = 0;
            // error(errortext, 500);
        }
    }
    // 20071224
    /*  for (j=0; j < img->height; j++)
    for (i=0; i < img->width; i++) 20080515*/
    for (j=0; j< img_height; j++)
        for(i=0; i< img_width; i++)
        {
            imgY_ref[j][i]=fgetc(p_ref);
        }

        for (uv=0; uv < 2; uv++)
            /*    for (j=0; j < img->height_cr ; j++)
            for (i=0; i < img->width_cr; i++) 20080515*/
            for (j=0; j < img_height_cr ; j++)
                for (i=0; i < img_width_cr; i++)
                    imgUV_ref[uv][j][i]=fgetc(p_ref);

        img->quad[0]=0;
        diff_y=0;

        for (j=0; j < img_height; ++j)
        {
            for (i=0; i < img_width; ++i)
            {
                if(img->type==B_IMG && !eos)  //HiSilicon
                    diff_y += img->quad[abs(imgY[j][i]-imgY_ref[j][i])];
                else
                    diff_y += img->quad[abs(imgY_prev[j][i]-imgY_ref[j][i])];
                if (diff_y != 0)
                {
                    int mb_nr, b8;
                    mb_nr = (j/16)*(img->width/16)+i/16;
                    b8 = ((j-j/16*16) / 8) * 2 + ((i-i/16*16)/8);
#ifdef _OUTPUT_ERROR_MB_
                    switch(img->type)
                    {
                    case I_IMG:
                        printf("img_type:I_IMG  ");
                        break;
                    case P_IMG:
                        printf("img_type:P_IMG  ");
                        break;
                    case B_IMG:
                        printf("img_type:B_IMG  ");
                        break;
                    }
                    printf("frameno(display order):%4d, luma mbnr:%d, b8:%4d, mode:%4d, b8mode:%4d, pdir:%4d\n", FrameNum, mb_nr, b8, mb_data[mb_nr].mb_type, mb_data[mb_nr].b8mode[b8], mb_data[mb_nr].b8pdir[b8]);
                    printf("ref:%4d, curr:%4d, y:%4d, x:%4d\n", imgY_ref[j][i], imgY_prev[j][i], j, i);
                    exit(0);
#endif
                }
            }
        }

        // Chroma
        diff_u=0;
        diff_v=0;

        for (j=0; j < img_height_cr; ++j)
        {
            for (i=0; i < img_width_cr; ++i)
            {
                if(img->type==B_IMG && !eos)
                {
                    diff_u += img->quad[abs(imgUV_ref[0][j][i]-imgUV[0][j][i])];
                    diff_v += img->quad[abs(imgUV_ref[1][j][i]-imgUV[1][j][i])];
                }
                else
                {
                    diff_u += img->quad[abs(imgUV_ref[0][j][i]-imgUV_prev[0][j][i])];
                    diff_v += img->quad[abs(imgUV_ref[1][j][i]-imgUV_prev[1][j][i])];
                }
#ifdef _OUTPUT_ERROR_MB_
                if (diff_u != 0)
                {
                    int mb_nr;
                    mb_nr = (j/16)*(img->width/16)+i/8;
                    printf("chroma u error, mbnr:%4d\n", mb_nr);
                    exit(0);
                }
                else if (diff_v != 0)
                {
                    int mb_nr;
                    mb_nr = (j/16)*(img->width/16)+i/8;
                    printf("chroma v error, mbnr:%4d\n", mb_nr);
                    exit(0);
                }
#endif
            }
        }

        // Collecting SNR statistics
        if (diff_y != 0)
            snr->snr_y=(float)(10*log10(65025*(float)(img_width/*img->width*/)*(img_height/*img->height*/)/(float)diff_y));        // luma snr for current frame
        if (diff_u != 0)
            snr->snr_u=(float)(10*log10(65025*(float)(img_width/*img->width*/)*(img_height/*img->height*/)/(float)(/*4*/uvformat*diff_u)));    //  chroma snr for current frame,422
        if (diff_v != 0)
            snr->snr_v=(float)(10*log10(65025*(float)(img_width/*img->width*/)*(img_height/*img->height*/)/(float)(/*4*/uvformat*diff_v)));    //  chroma snr for current frame,422

        if (img->number == 0) // first
        {
            snr->snr_y1=(float)(10*log10(65025*(float)(img_width/*img->width*/)*(img_height/*img->height*/)/(float)diff_y));       // keep luma snr for first frame
            snr->snr_u1=(float)(10*log10(65025*(float)(img_width/*img->width*/)*(img_height/*img->height*/)/(float)(/*4*/uvformat*diff_u)));   // keep chroma snr for first frame,422
            snr->snr_v1=(float)(10*log10(65025*(float)(img_width/*img->width*/)*(img_height/*img->height*/)/(float)(/*4*/uvformat*diff_v)));   // keep chroma snr for first frame,422
            snr->snr_ya=snr->snr_y1;
            snr->snr_ua=snr->snr_u1;
            snr->snr_va=snr->snr_v1;

            if (diff_y == 0)
                snr->snr_ya=50; // need to assign a reasonable large number so avg snr of entire sequece isn't infinite
            if (diff_u == 0)
                snr->snr_ua=50;
            if (diff_v == 0)
                snr->snr_va=50;
        }
        else
        {
            snr->snr_ya=(float)(snr->snr_ya*(img->number+Bframe_ctr)+snr->snr_y)/(img->number+Bframe_ctr+1); // average snr chroma for all frames
            snr->snr_ua=(float)(snr->snr_ua*(img->number+Bframe_ctr)+snr->snr_u)/(img->number+Bframe_ctr+1); // average snr luma for all frames
            snr->snr_va=(float)(snr->snr_va*(img->number+Bframe_ctr)+snr->snr_v)/(img->number+Bframe_ctr+1); // average snr luma for all frames
        }
}


/*
*************************************************************************
* Function:Interpolation of 1/4 subpixel
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/
void get_block(int ref_frame,int x_pos, int y_pos, struct img_par *img, int block[8][8], unsigned char **ref_pic)
{

    int dx, dy;
    int x, y;
    int i, j;
    int maxold_x,maxold_y;
    int result;
    int tmp_res[26][26];
    int tmp_res_2[26][26];
    static const int COEF_HALF[4] = {
        -1, 5, 5, -1
    };
    static const int COEF_QUART[4] = {
        1, 7, 7, 1
    };

    //    A  a  1  b  B
    //    c  d  e  f
    //    2  h  3  i
    //    j  k  l  m
    //    C           D

    dx = x_pos&3;
    dy = y_pos&3;
    x_pos = (x_pos-dx)/4;
    y_pos = (y_pos-dy)/4;
    maxold_x = img->width-1;
    maxold_y = img->height-1;


    if (dx == 0 && dy == 0) {  //fullpel position: A
        for (j = 0; j < B8_SIZE; j++)
            for (i = 0; i < B8_SIZE; i++)
                block[i][j] = ref_pic[max(0,min(maxold_y,y_pos+j))][max(0,min(maxold_x,x_pos+i))];
    }
    else
    { /* other positions */
        if((dx==2) && (dy==0)){//horizonal 1/2 position: 1
            for (j = 0; j < B8_SIZE; j++){
                for (i = 0; i < B8_SIZE; i++) {
                    for (result = 0, x = -1; x < 3; x++)
                        result += ref_pic[max(0,min(maxold_y,y_pos+j))][max(0,min(maxold_x,x_pos+i+x))]*COEF_HALF[x+1];
                    block[i][j] = max(0, min(255, (result+4)/8));
                }
            }
        }

        else if(((dx==1) || (dx==3)) && (dy==0)){//horizonal 1/4 position: a and b
            for (j = 0; j < B8_SIZE; j++) {
                for (i = -1; i < B8_SIZE+1; i++) {

                    for (result = 0, x = -1; x < 3; x++)
                        result += ref_pic[max(0,min(maxold_y,y_pos+j))][max(0,min(maxold_x,x_pos+i+x))]*COEF_HALF[x+1];

                    tmp_res[j][2*(i+1)] = result;
                    tmp_res[j][2*(i+1)+1] = ref_pic[max(0,min(maxold_y,y_pos+j))][max(0,min(maxold_x,x_pos+i+1))]*8;
                }
            }
            for (j = 0; j < B8_SIZE; j++) {
                for (i = 0; i < B8_SIZE; i++) {

                    for (result = 0, x = -1; x < 3; x++)
                        if(dx==1)//a
                            result += tmp_res[j][2*i+x+1]*COEF_QUART[x+1];
                        else//b
                            result += tmp_res[j][2*i+x+2]*COEF_QUART[x+1];

                    block[i][j] = max(0, min(255, (result+64)/128));
                }
            }
        }

        else if((dy==2) && (dx==0)){//vertical 1/2 position: 2
            for (j = 0; j < B8_SIZE; j++) {
                for (i = 0; i < B8_SIZE; i++) {
                    for (result = 0, y = -1; y < 3; y++)
                        result += ref_pic[max(0,min(maxold_y,y_pos+j+y))][max(0,min(maxold_x,x_pos+i))]*COEF_HALF[y+1];
                    block[i][j] = max(0, min(255, (result+4)/8));
                }
            }
        }

        else if(((dy==1) || (dy==3)) && (dx==0)){//vertical 1/4 position: c and j
            for (j = -1; j < B8_SIZE+1; j++) {
                for (i = 0; i < B8_SIZE; i++) {

                    for (result = 0, y = -1; y < 3; y++)
                        result += ref_pic[max(0,min(maxold_y,y_pos+j+y))][max(0,min(maxold_x,x_pos+i))]*COEF_HALF[y+1];

                    tmp_res[2*(j+1)][i] = result;
                    tmp_res[2*(j+1)+1][i] = ref_pic[max(0,min(maxold_y,y_pos+j+1))][max(0,min(maxold_x,x_pos+i))]*8;
                }
            }
            for (j = 0; j < B8_SIZE; j++) {
                for (i = 0; i < B8_SIZE; i++) {

                    for (result = 0, y = -1; y < 3; y++)
                        if(dy==1)//c
                            result += tmp_res[2*j+y+1][i]*COEF_QUART[y+1];
                        else//j
                            result += tmp_res[2*j+y+2][i]*COEF_QUART[y+1];

                    block[i][j] = max(0, min(255, (result+64)/128));
                }
            }
        }

        else if((dx==2) && (dy==2)){//horizonal and vertical 1/2 position: 3
            for (j = -1; j < B8_SIZE+2; j++) {
                for (i = 0; i < B8_SIZE; i++) {

                    for (result = 0, x = -1; x < 3; x++)
                        result += ref_pic[max(0,min(maxold_y,y_pos+j))][max(0,min(maxold_x,x_pos+i+x))]*COEF_HALF[x+1];

                    tmp_res[j+1][i] = result;
                }
            }
            for (j = 0; j < B8_SIZE; j++) {
                for (i = 0; i < B8_SIZE; i++) {

                    for (result = 0, y = -1; y < 3; y++)
                        result += tmp_res[j+y+1][i]*COEF_HALF[y+1];

                    block[i][j] = max(0, min(255, (result+32)/64));
                }
            }
        }

        else if(((dx==1) || (dx==3)) && dy==2){//horizonal and vertical 1/4 position: h and i
            for (j = 0; j < B8_SIZE; j++) {
                for (i = -2; i < B8_SIZE+3; i++) {

                    for (result = 0, y = -1; y < 3; y++)
                        result += ref_pic[max(0,min(maxold_y,y_pos+j+y))][max(0,min(maxold_x,x_pos+i))]*COEF_HALF[y+1];

                    tmp_res[j][i+2] = result;
                }
            }
            for (j = 0; j < B8_SIZE; j++) {
                for (i = 0; i < B8_SIZE+2; i++) {

                    for (result = 0, x = -1; x < 3; x++)
                        result += tmp_res[j][i+1+x]*COEF_HALF[x+1];

                    tmp_res_2[j][2*i] = result;
                    tmp_res_2[j][2*i+1] = tmp_res[j][i+2]*8;
                }
            }
            for (j = 0; j < B8_SIZE; j++) {
                for (i = 0; i < B8_SIZE; i++) {

                    for (result = 0, x = -1; x < 3; x++)
                        if(dx==1)//h
                            result += tmp_res_2[j][2*i+x+1]*COEF_QUART[x+1];
                        else
                            result += tmp_res_2[j][2*i+x+2]*COEF_QUART[x+1];

                    block[i][j] = max(0, min(255, (result+512)/1024));
                }
            }
        }

        else if(((dy==1) || (dy==3)) && (dx==2)){//vertical and horizonal 1/4 position: e and l
            for (j = -2; j < B8_SIZE+3; j++) {
                for (i = 0; i < B8_SIZE; i++) {

                    for (result = 0, x = -1; x < 3; x++)
                        result += ref_pic[max(0,min(maxold_y,y_pos+j))][max(0,min(maxold_x,x_pos+i+x))]*COEF_HALF[x+1];

                    tmp_res[j+2][i] =result;
                }
            }
            for (j = 0; j < B8_SIZE+2; j++) {
                for (i = 0; i < B8_SIZE; i++) {

                    for (result = 0, y = -1; y < 3; y++)
                        result += tmp_res[j+y+1][i]*COEF_HALF[y+1];

                    tmp_res_2[2*j][i] = result;
                    tmp_res_2[2*j+1][i] =  tmp_res[j+2][i]*8;
                }
            }
            for (j = 0; j < B8_SIZE; j++) {
                for (i = 0; i < B8_SIZE; i++) {

                    for (result = 0, y = -1; y < 3; y++)
                        if(dy==1)//e
                            result += tmp_res_2[2*j+y+1][i]*COEF_QUART[y+1];
                        else//l
                            result += tmp_res_2[2*j+y+2][i]*COEF_QUART[y+1];

                    block[i][j] = max(0, min(255, (result+512)/1024));
                }
            }
        }

        else{//Diagonal 1/4 position : d, f, k and m
            for (j = -1; j < B8_SIZE+2; j++) {
                for (i = 0; i < B8_SIZE; i++) {

                    for (result = 0, x = -1; x < 3; x++)
                        result += ref_pic[max(0,min(maxold_y,y_pos+j))][max(0,min(maxold_x,x_pos+i+x))]*COEF_HALF[x+1];

                    tmp_res[j+1][i] = result;
                }
            }
            for (j = 0; j < B8_SIZE; j++) {
                for (i = 0; i < B8_SIZE; i++) {

                    for (result = 0, y = -1; y < 3; y++)
                        result += tmp_res[j+y+1][i]*COEF_HALF[y+1];

                    tmp_res_2[j][i] = result;
                }
            }
            for (j = 0; j < B8_SIZE; j++) {
                for (i = 0; i < B8_SIZE; i++) {
                    if((dx==1) && (dy==1))//d
                        result = tmp_res_2[j][i]+ref_pic[max(0,min(maxold_y,y_pos+j))][max(0,min(maxold_x,x_pos+i))]*64;
                    else if((dx==3) && (dy==1))//f
                        result = tmp_res_2[j][i]+ref_pic[max(0,min(maxold_y,y_pos+j))][max(0,min(maxold_x,x_pos+i+1))]*64;
                    else if((dx==1) && (dy==3))//k
                        result = tmp_res_2[j][i]+ref_pic[max(0,min(maxold_y,y_pos+j+1))][max(0,min(maxold_x,x_pos+i))]*64;
                    else if((dx==3) && (dy==3))//m
                        result = tmp_res_2[j][i]+ref_pic[max(0,min(maxold_y,y_pos+j+1))][max(0,min(maxold_x,x_pos+i+1))]*64;
                    block[i][j] = max(0, min(255, (result+64)/128));
                }
            }
        }
    }
}

/*
*************************************************************************
* Function:Reads new slice from bit_stream
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

int Header()
{
    unsigned char *Buf;
    int startcodepos,length;
    static unsigned long prev_pos=0 , curr_pos=0;  // 08.16.2007
    static unsigned long prev_pic=0, curr_pic=0;  // 08.16.2007

    if ((Buf = (char*)calloc (MAX_CODED_FRAME_SIZE , sizeof(char))) == NULL)
        no_mem_exit("GetAnnexbNALU: Buf");

    while (1)
    {
        StartCodePosition = GetOneUnit(Buf,&startcodepos,&length);    //jlzheng  7.5

        switch(Buf[startcodepos])
        {
        case SEQUENCE_HEADER_CODE:
            SequenceHeader(Buf,startcodepos,length);
            img->seq_header_indicate = 1;                // xiaozhen zheng, 20071009
            break;
        case EXTENSION_START_CODE:
            extension_data(Buf,startcodepos,length);
            break;
        case USER_DATA_START_CODE:
            user_data(Buf,startcodepos,length);
            break;
        case VIDEO_EDIT_CODE:
            video_edit_code_data(Buf,startcodepos,length);
            break;
        case I_PICTURE_START_CODE:
            curr_pic=prev_pos;      // 08.16.2007
            bn=curr_pic-prev_pic;    // 08.16.2007
            I_Picture_Header(Buf,startcodepos,length);
            calc_picture_distance(img);
            // xiaozhen zheng, Discard B pictures, 20071009
            if(!img->seq_header_indicate)
            {
                img->B_discard_flag = 1;
                fprintf(stdout,"    I   %3d\t\tDIDSCARD!!\n", img->tr);
                break;
            }
            break; /* modified 08.16.2007 ---for dec user data after pic header
                   free(Buf);
                   return SOP;*/
        case PB_PICTURE_START_CODE:
            curr_pic=prev_pos;    // 08.16.2007
            bn=curr_pic-prev_pic;  // 08.16.2007
            PB_Picture_Header(Buf,startcodepos,length);
            calc_picture_distance(img);
            // xiaozhen zheng, 20071009
            if(!img->seq_header_indicate)
            {
                img->B_discard_flag = 1;
                if(img->type==P_IMG)
                    fprintf(stdout,"    P   %3d\t\tDIDSCARD!!\n", img->tr);
                else
                    fprintf(stdout,"    B   %3d\t\tDIDSCARD!!\n", img->tr);
                break;
            }
            if (img->seq_header_indicate==1 && img->type!=B_IMG) {
                img->B_discard_flag = 0;
            }
            if (img->type==B_IMG && img->B_discard_flag==1 && !img->no_forward_reference) {
                fprintf(stdout,"    B   %3d\t\tDIDSCARD!!\n", img->tr);
                break;
            }
            // xiaozhen zheng, 20071009
            break;    /* modified 08.16.2007 ---for dec user data after pic header
                      free(Buf);
                      return SOP;  */
        case SEQUENCE_END_CODE:
            free(Buf);
            return EOS;
            break;
        default:
            /* // 08.16.2007
            if (Buf[startcodepos]>=SLICE_START_CODE_MIN && Buf[startcodepos]<=SLICE_START_CODE_MAX) {
            break;
            // 08.16.2007 */
            if ((Buf[startcodepos]>=SLICE_START_CODE_MIN && Buf[startcodepos]<=SLICE_START_CODE_MAX)
                && ((!img->seq_header_indicate) || (img->type==B_IMG && img->B_discard_flag==1 && !img->no_forward_reference))) {
                    break;
            }
            else if (Buf[startcodepos]==SLICE_START_CODE_MIN) {
                if ((temp_slice_buf = (char*)calloc (MAX_CODED_FRAME_SIZE , sizeof(char))) == NULL)// modified 08.16.2007
                    no_mem_exit("GetAnnexbNALU: Buf");
                first_slice_length=length;
                first_slice_startpos=startcodepos;
                memcpy(temp_slice_buf,Buf,length);
                free(Buf);
                return SOP;
            } else {
                printf("Can't find start code");
                free(Buf);
                return EOS;
            }
        }
    }

}

/*
*************************************************************************
* Function:Initializes the parameters for a new frame
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/


void init_frame(struct img_par *img, struct inp_par *inp)
{
    static int first_P = TRUE;
    int i,j;


    img->top_bot = -1;
    last_P_no = last_P_no_frm;
    nextP_tr = nextP_tr_frm;

    if (img->number == 0) // first picture
    {
        //    nextP_tr=prevP_tr=img->tr;           // Xiaozhen ZHENG
        nextP_tr=prevP_tr=img->pic_distance; // Xiaozhen ZHENG, 2007.05.01
    }
    else if(img->type == I_IMG || img->type == P_IMG )
    {
        //    nextP_tr=img->tr;             // Commented by Xiaozhen ZHENG, 2007.05.01
        nextP_tr=img->pic_distance;   // Added by Xiaozhen ZHENG, 2007.05.01

        if(first_P) // first P picture
        {
            first_P = FALSE;
            P_interval=nextP_tr-prevP_tr; //! TO 4.11.2001 we get problems here in case the first P-Frame was lost
        }

        //added by  Zheng Xiaozhen, HiSilicon, 2007.03.21 Begin
        if (p_ref)
            find_snr(snr,img,p_ref);      // if ref sequence exist

        if(pre_img_type == I_IMG) // I picture
            fprintf(stdout,"%3d(I)  %3d %5d %7.4f %7.4f %7.4f %5d\n",
            FrameNum, pre_img_tr, pre_img_qp,snr->snr_y,snr->snr_u,snr->snr_v,pre_tmp_time);
        else if(pre_img_type == P_IMG) // P pictures
        {
            if (pre_img_types == I_IMG)
                fprintf(stdout,"%3d(I)  %3d %5d %7.4f %7.4f %7.4f %5d\n",
                FrameNum, pre_img_tr, pre_img_qp,snr->snr_y,snr->snr_u,snr->snr_v,pre_tmp_time);   //jlzheng 7.3
            else
                fprintf(stdout,"%3d(P)  %3d %5d %7.4f %7.4f %7.4f %5d\n",
                FrameNum, pre_img_tr, pre_img_qp,snr->snr_y,snr->snr_u,snr->snr_v,pre_tmp_time);
        }

        report_frame(snr,pre_tmp_time);
        //added by  Zheng Xiaozhen, HiSilicon, 2007.03.21 End

#ifdef _OUTPUT_DEC_REC
        write_prev_Pframe(img, p_out);  // imgY_prev, imgUV_prev -> file
#endif
    }

    // allocate memory for frame buffers
    if (img->number == 0)
    {
        init_global_buffers(inp, img);
    }

    for(i=0;i<img->width/(2*BLOCK_SIZE)+1;i++)          // set edge to -1, indicate nothing to predict from
    {
        img->ipredmode[i+1][0]=-1;
        img->ipredmode[i+1][img->height/(2*BLOCK_SIZE)+1]=-1;
    }

    for(j=0;j<img->height/(2*BLOCK_SIZE)+1;j++)
    {
        img->ipredmode[0][j+1]=-1;
        img->ipredmode[img->width/(2*BLOCK_SIZE)+1][j+1]=-1;
    }

    img->ipredmode    [0][0]=-1;

    for(i=0; i<img->max_mb_nr; i++)
    {
        mb_data[i].slice_nr = -1;
    }

    //luma
    mref[0] = mref_frm[0]; //mref[ref_index][yuv]height(height/2)][width] ref_index=0,1 for P frame, ref_index = 0,1,2,3 for P field
    mref[1] = mref_frm[1];
    mref_fref[0] = mref_fref_frm[0]; //mref_fref[ref_index][yuv]height(height/2)][width] ref_index=0 for B frame, ref_index = 0,1 for B field
    mref_bref[0] = mref_bref_frm[0];

    //chroma
    mcef[0][0] = mcef_frm[0][0];       //mcef[ref_index][uv][height][width]
    mcef[0][1] = mcef_frm[0][1];
    mcef[1][0] = mcef_frm[1][0];
    mcef[1][1] = mcef_frm[1][1];

    mcef_fref[0][0] =   mcef_fref_frm[0][0]; //mcef_fref[ref_index][uv][height/2][width]
    mcef_fref[0][1] =   mcef_fref_frm[0][1];
    mcef_bref[0][0] =   mcef_bref_frm[0][0];
    mcef_bref[0][1] =   mcef_bref_frm[0][1];

    imgY = imgY_frm =  current_frame[0];
    imgUV = imgUV_frm =  &current_frame[1];

    img->mv = img->mv_frm;
    refFrArr = refFrArr_frm;

    img->fw_refFrArr = img->fw_refFrArr_frm;
    img->bw_refFrArr = img->bw_refFrArr_frm;
}


void init_frame_buffer()
{

    //luma
    mref[0] = mref_frm[0]; //mref[ref_index][yuv]height(height/2)][width] ref_index=0,1 for P frame, ref_index = 0,1,2,3 for P field
    mref[1] = mref_frm[1];
    mref_fref[0] = mref_fref_frm[0]; //mref_fref[ref_index][yuv]height(height/2)][width] ref_index=0 for B frame, ref_index = 0,1 for B field
    mref_bref[0] = mref_bref_frm[0];

    //chroma
    mcef[0][0] = mcef_frm[0][0];       //mcef[ref_index][uv][height][width]
    mcef[0][1] = mcef_frm[0][1];
    mcef[1][0] = mcef_frm[1][0];
    mcef[1][1] = mcef_frm[1][1];

    mcef_fref[0][0] =   mcef_fref_frm[0][0]; //mcef_fref[ref_index][uv][height/2][width]
    mcef_fref[0][1] =   mcef_fref_frm[0][1];
    mcef_bref[0][0] =   mcef_bref_frm[0][0];
    mcef_bref[0][1] =   mcef_bref_frm[0][1];

    imgY_frm =  current_frame[0];
    imgUV_frm =  &current_frame[1];
}


void init_top(struct img_par *img, struct inp_par *inp)
{
    int i,j;

    img->top_bot = 0;

    for(i=0;i<img->width/(2*BLOCK_SIZE)+1;i++)          // set edge to -1, indicate nothing to predict from
    {
        img->ipredmode[i+1][0]=-1;
        img->ipredmode[i+1][img->height/(2*BLOCK_SIZE)+1]=-1;
    }

    for(j=0;j<img->height/(2*BLOCK_SIZE)+1;j++)
    {
        img->ipredmode[0][j+1]=-1;
        img->ipredmode[img->width/(2*BLOCK_SIZE)+1][j+1]=-1;
    }

    img->ipredmode    [0][0]=-1;


    for(i=0; i<img->max_mb_nr; i++)
    {
        mb_data[i].slice_nr = -1;
    }

    //luma
    for (i=0;i<4;i++)
    {
        mref[i] = mref_fld[i]; //mref[ref_index][yuv]height(height/2)][width] ref_index=0,1 for P frame, ref_index = 0,1,2,3 for P field
    }
    //chroma
    for(j=0;j<4;j++)
        for(i=0;i<2;i++)
        {
            mcef[j][i] = mcef_fld[j][i];       //mcef[ref_index][uv][height][width]
        }

        //forward/backward
        for (i=0;i<2;i++)
        {
            mref_fref[i] = mref_fref_fld[i]; //mref_fref[ref_index][yuv]height(height/2)][width] ref_index=0 for B frame, ref_index = 0,1 for B field
            mref_bref[i] = mref_bref_fld[i];
        }

        for(j=0;j<2;j++)
            for(i=0;i<2;i++)
            {
                mcef_fref[j][i] =   mcef_fref_fld[j][i]; //mcef_fref[ref_index][uv][height/2][width]
                mcef_bref[j][i] =   mcef_bref_fld[j][i];
            }

            imgY = imgY_top = current_field[0];
            imgUV = imgUV_top =  &current_field[1];

            img->mv = img->mv_top;
            refFrArr = refFrArr_top;

            img->fw_refFrArr = img->fw_refFrArr_top;
            img->bw_refFrArr = img->bw_refFrArr_top;
}


void init_top_buffer()
{
    int i,j;
    //luma
    for (i=0;i<4;i++)
    {
        mref[i] = mref_fld[i]; //mref[ref_index][yuv]height(height/2)][width] ref_index=0,1 for P frame, ref_index = 0,1,2,3 for P field
    }
    //chroma
    for(j=0;j<4;j++)
        for(i=0;i<2;i++)
        {
            mcef[j][i] = mcef_fld[j][i];       //mcef[ref_index][uv][height][width]
        }

        //forward/backward
        for (i=0;i<2;i++)
        {
            mref_fref[i] = mref_fref_fld[i]; //mref_fref[ref_index][yuv]height(height/2)][width] ref_index=0 for B frame, ref_index = 0,1 for B field
            mref_bref[i] = mref_bref_fld[i];
        }

        for(j=0;j<2;j++)
            for(i=0;i<2;i++)
            {
                mcef_fref[j][i] =   mcef_fref_fld[j][i]; //mcef_fref[ref_index][uv][height/2][width]
                mcef_bref[j][i] =   mcef_bref_fld[j][i];
            }

            imgY_top = current_field[0];
            imgUV_top =  &current_field[1];

            img->mv = img->mv_top;
            refFrArr = refFrArr_top;

            img->fw_refFrArr = img->fw_refFrArr_top;
            img->bw_refFrArr = img->bw_refFrArr_top;
}

void init_bot_buffer()
{
    //luma
    int i,j;
    for (i=0;i<4;i++)
    {
        mref[i] = mref_fld[i]; //mref[ref_index][yuv]height(height/2)][width] ref_index=0,1 for P frame, ref_index = 0,1,2,3 for P field
    }
    //chroma
    for(j=0;j<4;j++)
        for(i=0;i<2;i++)
        {
            mcef[j][i] = mcef_fld[j][i];       //mcef[ref_index][uv][height][width]
        }

        //forward/backward
        for (i=0;i<2;i++)
        {
            mref_fref[i] = mref_fref_fld[i]; //mref_fref[ref_index][yuv]height(height/2)][width] ref_index=0 for B frame, ref_index = 0,1 for B field
            mref_bref[i] = mref_bref_fld[i];
        }

        for(j=0;j<2;j++)
            for(i=0;i<2;i++)
            {
                mcef_fref[j][i] =   mcef_fref_fld[j][i]; //mcef_fref[ref_index][uv][height/2][width]
                mcef_bref[j][i] =   mcef_bref_fld[j][i];
            }

            imgY_bot =  current_field[0];
            imgUV_bot = &current_field[1];

            img->mv = img->mv_bot;
            refFrArr = refFrArr_bot;

            img->fw_refFrArr = img->fw_refFrArr_bot;
            img->bw_refFrArr = img->bw_refFrArr_bot;
}

void init_bot(struct img_par *img, struct inp_par *inp)
{
    int i,j;

    img->top_bot = 1;
    img->current_mb_nr = 0;

    for(i=0;i<img->width/(2*BLOCK_SIZE)+1;i++)          // set edge to -1, indicate nothing to predict from
    {
        img->ipredmode[i+1][0]=-1;
        img->ipredmode[i+1][img->height/(2*BLOCK_SIZE)+1]=-1;
    }
    for(j=0;j<img->height/(2*BLOCK_SIZE)+1;j++)
    {
        img->ipredmode[0][j+1]=-1;
        img->ipredmode[img->width/(2*BLOCK_SIZE)+1][j+1]=-1;
    }

    img->ipredmode    [0][0]=-1;

    for(i=0; i<img->max_mb_nr; i++)
    {
        mb_data[i].slice_nr = -1;
    }

    //luma
    for (i=0;i<4;i++)
    {
        mref[i] = mref_fld[i]; //mref[ref_index][yuv]height(height/2)][width] ref_index=0,1 for P frame, ref_index = 0,1,2,3 for P field
    }
    //chroma
    for(j=0;j<4;j++)
        for(i=0;i<2;i++)
        {
            mcef[j][i] = mcef_fld[j][i];       //mcef[ref_index][uv][height][width]
        }

        //forward/backward
        for (i=0;i<2;i++)
        {
            mref_fref[i] = mref_fref_fld[i]; //mref_fref[ref_index][yuv]height(height/2)][width] ref_index=0 for B frame, ref_index = 0,1 for B field
            mref_bref[i] = mref_bref_fld[i];
        }

        for(j=0;j<2;j++)
            for(i=0;i<2;i++)
            {
                mcef_fref[j][i] =   mcef_fref_fld[j][i]; //mcef_fref[ref_index][uv][height/2][width]
                mcef_bref[j][i] =   mcef_bref_fld[j][i];
            }

            imgY = imgY_bot =  current_field[0];
            imgUV = imgUV_bot = &current_field[1];

            img->mv = img->mv_bot;
            refFrArr = refFrArr_bot;

            img->fw_refFrArr = img->fw_refFrArr_bot;
            img->bw_refFrArr = img->bw_refFrArr_bot;

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

void split_field_top()
{
    int i;

    imgY = imgY_top;
    imgUV = imgUV_top;

    for (i=0; i<img->height; i++)
    {
        memcpy(imgY[i], imgY_frm[i*2], img->width);
    }

    for (i=0; i<img->height_cr; i++)
    {
        memcpy(imgUV[0][i], imgUV_frm[0][i*2], img->width_cr);
        memcpy(imgUV[1][i], imgUV_frm[1][i*2], img->width_cr);
    }
}


/*
*************************************************************************
* Function:extract bottom field from a frame
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

void split_field_bot()
{
    int i;

    imgY = imgY_bot;
    imgUV = imgUV_bot;

    for (i=0; i<img->height; i++)
    {
        memcpy(imgY[i], imgY_frm[i*2 + 1], img->width);
    }

    for (i=0; i<img->height_cr; i++)
    {
        memcpy(imgUV[0][i], imgUV_frm[0][i*2 + 1], img->width_cr);
        memcpy(imgUV[1][i], imgUV_frm[1][i*2 + 1], img->width_cr);
    }
}


/*
*************************************************************************
* Function:decodes one picture
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

void picture_data(struct img_par *img,struct inp_par *inp)
{
    unsigned char *Buf;
    int startcodepos,length;
    const int mb_nr = img->current_mb_nr;
    int mb_width = img->width/16;
    int first_slice =1; //08.16.2007 tianf
    Macroblock *currMB = &mb_data[mb_nr];
    img->current_slice_nr = -1;                  // jlzheng 6.30
    currentbitoffset = currStream->frame_bitoffset;   // jlzheng 6.30
    currStream->frame_bitoffset = 0;                  // jlzheng 6.30
    if ((Buf = (char*)calloc (MAX_CODED_FRAME_SIZE , sizeof(char))) == NULL)
        no_mem_exit("GetAnnexbNALU: Buf");   //jlzheng  6.30

    img->cod_counter=-1;

    while (img->current_mb_nr<img->PicSizeInMbs) // loop over macroblocks
    {
        //decode slice header   jlzheng 6.30
        if( img->current_mb_nr%mb_width ==0 )
        {
            if(img->cod_counter<=0)
            {
                /* // 08.16.2007
                if(checkstartcode())
                {
                GetOneUnit(Buf,&startcodepos,&length);
                SliceHeader(Buf,startcodepos,length);
                img->current_slice_nr++;
                img->cod_counter = -1; // Yulj 2004.07.15
                }
                // 08.16.2007 */

                /* 08.16.2007--for user data after pic header */
                if(first_slice)
                {
                    //         if(checkstartcode())
                    //         {
                    SliceHeader(temp_slice_buf,first_slice_startpos,first_slice_length);
                    free(temp_slice_buf);
                    img->current_slice_nr++;
                    img->cod_counter = -1; // Yulj 2004.07.15
                    first_slice=0;
                    //         }
                }
                else /* 08.16.2007--for user data after pic header */
                {
                    if(checkstartcode())
                    {
                        GetOneUnit(Buf,&startcodepos,&length);
                        SliceHeader(Buf,startcodepos,length);
                        img->current_slice_nr++;
                        img->cod_counter = -1; // Yulj 2004.07.15
                    }
                }
            }
        }  //decode slice header

        if(img->current_mb_nr == 0)
            img->current_slice_nr = 0;
        mb_data[img->current_mb_nr].slice_nr = img->current_slice_nr;     // jlzheng 6.30

#if TRACE
        // Here was the slice nr from the mb_data used.  This slice number is only set after
        // the reconstruction of an MB and hence here not yet valid
        fprintf(p_trace,"\n*********** Pic: %i (I/P) MB: %i Slice: %i", FrameNum, img->current_mb_nr, img->current_slice_nr);
        switch(img->type)
        {
        case I_IMG:
            fprintf(p_trace,"I_IMG **********\n");
            break;
        case P_IMG:
            fprintf(p_trace,"P_IMG **********\n");
            break;
        case B_IMG:
            fprintf(p_trace,"B_IMG **********\n");
            break;
        }
#endif

        if (img->type == P_IMG)
        {
            img->type = P_IMG;
        }
        // Initializes the current macroblock
        start_macroblock(img,inp);
        // Get the syntax elements from the NAL
        read_one_macroblock(img,inp);
        // decode one macroblock
        decode_one_macroblock(img,inp);
        img->current_mb_nr++;
    }

    free(Buf);

    DeblockFrame (img, imgY, imgUV);

}


void top_field(struct img_par *img,struct inp_par *inp)
{

    unsigned char *Buf;
    int startcodepos,length;
    const int mb_nr = img->current_mb_nr;
    int mb_width = img->width/16;
    int first_slice=1; /* 08.16.2007--for user data after pic header */
    Macroblock *currMB = &mb_data[mb_nr];
    img->current_slice_nr = -1;     // jlzheng 6.30
    currentbitoffset = currStream->frame_bitoffset;    // jlzheng 6.30
    currStream->frame_bitoffset = 0;   // jlzheng 6.30
    if ((Buf = (char*)calloc (MAX_CODED_FRAME_SIZE , sizeof(char))) == NULL)
        no_mem_exit("GetAnnexbNALU: Buf");   //jlzheng

    img->cod_counter=-1;


    while (img->current_mb_nr<img->PicSizeInMbs) // loop over macroblocks
    {
        //decode slice header   jlzheng 6.30
        if( img->current_mb_nr%mb_width ==0 )
        {
            if(img->cod_counter<=0)
            {
                /* // 08.16.2007
                if(checkstartcode())
                {
                GetOneUnit(Buf,&startcodepos,&length);
                SliceHeader(Buf,startcodepos,length);
                img->current_slice_nr++;
                img->cod_counter = -1; // Yulj 2004.07.15
                }
                // 08.16.2007 */

                /* 08.16.2007--for user data after pic header */
                if(first_slice)
                {
                    if(checkstartcode())
                    {
                        SliceHeader(temp_slice_buf,first_slice_startpos,first_slice_length);
                        free(temp_slice_buf);
                        img->current_slice_nr++;
                        img->cod_counter = -1; // Yulj 2004.07.15
                        first_slice=0;
                    }
                } /* 08.16.2007--for user data after pic header */
                else
                {
                    if(checkstartcode())
                    {
                        GetOneUnit(Buf,&startcodepos,&length);
                        SliceHeader(Buf,startcodepos,length);
                        img->current_slice_nr++;
                        img->cod_counter = -1; // Yulj 2004.07.15
                    }
                }
            }
        }  //decode slice header

        if(img->current_mb_nr == 0)
            img->current_slice_nr = 0;
        mb_data[img->current_mb_nr].slice_nr = img->current_slice_nr;   // jlzheng 6.30

#if TRACE
        // Here was the slice nr from the mb_data used.  This slice number is only set after
        // the reconstruction of an MB and hence here not yet valid

        fprintf(p_trace,"\n*********** Pic: %i (I/P) MB: %i Slice: %i Type %d **********\n", img->tr, img->current_mb_nr, img->current_slice_nr, img->type);
#endif

        // Initializes the current macroblock
        start_macroblock(img,inp);
        img->current_mb_nr_fld = img->current_mb_nr;
        // Get the syntax elements from the NAL
        read_one_macroblock(img,inp);
        // decode one macroblock
        decode_one_macroblock(img,inp);
        img->current_mb_nr++;

    }

    free(Buf);
    DeblockFrame (img, imgY, imgUV);

}


void bot_field(struct img_par *img,struct inp_par *inp)
{

    unsigned char *Buf;
    int startcodepos,length;
    const int mb_nr = img->current_mb_nr;
    int mb_width = img->width/16;
    Macroblock *currMB = &mb_data[mb_nr];
    img->current_slice_nr = -1;    // jlzheng 6.30
    currentbitoffset = currStream->frame_bitoffset;    // jlzheng 6.30
    currStream->frame_bitoffset = 0;   // jlzheng 6.30
    if ((Buf = (char*)calloc (MAX_CODED_FRAME_SIZE , sizeof(char))) == NULL)
        no_mem_exit("GetAnnexbNALU: Buf");   //jlzheng

    img->cod_counter=-1;


    while (img->current_mb_nr<img->PicSizeInMbs) // loop over macroblocks
    {
        //decode slice header   jlzheng 6.30
        if( img->current_mb_nr%mb_width ==0 )
        {
            if(img->cod_counter<=0)
            {
                if(checkstartcode())
                {
                    GetOneUnit(Buf,&startcodepos,&length);
                    SliceHeader(Buf,startcodepos,length);
                    img->current_slice_nr++;
                    img->cod_counter = -1; // Yulj 2004.07.15
                }
            }
        }  //decode slice header

        if(img->current_mb_nr == 0)
            img->current_slice_nr = 0;
        mb_data[img->current_mb_nr].slice_nr = img->current_slice_nr;   // jlzheng 6.30

#if TRACE
        // Here was the slice nr from the mb_data used.  This slice number is only set after
        // the reconstruction of an MB and hence here not yet valid

        fprintf(p_trace,"\n*********** Pic: %i (I/P) MB: %i Slice: %i Type %d **********\n", img->tr, img->current_mb_nr, img->current_slice_nr, img->type);
#endif

        // Initializes the current macroblock
        start_macroblock(img,inp);
        img->current_mb_nr_fld = img->current_mb_nr + img->PicSizeInMbs;
        // Get the syntax elements from the NAL
        read_one_macroblock(img,inp);
        // decode one macroblock
        decode_one_macroblock(img,inp);
        img->current_mb_nr++;
    }

    free(Buf);
    DeblockFrame (img, imgY, imgUV);

}

/*!
************************************************************************
* \brief
*    Generate a frame from top and bottom fields
************************************************************************
*/
void combine_field(struct img_par *img)
{
    int i;

    for (i=0; i<img->height; i++)
    {
        memcpy(imgY_frm[i*2], imgY_top[i], img->width);     // top field
        memcpy(imgY_frm[i*2 + 1], imgY_bot[i], img->width); // bottom field
    }

    for (i=0; i<img->height_cr; i++)
    {
        memcpy(imgUV_frm[0][i*2], imgUV_top[0][i], img->width_cr);
        memcpy(imgUV_frm[0][i*2 + 1], imgUV_bot[0][i], img->width_cr);
        memcpy(imgUV_frm[1][i*2], imgUV_top[1][i], img->width_cr);
        memcpy(imgUV_frm[1][i*2 + 1], imgUV_bot[1][i], img->width_cr);
    }
}


/*
*************************************************************************
* Function:Prepare field and frame buffer after frame decoding
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

void frame_postprocessing(struct img_par *img)
{
    if((img->number)&&(img->type==I_IMG || img->type == P_IMG))
    {
        nextP_tr_frm = nextP_tr;
    }

    //pic dist by Grandview Semi. @ [06-07-20 15:25]
    if (img->type==I_IMG || img->type == P_IMG) {
        img->PrevPicDistanceLsb = img->pic_distance;
        img->PicDistanceMsb = img->CurrPicDistanceMsb;
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
static void rotate_buffer ()
{
    Frame *f;

    f = fb->picbuf_short[1];
    fb->picbuf_short[1] = fb->picbuf_short[0];
    fb->picbuf_short[0] = f;

    mref[0] = fb->picbuf_short[0]->mref;
    mcef[0] = fb->picbuf_short[0]->mcef;
    mref[1] = fb->picbuf_short[1]->mref;
    mcef[1] = fb->picbuf_short[1]->mcef;
}


/*!
************************************************************************
* \brief
*    Store information for use in B picture
************************************************************************
*/
void store_field_MV(struct img_par *img)
{
    int i, j;

    if(img->type!=B_IMG)
    {
        if (img->picture_structure != FRAME)
        {
            for (i=0 ; i<img->width/8 ; i++)
            {
                for (j=0 ; j<img->height/16 ; j++)
                {
                    img->mv_frm[i+4][2*j][0] = img->mv_frm[i+4][2*j+1][0] = img->mv_top[i+4][j][0];
                    img->mv_frm[i+4][2*j][0] = img->mv_frm[i+4][2*j+1][0] = img->mv_top[i+4][j][0];
                    img->mv_frm[i+4][2*j][1] = img->mv_frm[i+4][2*j+1][1] = img->mv_top[i+4][j][1]*2;
                    img->mv_frm[i+4][2*j][1] = img->mv_frm[i+4][2*j+1][1] = img->mv_top[i+4][j][1]*2;


                    if (refFrArr_top[j][i] == -1)
                    {
                        refFrArr_frm[2*j][i] = refFrArr_frm[2*j+1][i] = -1;
                    }
                    else
                    {
                        refFrArr_frm[2*j][i] = refFrArr_frm[2*j+1][i] = (int)(refFrArr_top[j][i]/2);
                    }
                }
            }
        }
        else
        {
            for (i=0 ; i<img->width/8; i++)
            {
                for (j=0 ; j<img->height/16 ; j++)
                {
                    img->mv_top[i+4][j][0] = img->mv_bot[i+4][j][0] = (int)(img->mv_frm[i+4][2*j][0]);
                    img->mv_top[i+4][j][1] = img->mv_bot[i+4][j][1] = (int)((img->mv_frm[i+4][2*j][1])/2);

                    if (refFrArr_frm[2*j][i] == -1)
                    {
                        refFrArr_top[j][i] = refFrArr_bot[j][i] = -1;
                    }
                    else
                    {
                        // refFrArr_top[j][i] = refFrArr_bot[j][i] = refFrArr_frm[2*j][i]*2;
                        refFrArr_top[j][i] = refFrArr_frm[2*j][i]*2;
                        refFrArr_bot[j][i] = refFrArr_frm[2*j][i]*2 + 1;
                        //by oliver 0512
                    }
                }
            }
        }
    }
}
