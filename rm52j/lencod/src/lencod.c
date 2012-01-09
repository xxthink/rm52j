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

#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/timeb.h>
#include <stdlib.h>

#if defined WIN32
#include <conio.h>
#endif
#include <assert.h>

#include "global.h"
#include "configfile.h"
#include "memalloc.h"
#include "mbuffer.h"
#include "image.h"
#include "header.h"
#include "ratectl.h"
#include "vlc.h"  // Added by cjw, 20070327
#include "bitstream.h"

#ifdef FastME
#include "fast_me.h"
#endif


#define RM      "5"
#define VERSION "5.2 20080515"

InputParameters inputs, *input = &inputs;
ImageParameters images, *img   = &images;
SNRParameters   snrs,   *snr   = &snrs;
StatParameters  stats;


int    start_tr_in_this_IGOP = 0;
/*
*************************************************************************
* Function:Main function for encoder.
* Input:argc
number of command line arguments
argv
command line arguments
* Output:
* Return: exit code
* Attention:
*************************************************************************
*/

void Init_Motion_Search_Module ();
void Clear_Motion_Search_Module ();

int main(int argc,char **argv)
{
    int image_type;
    int M, N, np, nb, n;
    int len=0;      // Added by cjw, 20070327
    Bitstream *bitstream = currBitStream;  // Added by cjw, 20070327

    p_dec = p_dec_u = p_dec_v = p_stat = p_log = p_datpart = p_trace = NULL;

#ifdef _OUTPUT_MB_QSTEP_BITS_
    pf_mb_qstep_bits = fopen("mb_qstep_bits.txt", "a");
    memset(glb_mb_coff_bits, 0, 3*sizeof(int));
#endif
    memset(total_enc_frame, 0, 4*sizeof(int));
    Configure (argc, argv);
    init_img();
    //  DecideMvRange();

    frame_pic = malloc_picture();
    if (input->InterlaceCodingOption != FRAME_CODING)
    {
        top_pic = malloc_picture();
        bot_pic = malloc_picture();
    }

    init_rdopt ();
    init_frame_buffers(input,img);
    init_global_buffers();
    Init_Motion_Search_Module ();
    information_init();

    //Rate control 
    if(input->RCEnable)
        rc_init_seq();

    //FAST MOTION ESTIMATION. ZHIBO CHEN 2003.3
#ifdef FastME
    DefineThreshold();
    PartCalMad = PartCalMad_c;
#ifdef SSE_ME
    PartCalMad = PartCalMad_sse;
#endif
#else
#ifdef SSE_ME
    FullPelBlockMotionSearch = FullPelBlockMotionSearch_sse;
#else
    FullPelBlockMotionSearch = FullPelBlockMotionSearch_c;
#endif
#endif

    // B pictures
    Bframe_ctr=0;
    tot_time=0;                 // time for total encoding session
    img->pn = -1;    
    input->skip_mode_flag = 1; 

    // Write sequence header 
    stats.bit_slice = start_sequence();
    printf("Sequence Header \n");
    img->Seqheader_flag=1;           // Add by cjw, 20070327
    img->count_PAFF=0;             // Add by cjw, 20070327
    img->curr_picture_distance = img->last_picture_distance = 0;  // Added by Xiaozhen Zheng, 20070405

    img->EncodeEnd_flag=0;     //Carmen, 2007/12/19, Bug Fix: No sequence end codes when stream has multiple sequences

    tmp_buf_cycle = img->buf_cycle;  

    for (img->number=0; img->number < input->no_frames; img->number++)
    {
        if(img->number!=0 && input->vec_period!=0 && img->number%(input->vec_period*input->seqheader_period*input->intra_period)==0 ){
            len=  WriteVideoEditCode(); //added by cjw 20070419
            stats.bit_slice += len;
        }

        if(img->number!=0 && input->seqheader_period!=0 && img->number%(input->seqheader_period*input->intra_period)==0){
            stats.bit_slice += terminate_sequence();  //Carmen, 2007/12/19, Bug Fix: No sequence end codes when stream has multiple sequences

            stats.bit_slice += start_sequence();
            printf("Sequence Header \n");
            img->Seqheader_flag=1;           
            img->count_PAFF=0;           
            img->nb_references = 0;      // restrict ref_num after the seq_header, 20071009
        }
        // Added by cjw, 20070327, End


        img->pn = (img->pn+1) % (img->buf_cycle+1); 
        img->buf_cycle = tmp_buf_cycle;  

        //frame_num for this frame
        img->frame_num = IMG_NUMBER % (1 << (LOG2_MAX_FRAME_NUM_MINUS4 + 4));

        SetImgType();

        stats.bit_use_header[img->type] += len;    // Added by cjw, 20070327

        //    if(img->number==0) //modified by cjw AVS 20070204  Spec. 7.2.3.1 //commented Hisilicon XiaoZhen Zheng 20070327
        //      picture_distance = 0;
        //    else
        //   {
        //      if(img->type==B_IMG)
        //          picture_distance = ((IMG_NUMBER - 1) * (input->jumpd + 1) + img->b_interval * img->b_frame_to_code)%256;  // Tsinghua 200701
        //      else
        //          picture_distance = (IMG_NUMBER * (input->jumpd + 1))%256;  // Tsinghua 200701
        //    }

        image_type = img->type;

        if(image_type == INTRA_IMG)    // jlzheng 7.21
            //img->buf_cycle /= 2;        
            img->buf_cycle  = 1;       // cjw 20060321 
        // for 1 reference is used field coding (I bottom only 1 reference is used)

        //Rate control
        if (img->type == INTRA_IMG)
        {
            if(input->RCEnable)
            {
                if (input->intra_period == 0)
                {
                    n = input->no_frames + (input->no_frames - 1) * input->successive_Bframe;

                    /* number of P frames */
                    np = input->no_frames-1; 

                    /* number of B frames */
                    nb = (input->no_frames - 1) * input->successive_Bframe;
                }else
                {
                    N = input->intra_period*(input->successive_Bframe+1);
                    M = input->successive_Bframe+1;
                    n = (img->number==0) ? N - ( M - 1) : N;

                    /* last GOP may contain less frames */
                    if(img->number/input->intra_period >= input->no_frames / input->intra_period)
                    {
                        if (img->number != 0)
                            n = (input->no_frames - img->number) + (input->no_frames - img->number - 1) * input->successive_Bframe + input->successive_Bframe;
                        else
                            n = input->no_frames  + (input->no_frames - 1) * input->successive_Bframe;
                    }

                    /* number of P frames */
                    if (img->number == 0)
                        np = (n + 2 * (M - 1)) / M - 1; /* first GOP */
                    else
                        np = (n + (M - 1)) / M - 1;

                    /* number of B frames */
                    nb = n - np - 1;
                }
                rc_init_GOP(np,nb);
            }
        }

        encode_one_frame(); // encode one I- or P-frame

        img->nb_references += 1;
        img->nb_references = min(img->nb_references, 2);// Tian Dong. June 7, 2002

        if ((input->successive_Bframe != 0) && (IMG_NUMBER > 0)) // B-frame(s) to encode
        {
            img->type = B_IMG;            // set image type to B-frame
            picture_coding_type = 1;
            img->types = INTER_IMG;

            //cjw for weighted prediction
            img->buf_cycle = tmp_buf_cycle;

            img->frame_num++;                 //increment frame_num once for B-frames
            img->frame_num %= (1 << (LOG2_MAX_FRAME_NUM_MINUS4 + 4));

            for(img->b_frame_to_code=1; img->b_frame_to_code<=input->successive_Bframe; img->b_frame_to_code++)
            {
                //    picture_distance = (IMG_NUMBER - 1) * (input->successive_Bframe + 1) + img->b_frame_to_code;  // Tsinghua 200701 //commented Hisilicon XiaoZhen Zheng 20070327
                encode_one_frame();  // encode one B-frame
            }
        }
    }

    // terminate sequence
    img->EncodeEnd_flag=1;    //Carmen, 2007/12/19, Bug Fix: No sequence end codes when stream has multiple sequences
    terminate_sequence();

    fclose(p_in);

    if (p_dec)
        fclose(p_dec);
    if (p_trace)
        fclose(p_trace);

    Clear_Motion_Search_Module ();

    // free structure for rd-opt. mode decision
    clear_rdopt ();

    // report everything
    report();

    free_picture (frame_pic);

    free_global_buffers();

    // free image mem
    free_img ();
#ifdef _OUTPUT_MB_QSTEP_BITS_
    fprintf(pf_mb_qstep_bits, "qstep:%7.2f\t %8d\t%8d\n", 1.0/(QP2Qstep(img->qp)), glb_mb_coff_bits[INTER_IMG], glb_mb_coff_bits[INTRA_IMG]);
    fclose(pf_mb_qstep_bits);
#endif
    return 0;
}

