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

#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/timeb.h>
#include <string.h>
#include <memory.h>
#include <assert.h>

#include "global.h"
#include "image.h"
#include "refbuf.h"
#include "mbuffer.h"
#include "header.h"
#include "memalloc.h"
#include "bitstream.h"
#include "ratectl.h"
#include "vlc.h"

#define TO_SAVE 4711
#define FROM_SAVE 4712
#define Clip(min,max,val) (((val)<(min))?(min):(((val)>(max))?(max):(val)))

static void code_a_picture(Picture *frame);
static void ReadOneFrame (int FrameNoInFile, int HeaderSize, int xs, int ys);
static void write_reconstructed_image();
static int  writeout_picture();
static int  writeout_slice();
static void find_snr();
static void frame_mode_buffer (int bit_frame, float snr_frame_y, float snr_frame_u, float snr_frame_v);
static void init_frame();
void init_field ();

void top_field(Picture *pic);
void bot_field(Picture *pic);

void combine_field();
int terminate_picture();


void put_buffer_frame();
void put_buffer_top();
void put_buffer_bot();
void interpolate_frame_to_fb();

static void CopyFrameToOldImgOrgVariables ();
static void UnifiedOneForthPix (pel_t ** imgY, pel_t ** imgU, pel_t ** imgV,
                                pel_t ** out4Y);
static void ReportFirstframe(int tmp_time);
static void ReportIntra(int tmp_time);
static void ReportP(int tmp_time);
static void ReportB(int tmp_time);

static int CalculateFrameNumber();  // Calculates the next frame number
static int FrameNumberInFile;       // The current frame number in the input file

// !! weighting prediction
// The two fuctions below can be rewritten by other algorithms
// Determine the parameters of weighting prediction
void estimate_weighting_factor();
//cjw 20051219 weighting prediction  for field 
void estimate_weighting_factor_field();

extern void DecideMvRange();  // mv_range, 20071009

#define  IClip( Min, Max, Val) (((Val)<(Min))? (Min):(((Val)>(Max))? (Max):(Val)))

extern StatParameters  stats;
//Rate control
int    QP;

/*
*************************************************************************
* Function:
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void stuffing_byte(int n)
{
    int i;
    Bitstream *currStream;

    currStream = currBitStream;

    for(i=0; i<n; i++)
    {
        currStream->streamBuffer[currStream->byte_pos++] = 0x80;
        currStream->bits_to_go = 8;
        currStream->byte_buf = 0;
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

static void picture_header()
{
    int len=0;

    Bitstream *bitstream = currBitStream;
    img->cod_counter = 0;

    if(img->type==INTRA_IMG)
    { 
        len =len+IPictureHeader(img->number);
        if((input->InterlaceCodingOption == FRAME_CODING)||(input->InterlaceCodingOption == FIELD_CODING))   //add by wuzhongmou
            img->count=img->count+2;        //add by wuzhongmou
        if((input->InterlaceCodingOption == PAFF_CODING))
            img->count=img->count+1;       //add by wuzhongmou

    }
    else
        len = len+PBPictureHeader();

    // Rate control
    img->NumberofHeaderBits +=len;
    if(img->BasicUnit<img->Frame_Total_Number_MB)
        img->NumberofBasicUnitHeaderBits +=len;


    // Update statistics
    stats.bit_slice += len;
    stats.bit_use_header[img->type] += len;

    img->ptype =img->type;
    WriteFrameFieldMBInHeader = 0;
}

/*
*************************************************************************
* Function:Encodes one frame
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int encode_one_frame ()
{
    static int prev_frame_no = 0; // POC200301
    static int consecutive_non_reference_pictures = 0; // POC200301
    int    i;

    time_t ltime1;
    time_t ltime2;

#ifdef WIN32
    struct _timeb tstruct1;
    struct _timeb tstruct2;
#else
    struct timeb tstruct1;
    struct timeb tstruct2;
#endif

    int tmp_time;
    int bits_frm = 0, bits_fld = 0;
    float dis_frm = 0, dis_frm_y = 0, dis_frm_u = 0, dis_frm_v = 0;
    float dis_fld = 0, dis_fld_y = 0, dis_fld_u = 0, dis_fld_v = 0;
    int framesize;

    //Rate control
    int pic_type, bits = 0; 

    //cjw 
    second_IField=0;


#ifdef WIN32
    _ftime (&tstruct1);           // start time ms
#else
    ftime (&tstruct1);
#endif
    time (&ltime1);               // start time s

    init_frame ();           // initial frame variables

    FrameNumberInFile = CalculateFrameNumber();

    ReadOneFrame (FrameNumberInFile, input->infile_header, input->img_width, input->img_height);  //modify by wuzhongmou 0610

    CopyFrameToOldImgOrgVariables();

    //    DecideMvRange();  // mv_range, 20071009  // 20071224

    current_slice_bytepos=0;  //qhg 20060327 for de-emulation

    img->types = img->type;

    //Rate control
    img->FieldControl=0; 
    if(input->RCEnable)
    { 
        /*update the number of MBs in the basic unit for MB adaptive 
        f/f coding*/
        img->BasicUnit=input->basicunit;

        rc_init_pict(1,0,1); 
        img->qp  = updateQuantizationParameter(0); 

        pic_type = img->type;
        QP =0;
    }


    if (input->InterlaceCodingOption != FIELD_CODING)  // !! frame coding
    {

        put_buffer_frame ();     //initialize frame buffer

        //frame picture
        if(input->progressive_frame)  //add by wuzhongmou 0610
        {
            img->progressive_frame = 1;
            img->picture_structure = 1;
        }else
        {
            img->progressive_frame = 0;
            img->picture_structure = 1;
        }                 //add by wuzhongmou 0610

        DecideMvRange();  // 20071224

        if (img->type == B_IMG)
            Bframe_ctr++;         // Bframe_ctr only used for statistics, should go to stats.

        singlefactor =(float) ((Bframe_ctr & 0x01) ? 2 : 0.5);
        // !!  [1/5/2004]
        // !!  calculate the weighting parameter
        //cjw 20051219
        img->LumVarFlag = 0 ;  // !! default : no weighting prediction
        img->mb_weighting_flag = 1 ; 
        //cjw 20051219
        if(img->type != INTRA_IMG && input->slice_weighting_flag == 1){
            estimate_weighting_factor();
        }
        // !!  [1/5/2004]
        code_a_picture (frame_pic);

        if (img->type!=B_IMG)
            Update_Picture_Buffers();

        stats.bit_ctr_emulationprevention += stats.em_prev_bits_frm;

        frame_mode_buffer (bits_frm, dis_frm_y, dis_frm_u, dis_frm_v);
    }

    if (input->InterlaceCodingOption != FRAME_CODING)  // !! field coding
    {
        int i;

        //Rate control
        int old_pic_type;              // picture type of top field used for rate control    
        int TopFieldBits;

        //Rate control
        old_pic_type = img->type;
        img->FieldControl=1;

        if(input->InterlaceCodingOption==FIELD_CODING)
            img->FieldFrame = 0;//  [5/8/2007 Leeswan]

        if(input->RCEnable)
        {
            img->BasicUnit=input->basicunit;

            if(input->InterlaceCodingOption==1)
                rc_init_pict(0,1,1); 
            else
                rc_init_pict(0,1,0);

            img->qp  = updateQuantizationParameter(1); 
        }

        img->TopFieldFlag=1;//  [5/8/2007 Leeswan]


        singlefactor = 1;
        //non frame picture
        if(input->progressive_frame)  //add by wuzhongmou 0610
        {
            error ("invalid match of Progressive_frame and InterlaceCodingOption ",100);
        }else
        {
            img->progressive_frame = 0;
            img->picture_structure = 0;
        }   

        DecideMvRange();  // 20071224

        //resize picture size, reference number for field
        img->height >>= 1;
        img->height_cr >>= 1;

        img->nb_references = img->nb_references*2; // coded available reference number

        if (input->InterlaceCodingOption == FIELD_CODING)
        {
            AllocateBitstream();   //start bit stream for field coding
            if (img->type == B_IMG)
                Bframe_ctr++;         // Bframe_ctr only used for statistics, should go to stats.
        }
        else
        {
            // save coded bitstream from frame coding
            pic_buf = (byte *) calloc(img->width*img->height*4,1);
            memcpy(pic_buf,currBitStream->streamBuffer,currBitStream->byte_pos);
            framesize = currBitStream->byte_pos;
            memset(currBitStream,0,sizeof(currBitStream));
            currBitStream->bits_to_go = 8;
        }

        //coded picture header
        picture_header();
        img->buf_cycle <<=1;   // restrict maximum reference numuber
        img->old_type = img->type;
        //initialize buffer
        put_buffer_top();

        // !! start [12/28/2005] shenyanfei cjw
        //cjw 20051219
        img->LumVarFlag = 0 ;  // !! default : no weighting prediction
        img->mb_weighting_flag = 1 ; 
        //cjw 20051219
        if(img->type != INTRA_IMG && input->slice_weighting_flag == 1){
            estimate_weighting_factor_field();
        }
        // !! end [12/28/2005] shenyanfei cjw


        init_field();
        top_field(top_pic);

        //Rate control
        TopFieldBits=top_pic->bits_per_picture;

        if (img->type != B_IMG)
        {
            Update_Picture_Buffers_top_field();
            //all I- and P-frames
            interpolate_frame_to_fb ();

        }
        else
        {  // !! B frame shenyanfei 
            //current_field = ref_fld[4];
            for (i=0; i<img->height; i++)
            {
                memcpy(imgY_com[i*2], imgY_top[i], img->width);     // top field
            }

            for (i=0; i<img->height_cr; i++)
            {
                memcpy(imgUV_com[0][i*2], imgUV_top[0][i], img->width_cr);
                memcpy(imgUV_com[1][i*2], imgUV_top[1][i], img->width_cr);
            }
        }

        if(img->type==INTRA_IMG){
            img->type = INTER_IMG;           
            img->buf_cycle /= 2;       // cjw 20051230 I bot field reference number 
            second_IField=1;
        }

        img->nb_references++;

        //Rate control
        if(input->RCEnable)  
            setbitscount(TopFieldBits);
        if(input->RCEnable)
        {
            rc_init_pict(0,0,0); 
            img->qp  = updateQuantizationParameter(0); 
        }

        img->TopFieldFlag=0;   //  [5/8/2007 Leeswan]

        //initialize buffer
        put_buffer_bot();

        // !! start [12/28/2005] shenyanfei cjw
        //cjw 20051219
        img->LumVarFlag = 0 ;  // !! default : no weighting prediction
        img->mb_weighting_flag = 1 ; 
        //cjw 20051219
        if(img->type != INTRA_IMG && input->slice_weighting_flag == 1){
            estimate_weighting_factor_field();
        }
        // !! end [12/28/2005] shenyanfei cjw

        init_field();
        bot_field(bot_pic);
        terminate_picture();          //delete by jlzheng  6.30

        if (img->type != B_IMG)       //all I- and P-frames
        {
            Update_Picture_Buffers_bot_field();
            interpolate_frame_to_fb ();
        }else  // !! B frame shenyanfei 
        {
            for (i=0; i<img->height; i++)
            {
                memcpy(imgY_com[i*2 + 1], imgY_bot[i], img->width); // bottom field
            }

            for (i=0; i<img->height_cr; i++)
            {
                memcpy(imgUV_com[0][i*2 + 1], imgUV_bot[0][i], img->width_cr);
                memcpy(imgUV_com[1][i*2 + 1], imgUV_bot[1][i], img->width_cr);
            }
        }

        if(img->type!=B_IMG)
            combine_field();

        imgY = imgY_com;
        imgUV = imgUV_com;
        imgY_org  = imgY_org_frm;
        imgUV_org = imgUV_org_frm; 

        img->height <<= 1;
        img->height_cr <<= 1;

        if (second_IField!=1)
        {
            img->buf_cycle >>= 1;
        }

        img->nb_references = (img->nb_references-1)/2;

        if (input->InterlaceCodingOption != FIELD_CODING)
        {
            find_distortion (snr, img);   // find snr from original frame picture

            bot_pic->distortion_y = snr->snr_y;
            bot_pic->distortion_u = snr->snr_u;
            bot_pic->distortion_v = snr->snr_v;
        }
        // restore reference number and image size
    }

    if(input->InterlaceCodingOption == PAFF_CODING)  // !! picture adaptive frame  field coding
    {
        if (!picture_structure_decision(frame_pic,top_pic,bot_pic))    
        {      
            //split frame to field
            img->height >>= 1;
            img->height_cr >>= 1;
            if (img->type != B_IMG)
            {
                split_field_top();
                split_field_bot();
            }
            img->height <<= 1 ;
            img->height_cr <<= 1;

            //restore buffer
            img->picture_structure = 1;
            currBitStream->byte_pos = framesize;
            memcpy(currBitStream->streamBuffer, pic_buf,framesize);

            writeout_picture ();

            imgY = imgY_frm;
            imgUV = imgUV_frm;
        }else
        {//field
            //update refernce frame buffer

            if (img->type != B_IMG)
            {
                //integer pixel buffer
                for (i=0; i<img->height; i++)
                {
                    memcpy(imgY_frm[i],imgY_com[i], img->width);     // top field
                }

                for (i=0; i<img->height_cr; i++)
                {
                    memcpy(imgUV_frm[0][i], imgUV_com[0][i], img->width_cr);
                    memcpy(imgUV_frm[1][i], imgUV_com[1][i], img->width_cr);
                }
                //update 1/4 pixel reference buffer
                imgY = imgY_frm;
                imgUV = imgUV_frm;
                mref[0] = mref_frm[0];
                interpolate_frame_to_fb();
            }

            writeout_picture ();
        }

        //Rate control
        if (!(img->type==B_IMG))//  [5/11/2007 Leeswan]
        {
            if(img->picture_structure==0)
                img->FieldFrame=0;  //  [5/8/2007 Leeswan]
            /*the current choice is field coding*/
            else
                img->FieldFrame=1;   //  [5/8/2007 Leeswan]
        }
    }
    else
    {   

        writeout_picture ();
    }

    if (input->InterlaceCodingOption != FRAME_CODING)
    {
        store_field_MV (IMG_NUMBER);      // assume that img->number = frame_number
    }

    FreeBitstream();  
    find_snr ();
    free(pic_buf);

    time (&ltime2);               // end time sec