/*
*************************************************************************
* Function:Initializes the Image structure with appropriate parameters.
* Input:Input Parameters struct inp_par *inp
* Output:Image Parameters struct img_par *img
* Return: 
* Attention:
*************************************************************************
*/

void init_img()
{
    int i,j;
    float FrameRate[8] = {{24000/1001}, {24}, {25}, {30000/1001}, {30}, {50}, {60000/1001}, {60}};


    img->no_multpred=input->no_multpred;
    img->buf_cycle = input->no_multpred;

    img->lindex=0;
    img->max_lindex=0;
    img->width    = (input->img_width+img->auto_crop_right);  //add by wuzhongmou 0610
    img->height   = (input->img_height+img->auto_crop_bottom); //add by wuzhongmou 0610
    img->width_cr =img->width/2;                                
    img->height_cr= img->height/(input->chroma_format==2?1:2);//add by wuzhongmou 0610, X ZHENG 422

    img->framerate = (int)FrameRate[input->frame_rate_code-1];

    //added by X ZHENG, 20080515
    if (input->slice_row_nr==0)
        input->slice_row_nr=img->height/16;

    //img->framerate=INIT_FRAME_RATE;   // The basic frame rate (of the original sequence)
    if(input->InterlaceCodingOption != FRAME_CODING) 
        img->buf_cycle *= 2;

    get_mem_mv (&(img->mv));
    get_mem_mv (&(img->p_fwMV));
    get_mem_mv (&(img->p_bwMV));
    get_mem_mv (&(img->all_mv));
    get_mem_mv (&(img->all_bmv));

    get_mem_ACcoeff (&(img->cofAC));
    get_mem_DCcoeff (&(img->cofDC));
    get_mem_mv (&(img->omv));
    get_mem_mv (&(img->all_omv));
    get_mem_mv (&(img->omv_fld));
    get_mem_mv (&(img->all_omv_fld));

    /*Lou Start*/
    get_mem4Dint(&(img->chromacofAC),2,4,2,17);
    /*Lou End*/

    /*lgp*/
    if(input->InterlaceCodingOption != FRAME_CODING) 
        img->buf_cycle /= 2;

    if ((img->MB_SyntaxElements = (SyntaxElement*)calloc (((img->height*img->width/256)+1200), sizeof(SyntaxElement))) == NULL)
        no_mem_exit ("init_img: MB_SyntaxElements");     

    if ((img->quad = (int*)calloc (511, sizeof(int))) == NULL)
        no_mem_exit ("init_img: img->quad");

    img->quad+=255;

    for (i=0; i < 256; ++i) // fix from TML1 / TML2 sw, truncation removed
    {
        img->quad[i]=img->quad[-i]=i*i;
    }

    if(((img->mb_data) = (Macroblock *) calloc((img->width/MB_BLOCK_SIZE) * (img->height/MB_BLOCK_SIZE),sizeof(Macroblock))) == NULL)
        no_mem_exit("init_img: img->mb_data");


    for (i=0; i < (img->width/MB_BLOCK_SIZE) * (img->height/MB_BLOCK_SIZE); i++)
    {  
        get_mem4Dint (&(img->mb_data[i].cofAC),6,4,2,65);/*lgp*dct*modify*/
        get_mem4Dint(&(img->mb_data[i].chromacofAC),2,4,2,17);

    }

    for (i=0; i < (img->width/MB_BLOCK_SIZE) * (img->height/MB_BLOCK_SIZE); i++)
    {
        img->mb_data[i].slice_nr = 0;
    }

    // allocate memory for intra pred mode buffer for each block: img->ipredmode
    get_mem2Dint(&(img->ipredmode), img->width/B8_SIZE+100, img->height/B8_SIZE+100);        //need two extra rows at right and bottom

    // Prediction mode is set to -1 outside the frame, indicating that no prediction can be made from this part
    for (i=0; i < img->width/(B8_SIZE)+100; i++)
    {
        for (j=0; j < img->height/(B8_SIZE)+100; j++)
        {
            img->ipredmode[i][j]=-1;
        }
    }

}

/*
*************************************************************************
* Function:Free the Image structures
* Input:Image Parameters struct img_par *img
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void free_img ()
{
    if(input->InterlaceCodingOption != FRAME_CODING) 
        img->buf_cycle *= 2;

    free_mem_mv (img->mv);
    free_mem_mv (img->p_fwMV);
    free_mem_mv (img->p_bwMV);
    free_mem_mv (img->all_mv);
    free_mem_mv (img->all_bmv);
    free_mem_mv (img->omv);
    free_mem_mv (img->all_omv);
    free_mem_mv (img->omv_fld);
    free_mem_mv (img->all_omv_fld);

    if(input->InterlaceCodingOption != FRAME_CODING) 
        img->buf_cycle /= 2;

    /*Lou Start*/
    free_mem4Dint(img->chromacofAC, 2, 4 );
    /*Lou End*/

    free_mem_ACcoeff (img->cofAC);
    free_mem_DCcoeff (img->cofDC);
    free (img->quad-255);
    free(img->MB_SyntaxElements);  //by oliver 0612
}

/*
*************************************************************************
* Function:Allocates the picture structure along with its dependent
data structures
* Input:
* Output:
* Return: Pointer to a Picture
* Attention:
*************************************************************************
*/

Picture *malloc_picture()
{
    Picture *pic;

    if ((pic = calloc (1, sizeof (Picture))) == NULL)
        no_mem_exit ("malloc_picture: Picture structure");

    return pic;
}

/*
*************************************************************************
* Function:Frees a picture
* Input:pic: POinter to a Picture to be freed
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void free_picture(Picture *pic)
{
    if (pic != NULL)
    {
        free (pic);
    }

}
/*
*************************************************************************
* Function:Reports the gathered information to appropriate outputs
* Input:  struct inp_par *inp,                                            \n
struct img_par *img,                                            \n
struct stat_par *stat,                                          \n
struct stat_par *stat        
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void report()
{
    int64 bit_use[2][2] ; //[intra/inter][frameno/bitrate]
    int i,j;
    char name[20];
    int64 bit_use_Bframe=0;
    int64 total_bits;
    float frame_rate;
    float mean_motion_info_bit_use[2];

#ifndef WIN32
    time_t now;
    struct tm *l_time;
    char string[1000];
#else
    char timebuf[128];
#endif

    bit_use[0][0]=1;
    bit_use[1][0]=max(1,input->no_frames-1);
    total_enc_frame[3] = total_enc_frame[INTRA_IMG] + total_enc_frame[INTER_IMG] + total_enc_frame[B_IMG];
    //  Accumulate bit usage for inter and intra frames
    bit_use[0][1]=bit_use[1][1]=0;

    for (i=0; i < 11; i++)
        bit_use[1][1] += stats.bit_use_mode_inter[0][i];

    for (j=0;j<2;j++)
    {
        bit_use[j][1]+=stats.bit_use_header[j];
        bit_use[j][1]+=stats.bit_use_mb_type[j];
        bit_use[j][1]+=stats.tmp_bit_use_cbp[j];
        bit_use[j][1]+=stats.bit_use_coeffY[j];
        bit_use[j][1]+=stats.bit_use_coeffC[j];
        bit_use[j][1]+=stats.bit_use_delta_quant[j];
        bit_use[j][1]+=stats.bit_use_stuffingBits[j];
    }

    // B pictures
    if(Bframe_ctr!=0)
    {
        bit_use_Bframe=0;
        for(i=0; i<11; i++)
            bit_use_Bframe += stats.bit_use_mode_inter[1][i]; 
        bit_use_Bframe += stats.bit_use_header[2];
        bit_use_Bframe += stats.bit_use_mb_type[2];
        bit_use_Bframe += stats.tmp_bit_use_cbp[2];
        bit_use_Bframe += stats.bit_use_coeffY[2];
        bit_use_Bframe += stats.bit_use_coeffC[2];
        bit_use_Bframe += stats.bit_use_delta_quant[2];
        bit_use_Bframe +=stats.bit_use_stuffingBits[2];

        stats.bitrate_P=(stats.bit_ctr_0+stats.bit_ctr_P)/(float)(total_enc_frame[INTER_IMG]);
        stats.bitrate_B=(stats.bit_ctr_B)/((float)total_enc_frame[B_IMG]);
    }
    else
    {
        if (input->no_frames > 1)
        {
            stats.bitrate=(bit_use[0][1]+bit_use[1][1])*(float)img->framerate/(total_enc_frame[3]);
        }
    }

    fprintf(stdout,"-----------------------------------------------------------------------------\n");
    fprintf(stdout,   " Freq. for encoded bitstream       : %1.0f\n",(float)img->framerate/(float)(input->jumpd+1));
    if(input->hadamard)
        fprintf(stdout," Hadamard transform                : Used\n");
    else
        fprintf(stdout," Hadamard transform                : Not used\n");

    fprintf(stdout," Image (Encoding) format                      : %dx%d\n",img->width,img->height); //add by wuzhongmou 0610
    fprintf(stdout," Image (Recon) format                      : %dx%d\n",(img->width-img->auto_crop_right),(img->height-img->auto_crop_bottom)); 

    if(input->intra_upd)
        fprintf(stdout," Error robustness                  : On\n");
    else
        fprintf(stdout," Error robustness                  : Off\n");
    fprintf(stdout,    " Search range                      : %d\n",input->search_range);


    fprintf(stdout,   " No of ref. frames used in P pred  : %d\n",input->no_multpred);
    if(input->successive_Bframe != 0)
        fprintf(stdout,   " No of ref. frames used in B pred  : %d\n",input->no_multpred);

    fprintf(stdout,   " Total encoding time for the seq.  : %.3f sec \n",tot_time*0.001);

    // B pictures
    fprintf(stdout, " Sequence type                     :" );

    if(input->successive_Bframe==1)   fprintf(stdout, " IBPBP (QP: I %d, P %d, B %d) \n",
        input->qp0, input->qpN, input->qpB);
    else if(input->successive_Bframe==2) fprintf(stdout, " IBBPBBP (QP: I %d, P %d, B %d) \n",
        input->qp0, input->qpN, input->qpB);
    else if(input->successive_Bframe==0) fprintf(stdout, " IPPP (QP: I %d, P %d) \n",   input->qp0, input->qpN);

    // report on entropy coding  method
    fprintf(stdout," Entropy coding method             : VLC\n");

    if (input->rdopt)
        fprintf(stdout," RD-optimized mode decision        : used\n");
    else
        fprintf(stdout," RD-optimized mode decision        : not used\n");

    fprintf(stdout,"------------------ Average data all frames  ---------------------------------\n");
    fprintf(stdout," SNR Y(dB)                         : %5.2f\n",snr->snr_ya);
    fprintf(stdout," SNR U(dB)                         : %5.2f\n",snr->snr_ua);
    fprintf(stdout," SNR V(dB)                         : %5.2f\n",snr->snr_va);

    if(Bframe_ctr!=0)
    {
        fprintf(stdout, " Total bits                        : %lld (I %lld, P %lld, B %lld) \n",
            total_bits=stats.bit_ctr_P + stats.bit_ctr_0 + stats.bit_ctr_B, stats.bit_ctr_0, stats.bit_ctr_P, stats.bit_ctr_B);

        frame_rate = (float)(img->framerate *(input->successive_Bframe + 1)) / (float) (input->jumpd+1);
        stats.bitrate= ((float) total_bits * frame_rate)/((float) (total_enc_frame[3]));

        fprintf(stdout, " Bit rate (kbit/s)  @ %2.2f Hz     : %5.2f\n", frame_rate, stats.bitrate/1000);
    }
    else
    {
        fprintf(stdout, " Total bits                        : %lld (I %lld, P %lld) \n",
            total_bits=stats.bit_ctr_P + stats.bit_ctr_0 , stats.bit_ctr_0, stats.bit_ctr_P);

        frame_rate = (float)img->framerate / ( (float) (input->jumpd + 1) );
        stats.bitrate= ((float) total_bits * frame_rate)/((float) (total_enc_frame[3]) );

        fprintf(stdout, " Bit rate (kbit/s)  @ %2.2f Hz     : %5.2f\n", frame_rate, stats.bitrate/1000);
    }


    fprintf(stdout, " Bits to avoid Startcode Emulation : %lld \n", stats.bit_ctr_emulationprevention);
    fprintf(stdout, " Bits for parameter sets           : %lld \n", stats.bit_ctr_parametersets);
    fprintf(stdout,"-----------------------------------------------------------------------------\n");
    fprintf(stdout,"Exit RM %s encoder ver %s ", RM, VERSION);
    fprintf(stdout,"\n");

    // status file
    if ((p_stat=fopen("stat.dat","w"))==0)
    {
        snprintf(errortext, ET_SIZE, "Error open file %s", "stat.dat");
        error(errortext, 500);
    }

    fprintf(p_stat,"\n ------------------ Average data all frames  ------------------------------\n");
    fprintf(p_stat," SNR Y(dB)                         : %5.2f\n",snr->snr_ya);
    fprintf(p_stat," SNR U(dB)                         : %5.2f\n",snr->snr_ua);
    fprintf(p_stat," SNR V(dB)                         : %5.2f\n",snr->snr_va);
    fprintf(p_stat, " Total bits                        : %d (I %5d, P BS %5d, B %d) \n",
        total_bits=stats.bit_ctr_P + stats.bit_ctr_0 + stats.bit_ctr_B, stats.bit_ctr_0, stats.bit_ctr_P, stats.bit_ctr_B);

    fprintf(p_stat, " Bit rate (kbit/s)  @ %2.2f Hz     : %5.2f\n", frame_rate, stats.bitrate/1000);
    fprintf(p_stat," -------------------------------------------------------------- \n");
    fprintf(p_stat,"  This file contains statistics for the last encoded sequence   \n");
    fprintf(p_stat," -------------------------------------------------------------- \n");
    fprintf(p_stat,   " Sequence                     : %s\n",input->infile);
    fprintf(p_stat,   " No.of coded pictures         : %4d\n",total_enc_frame[3]);
    fprintf(p_stat,   " Freq. for encoded bitstream  : %4.0f\n",frame_rate);

    // B pictures
    if(input->successive_Bframe != 0)
    {
        fprintf(p_stat,   " BaseLayer Bitrate(kb/s)      : %6.2f\n", stats.bitrate_P/1000);
        fprintf(p_stat,   " EnhancedLyaer Bitrate(kb/s)  : %6.2f\n", stats.bitrate_B/1000);
    }
    else
        fprintf(p_stat,   " Bitrate(kb/s)                : %6.2f\n", stats.bitrate/1000);

    if(input->hadamard)
        fprintf(p_stat," Hadamard transform           : Used\n");
    else
        fprintf(p_stat," Hadamard transform           : Not used\n");

    fprintf(p_stat,  " Image format                 : %dx%d\n",img->width,img->height); //add by wuzhongmou 0610

    if(input->intra_upd)
        fprintf(p_stat," Error robustness             : On\n");
    else
        fprintf(p_stat," Error robustness             : Off\n");

    fprintf(p_stat,  " Search range                 : %d\n",input->search_range);


    fprintf(p_stat,   " No of frame used in P pred   : %d\n",input->no_multpred);
    if(input->successive_Bframe != 0)
        fprintf(p_stat, " No of frame used in B pred   : %d\n",input->no_multpred);

    fprintf(p_stat,   " Entropy coding method        : VLC\n");

    fprintf(p_stat," Search range restrictions    : none\n");

    if (input->rdopt)
        fprintf(p_stat," RD-optimized mode decision   : used\n");
    else
        fprintf(p_stat," RD-optimized mode decision   : not used\n");

    fprintf(p_stat," -------------------|---------------|---------------|\n");
    fprintf(p_stat,"     Item           |     Intra     |   All frames  |\n");
    fprintf(p_stat," -------------------|---------------|---------------|\n");
    fprintf(p_stat," SNR Y(dB)          |");
    fprintf(p_stat," %5.2f         |",snr->snr_y1);
    fprintf(p_stat," %5.2f         |\n",snr->snr_ya);
    fprintf(p_stat," SNR U/V (dB)       |");
    fprintf(p_stat," %5.2f/%5.2f   |",snr->snr_u1,snr->snr_v1);
    fprintf(p_stat," %5.2f/%5.2f   |\n",snr->snr_ua,snr->snr_va);

    // QUANT.
    fprintf(p_stat," Average quant      |");
    fprintf(p_stat," %5d         |",absm(input->qp0));
    fprintf(p_stat," %5.2f         |\n",(float)stats.quant1/max(1.0,(float)stats.quant0));

    // MODE
    fprintf(p_stat,"\n -------------------|---------------|\n");
    fprintf(p_stat,"   Intra            |   Mode used   |\n");
    fprintf(p_stat," -------------------|---------------|\n");
    fprintf(p_stat," Mode 0  intra old  | %5d         |\n",stats.mode_use_intra[I4MB]);
    fprintf(p_stat,"\n -------------------|---------------|-----------------|\n");
    fprintf(p_stat,"   Inter            |   Mode used   | MotionInfo bits |\n");
    fprintf(p_stat," -------------------|---------------|-----------------|");
    fprintf(p_stat,"\n Mode  0  (copy)    | %5d         |    %8.2f     |",stats.mode_use_inter[0][0   ],(float)stats.bit_use_mode_inter[0][0   ]/(float)bit_use[1][0]);
    fprintf(p_stat,"\n Mode  1  (16x16)   | %5d         |    %8.2f     |",stats.mode_use_inter[0][1   ],(float)stats.bit_use_mode_inter[0][1   ]/(float)bit_use[1][0]);
    fprintf(p_stat,"\n Mode  2  (16x8)    | %5d         |    %8.2f     |",stats.mode_use_inter[0][2   ],(float)stats.bit_use_mode_inter[0][2   ]/(float)bit_use[1][0]);
    fprintf(p_stat,"\n Mode  3  (8x16)    | %5d         |    %8.2f     |",stats.mode_use_inter[0][3   ],(float)stats.bit_use_mode_inter[0][3   ]/(float)bit_use[1][0]);
    fprintf(p_stat,"\n Mode  4  (8x8)     | %5d         |    %8.2f     |",stats.mode_use_inter[0][P8x8],(float)stats.bit_use_mode_inter[0][P8x8]/(float)bit_use[1][0]);
    fprintf(p_stat,"\n Mode  5  intra old | %5d         |-----------------|",stats.mode_use_inter[0][I4MB]);
    mean_motion_info_bit_use[0] = (float)(stats.bit_use_mode_inter[0][0] + stats.bit_use_mode_inter[0][1] + stats.bit_use_mode_inter[0][2] 
    + stats.bit_use_mode_inter[0][3] + stats.bit_use_mode_inter[0][P8x8])/(float) bit_use[1][0]; 

    // MODE
    fprintf(p_stat,"\n -------------------|---------------|\n");
    fprintf(p_stat," MB type            | Number used   |\n");
    fprintf(p_stat," -------------------|---------------|");
    fprintf(p_stat,"\n NS MB              | %5d         |",stats.mb_use_mode[0]);
    fprintf(p_stat,"\n VS MB              | %5d         |",stats.mb_use_mode[1]);

    // B pictures
    if(input->successive_Bframe!=0 && Bframe_ctr!=0)
    {
        fprintf(p_stat,"\n\n -------------------|---------------|-----------------|\n");
        fprintf(p_stat,"   B frame          |   Mode used   | MotionInfo bits |\n");
        fprintf(p_stat," -------------------|---------------|-----------------|");
        fprintf(p_stat,"\n Mode  0  (copy)    | %5d         |    %8.2f     |",stats.mode_use_inter[1][0   ],(float)stats.bit_use_mode_inter[1][0   ]/(float)Bframe_ctr);
        fprintf(p_stat,"\n Mode  1  (16x16)   | %5d         |    %8.2f     |",stats.mode_use_inter[1][1   ],(float)stats.bit_use_mode_inter[1][1   ]/(float)Bframe_ctr);
        fprintf(p_stat,"\n Mode  2  (16x8)    | %5d         |    %8.2f     |",stats.mode_use_inter[1][2   ],(float)stats.bit_use_mode_inter[1][2   ]/(float)Bframe_ctr);
        fprintf(p_stat,"\n Mode  3  (8x16)    | %5d         |    %8.2f     |",stats.mode_use_inter[1][3   ],(float)stats.bit_use_mode_inter[1][3   ]/(float)Bframe_ctr);
        fprintf(p_stat,"\n Mode  4  (8x8)     | %5d         |    %8.2f     |",stats.mode_use_inter[1][P8x8],(float)stats.bit_use_mode_inter[1][P8x8]/(float)Bframe_ctr);
        fprintf(p_stat,"\n Mode  5  intra old | %5d         |-----------------|",stats.mode_use_inter[1][I4MB]);
        mean_motion_info_bit_use[1] = (float)(stats.bit_use_mode_inter[1][0] + stats.bit_use_mode_inter[1][1] + stats.bit_use_mode_inter[1][2] 
        + stats.bit_use_mode_inter[1][3] + stats.bit_use_mode_inter[1][P8x8])/(float) Bframe_ctr; 
    }

    fprintf(p_stat,"\n\n --------------------|----------------|----------------|----------------|\n");
    fprintf(p_stat,"  Bit usage:         |      Intra     |      Inter     |    B frame     |\n");
    fprintf(p_stat," --------------------|----------------|----------------|----------------|\n");
    fprintf(p_stat," Header              |");
    fprintf(p_stat," %10.2f     |",(float) stats.bit_use_header[0]/bit_use[0][0]);
    fprintf(p_stat," %10.2f     |",(float) stats.bit_use_header[1]/bit_use[1][0]);

    if(input->successive_Bframe!=0 && Bframe_ctr!=0)
        fprintf(p_stat," %10.2f     |",(float) stats.bit_use_header[2]/Bframe_ctr);
    else fprintf(p_stat," %10.2f     |", 0.);

    fprintf(p_stat,"\n");
    fprintf(p_stat," Mode                |");
    fprintf(p_stat," %10.2f     |",(float)stats.bit_use_mb_type[0]/bit_use[0][0]);
    fprintf(p_stat," %10.2f     |",(float)stats.bit_use_mb_type[1]/bit_use[1][0]);

    if(input->successive_Bframe!=0 && Bframe_ctr!=0)
        fprintf(p_stat," %10.2f     |",(float)stats.bit_use_mb_type[2]/Bframe_ctr);
    else 
        fprintf(p_stat," %10.2f     |", 0.);

    fprintf(p_stat,"\n");
    fprintf(p_stat," Motion Info         |");
    fprintf(p_stat,"        ./.     |");
    fprintf(p_stat," %10.2f     |",mean_motion_info_bit_use[0]);

    if(input->successive_Bframe!=0 && Bframe_ctr!=0)
        fprintf(p_stat," %10.2f     |",mean_motion_info_bit_use[1]);
    else 
        fprintf(p_stat," %10.2f     |", 0.);

    fprintf(p_stat,"\n");
    fprintf(p_stat," CBP Y/C             |");

    for (j=0; j < 2; j++)
    {
        fprintf(p_stat," %10.2f     |", (float)stats.tmp_bit_use_cbp[j]/bit_use[j][0]);
    }

    if(input->successive_Bframe!=0 && Bframe_ctr!=0)
        fprintf(p_stat," %10.2f     |", (float)stats.tmp_bit_use_cbp[2]/Bframe_ctr);
    else 
        fprintf(p_stat," %10.2f     |", 0.);

    fprintf(p_stat,"\n");

    if(input->successive_Bframe!=0 && Bframe_ctr!=0)
        fprintf(p_stat," Coeffs. Y           | %10.2f     | %10.2f     | %10.2f     |\n",
        (float)stats.bit_use_coeffY[0]/bit_use[0][0], (float)stats.bit_use_coeffY[1]/bit_use[1][0], (float)stats.bit_use_coeffY[2]/Bframe_ctr);
    else
        fprintf(p_stat," Coeffs. Y           | %10.2f     | %10.2f     | %10.2f     |\n",
        (float)stats.bit_use_coeffY[0]/bit_use[0][0], (float)stats.bit_use_coeffY[1]/(float)bit_use[1][0], 0.);

    if(input->successive_Bframe!=0 && Bframe_ctr!=0)
        fprintf(p_stat," Coeffs. C           | %10.2f     | %10.2f     | %10.2f     |\n",
        (float)stats.bit_use_coeffC[0]/bit_use[0][0], (float)stats.bit_use_coeffC[1]/bit_use[1][0], (float)stats.bit_use_coeffC[2]/Bframe_ctr);
    else
        fprintf(p_stat," Coeffs. C           | %10.2f     | %10.2f     | %10.2f     |\n",
        (float)stats.bit_use_coeffC[0]/bit_use[0][0], (float)stats.bit_use_coeffC[1]/bit_use[1][0], 0.);

    if(input->successive_Bframe!=0 && Bframe_ctr!=0)
        fprintf(p_stat," Delta quant         | %10.2f     | %10.2f     | %10.2f     |\n",
        (float)stats.bit_use_delta_quant[0]/bit_use[0][0], (float)stats.bit_use_delta_quant[1]/bit_use[1][0], (float)stats.bit_use_delta_quant[2]/Bframe_ctr);
    else
        fprintf(p_stat," Delta quant         | %10.2f     | %10.2f     | %10.2f     |\n",
        (float)stats.bit_use_delta_quant[0]/bit_use[0][0], (float)stats.bit_use_delta_quant[1]/bit_use[1][0], 0.);

    if(input->successive_Bframe!=0 && Bframe_ctr!=0)
        fprintf(p_stat," Stuffing Bits       | %10.2f     | %10.2f     | %10.2f     |\n",
        (float)stats.bit_use_stuffingBits[0]/bit_use[0][0], (float)stats.bit_use_stuffingBits[1]/bit_use[1][0], (float)stats.bit_use_stuffingBits[2]/Bframe_ctr);
    else
        fprintf(p_stat," Stuffing Bits       | %10.2f     | %10.2f     | %10.2f     |\n",
        (float)stats.bit_use_stuffingBits[0]/bit_use[0][0], (float)stats.bit_use_stuffingBits[1]/bit_use[1][0], 0.);

    fprintf(p_stat," --------------------|----------------|----------------|----------------|\n");
    fprintf(p_stat," average bits/frame  |");

    for (i=0; i < 2; i++)
    {
        fprintf(p_stat," %10.2f     |", (float) bit_use[i][1]/(float) bit_use[i][0] );
    }

    if(input->successive_Bframe!=0 && Bframe_ctr!=0)
        fprintf(p_stat," %10.2f     |", (float) bit_use_Bframe/ (float) Bframe_ctr );
    else 
        fprintf(p_stat," %10.2f     |", 0.);

    fprintf(p_stat,"\n");
    fprintf(p_stat," --------------------|----------------|----------------|----------------|\n");

    fclose(p_stat);

    // write to log file
    if ((p_log=fopen("log.dat","r"))==0)                      // check if file exist
    {
        if ((p_log=fopen("log.dat","a"))==NULL)            // append new statistic at the end
        {
            snprintf(errortext, ET_SIZE, "Error open file %s  \n","log.dat");
            error(errortext, 500);
        }
        else                                            // Create header for new log file
        {
            fprintf(p_log," --------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
            fprintf(p_log,"|                    Encoder statistics. This file is generated during first encoding session, new sessions will be appended                                               |\n");
            fprintf(p_log," ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
            fprintf(p_log,"| Date  | Time  |    Sequence        |#Img|Quant1|QuantN|Format|Hadamard|Search r|#Ref | Freq |Intra upd|SNRY 1|SNRU 1|SNRV 1|SNRY N|SNRU N|SNRV N|#Bitr P|#Bitr B|#BitRate|\n");
            fprintf(p_log," ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
        }
    }
    else
    {
        fclose (p_log);
        if ((p_log=fopen("log.dat","a"))==NULL)            // File exist,just open for appending
        {
            snprintf(errortext, ET_SIZE, "Error open file %s  \n","log.dat");
            error(errortext, 500);
        }
    }

#ifdef WIN32
    _strdate( timebuf );
    fprintf(p_log,"| %1.5s |",timebuf );

    _strtime( timebuf);
    fprintf(p_log," % 1.5s |",timebuf);
#else
    now = time ((time_t *) NULL); // Get the system time and put it into 'now' as 'calender time'
    time (&now);
    l_time = localtime (&now);
    strftime (string, sizeof string, "%d-%b-%Y", l_time);
    fprintf(p_log,"| %1.5s |",string );

    strftime (string, sizeof string, "%H:%M:%S", l_time);
    fprintf(p_log," %1.5s |",string );
#endif

    for (i=0;i<20;i++)
        name[i]=input->infile[i+max(0,strlen(input->infile)-20)]; // write last part of path, max 20 chars

    fprintf(p_log,"%20.20s|",name);
    fprintf(p_log,"%3d |",input->no_frames);
    fprintf(p_log,"  %2d  |",input->qp0);
    fprintf(p_log,"  %2d  |",input->qpN);
    fprintf(p_log,"%dx%d|",img->width,img->height);  //add by wuzhongmou 0610

    if (input->hadamard==1)
        fprintf(p_log,"   ON   |");
    else
        fprintf(p_log,"   OFF  |");

    fprintf(p_log,"   %2d   |",input->search_range );
    fprintf(p_log," %2d  |",input->no_multpred);
    fprintf(p_log," %2d  |",img->framerate/(input->jumpd+1));

    if (input->intra_upd==1)
        fprintf(p_log,"   ON    |");
    else
        fprintf(p_log,"   OFF   |");

    fprintf(p_log,"%5.3f|",snr->snr_y1);
    fprintf(p_log,"%5.3f|",snr->snr_u1);
    fprintf(p_log,"%5.3f|",snr->snr_v1);
    fprintf(p_log,"%5.3f|",snr->snr_ya);
    fprintf(p_log,"%5.3f|",snr->snr_ua);
    fprintf(p_log,"%5.3f|",snr->snr_va);

    fprintf(p_log,"%7.0f|",stats.bitrate_P);
    fprintf(p_log,"%7.0f|",stats.bitrate_B);
    fprintf(p_log,"%7.2f|",stats.bitrate/1000);
    fprintf(p_log, "\n");
    fclose(p_log);

}


/*
*************************************************************************
* Function:Prints the header of the protocol.
* Input:struct inp_par *inp
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void information_init()
{
    printf("-----------------------------------------------------------------------------\n");
    printf(" Input YUV file                    : %s \n",input->infile);
    printf(" Output AVS bitstream              : %s \n",input->outfile);
    if (p_dec != NULL)
        printf(" Output YUV file                   : %s \n",input->ReconFile);
    printf(" Output log file                   : log.dat \n");
    printf(" Output statistics file            : stat.dat \n");
    printf("-----------------------------------------------------------------------------\n");
    printf(" Frame   Bit/pic   QP   SnrY    SnrU    SnrV    Time(ms)  FRM/FLD  IntraMBs\n");
}

/*
*************************************************************************
* Function:Dynamic memory allocation of frame size related global buffers
buffers are defined in global.h, allocated memory must be freed in
void free_global_buffers()
* Input:  Input Parameters struct inp_par *inp,                            \n
Image Parameters struct img_par *img
* Output:
* Return: Number of allocated bytes
* Attention:
*************************************************************************
*/