#ifdef WIN32
    _ftime (&tstruct2);           // end time ms
#else
    ftime (&tstruct2);            // end time ms
#endif

    tmp_time = (ltime2 * 1000 + tstruct2.millitm) - (ltime1 * 1000 + tstruct1.millitm);
    tot_time = tot_time + tmp_time;



    // Write reconstructed images
    write_reconstructed_image ();

    //Rate control
    if(input->RCEnable)
    {
        bits = stats.bit_ctr-stats.bit_ctr_n;//CABAC*/
        rc_update_pict_frame(bits);
    }


    if (IMG_NUMBER == 0)
        ReportFirstframe(tmp_time);
    else
    {
        //Rate control
        if(input->RCEnable)
        {
            if(input->InterlaceCodingOption==0)
                bits=stats.bit_ctr-stats.bit_ctr_n;
            else
            {
                bits = stats.bit_ctr -Pprev_bits; // used for rate control update */
                Pprev_bits = stats.bit_ctr;
            }
        }

        switch (img->type)
        {
        case INTRA_IMG:
            stats.bit_ctr_P += stats.bit_ctr - stats.bit_ctr_n;
            ReportIntra(tmp_time);
            break;
        case B_IMG:
            stats.bit_ctr_B += stats.bit_ctr - stats.bit_ctr_n;
            ReportB(tmp_time);
            break;
        default:      // P, P_MULTPRED?
            stats.bit_ctr_P += stats.bit_ctr - stats.bit_ctr_n;
            ReportP(tmp_time);
        }
    }

    stats.bit_ctr_n = stats.bit_ctr;
    //Rate control
    if(input->RCEnable) 
    {
        rc_update_pict(bits);
        /*update the parameters of quadratic R-D model*/
        if((img->type==INTER_IMG)&&(input->InterlaceCodingOption==0))
            updateRCModel();
        else if((img->type==INTER_IMG)&&(input->InterlaceCodingOption!=0)\
            &&(img->IFLAG==0))
            updateRCModel();
    }
    total_enc_frame[img->type]++;
    if (IMG_NUMBER == 0)
        return 0;
    else
        return 1;
}

/*
*************************************************************************
* Function:This function write out a picture
* Input:
* Output:
* Return: 0 if OK,                                                         \n
1 in case of error
* Attention:
*************************************************************************
*/

static int writeout_picture()
{

    assert (currBitStream->bits_to_go == 8);    //! should always be the case, the                                              //! byte alignment is done in terminate_slice
    WriteBitstreamtoFile();

    return 0;   
}