int init_global_buffers()
{
    int memory_size=0;
    int height_field = img->height/2;
    int refnum;
    int i;

    if(input->chroma_format==2) //wzm,422
    {
        imgY_org_buffer = (byte*)malloc(input->img_height*input->img_width*2);
        memory_size += input->img_height*input->img_width*2;
    }
    else if(input->chroma_format==1) //wzm,422
    {
        imgY_org_buffer = (byte*)malloc(input->img_height*input->img_width*3/2); //add by wuzhongmou 0610
        memory_size += input->img_height*input->img_width*3/2;  //add by wuzhongmou 0610
    }

    // allocate memory for reference frame buffers: imgY_org, imgUV_org
    // byte imgY_org[288][352];
    // byte imgUV_org[2][144][176];
    memory_size += get_mem2D(&imgY_org_frm, img->height, img->width);
    memory_size += get_mem3D(&imgUV_org_frm, 2, img->height_cr, img->width_cr);

    // allocate memory for temp P and B-frame motion vector buffer: tmp_mv, temp_mv_block
    // int tmp_mv[2][72][92];  ([2][72][88] should be enough)
    memory_size += get_mem3Dint(&tmp_mv_frm, 2, img->height/BLOCK_SIZE, img->width/BLOCK_SIZE+4);

    // allocate memory for reference frames of each block: refFrArr
    // int  refFrArr[72][88];
    memory_size += get_mem2Dint(&refFrArr_frm, img->height/BLOCK_SIZE, img->width/BLOCK_SIZE);

    if(input->successive_Bframe!=0)
    {    
        // allocate memory for temp B-frame motion vector buffer: fw_refFrArr, bw_refFrArr
        // int ...refFrArr[72][88];
        memory_size += get_mem2Dint(&fw_refFrArr_frm, img->height/BLOCK_SIZE, img->width/BLOCK_SIZE);
        memory_size += get_mem2Dint(&bw_refFrArr_frm, img->height/BLOCK_SIZE, img->width/BLOCK_SIZE);
    }

    // allocate memory for B frame coding: nextP_imgY, nextP_imgUV
    // byte nextP_imgY[288][352];
    // byte nextP_imgUV[2][144][176];
    memory_size += get_mem2D(&nextP_imgY, img->height, img->width);
    memory_size += get_mem3D(&nextP_imgUV, 2, img->height_cr, img->width_cr);

    if(input->successive_Bframe!=0)
    {
        // allocate memory for temp B-frame motion vector buffer: tmp_fwMV, tmp_bwMV, dfMV, dbMV
        // int ...MV[2][72][92];  ([2][72][88] should be enough)
        memory_size += get_mem3Dint(&tmp_fwMV, 2, img->height/BLOCK_SIZE, img->width/BLOCK_SIZE+4);
        memory_size += get_mem3Dint(&tmp_bwMV, 2, img->height/BLOCK_SIZE, img->width/BLOCK_SIZE+4);
        memory_size += get_mem3Dint(&dfMV,     2, img->height/BLOCK_SIZE, img->width/BLOCK_SIZE+4);
        memory_size += get_mem3Dint(&dbMV,     2, img->height/BLOCK_SIZE, img->width/BLOCK_SIZE+4);
    }

    // allocate memory for temp quarter pel luma frame buffer: img4Y_tmp
    // int img4Y_tmp[576][704];  (previously int imgY_tmp in global.h)
    //memory_size += get_mem2Dint(&img4Y_tmp, img->height+2*IMG_PAD_SIZE, (img->width+2*IMG_PAD_SIZE)*4);
    memory_size += get_mem2Dint(&img4Y_tmp, (img->height+2*IMG_PAD_SIZE)*4, (img->width+2*IMG_PAD_SIZE)*4);


    if (input->InterlaceCodingOption != FRAME_CODING)
    {
        // allocate memory for encoding frame buffers: imgY, imgUV
        // byte imgY[288][352];
        // byte imgUV[2][144][176];
        memory_size += get_mem2D(&imgY_com, img->height, img->width);
        memory_size += get_mem3D(&imgUV_com, 2, img->height_cr, img->width_cr); //X ZHENG,422 interlace

        memory_size += get_mem2D(&imgY_org_top, img->height/2, img->width);
        memory_size += get_mem3D(&imgUV_org_top, 2, img->height_cr/2, img->width_cr);

        memory_size += get_mem2D(&imgY_org_bot, img->height/2, img->width);
        memory_size += get_mem3D(&imgUV_org_bot, 2, img->height_cr/2, img->width_cr);


        // allocate memory for encoding frame buffers: imgY, imgUV
        // byte imgY[288][352];
        // byte imgUV[2][144][176];
        memory_size += get_mem2D(&imgY_top, height_field, img->width);
        memory_size += get_mem3D(&imgUV_top, 2, img->height_cr/2, img->width_cr); //X ZHENG,422 interlace
        memory_size += get_mem2D(&imgY_bot, height_field, img->width);
        memory_size += get_mem3D(&imgUV_bot, 2, img->height_cr/2, img->width_cr); //X ZHENG,422 interlace


        if(input->successive_Bframe!=0)
        {
            // allocate memory for temp B-frame motion vector buffer: fw_refFrArr, bw_refFrArr
            // int ...refFrArr[72][88];
            memory_size += get_mem2Dint(&fw_refFrArr_top, height_field/BLOCK_SIZE, img->width/BLOCK_SIZE);
            memory_size += get_mem2Dint(&bw_refFrArr_top, height_field/BLOCK_SIZE, img->width/BLOCK_SIZE);
            memory_size += get_mem2Dint(&fw_refFrArr_bot, height_field/BLOCK_SIZE, img->width/BLOCK_SIZE);
            memory_size += get_mem2Dint(&bw_refFrArr_bot, height_field/BLOCK_SIZE, img->width/BLOCK_SIZE);
        }

        // allocate memory for temp P and B-frame motion vector buffer: tmp_mv, temp_mv_block
        // int tmp_mv[2][72][92];  ([2][72][88] should be enough)
        memory_size += get_mem3Dint(&tmp_mv_top, 2, height_field/BLOCK_SIZE, img->width/BLOCK_SIZE+4);
        memory_size += get_mem3Dint(&tmp_mv_bot, 2, height_field/BLOCK_SIZE, img->width/BLOCK_SIZE+4);

        // allocate memory for reference frames of each block: refFrArr
        // int  refFrArr[72][88];
        memory_size += get_mem2Dint(&refFrArr_top, height_field/BLOCK_SIZE, img->width/BLOCK_SIZE);
        memory_size += get_mem2Dint(&refFrArr_bot, height_field/BLOCK_SIZE, img->width/BLOCK_SIZE);
    }

    //FAST MOTION ESTIMATION. ZHIBO CHEN 2003.3
#ifdef FastME
    memory_size += get_mem_FME();
#endif


    for(refnum=0; refnum<3; refnum++)
        for (i=0; i<3; i++)
        {
            if (i==0)
            {
                get_mem2D(&reference_frame[refnum][i],img->height,img->width);
            }else
            {
                get_mem2D(&reference_frame[refnum][i],img->height_cr,img->width_cr);
            }
        }

        //forward reference frame buffer
        ref_frm[0] = reference_frame[0]; //reference_frame[ref_index][yuv][height][width],ref_frm[ref_index][yuv][height][width]
        ref_frm[1] = reference_frame[1];
        current_frame = reference_frame[2];

        //allocate field buffer
        if(input->InterlaceCodingOption != FRAME_CODING)
        {
            for(refnum=0; refnum<6; refnum++)
                for (i=0; i<3; i++)
                {
                    if (i==0)
                    {
                        get_mem2D(&reference_field[refnum][i],img->height/2,img->width);
                    }else
                    {
                        get_mem2D(&reference_field[refnum][i],img->height_cr/2,img->width_cr);
                    }
                }

                //forward reference frame buffer
                for (i=0; i<4; i++)
                    ref_fld[i] = reference_field[i];
                current_field = reference_field[4];
                ref_fld[4] = reference_field[5];
        }

        // !! 
        allalpha_lum = (int*)malloc(((img->height*img->width)/256)*sizeof(int));
        allbelta_lum = (int*)malloc(((img->height*img->width)/256)*sizeof(int));

        return (memory_size);
}

/*
*************************************************************************
* Function:Free allocated memory of frame size related global buffers
buffers are defined in global.h, allocated memory is allocated in
int get_mem4global_buffers()
* Input: Input Parameters struct inp_par *inp,                             \n
Image Parameters struct img_par *img
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void free_global_buffers()
{
    int  i,j;

    free(imgY_org_buffer);
    free_mem2D(imgY_org_frm);
    free_mem3D(imgUV_org_frm,2);
    free_mem3Dint(tmp_mv_frm,2);
    free_mem2Dint(refFrArr_frm);

    // number of reference frames increased by one for next P-frame
    for(i=0;i<2;i++)
        free(mref_frm[i]);

    for(i=0;i<3;i++)
    {
        free(reference_frame[0][i]);
        free(reference_frame[1][i]);
        free(reference_frame[2][i]);
    }

    if (input->InterlaceCodingOption != FRAME_CODING)
    {
        for(j=0;j<6;j++)
            for(i=0;i<3;i++)
            {
                free(reference_field[j][i]);
            }
    }

    free_mem2D(nextP_imgY);
    free_mem3D(nextP_imgUV,2);

    // free multiple ref frame buffers
    // number of reference frames increased by one for next P-frame

    if(input->successive_Bframe!=0)
    {
        // free last P-frame buffers for B-frame coding
        free_mem3Dint(tmp_fwMV,2);
        free_mem3Dint(tmp_bwMV,2);
        free_mem3Dint(dfMV,2);
        free_mem3Dint(dbMV,2);
        free_mem2Dint(fw_refFrArr_frm);
        free_mem2Dint(bw_refFrArr_frm);    
    } // end if B frame

    free_mem2Dint(img4Y_tmp);    // free temp quarter pel frame buffer

    // free mem, allocated in init_img()
    // free intra pred mode buffer for blocks
    free_mem2Dint(img->ipredmode);  

    if (input->InterlaceCodingOption != FRAME_CODING)
    {
        free_mem2D(imgY_com);
        // free multiple ref frame buffers
        // number of reference frames increased by one for next P-frame
        for (i=0;i<4;i++)
            free(mref_fld[i]);
#if 0
        free(parity_fld);


        if(input->successive_Bframe!=0)
        {
            // free last P-frame buffers for B-frame coding
            free_mem2Dint(fw_refFrArr_fld);
            free_mem2Dint(bw_refFrArr_fld);
        } // end if B frame


        free (Refbuf11_fld);

        free_mem3Dint(tmp_mv_fld,2);
        free_mem2Dint(refFrArr_fld);
#endif    
    }

    /*  if (1)//input->InterlaceCodingOption == SMB_CODING)
    {    
    for (i=0; i < (img->width/MB_BLOCK_SIZE) * (img->height/MB_BLOCK_SIZE); i++)
    {  
    free_mem4Dint ((img->mb_data[i].cofAC),6,4);
    free_mem4Dint(img->mb_data[i].chromacofAC, 2, 4 );
    }
    }*/

    free(img->mb_data);

    // !! 
    free(allbelta_lum) ;
    free(allalpha_lum) ;

    //FAST MOTION ESTIMATION. ZHIBO CHEN 2003.3