/*
*************************************************************************
* Function:Encodes a frame picture
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

static void code_a_picture (Picture *frame)
{
    stats.em_prev_bits_frm = 0;
    AllocateBitstream();

    picture_header();
    picture_data(frame);

    frame->bits_per_picture = 8 * (currBitStream->byte_pos);  
    if (input->InterlaceCodingOption != FRAME_CODING)
    {
        find_distortion (snr, img);
        frame->distortion_y = snr->snr_y;
        frame->distortion_u = snr->snr_u;
        frame->distortion_v = snr->snr_v;
    }
}

/*
*************************************************************************
* Function:Frame Mode Buffer
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

static void frame_mode_buffer (int bit_frame, float snr_frame_y, float snr_frame_u, float snr_frame_v)
{   
    if (img->type != B_IMG)       //all I- and P-frames
        interpolate_frame_to_fb ();
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

static void init_frame ()
{
    int i, j, k;
    int prevP_no, nextP_no;

    img->top_bot = -1;    //Yulj 2004.07.20
    img->current_mb_nr = 0;
    img->current_slice_nr = 0;

    //---Yulj 2004.07.15
    {
        int widthMB, heightMB;
        widthMB = img->width  / MB_BLOCK_SIZE;
        heightMB = img->height / MB_BLOCK_SIZE;
        img->mb_no_currSliceLastMB = ( input->slice_row_nr != 0 )
            ? min(input->slice_row_nr * widthMB - 1, widthMB * heightMB - 1)
            : widthMB * heightMB - 1 ;
    }
    //---end.

    stats.bit_slice = 0;
    img->coded_mb_nr = 0;

    img->mb_y = img->mb_x = 0;
    img->block_y = img->pix_y = img->pix_c_y = 0; 
    img->block_x = img->pix_x = img->block_c_x = img->pix_c_x = 0;

    refFrArr    = refFrArr_frm;
    fw_refFrArr = fw_refFrArr_frm;
    bw_refFrArr = bw_refFrArr_frm;

    if (img->type != B_IMG)
    {

        img->tr = IMG_NUMBER * (input->jumpd + 1);
        img->imgtr_last_prev_P_frm = img->imgtr_last_P_frm;//Lou 1016

        // Added by XiaoZhen Zheng, HiSilicon, 20070405
        if (img->type==INTRA_IMG && img->Seqheader_flag==1) {
            img->last_picture_distance = img->curr_picture_distance;
            img->curr_picture_distance = (IMG_NUMBER * (input->jumpd + 1))%256;
        }
        //  picture_distance = (IMG_NUMBER * (input->jumpd + 1))%256-img->curr_picture_distance;  //Hisilicon XiaoZhen Zheng 20070405   // 20071009
        //    picture_distance = (IMG_NUMBER * (input->jumpd + 1))%256;  //Hisilicon XiaoZhen Zheng 20070327

        picture_distance = (IMG_NUMBER * (input->jumpd + 1))%256;   // xz zheng, 20071009


        img->imgtr_last_P_frm = img->imgtr_next_P_frm;
        img->imgtr_next_P_frm = picture_distance;  // Tsinghua 200701

        if (IMG_NUMBER != 0 && input->successive_Bframe != 0)     // B pictures to encode
            nextP_tr_frm = picture_distance;                      // Added by Xiaozhen ZHENG, 2007.05.01
        //      nextP_tr_frm = img->tr;                               // Commented by Xiaozhen ZHENG, 2007.05.01

        //Rate control
        if(!input->RCEnable)
        {
            if (img->type == INTRA_IMG)
                img->qp = input->qp0;   // set quant. parameter for I-frame
            else
            {
                img->qp = input->qpN;
            }
        }
    }
    else
    {
        img->p_interval = input->jumpd + 1;
        prevP_no = (IMG_NUMBER - 1) * img->p_interval;
        nextP_no = (IMG_NUMBER) * img->p_interval;

        img->b_interval =
            (int) ((float) (input->jumpd + 1) / (input->successive_Bframe + 1.0) + 0.49999);

        img->tr = prevP_no + img->b_interval * img->b_frame_to_code;      // from prev_P

        //  picture_distance = ((IMG_NUMBER - 1) * (input->jumpd + 1) + img->b_interval * img->b_frame_to_code)%256;  //Hisilicon XiaoZhen Zheng 20070327

        /* // xz zheng, 20071009
        if(img->Seqheader_flag==1)
        picture_distance = ((IMG_NUMBER - 1) * (input->jumpd + 1) + img->b_interval * img->b_frame_to_code)%256-img->last_picture_distance;  //Hisilicon XiaoZhen Zheng 20070405
        else
        picture_distance = ((IMG_NUMBER - 1) * (input->jumpd + 1) + img->b_interval * img->b_frame_to_code)%256-img->curr_picture_distance;  //Hisilicon XiaoZhen Zheng 20070405
        // xz zheng, 20071009 */

        //  picture_distance = ((IMG_NUMBER - 1) * (input->jumpd + 1) + img->b_interval * img->b_frame_to_code)%256-img->last_picture_distance;  //Hisilicon XiaoZhen Zheng 20070405

        picture_distance = ((IMG_NUMBER - 1) * (input->jumpd + 1) + img->b_interval * img->b_frame_to_code)%256; // xz zheng, 20071009

        if (img->tr >= nextP_no)
            img->tr = nextP_no - 1;

        //Rate control
        if(!input->RCEnable)
            img->qp = input->qpB;

        // initialize arrays

        if(!img->picture_structure) //field coding
        {
            for (k = 0; k < 2; k++)
                for (i = 0; i < img->height / BLOCK_SIZE; i++)
                    for (j = 0; j < img->width / BLOCK_SIZE + 4; j++)
                    {
                        tmp_fwMV[k][i][j] = 0;
                        tmp_bwMV[k][i][j] = 0;
                        dfMV[k][i][j] = 0;
                        dbMV[k][i][j] = 0;
                    }

                    for (i = 0; i < img->height / B8_SIZE; i++)
                        for (j = 0; j < img->width / BLOCK_SIZE; j++)
                        {
                            fw_refFrArr[i][j] = bw_refFrArr[i][j] = -1;
                        }
        }else
        {
            for (k = 0; k < 2; k++)
                for (i = 0; i < img->height / BLOCK_SIZE; i++)
                    for (j = 0; j < img->width / BLOCK_SIZE + 4; j++)
                    {
                        tmp_fwMV[k][i][j] = 0;
                        tmp_bwMV[k][i][j] = 0;
                        dfMV[k][i][j] = 0;
                        dbMV[k][i][j] = 0;
                    }

                    for (i = 0; i < img->height / BLOCK_SIZE; i++)
                        for (j = 0; j < img->width / BLOCK_SIZE; j++)
                        {
                            fw_refFrArr[i][j] = bw_refFrArr[i][j] = -1;
                        }
        }    
    }

    img->total_number_mb = (img->width * img->height) / (MB_BLOCK_SIZE * MB_BLOCK_SIZE); 


}


void init_field ()
{

    img->current_mb_nr = 0;
    img->current_slice_nr = 0;
    stats.bit_slice = 0;
    img->coded_mb_nr = 0;

    img->mb_y = img->mb_x = 0;
    img->block_y = img->pix_y = img->pix_c_y = 0; 
    img->block_x = img->pix_x = img->block_c_x = img->pix_c_x = 0;

    //---Yulj 2004.07.15
    {
        int widthMB, heightMB;
        widthMB = img->width  / MB_BLOCK_SIZE;
        heightMB = img->height / MB_BLOCK_SIZE;
        img->mb_no_currSliceLastMB = ( input->slice_row_nr != 0 )
            ? min(input->slice_row_nr * widthMB - 1, widthMB * heightMB - 1)
            : widthMB * heightMB - 1 ;
    }
    //---end.

    img->total_number_mb = (img->width * img->height) / (MB_BLOCK_SIZE * MB_BLOCK_SIZE);  
}

/*
*************************************************************************
* Function:Writes reconstructed image(s) to file
This can be done more elegant!
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

static void write_reconstructed_image ()
{
    int i, j, k;
    int start = 0, inc = 1;
    //==== Bug Fix ====//
    //Carmen, 2007/12/20, Bug Fix: correct picture size for outputted reconstructed pictures
#ifdef _BUGFIX_NON16MUL_PICSIZE
    int writeout_width=(img->width-img->auto_crop_right); 
    int writeout_height=(img->height-img->auto_crop_bottom);  
    int writeout_width_cr =(img->width-img->auto_crop_right)/2;  //X ZHENG,422
    int writeout_height_cr=(img->height-img->auto_crop_bottom)/(input->chroma_format==2?1:2); //X ZHENG,422
#else
    int writeout_width=(img->width); 
    int writeout_height=(img->height);  
    int writeout_width_cr =img->width/2;  //X ZHENG,422
    int writeout_height_cr=img->height/(input->chroma_format==2?1:2); //X ZHENG,422
#endif
    //==== Bug Fix ====//
    //Carmen, 2007/12/20, Bug Fix: correct picture size for outputted reconstructed pictures

    if (p_dec != NULL)
    {
        if (img->type != B_IMG)
        {
            // write reconstructed image (IPPP)
            if (input->successive_Bframe == 0)
            {
                for (i = start; i < writeout_height /*img->height*/; i += inc)
                    for (j = 0; j < writeout_width /*img->width*/; j++)
                        fputc (imgY[i][j], p_dec);

                for (k = 0; k < 2; ++k)
                    for (i = start; i < writeout_height_cr; i += inc)  //modified by X ZHENG,422
                        for (j = 0; j < writeout_width_cr; j++)
                            fputc (imgUV[k][i][j], p_dec);
            }
            // write reconstructed image (IBPBP) : only intra written
            else if (IMG_NUMBER == 0 && input->successive_Bframe != 0)
            {
                for (i = start; i < writeout_height /*img->height*/; i += inc)
                    for (j = 0; j < writeout_width /*img->width*/; j++)
                        fputc (imgY[i][j], p_dec);


                for (k = 0; k < 2; ++k)
                    for (i = start; i < writeout_height_cr; i += inc) //modified by X ZHENG,422
                        for (j = 0; j < writeout_width_cr; j++)
                        {
                            //imgUV[1][i][j]=0;
                            fputc (imgUV[k][i][j], p_dec);
                        }
            }

            // next P picture. This is saved with recon B picture after B picture coding
            if (IMG_NUMBER != 0 && input->successive_Bframe != 0)
            {
                for (i = start; i < writeout_height /*img->height*/; i += inc)
                    for (j = 0; j < writeout_width /*img->width*/; j++)
                        nextP_imgY[i][j] = imgY[i][j];

                for (k = 0; k < 2; ++k)
                    for (i = start; i < writeout_height_cr; i += inc) //modified by X ZHENG,422
                        for (j = 0; j < writeout_width_cr; j++)
                            nextP_imgUV[k][i][j] = imgUV[k][i][j];
            }
        }
        else
        {
            for (i = start; i < writeout_height /*img->height*/; i += inc)
                for (j = 0; j < writeout_width /*img->width*/; j++)
                    fputc (imgY[i][j], p_dec);

            for (k = 0; k < 2; ++k)
                for (i = start; i < writeout_height_cr; i += inc)  //modified by X ZHENG,422
                    for (j = 0; j < writeout_width_cr; j++)
                        fputc (imgUV[k][i][j], p_dec);

            // If this is last B frame also store P frame
            if (img->b_frame_to_code == input->successive_Bframe)
            {
                // save P picture
                for (i = start; i < writeout_height /*img->height*/; i += inc)
                    for (j = 0; j < writeout_width /*img->width*/; j++)
                        fputc (nextP_imgY[i][j], p_dec);

                for (k = 0; k < 2; ++k)
                    for (i = start; i < writeout_height_cr; i += inc) //modified by X ZHENG,422
                        for (j = 0; j < writeout_width_cr; j++)
                            fputc (nextP_imgUV[k][i][j], p_dec);
            }
        }
    }
}