#ifdef FastME
    free_mem_FME();
#endif
}

/*
*************************************************************************
* Function:Allocate memory for mv
* Input:Image Parameters struct img_par *img                             \n
int****** mv
* Output:
* Return: memory size in bytes
* Attention:
*************************************************************************
*/

int get_mem_mv (int****** mv)
{
    int i, j, k, l;

    if ((*mv = (int*****)calloc(2,sizeof(int****))) == NULL)
        no_mem_exit ("get_mem_mv: mv");
    for (i=0; i<2; i++)
    {
        if (((*mv)[i] = (int****)calloc(2,sizeof(int***))) == NULL)
            no_mem_exit ("get_mem_mv: mv");
        for (j=0; j<2; j++)
        {
            if (((*mv)[i][j] = (int***)calloc(img->buf_cycle,sizeof(int**))) == NULL)
                no_mem_exit ("get_mem_mv: mv");
            for (k=0; k<img->buf_cycle; k++)
            {
                if (((*mv)[i][j][k] = (int**)calloc(9,sizeof(int*))) == NULL)
                    no_mem_exit ("get_mem_mv: mv");
                for (l=0; l<9; l++)
                    if (((*mv)[i][j][k][l] = (int*)calloc(2,sizeof(int))) == NULL)
                        no_mem_exit ("get_mem_mv: mv");
            }
        }
    }
    return 2*2*img->buf_cycle*9*2*sizeof(int);
}