/*
*************************************************************************
* Function:Choose interpolation method depending on MV-resolution
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void interpolate_frame_to_fb ()
{                             // write to mref[]
    UnifiedOneForthPix (imgY, imgUV[0], imgUV[1], mref[0]);
}

/*
*************************************************************************
* Function:Upsample 4 times, store them in out4x.  Color is simply copied
* Input:srcy, srcu, srcv, out4y, out4u, out4v
* Output:
* Return: 
* Attention:Side Effects_
Uses (writes) img4Y_tmp.  This should be moved to a static variable
in this module
*************************************************************************
*/
#define  IClip( Min, Max, Val) (((Val)<(Min))? (Min):(((Val)>(Max))? (Max):(Val)))

static void UnifiedOneForthPix (pel_t ** imgY, pel_t ** imgU, pel_t ** imgV,
                                pel_t ** out4Y)
{
    int is;
    int i, j;// j4;
    int ie2, je2, jj, maxy;

    //horizonal 1/2 interpolation
    for (j = -IMG_PAD_SIZE; j < img->height + IMG_PAD_SIZE; j++)
        for (i = -IMG_PAD_SIZE; i < img->width + IMG_PAD_SIZE; i++)
        {
            jj = IClip (0, img->height - 1, j);
            is = 5 * (imgY[jj][IClip (0, img->width - 1, i)] + imgY[jj][IClip (0, img->width - 1, i + 1)]) +
                - (imgY[jj][IClip (0, img->width - 1, i - 1)] + imgY[jj][IClip (0, img->width - 1, i + 2)]);
            //store horizonal 1/2 pixel value
            img4Y_tmp[(j + IMG_PAD_SIZE)*4][(i + IMG_PAD_SIZE) * 4 + 2] = is;
            //store 1/1 pixel vaule
            img4Y_tmp[(j + IMG_PAD_SIZE)*4][(i + IMG_PAD_SIZE) * 4] = imgY[IClip (0, img->height - 1, j)][IClip (0, img->width - 1, i)]*8;
        }

        //vertical 1/2 interpolation
        for (i = 0; i < (img->width + 2 * IMG_PAD_SIZE) * 4; i+=2)
        {
            for (j = 0; j < (img->height + 2 * IMG_PAD_SIZE) * 4; j+=4)
            {
                maxy = (img->height + 2 * IMG_PAD_SIZE) * 4 - 4;
                is = 5 * (img4Y_tmp[j][i] + img4Y_tmp[min (maxy, j + 4)][i])
                    - (img4Y_tmp[max (0, j - 4)][i] + img4Y_tmp[min (maxy, j + 8)][i]);
                img4Y_tmp[j + 2][i] =is;
            }
            for (j = 0; j < (img->height + 2 * IMG_PAD_SIZE) * 4; j+=4)
            {
                img4Y_tmp[j][i] =img4Y_tmp[j][i]*8;
            }
        }
        // 1/4 pix
        ie2 = (img->width  + 2 * IMG_PAD_SIZE - 1) * 4;
        je2 = (img->height + 2 * IMG_PAD_SIZE - 1) * 4;

        //horizonal 1/4 interpolation
        for (j = 0; j < (img->height  + 2 * IMG_PAD_SIZE)*4; j += 2)
            for (i = 0; i < (img->width  + 2 * IMG_PAD_SIZE)*4; i += 2)
            {
                //  '-'
                img4Y_tmp[j][i+1]=IClip (0,255,
                    (int) ((1* img4Y_tmp[j][IClip (0, (img->width + 2 * IMG_PAD_SIZE) * 4 - 2, i-2)] +
                    7* img4Y_tmp[j][IClip (0, (img->width + 2 * IMG_PAD_SIZE) * 4 - 2, i  )] +
                    7* img4Y_tmp[j][IClip (0, (img->width + 2 * IMG_PAD_SIZE) * 4 - 2, i+2)] +
                    1* img4Y_tmp[j][IClip (0, (img->width + 2 * IMG_PAD_SIZE) * 4 - 2, i+4)] + 512)/ 1024)
                    );
            }

            //vertical 1/4 interpolation
            for (i = 0; i < (img->width  + 2 * IMG_PAD_SIZE)*4; i++)
            {
                for (j = 0; j < (img->height  + 2 * IMG_PAD_SIZE)*4; j += 2)
                {
                    // '|'
                    if (i % 2 == 0)
                    {
                        img4Y_tmp[j+1][i]=IClip(0,255,
                            (int) ((1* img4Y_tmp[IClip (0, (img->height + 2 * IMG_PAD_SIZE) * 4 - 2, j-2)][i] +
                            7* img4Y_tmp[IClip (0, (img->height + 2 * IMG_PAD_SIZE) * 4 - 2, j  )][i] +
                            7* img4Y_tmp[IClip (0, (img->height + 2 * IMG_PAD_SIZE) * 4 - 2, j+2)][i] +
                            1* img4Y_tmp[IClip (0, (img->height + 2 * IMG_PAD_SIZE) * 4 - 2, j+4)][i] + 512) / 1024)
                            );
                    }

                    else if (j % 4 == 0 && i % 4 == 1)
                    {
                        // '\'
                        img4Y_tmp[j+1][i]=IClip(0, 255,
                            (int) ((img4Y_tmp[j+2][i+1] + img4Y_tmp[j][i-1] + 64) / 128)
                            );
                    }
                    else if(j % 4 == 2 && i % 4 == 3){
                        img4Y_tmp[j+1][i]=IClip(0, 255,
                            (int) ((img4Y_tmp[j][i-1]
                        + img4Y_tmp[min((img->height + 2 * IMG_PAD_SIZE - 1) * 4, j+2)][min((img->width + 2 * IMG_PAD_SIZE - 1) * 4, i+1)] + 64 )/ 128)
                            );

                    }
                    else if(j % 4 == 0 && i % 4 == 3)
                    {
                        //  '/'
                        img4Y_tmp[j+1][i]=IClip(0, 255,
                            (int) ((img4Y_tmp[j+2][i-1]
                        + img4Y_tmp[j][min((img->width + 2 * IMG_PAD_SIZE - 1) * 4, i+1)] + 64) / 128)
                            );
                    }
                    else if(j % 4 == 2 && i % 4 == 1){
                        //  '/'
                        img4Y_tmp[j+1][i]=IClip(0, 255,
                            (int) ((img4Y_tmp[j][i+1]
                        + img4Y_tmp[min((img->height + 2 * IMG_PAD_SIZE - 1) * 4, j+2)][i-1] + 64) / 128)
                            );
                    }

                }
            }


            for (j = 0; j < (img->height  + 2 * IMG_PAD_SIZE)*4; j += 2)
                for (i = 0; i < (img->width  + 2 * IMG_PAD_SIZE)*4; i += 2)
                {
                    img4Y_tmp[j][i]=IClip(0, 255, (int) (img4Y_tmp[j][i] + 32) / 64);
                }


                for (j = 0; j < (img->height  + 2 * IMG_PAD_SIZE)*4; j ++)
                    for (i = 0; i < (img->width  + 2 * IMG_PAD_SIZE)*4; i ++)
                    {
                        PutPel_14 (out4Y, j - IMG_PAD_SIZE * 4, i - IMG_PAD_SIZE * 4, (pel_t) img4Y_tmp[j][i]);
                    }
}

static void UnifiedOneForthPix_old (pel_t ** imgY, pel_t ** imgU, pel_t ** imgV,
                                    pel_t ** out4Y)
{
    int is;
    int i, j;// j4;
    int ie2, je2, jj, maxy;

    //horizonal 1/2 interpolation
    for (j = -IMG_PAD_SIZE; j < img->height + IMG_PAD_SIZE; j++)
        for (i = -IMG_PAD_SIZE; i < img->width + IMG_PAD_SIZE; i++)
        {
            jj = IClip (0, img->height - 1, j);
            is = 5 * (imgY[jj][IClip (0, img->width - 1, i)] + imgY[jj][IClip (0, img->width - 1, i + 1)]) +
                - (imgY[jj][IClip (0, img->width - 1, i - 1)] + imgY[jj][IClip (0, img->width - 1, i + 2)]);
            //store horizonal 1/2 pixel value
            img4Y_tmp[(j + IMG_PAD_SIZE)*4][(i + IMG_PAD_SIZE) * 4 + 2] = is;
            //store 1/1 pixel vaule
            img4Y_tmp[(j + IMG_PAD_SIZE)*4][(i + IMG_PAD_SIZE) * 4] = imgY[IClip (0, img->height - 1, j)][IClip (0, img->width - 1, i)]*8;
        }

        //vertical 1/2 interpolation
        for (i = 0; i < (img->width + 2 * IMG_PAD_SIZE) * 4; i+=2)
        {
            for (j = 0; j < (img->height + 2 * IMG_PAD_SIZE) * 4; j+=4)
            {
                maxy = (img->height + 2 * IMG_PAD_SIZE) * 4 - 4;
                is = 5 * (img4Y_tmp[j][i] + img4Y_tmp[min (maxy, j + 4)][i])
                    - (img4Y_tmp[max (0, j - 4)][i] + img4Y_tmp[min (maxy, j + 8)][i]);
                img4Y_tmp[j + 2][i] =is;
            }
            for (j = 0; j < (img->height + 2 * IMG_PAD_SIZE) * 4; j+=4)
            {
                img4Y_tmp[j][i] =img4Y_tmp[j][i]*8;
            }
        }
        // 1/4 pix
        ie2 = (img->width  + 2 * IMG_PAD_SIZE - 1) * 4;
        je2 = (img->height + 2 * IMG_PAD_SIZE - 1) * 4;

        //horizonal 1/4 interpolation
        for (j = 0; j < (img->height  + 2 * IMG_PAD_SIZE)*4; j += 2)
            for (i = 0; i < (img->width  + 2 * IMG_PAD_SIZE)*4; i += 2)
            {
                //  '-'
                img4Y_tmp[j][i+1]=IClip (0,255,
                    (int) ((1* img4Y_tmp[j][IClip (0, (img->width + 2 * IMG_PAD_SIZE) * 4 - 2, i-2)] +
                    7* img4Y_tmp[j][IClip (0, (img->width + 2 * IMG_PAD_SIZE) * 4 - 2, i  )] +
                    7* img4Y_tmp[j][IClip (0, (img->width + 2 * IMG_PAD_SIZE) * 4 - 2, i+2)] +
                    1* img4Y_tmp[j][IClip (0, (img->width + 2 * IMG_PAD_SIZE) * 4 - 2, i+4)] + 512)/ 1024)
                    );
            }

            //vertical 1/4 interpolation
            for (i = 0; i < (img->width  + 2 * IMG_PAD_SIZE)*4; i++)
            {
                for (j = 0; j < (img->height  + 2 * IMG_PAD_SIZE)*4; j += 2)
                {
                    // '|'
                    if (i % 2 == 0)
                    {
                        img4Y_tmp[j+1][i]=IClip(0,255,
                            (int) ((1* img4Y_tmp[IClip (0, (img->height + 2 * IMG_PAD_SIZE) * 4 - 2, j-2)][i] +
                            7* img4Y_tmp[IClip (0, (img->height + 2 * IMG_PAD_SIZE) * 4 - 2, j  )][i] +
                            7* img4Y_tmp[IClip (0, (img->height + 2 * IMG_PAD_SIZE) * 4 - 2, j+2)][i] +
                            1* img4Y_tmp[IClip (0, (img->height + 2 * IMG_PAD_SIZE) * 4 - 2, j+4)][i] + 512) / 1024)
                            );
                    }

                    else if (j % 4 == 0 && i % 4 == 1)
                    {
                        // '\'
                        img4Y_tmp[j+1][i]=IClip(0, 255,
                            (int) ((img4Y_tmp[j+2][i+1] + img4Y_tmp[j][i-1] + 64) / 128)
                            );
                    }
                    else if(j % 4 == 2 && i % 4 == 3){
                        img4Y_tmp[j+1][i]=IClip(0, 255,
                            (int) ((img4Y_tmp[j][i-1]
                        + img4Y_tmp[min((img->height + 2 * IMG_PAD_SIZE - 1) * 4, j+2)][min((img->width + 2 * IMG_PAD_SIZE - 1) * 4, i+1)] + 64 )/ 128)
                            );

                    }
                    else if(j % 4 == 0 && i % 4 == 3)
                    {
                        //  '/'
                        img4Y_tmp[j+1][i]=IClip(0, 255,
                            (int) ((img4Y_tmp[j+2][i-1]
                        + img4Y_tmp[j][min((img->width + 2 * IMG_PAD_SIZE - 1) * 4, i+1)] + 64) / 128)
                            );
                    }
                    else if(j % 4 == 2 && i % 4 == 1){
                        //  '/'
                        img4Y_tmp[j+1][i]=IClip(0, 255,
                            (int) ((img4Y_tmp[j][i+1]
                        + img4Y_tmp[min((img->height + 2 * IMG_PAD_SIZE - 1) * 4, j+2)][i-1] + 64) / 128)
                            );
                    }

                }
            }


            for (j = 0; j < (img->height  + 2 * IMG_PAD_SIZE)*4; j += 2)
                for (i = 0; i < (img->width  + 2 * IMG_PAD_SIZE)*4; i += 2)
                {
                    img4Y_tmp[j][i]=IClip(0, 255, (int) (img4Y_tmp[j][i] + 32) / 64);
                }


                for (j = 0; j < (img->height  + 2 * IMG_PAD_SIZE)*4; j ++)
                    for (i = 0; i < (img->width  + 2 * IMG_PAD_SIZE)*4; i ++)
                    {
                        PutPel_14 (out4Y, j - IMG_PAD_SIZE * 4, i - IMG_PAD_SIZE * 4 ,img4Y_tmp[j][i]);
                    }
}