/*
*************************************************************************
* Function:Free memory from mv
* Input:int****** mv
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void free_mem_mv (int***** mv)
{
    int i, j, k, l;

    for (i=0; i<2; i++)
    {
        for (j=0; j<2; j++)
        {
            for (k=0; k<img->buf_cycle; k++)
            {
                for (l=0; l<9; l++)
                    free (mv[i][j][k][l]);
                free (mv[i][j][k]);
            }
            free (mv[i][j]);
        }
        free (mv[i]);
    }
    free (mv);
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

int get_direct_mv (int****** mv,int mb_x,int mb_y)
{
    int i, j, k, l;

    if ((*mv = (int*****)calloc(mb_y,sizeof(int****))) == NULL)
        no_mem_exit ("get_mem_mv: mv");
    for (i=0; i<mb_y; i++)
    {
        if (((*mv)[i] = (int****)calloc(mb_x,sizeof(int***))) == NULL)
            no_mem_exit ("get_mem_mv: mv");
        for (j=0; j<mb_x; j++)
        {
            if (((*mv)[i][j] = (int***)calloc(2,sizeof(int**))) == NULL)
                no_mem_exit ("get_mem_mv: mv");
            for (k=0; k<2; k++)
            {
                if (((*mv)[i][j][k] = (int**)calloc(2,sizeof(int*))) == NULL)
                    no_mem_exit ("get_mem_mv: mv");
                for (l=0; l<2; l++)
                    if (((*mv)[i][j][k][l] = (int*)calloc(3,sizeof(int))) == NULL)
                        no_mem_exit ("get_mem_mv: mv");
            }
        }
    }
    return mb_x*mb_y*2*2*3*sizeof(int);
}

/*
*************************************************************************
* Function:Free memory from mv
* Input:int****** mv
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void free_direct_mv (int***** mv,int mb_x,int mb_y)
{
    int i, j, k, l;

    for (i=0; i<mb_y; i++)
    {
        for (j=0; j<mb_x; j++)
        {
            for (k=0; k<2; k++)
            {
                for (l=0; l<2; l++)
                    free (mv[i][j][k][l]);
                free (mv[i][j][k]);
            }
            free (mv[i][j]);
        }
        free (mv[i]);
    }
    free (mv);
}



/*
*************************************************************************
* Function:Allocate memory for AC coefficients
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int get_mem_ACcoeff (int***** cofAC)
{
    int i, j, k;

    if ((*cofAC = (int****)calloc (8, sizeof(int***))) == NULL)              no_mem_exit ("get_mem_ACcoeff: cofAC");
    for (k=0; k<8; k++) //wzm,422
    {
        if (((*cofAC)[k] = (int***)calloc (4, sizeof(int**))) == NULL)         no_mem_exit ("get_mem_ACcoeff: cofAC");
        for (j=0; j<4; j++)
        {
            if (((*cofAC)[k][j] = (int**)calloc (2, sizeof(int*))) == NULL)      no_mem_exit ("get_mem_ACcoeff: cofAC");
            for (i=0; i<2; i++)
            {
                if (((*cofAC)[k][j][i] = (int*)calloc (65, sizeof(int))) == NULL)  no_mem_exit ("get_mem_ACcoeff: cofAC"); // 18->65 for AVS
            }
        }
    }
    return 8*4*2*65*sizeof(int);// 18->65 for AVS
}

/*
*************************************************************************
* Function:Allocate memory for DC coefficients
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int get_mem_DCcoeff (int**** cofDC)
{
    int j, k;

    if ((*cofDC = (int***)calloc (3, sizeof(int**))) == NULL)           no_mem_exit ("get_mem_DCcoeff: cofDC");
    for (k=0; k<3; k++)
    {
        if (((*cofDC)[k] = (int**)calloc (2, sizeof(int*))) == NULL)      no_mem_exit ("get_mem_DCcoeff: cofDC");
        for (j=0; j<2; j++)
        {
            if (((*cofDC)[k][j] = (int*)calloc (65, sizeof(int))) == NULL)  no_mem_exit ("get_mem_DCcoeff: cofDC"); // 18->65 for AVS
        }
    }
    return 3*2*65*sizeof(int); // 18->65 for AVS
}


/*
*************************************************************************
* Function:Free memory of AC coefficients
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void free_mem_ACcoeff (int**** cofAC)
{
    int i, j, k;

    for (k=0; k<8; k++)
    {
        for (i=0; i<4; i++)
        {
            for (j=0; j<2; j++)
            {
                free (cofAC[k][i][j]);
            }
            free (cofAC[k][i]);
        }
        free (cofAC[k]);
    }
    free (cofAC);
}

/*
*************************************************************************
* Function:Free memory of DC coefficients
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void free_mem_DCcoeff (int*** cofDC)
{
    int i, j;

    for (j=0; j<3; j++)
    {
        for (i=0; i<2; i++)
        {
            free (cofDC[j][i]);
        }
        free (cofDC[j]);
    }
    free (cofDC);
}


/*
*************************************************************************
* Function:SetImgType
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void SetImgType()
{
    if (input->intra_period == 0)
    {
        if (IMG_NUMBER == 0)
        {
            img->type = INTRA_IMG;        // set image type for first image to I-frame
        }
        else
        {
            img->type = INTER_IMG;        // P-frame
            picture_coding_type= 0;
        }
    }
    else
    {
        if ((IMG_NUMBER%input->intra_period) == 0)
        {
            img->type = INTRA_IMG;
        }
        else
        {
            img->type = INTER_IMG;        // P-frame
        }
    }

}
void report_stats_on_error(void)
{
    // terminate sequence
    terminate_sequence();

    fclose(p_in);

    if (p_dec)
        fclose(p_dec);
    if (p_trace)
        fclose(p_trace);

    Clear_Motion_Search_Module ();

    // free structure for rd-opt. mode decision
    clear_rdopt ();

    // report everything
    report();

    free_picture (frame_pic);

    free_global_buffers();

    // free image mem
    free_img ();
#ifdef _OUTPUT_MB_QSTEP_BITS_
    fprintf(pf_mb_qstep_bits, "qstep:%7.2f\t %8d\t%8d\n", 1.0/(QP2Qstep(img->qp)), glb_mb_coff_bits[INTER_IMG], glb_mb_coff_bits[INTRA_IMG]);
    fclose(pf_mb_qstep_bits);
#endif
    exit (-1);
}