/*
*************************************************************************
* Function:Find SNR for all three components
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

static void find_snr ()
{
    int i, j;
    int diff_y, diff_u, diff_v;
    int impix;
    int uvformat=input->chroma_format==2?2:4;  //X ZHENG,422

    //  Calculate  PSNR for Y, U and V.
    //     Luma.
    impix = img->height * img->width;

    diff_y = 0;
    for (i = 0; i < img->width; ++i)
    {
        for (j = 0; j < img->height; ++j)
        {
            diff_y += img->quad[imgY_org[j][i] - imgY[j][i]];
        }
    }

    //     Chroma.
    diff_u = 0;
    diff_v = 0;

    for (i = 0; i < img->width_cr; i++)
    {
        for (j = 0; j < img->height_cr; j++)
        {
            diff_u += img->quad[imgUV_org[0][j][i] - imgUV[0][j][i]];
            diff_v += img->quad[imgUV_org[1][j][i] - imgUV[1][j][i]];
        }
    }

    //  Collecting SNR statistics
    if (diff_y != 0)
    {
        snr->snr_y = (float) (10 * log10 (65025 * (float) impix / (float) diff_y));         // luma snr for current frame
        snr->snr_u = (float) (10 * log10 (65025 * (float) impix / (float) (uvformat * diff_u)));   // u croma snr for current frame, 1/4 of luma samples, 422
        snr->snr_v = (float) (10 * log10 (65025 * (float) impix / (float) (uvformat * diff_v)));   // v croma snr for current frame, 1/4 of luma samples, 422
    }

    if (img->number == 0)
    {
        snr->snr_y1 = (float) (10 * log10 (65025 * (float) impix / (float) diff_y));        // keep luma snr for first frame
        snr->snr_u1 = (float) (10 * log10 (65025 * (float) impix / (float) (uvformat * diff_u)));  // keep croma u snr for first frame
        snr->snr_v1 = (float) (10 * log10 (65025 * (float) impix / (float) (uvformat * diff_v)));  // keep croma v snr for first frame
        snr->snr_ya = snr->snr_y1;
        snr->snr_ua = snr->snr_u1;
        snr->snr_va = snr->snr_v1;
    }
    // B pictures
    else
    {
        snr->snr_ya = (float) (snr->snr_ya * (img->number + Bframe_ctr) + snr->snr_y) / (img->number + Bframe_ctr + 1); // average snr lume for all frames inc. first
        snr->snr_ua = (float) (snr->snr_ua * (img->number + Bframe_ctr) + snr->snr_u) / (img->number + Bframe_ctr + 1); // average snr u croma for all frames inc. first, 422
        snr->snr_va = (float) (snr->snr_va * (img->number + Bframe_ctr) + snr->snr_v) / (img->number + Bframe_ctr + 1); // average snr v croma for all frames inc. first, 422
    }

}

/*
*************************************************************************
* Function:Find distortion for all three components
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/
void find_distortion ()
{
    int i, j;
    int diff_y, diff_u, diff_v;
    int impix;

    //  Calculate  PSNR for Y, U and V.
    //     Luma.
    impix = img->height * img->width;

    diff_y = 0;
    for (i = 0; i < img->width; ++i)
    {
        for (j = 0; j < img->height; ++j)
        {
            diff_y += img->quad[abs (imgY_org[j][i] - imgY[j][i])];
        }
    }

    //     Chroma.
    diff_u = 0;
    diff_v = 0;

    for (i = 0; i < img->width_cr; i++)
    {
        for (j = 0; j < img->height_cr; j++)
        {
            diff_u += img->quad[abs (imgUV_org[0][j][i] - imgUV[0][j][i])];
            diff_v += img->quad[abs (imgUV_org[1][j][i] - imgUV[1][j][i])];
        }
    }

    // Calculate real PSNR at find_snr_avg()
    snr->snr_y = (float) diff_y;
    snr->snr_u = (float) diff_u;
    snr->snr_v = (float) diff_v;

}

/*!
************************************************************************
* \brief
*    RD decision of frame and field coding 
************************************************************************
*/
int decide_fld_frame(float snr_frame_Y, float snr_field_Y, int bit_field, int bit_frame, double lambda_picture)
{
    double cost_frame, cost_field;

    cost_frame = bit_frame * lambda_picture + snr_frame_Y;
    cost_field = bit_field * lambda_picture + snr_field_Y;

    if (cost_field > cost_frame)
        return (0);
    else
        return (1);
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

static void ReportFirstframe(int tmp_time)
{
    int bits;
    // 
    //   FILE *file = fopen("stat.dat","at");
    //   
    //   fprintf(file,"\n -------------------- DEBUG_INFO_START -------------------- \n");
    //   
    //   fprintf (file,"%3d(I)  %8d %4d %7.4f %7.4f %7.4f  %5d          %3d      %3s\n",
    //     frame_no, stats.bit_ctr - stats.bit_ctr_n,
    //     img->qp, snr->snr_y, snr->snr_u, snr->snr_v, tmp_time,
    //     intras, img->picture_structure ? "FLD" : "FRM");
    //   
    //   fclose(file);

    printf("%3d(I)  ", frame_no);
    printf("%8d ", stats.bit_ctr - stats.bit_ctr_n);
    printf("%4d ", img->qp);
    printf("%7.4f ", snr->snr_y);
    printf("%7.4f ", snr->snr_u);
    printf("%7.4f ", snr->snr_v);
    printf("%5d ", tmp_time);
    printf("%s\n", img->picture_structure ? "FRM":"FLD");

    //printf ("%3d(I)  %8d %4d %7.4f %7.4f %7.4f  %5d       %s \n",frame_no, stats.bit_ctr - stats.bit_ctr_n,img->qp, snr->snr_y, snr->snr_u, snr->snr_v, tmp_time, img->picture_structure ? "FRM":"FLD" );

    //Rate control
    if(input->RCEnable)
    {
        if(input->InterlaceCodingOption==0)
            bits = stats.bit_ctr-stats.bit_ctr_n; // used for rate control update 
        else
        {
            bits = stats.bit_ctr - Iprev_bits; // used for rate control update 
            Iprev_bits = stats.bit_ctr;
        }
    }

    stats.bitr0 = stats.bitr;
    stats.bit_ctr_0 = stats.bit_ctr;
    stats.bit_ctr = 0;

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

static void ReportIntra(int tmp_time)
{
    //   FILE *file = fopen("stat.dat","at");
    // 
    //   fprintf (file,"%3d(I)  %8d %4d %7.4f %7.4f %7.4f  %5d      \n",
    //     frame_no, stats.bit_ctr - stats.bit_ctr_n,
    //     img->qp, snr->snr_y, snr->snr_u, snr->snr_v, tmp_time );
    //   
    //  fclose(file);
    printf("%3d(I)  ", frame_no);
    printf("%8d ", stats.bit_ctr - stats.bit_ctr_n);
    printf("%4d", img->qp);
    printf(" %7.4f %7.4f %7.4f", snr->snr_y, snr->snr_u, snr->snr_v);
    printf("  %5d", tmp_time);
    printf(" %3s\n", img->picture_structure ? "FRM":"FLD");
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

static void ReportB(int tmp_time)
{
    //   FILE *file = fopen("stat.dat","at");
    // 
    //   fprintf (file,"%3d(B)  %8d %4d %7.4f %7.4f %7.4f  %5d        \n",
    //     frame_no, stats.bit_ctr - stats.bit_ctr_n, img->qp,
    //     snr->snr_y, snr->snr_u, snr->snr_v, tmp_time);
    //   
    //  fclose(file);
    printf("%3d(B)  ", frame_no);
    printf("%8d ", stats.bit_ctr - stats.bit_ctr_n);
    printf("%4d ", img->qp);
    printf("%7.4f %7.4f %7.4f  ", snr->snr_y, snr->snr_u, snr->snr_v);
    printf("%5d", tmp_time);
    printf("       %3s\n", img->picture_structure ? "FRM":"FLD");
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

static void ReportP(int tmp_time)
{            
    //   FILE *file = fopen("stat.dat","at");
    char type = (img->types==INTRA_IMG)?'I':'P';
    // 
    //   fprintf (file,"%3d(%c)  %8d %4d %7.4f %7.4f %7.4f  %5d        %3d\n",
    //     frame_no, type, stats.bit_ctr - stats.bit_ctr_n, img->qp, snr->snr_y,
    //     snr->snr_u, snr->snr_v, tmp_time,
    //     intras);
    //   
    //  fclose(file);

    printf("%3d(%c)  ", frame_no, type);
    printf("%8d ", stats.bit_ctr - stats.bit_ctr_n);
    printf("%4d", img->qp);
    printf(" %7.4f %7.4f %7.4f", snr->snr_y,snr->snr_u, snr->snr_v);
    printf("  %5d", tmp_time);
    printf("       %3s", img->picture_structure ? "FRM":"FLD");
    printf("%3d    \n", intras);
    //printf ("%3d(%c)  %8d %4d %7.4f %7.4f %7.4f  %5d       %3s     %3d    \n",
    //  frame_no, type, stats.bit_ctr - stats.bit_ctr_n, img->qp, snr->snr_y,
    //  snr->snr_u, snr->snr_v, tmp_time,
    //   img->picture_structure ? "FRM":"FLD",intras);

}

/*
*************************************************************************
* Function:Copies contents of a Sourceframe structure into the old-style
*    variables imgY_org_frm and imgUV_org_frm.  No other side effects
* Input:  sf the source frame the frame is to be taken from
* Output:
* Return: 
* Attention:
*************************************************************************
*/

// image padding, 20071009
static void CopyFrameToOldImgOrgVariables ()
{
    int x, y;
    byte *u_buffer,*v_buffer;
    int input_height_cr = input->chroma_format==1?input->img_height/2 : input->img_height; //X ZHENG,422
    int img_height_cr   = input->chroma_format==1?img->height/2 : img->height;             //X ZHENG,422
    u_buffer=imgY_org_buffer+input->img_width*input->img_height;
    if(input->chroma_format==2)  //wzm,422
        v_buffer=imgY_org_buffer+input->img_width*input->img_height*3/2;
    else if(input->chroma_format==1)
        v_buffer=imgY_org_buffer+input->img_width*input->img_height*5/4;

    // Y
    for (y=0; y<input->img_height; y++)
        for (x=0; x<input->img_width; x++)
            imgY_org_frm [y][x] = imgY_org_buffer[y*input->img_width+x];

    // Y's width padding
    for (y=0; y<input->img_height; y++)
        for (x=input->img_width; x<img->width; x++)
            imgY_org_frm [y][x] = imgY_org_frm [y][x-1];

    //Y's padding bottom border
    for (y=input->img_height; y<img->height; y++)
        for (x=0; x<img->width; x++)
            imgY_org_frm [y][x] = imgY_org_frm [y-1][x];

    // UV
    for (y=0; y<input_height_cr; y++) //X ZHENG, 422
        for (x=0; x<input->img_width/2; x++)
        {
            imgUV_org_frm[0][y][x] = u_buffer[y*input->img_width/2+x];
            imgUV_org_frm[1][y][x] = v_buffer[y*input->img_width/2+x];
        }

        // UV's padding width
        for (y=0; y<input_height_cr; y++)  //X ZHENG, 422
            for (x=input->img_width/2; x<img->width/2; x++)
            {
                imgUV_org_frm [0][y][x] = imgUV_org_frm [0][y][x-1];
                imgUV_org_frm [1][y][x] = imgUV_org_frm [1][y][x-1];
            }

            // UV's padding bottom border
            for (y=input_height_cr; y<img_height_cr; y++)  //X ZHENG, 422
                for (x=0; x<img->width/2; x++)
                {
                    imgUV_org_frm [0][y][x] = imgUV_org_frm [0][y-1][x];
                    imgUV_org_frm [1][y][x] = imgUV_org_frm [1][y-1][x];
                }

                if(input->InterlaceCodingOption != FRAME_CODING)
                {    
                    if(input->InterlaceCodingOption != FRAME_CODING)
                    {
                        // Y, interlace
                        for (y=0; y<input->img_height; y += 2)
                            for (x=0; x<input->img_width/*img->width*/; x++)  //modified by X ZHENG, 20080515
                            {
                                imgY_org_top [y/2][x] = imgY_org_frm [y][x]; // !! Lum component for top field 
                                imgY_org_bot [y/2][x] = imgY_org_frm [y+1][x]; // !! Lum component for bot field
                            }

                            // Y's width padding, interlace, //added by X ZHENG, 20080515
                            for (y=0; y<input->img_height; y += 2)
                                for (x=input->img_width; x<img->width; x++)  
                                {
                                    imgY_org_top [y/2][x] = imgY_org_frm [y][x]; // !! Lum component for top field 
                                    imgY_org_bot [y/2][x] = imgY_org_frm [y+1][x]; // !! Lum component for bot field
                                }

                                // Y's padding bottom border, interlace
                                for (y=input->img_height/2; y<img->height/2; y ++)
                                    for (x=0; x<img->width; x++)
                                    {
                                        imgY_org_top [y][x] = imgY_org_top [y-1][x]; // !! Lum component for top field 
                                        imgY_org_bot [y][x] = imgY_org_bot [y-1][x]; // !! Lum component for bot field
                                    }


                                    // UV, interlace
                                    for (y=0; y<input_height_cr; y += 2)  //X ZHENG, 422
                                        for (x=0; x</*img->width*/input->img_width/2; x++)  //modified by X ZHENG, 20080515
                                        {
                                            imgUV_org_top[0][y/2][x] = imgUV_org_frm [0][y][x];    // !! Cr and Cb component for top field
                                            imgUV_org_top[1][y/2][x] = imgUV_org_frm [1][y][x];
                                            imgUV_org_bot[0][y/2][x] = imgUV_org_frm [0][y+1][x]; // !! Cr and Cb component for bot field
                                            imgUV_org_bot[1][y/2][x] = imgUV_org_frm [1][y+1][x];
                                        }  //2007.07.27 tianf*/ 

                                        // UV's width padding, interlace, //added by X ZHENG, 20080515, 422
                                        for (y=0; y<input_height_cr; y += 2)
                                            for (x=input->img_width/2; x<img->width/2; x++)
                                            {
                                                imgUV_org_top[0][y/2][x] = imgUV_org_frm [0][y][x];    // !! Cr and Cb component for top field
                                                imgUV_org_top[1][y/2][x] = imgUV_org_frm [1][y][x];
                                                imgUV_org_bot[0][y/2][x] = imgUV_org_frm [0][y+1][x]; // !! Cr and Cb component for bot field
                                                imgUV_org_bot[1][y/2][x] = imgUV_org_frm [1][y+1][x];
                                            }  //2007.07.27 tianf*/ 

                                            // UV's padding bottom border, interlace
                                            for (y=input_height_cr/2==0?1:input_height_cr/2; y<img_height_cr/2; y ++) //modified by X ZHENG, 20080515, 422
                                                for (x=0; x<img->width/2; x++)
                                                {
                                                    imgUV_org_top[0][y][x] = imgUV_org_top[0][y-1][x];    // !! Cr and Cb component for top field
                                                    imgUV_org_top[1][y][x] = imgUV_org_top[1][y-1][x];
                                                    imgUV_org_bot[0][y][x] = imgUV_org_bot[0][y-1][x]; // !! Cr and Cb component for bot field
                                                    imgUV_org_bot[1][y][x] = imgUV_org_bot[1][y-1][x];
                                                }  //2007.07.27 tianf*/ 

                    }

                }
}
// image padding, 20071009

/*
*************************************************************************
* Function: Calculates the absolute frame number in the source file out
of various variables in img-> and input->
* Input:
* Output:
* Return: frame number in the file to be read
* Attention: \side effects
global variable frame_no updated -- dunno, for what this one is necessary
*************************************************************************
*/


static int CalculateFrameNumber()
{
    if (img->type == B_IMG)
        //    frame_no = (IMG_NUMBER - 1) * (input->jumpd + 1) + img->b_interval * img->b_frame_to_code;  // 20071009
        frame_no = img->tr;    // display index, xz zheng, 20071009
    else
    {
        //    frame_no = IMG_NUMBER * (input->jumpd + 1);   // 20071009
        frame_no = img->tr;   // 20071009
    }

    return frame_no;
}

/*
*************************************************************************
* Function:Reads one new frame from file
* Input: FrameNoInFile: Frame number in the source file
HeaderSize: Number of bytes in the source file to be skipped
xs: horizontal size of frame in pixels, must be divisible by 16
ys: vertical size of frame in pixels, must be divisible by 16 or
32 in case of MB-adaptive frame/field coding
sf: Sourceframe structure to which the frame is written
* Output:
* Return: 
* Attention:
*************************************************************************
*/

static void ReadOneFrame (int FrameNoInFile, int HeaderSize, int xs, int ys)
{
    //const unsigned int  bytes_y = input->img_width *input->stuff_height; //modify by wuzhongmou 0610
    unsigned int  bytes_y /*= input->img_width *input->img_height*/; //add by wuzhongmou 0610,422
    unsigned int bytes_uv /*= bytes_y/4*/;              //wzm,422
    int framesize_in_bytes /*= bytes_y + 2*bytes_uv*/;  //wzm,422
    int off_y /*= input->img_width*input->img_height*/;       //wzm,422
    int off_uv /*= off_y/4*/;    //wzm,422
    int bytespos_overflow;     //Carmen, 2008/02/25

    //added by wzm,422
    if(input->chroma_format==2)  {
        bytes_y  = input->img_width * input->img_height;
        bytes_uv = bytes_y/2;
        framesize_in_bytes = bytes_y + 2*bytes_uv;
        off_y    = input->img_width*input->img_height;
        off_uv   = off_y/2;
    }
    else if(input->chroma_format==1) {
        bytes_y  = input->img_width * input->img_height; 
        bytes_uv = bytes_y/4;
        framesize_in_bytes = bytes_y + 2*bytes_uv;
        off_y    = input->img_width*input->img_height;
        off_uv   = off_y/4;
    }
    //wzm,422

    assert (FrameNumberInFile == FrameNoInFile);

    //Carmen, 2008/02/25, Problem in seeking input files when the pos to read is larger than 31 bits
    bytespos_overflow=((framesize_in_bytes*FrameNoInFile + HeaderSize)&0x80000000)?(1):(0);
    if (bytespos_overflow)
    {
        unsigned int bytes_to_read=(framesize_in_bytes*FrameNoInFile + HeaderSize);
        unsigned int num_overflow=(unsigned int)((bytes_to_read)/0x7FFFFFFF);
        unsigned int read_offset=(unsigned int)((bytes_to_read)%0x7FFFFFFF);
        unsigned int ocnt;

        printf("num_overflow=%d read_offset=%d\n", num_overflow, read_offset);      
        for (ocnt=0; ocnt<num_overflow; ocnt++)
        {
            if (fseek (p_in, 0x7FFFFFFF, SEEK_SET) != 0)
            {
                error ("ReadOneFrame: there is error in seeking 0x7FFFFFFF bytes! in p_in", -1);
            }       
        }
        if (fseek (p_in, read_offset, SEEK_SET) != 0)
        {
            error ("ReadOneFrame: there is error in seeking read_offset bytes! in p_in", -1);
        }       
    }
    else // modified by Carmen, 2008/02/25, Problem in seeking input files when the pos to read is larger than 31 bits
        if (fseek (p_in, framesize_in_bytes*FrameNoInFile + HeaderSize, SEEK_SET) != 0)
            error ("ReadOneFrame: cannot fseek to (Header size) in p_in", -1);

    if (fread (imgY_org_buffer, 1, bytes_y, p_in) != (unsigned )bytes_y)
    {
        printf ("ReadOneFrame: cannot read %d bytes from input file, unexpected EOF?, exiting", bytes_y);
        report_stats_on_error();
    }



    if (fread (imgY_org_buffer + off_y, 1, bytes_uv, p_in) != (unsigned )bytes_uv)
    {
        printf ("ReadOneFrame: cannot read %d bytes from input file, unexpected EOF?, exiting", bytes_y);
        exit (-1);
    }



    if (fread (imgY_org_buffer + off_y + off_uv, 1, bytes_uv, p_in) != (unsigned )bytes_uv)  //wzm,422
    {
        printf ("ReadOneFrame: cannot read %d bytes from input file, unexpected EOF?, exiting", bytes_y);
        exit (-1);
    }


}

/*
*************************************************************************
* Function:point to frame coding variables 
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void put_buffer_frame()
{
    int i,j;

    imgY_org  = imgY_org_frm;
    imgUV_org = imgUV_org_frm;  
    tmp_mv    = tmp_mv_frm;

    //initialize ref index 1/4 pixel
    for(i=0;i<2;i++)
    {
        mref[i] = mref_frm[i];
    }

    //integer pixel for chroma
    for(i=0;i<2;i++)
        for(j=0;j<2;j++)
        {
            mcef[i][j]   = ref_frm[i][j+1];  
        }

        //integer pixel for luma
        for(i=0;i<2;i++)
            Refbuf11[i] = &ref_frm[i][0][0][0];

        //current reconstructed image 

        imgY = imgY_frm = current_frame[0];
        imgUV = imgUV_frm =  &current_frame[1];

        refFrArr    = refFrArr_frm;
        fw_refFrArr = fw_refFrArr_frm;
        bw_refFrArr = bw_refFrArr_frm;
}

/*
*************************************************************************
* Function:point to top field coding variables 
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/
void put_buffer_top()
{
    int i,j;

    img->fld_type = 0;


    imgY_org  = imgY_org_top;
    imgUV_org = imgUV_org_top;

    //intial ref index for field coding 
    for(i=0;i<4;i++)              // 3 2 1 0
        ref[i] = ref_fld[i]; 

    //initialize ref index 1/4 pixel
    for(i=0;i<4;i++)
    {
        mref[i] = mref_fld[i];
    }

    //integer chroma pixel for interlace 
    for (j=0;j<4;j++)//ref_index = 0
        for (i=0;i<2;i++)
        {
            mcef[j][i] = ref_fld[j][i+1];
        }

        //integer luma pixel for interlace
        for(i=0;i<4;i++)
        {
            Refbuf11[i] = &ref[i][0][0][0];
        }

        imgY  = imgY_top   = current_field[0];
        imgUV = imgUV_top  = &current_field[1];

        tmp_mv = tmp_mv_top;
        refFrArr = refFrArr_top;
        fw_refFrArr = fw_refFrArr_top;
        bw_refFrArr = bw_refFrArr_top;
}

/*
*************************************************************************
* Function:point to bottom field coding variables 
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void put_buffer_bot()
{
    int i,j;

    img->fld_type = 1; 

    imgY_org = imgY_org_bot;
    imgUV_org = imgUV_org_bot;

    tmp_mv = tmp_mv_bot;
    refFrArr = refFrArr_bot;
    fw_refFrArr = fw_refFrArr_bot;
    bw_refFrArr = bw_refFrArr_bot;

    //intial ref index for field coding 
    for(i=0;i<4;i++)
        ref[i] = ref_fld[i]; 

    //initialize ref index 1/4 pixel
    for(i=0;i<4;i++)
    {
        mref[i] = mref_fld[i];
    }

    //integer chroma pixel for interlace 
    for (j=0;j<4;j++)//ref_index = 0
        for (i=0;i<2;i++)
        {
            mcef[j][i] = ref_fld[j][i+1];
        }

        //integer luma pixel for interlace
        for(i=0;i<4;i++)
        {
            Refbuf11[i] = &ref[i][0][0][0];
        }
        //imgY = imgY_bot;
        //imgUV = imgUV_bot;
        imgY_bot = current_field[0];
        imgUV_bot = &current_field[1];
        imgY = imgY_bot;
        imgUV = imgUV_bot;
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

    UnifiedOneForthPix (imgY, imgUV[0], imgUV[1],
        mref[1]);
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

    UnifiedOneForthPix (imgY, imgUV[0], imgUV[1],
        mref[0]);
}

/*
*************************************************************************
* Function:update the decoder picture buffer
* Input:frame number in the bitstream and the video sequence
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void Update_Picture_Buffers()
{ 
    unsigned char ***tmp;
    unsigned char **tmp_y;
    int i;

    //update integer pixel reference buffer
    tmp = ref_frm[1];       //ref_frm[ref_index][yuv][height][width] ref_index = 0,1  for P frame
    ref_frm[1] = ref_frm[0];    // ref_index = 0, backward reference for B frame; 1: forward reference for B frame
    ref_frm[0] = current_frame; // current_frame: current image under reconstruction
    current_frame = tmp;  

    //update luma 1/4 pixel reference buffer mref[ref_index][height][width] ref_index = 0,1 for P frame
    tmp_y = mref_frm[1];              // ref_index = 0, forward refernce for B frame ; 1: backward refernce for B frame
    mref_frm[1] = mref_frm[0];
    mref_frm[0] = tmp_y;

    //initial reference index, and for coming interpolation in mref[0]
    for(i=0;i<2;i++)
    {
        mref[i] = mref_frm[i];
    }
}



void Update_Picture_Buffers_top_field()
{ 
    unsigned char ***tmp;
    unsigned char **tmp_y;
    int i,j;


    //update integer pixel reference frame buffer 
    tmp = ref_fld[4];
    for (i=4; i>0; i--)
        ref_fld[i] = ref_fld[i-1];
    ref_fld[0] = current_field;
    current_field = tmp;

    //update 
    tmp_y = mref_fld[3];
    for (j=3;j>0;j--)//ref_index = 0
    {
        mref_fld[j] = mref_fld[j-1];
    }
    mref_fld[0] = tmp_y;

    //for interpolation
    //for(i=0;i<3;i++)
    for(i=0;i<4;i++)
    {
        mref[i] = mref_fld[i];
    } 
}

void Update_Picture_Buffers_bot_field()
{ 
    unsigned char ***tmp;
    unsigned char **tmp_y;
    int i,j;


    //update integer pixel reference frame buffer 
    tmp = ref_fld[4];
    for (i=4; i>0; i--)
        ref_fld[i] = ref_fld[i-1];
    ref_fld[0] = current_field;
    current_field = tmp;

    //update 1/4 pixel reference for interpolation
    tmp_y = mref_fld[3];
    for (j=3;j>0;j--)//ref_index = 0
    {
        mref_fld[j] = mref_fld[j-1];
    }
    mref_fld[0] = tmp_y;

    //for interpolation
    //for(i=0;i<3;i++)
    for(i=0;i<4;i++)
    {
        mref[i] = mref_fld[i];
    } 
}

int DetectLumVar()
{
    int i , j ;
    int Histogtam_Cur[256] ;
    int Histogtam_Pre[256] ;
    int temp = 0 ;

    for( i = 0 ; i < 256 ; i++){
        Histogtam_Cur[i] = 0 ; 
        Histogtam_Pre[i] = 0 ;
    }

    for(j = 0 ; j < img->height ; j++){
        for( i = 0 ; i < img->width ; i++){
            Histogtam_Cur[imgY_org[j][i]] += 1 ;
            Histogtam_Pre[Refbuf11[0][j*img->width + i]] += 1 ;
        }
    }

    for(i = 0 ; i < 256 ; i++){
        temp += abs(Histogtam_Pre[i] - Histogtam_Cur[i]);
    }

    // if(temp >= ((img->height*img->width)*2)){
    if(temp >= ((img->height*img->width)/4)){
        return 1;
    }
    else
    {
        return 0;
    }
}

void CalculateBrightnessPar(int currentblock[16][16] , int preblock[16][16] , float *c , float *d)
{
    int N = 256 ;
    int i , j ;
    int m1,m2,m3,m4,m5,m6;

    m1 = m2 = m3 = m4 = m5 = m6 = 0 ;
    for(j = 0 ; j < 16 ; j++){
        for(i = 0 ; i < 16 ; i++){
            m1 += preblock[j][i]*preblock[j][i] ;
            m2 += preblock[j][i];
            m3 += preblock[j][i];
            m4 += 1;
            m5 += preblock[j][i]*currentblock[j][i] ;
            m6 += currentblock[j][i]; 
        }
    }
    *c = ((float)(m4*m5 - m2*m6)) / ((float)(m1*m4 - m2*m3));
    *d = ((float)(m3*m5 - m6*m1)) / ((float)(m3*m2 - m1*m4));
    return ;
}

void CalculatePar(int refnum)
{
    int mbx , mby ;
    int currmb[16][16] ;
    int refmb[16][16] ;
    float alpha ;
    float belta ;
    int i , j ;
    int Alpha_His[256];
    int Belta_His[256];
    int max_num = 0 ;
    int max_index = -1 ;
    int belta_sum = 0 ;

    for( i = 0 ; i < 256 ; i++){
        Alpha_His[i] = 0 ;
        Belta_His[i] = 0 ;
    }

    for(mby = 0 ; mby < img->height/16 ; mby++){
        for(mbx = 0 ; mbx < img->width/16 ; mbx++){
            for( j = 0 ; j < 16 ; j++){
                for( i = 0 ; i < 16 ; i++){
                    currmb[j][i] = imgY_org[mby*16+j][mbx*16+i];
                    refmb [j][i] = Refbuf11[refnum][(mby*16+j)*img->width + mbx*16+i] ;
                }
            }
            CalculateBrightnessPar(currmb,refmb,&alpha,&belta);
            allalpha_lum[mby*(img->width/16)+mbx] = (int)(alpha*32);
            allbelta_lum[mby*(img->width/16)+mbx] = (int)(belta);
        }
    }

    for(i = 0 ; i < ((img->height/16)*(img->width/16)) ; i++)
    {
        // !!  [12/28/2005] cjw  shenyanfei 
        if((0 < allalpha_lum[i]) &&( allalpha_lum[i] < 256)&&(abs(allbelta_lum[i]) < 127))
        {
            Alpha_His[abs(allalpha_lum[i])]++;
        }
    }

    for( i = 4 ; i < 256 ; i++) // !! 4-256 shenyanfei 
    {
        if(Alpha_His[i] > max_num)
        {
            max_num = Alpha_His[i] ;
            max_index = i ;
        }
    }

    for( i = 0 ; i < ((img->height/16)*(img->width/16)) ; i++){
        if(allalpha_lum[i] == max_index){
            belta_sum += allbelta_lum[i] ;
        }
    }
    img->lum_scale[refnum] = max_index ;

    if (max_num == 0) {
        max_num = 1;  //20080515
    }

    img->lum_shift[refnum] = belta_sum/max_num ;

    //cjw 20060327 for shift range limit  7.2.4
    img->lum_shift[refnum]= Clip3(-128,127,img->lum_shift[refnum]);

    if(max_num > ((img->height/16)*(img->width/16) / 2))
        img->mb_weighting_flag = 0 ; //all the NoIntra mbs are WP 
    else
        img->mb_weighting_flag = 1 ; 

    img->chroma_scale[refnum] = img->lum_scale[refnum] ;  // cjw default chroma value same with luma
    img->chroma_shift[refnum] = img->lum_shift[refnum]  ; // cjw default chroma value same with luma

    return ;
}

void estimate_weighting_factor()
{
    int   bframe    = (img->type==B_IMG);
    int   max_ref   = img->nb_references;
    int   ref_num ;

    if(max_ref > img->buf_cycle)
        max_ref = img->buf_cycle;

    // !! detection luminance variation
    img->LumVarFlag = DetectLumVar();
    img->LumVarFlag = 1; //cjw 20051230 just for debug

    if(img->LumVarFlag == 1){
        for(ref_num = 0 ; ref_num < max_ref ; ref_num++){
            CalculatePar(ref_num);
        }
    }
    return;
}

//then for field coding the parameters are achieved from frame coding function estimate_weighting_factor()
void estimate_weighting_factor_field()
{
    int   bframe    = (img->type==B_IMG);
    int   max_ref   = img->nb_references;
    int   ref_num ;

    if(max_ref > img->buf_cycle)
        max_ref = img->buf_cycle;

    // !! detection luminance variation
    img->LumVarFlag = DetectLumVar();  
    img->LumVarFlag = 1; //cjw 20051230 just for debug

    if (img->LumVarFlag == 1) {
        for(ref_num = 0 ; ref_num < max_ref ; ref_num++){
            CalculatePar(ref_num);
        }
    }
    return;
}
