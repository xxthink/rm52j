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



/*!
***********************************************************************
*  \mainpage
*     This is the AVS decoder reference software. For detailed documentation
*     see the comments in each file.
*
*  \author
*     The main contributors are listed in contributors.h
*
*
*  \note
*     tags are used for document system "doxygen"
*     available at http://www.doxygen.org
*
*  \par Limitations:
*     Using different NAL's the assignment of partition-id to containing
*     syntax elements may got lost, if this information is not transmitted.
*     The same has to be stated for the partitionlength if partitions are
*     merged by the NAL.
*  \par
*     The presented solution in Q15-K-16 solves both of this problems as the
*     departitioner parses the bitstream before decoding. Due to syntax element
*     dependencies both, partition bounds and partitionlength information can
*     be parsed by the departitioner.
*
*  \par Handling partition information in external file:
*     As the TML is still a work in progress, it makes sense to handle this
*     information for simplification in an external file, here called partition
*     information file, which can be found by the extension .dp extending the
*     original encoded AVS bitstream. In this file partition-ids followed by its
*     partitionlength is written. Instead of parsing the bitstream we get the
*     partition information now out of this file.
*     This information is assumed to be never sent over transmission channels
*     (simulation scenarios) as it's information we allways get using a
*     "real" departitioner before decoding
*
*  \par Extension of Interim File Format:
*     Therefore a convention has to be made within the interim file format.
*     The underlying NAL has to take care of fulfilling these conventions.
*     All partitions have to be bytealigned to be readable by the decoder,
*     So if the NAL-encoder merges partitions, >>this is only possible to use the
*     VLC structure of the AVS bitstream<<, this bitaligned structure has to be
*     broken up by the NAL-decoder. In this case the NAL-decoder is responsable to
*     read the partitionlength information from the partition information file.
*     Partitionlosses are signaled with a partition of zero length containing no
*     syntax elements.
*
*/

/*
*************************************************************************************
* File name: ldecod.c
* Function: TML decoder project main()
*
*************************************************************************************
*/

#include "contributors.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/timeb.h>
#include <assert.h>

#if defined WIN32
#include <conio.h>
#endif

#include "global.h"
#include "memalloc.h"
#include "mbuffer.h"
#include "annexb.h"

#define RM          "5"
#define VERSION     "5.2 20080515"

#define LOGFILE     "log.dec"
#define DATADECFILE "dataDec.txt"
#define TRACEFILE   "trace_dec.txt"

extern FILE* bits;

struct inp_par    *input;       //!< input parameters from input configuration file
struct snr_par    *snr;         //!< statistics
struct img_par    *img;         //!< image parameters

Bitstream *currStream;
FILE *reffile,*reffile2;

/*
*************************************************************************
* Function:main function for TML decoder
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/


int main(int argc, char **argv)
{
#ifdef _OUTPUT_MV
    pf_mv = fopen("mv.txt", "w");
#endif
    // allocate memory for the structures
    if ((input =  (struct inp_par *)calloc(1, sizeof(struct inp_par)))==NULL) no_mem_exit("main: input");
    if ((snr =  (struct snr_par *)calloc(1, sizeof(struct snr_par)))==NULL) no_mem_exit("main: snr");
    if ((img =  (struct img_par *)calloc(1, sizeof(struct img_par)))==NULL) no_mem_exit("main: img");

#ifdef _OUTPUT_CODENUM_
    pf_code_num = fopen("code_num.txt", "w");
#endif
#ifdef _OUTPUT_DISTANCE_
    pf_distance = fopen("distance.txt", "w");
#endif
    img->seq_header_indicate = 0;    // xiaozhen zheng, 20071009
    img->B_discard_flag = 0;         // xiaozhen zheng, 20071009

    currStream = AllocBitstream();

    eos = 0;  //added by Xiaozhen Zheng, HiSilicon, 2007.03.21

    // Read Configuration File
    /* // commented by xiaozhen zheng, 20071224
    if (argc != 2)
    {
    snprintf(errortext, ET_SIZE,
    "Usage: %s <config.dat> \n\t<config.dat> defines decoder parameters",argv[0]);
    error(errortext, 300);
    }
    */


    //  init_conf(input, argv[1]);  // 20071224
    init_conf(input,argc,argv); //changed by zjt of hw for decoder parameter configuring with command line

    OpenBitstreamFile (input->infile);

    // Allocate Slice data struct
    init(img);
    img->number=0;
    img->type = I_IMG;
    img->imgtr_last_P = 0;
    img->imgtr_next_P = 0;
    //img->Bframe_number=0;

    // Initial vec_flag
    vec_flag = 0;

    FrameNum = 0;  //added by Xiaozhen Zheng, HiSilicon, 2007.03.21

    // B pictures
    Bframe_ctr=0;

    // time for total decoding session
    tot_time = 0;
    do {  // modified by Carmen, 2008/01/22, For supporting multiple sequences in a stream
        //while (decode_one_frame(img, input, snr) != EOS);
        while ((decode_one_frame(img, input, snr) != EOS) /*&& (!IsEndOfBitstream())*/);
    } while (!IsEndOfBitstream()); // Carmen, 2008/01/22, For supporting multiple sequences in a stream

    //added by Xiaozhen Zheng HiSilicon, 2007.03.21 Begin
    eos = 1;
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
    //added by Xiaozhen Zheng HiSilicon, 2007.03.21 End

    report_frame(snr,pre_tmp_time);

    // B PICTURE : save the last P picture
#ifdef _OUTPUT_DEC_REC
    write_prev_Pframe(img, p_out);
#endif

    report(input, img, snr);
    FreeBitstream();
    free_global_buffers(input, img);

    CloseBitstreamFile();

    fclose(p_out);
    if (p_ref)
        fclose(p_ref);

#if TRACE
    fclose(p_trace);
#endif

    free (input);
    free (snr);
    free (img);
#ifdef _OUTPUT_DISTANCE_
    fclose(pf_distance);
#endif
#ifdef _OUTPUT_MV
    fclose(pf_mv);
#endif
    return 0;
}

/*
*************************************************************************
* Function:Initilize some arrays
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/


void init(struct img_par *img)  //!< image parameters
{
    int i;

    // initilize quad mat`rix used in snr routine
    for (i=0; i <  256; i++)
    {
        img->quad[i]=i*i;
    }

}

/*
*************************************************************************
* Function:Read input from configuration file
* Input:Name of configuration filename
* Output:
* Return:
* Attention:
*************************************************************************
*/

/* // commented by xiaozhen zheng, 20071224
void init_conf(struct inp_par *inp,
char *config_filename)*/
void init_conf(struct inp_par *inp, int numpar,char **config_str)
{
    FILE *fd;

    // read the decoder configuration file
    /* // commented by xiaozhen zheng, 20071224
    if((fd=fopen(config_filename,"r")) == NULL)
    {
    snprintf(errortext, ET_SIZE, "Error: Control file %s not found\n",config_filename);
    error(errortext, 300);
    }
    */
    //changed by zjt of hw   20071224
    if( numpar != 2 && numpar != 7 )
    {
        snprintf(errortext, ET_SIZE,
            "Usage: %s <config.dat> <avsfilename> <decfilename> <reffilename> <ref_num> <lfdisable>\n \
            ?-the decoder can be configured by configfile or by command line \n \
            ?-param with only <config.dat> defines decoder with file\n \
            ?-params with allof the params define decoder params with command line\n \
            ",config_str[0]);
        error(errortext, 300);
    }

    // read the decoder configuration file   20071224
    if((fd=fopen(config_str[1],"r")) == NULL)
    {
        snprintf(errortext, ET_SIZE, "Error: Control file %s not found\n",config_str[1]);
        error(errortext, 300);
    }

    fscanf(fd,"%s",inp->infile);                // AVS compressed input bitsream
    fscanf(fd,"%*[^\n]");

    fscanf(fd,"%s",inp->outfile);               // YUV 4:2:2 input format
    fscanf(fd,"%*[^\n]");

    fscanf(fd,"%s",inp->reffile);               // reference file
    fscanf(fd,"%*[^\n]");

    // Frame buffer size
    fscanf(fd,"%d,",&inp->buf_cycle);   // may be overwritten in case of RTP NAL
    fscanf(fd,"%*[^\n]");
    /* // 20071224
    if (inp->buf_cycle < 1)
    {
    snprintf(errortext, ET_SIZE, "Frame Buffer Size is %d. It has to be at least 1",inp->buf_cycle);
    error(errortext,1);
    }
    */

    fscanf(fd,"%d,",&inp->LFParametersFlag);
    fscanf(fd,"%*[^\n]");

    // 20071224
    if (numpar == 7) // if there are 7 input parameters, and then the decoder will be configured with the command line
    {

        strcpy(inp->infile,config_str[2]);
        strcpy(inp->outfile,config_str[3]);
        strcpy(inp->reffile,config_str[4]);

        inp->buf_cycle = atoi(config_str[5]);
        inp->LFParametersFlag = atoi(config_str[6]);
    }
    //change end by zjt of hw

    // 20071224
    if (inp->buf_cycle < 1)
    {
        snprintf(errortext, ET_SIZE, "Frame Buffer Size is %d. It has to be at least 1",inp->buf_cycle);
        error(errortext,1);
    }

#if TRACE
    if ((p_trace=fopen(TRACEFILE,"w"))==0)             // append new statistic at the end
    {
        snprintf(errortext, ET_SIZE, "Error open file %s!",TRACEFILE);
        error(errortext,500);
    }
#endif

    if ((p_out=fopen(inp->outfile,"wb"))==0)
    {
        snprintf(errortext, ET_SIZE, "Error open file %s ",inp->outfile);
        error(errortext,500);
    }

    fprintf(stdout,"--------------------------------------------------------------------------\n");
    //  fprintf(stdout," Decoder config file                    : %s \n",config_filename);  // 20071224
    fprintf(stdout," Decoder config file                    : %s \n",config_str[0]);
    fprintf(stdout,"--------------------------------------------------------------------------\n");
    fprintf(stdout," Input AVS bitstream                    : %s \n",inp->infile);
    fprintf(stdout," Output decoded YUV 4:2:0               : %s \n",inp->outfile);
    fprintf(stdout," Output status file                     : %s \n",LOGFILE);

    if ((p_ref=fopen(inp->reffile,"rb"))==0)
    {
        fprintf(stdout," Input reference file                   : %s does not exist \n",inp->reffile);
        fprintf(stdout,"                                          SNR values are not available\n");
        RefPicExist = 0;  // 20071224
    }
    else
    {
        fprintf(stdout," Input reference file                   : %s \n",inp->reffile);
        RefPicExist = 1;  // 20071224
    }

    fprintf(stdout,"--------------------------------------------------------------------------\n");
    fprintf(stdout," Frame   TR    QP   SnrY    SnrU    SnrV   Time(ms)\n");
}

/*
*************************************************************************
* Function:Reports the gathered information to appropriate outputs
* Input:
struct inp_par *inp,
struct img_par *img,
struct snr_par *stat
* Output:
* Return:
* Attention:
*************************************************************************
*/

void report(struct inp_par *inp, struct img_par *img, struct snr_par *snr)
{
#define OUTSTRING_SIZE 255
    char string[OUTSTRING_SIZE];
    FILE *p_log;

#ifndef WIN32
    time_t  now;
    struct tm *l_time;
#else
    char timebuf[128];
#endif

    fprintf(stdout,"-------------------- Average SNR all frames ------------------------------\n");
    fprintf(stdout," SNR Y(dB)           : %5.2f\n",snr->snr_ya);
    fprintf(stdout," SNR U(dB)           : %5.2f\n",snr->snr_ua);
    fprintf(stdout," SNR V(dB)           : %5.2f\n",snr->snr_va);
    fprintf(stdout," Total decoding time : %.3f sec \n",tot_time*0.001);
    fprintf(stdout,"--------------------------------------------------------------------------\n");
    fprintf(stdout," Exit RM %s decoder, ver %s ",RM,VERSION);
    fprintf(stdout,"\n");
    // write to log file

    snprintf(string, OUTSTRING_SIZE, "%s", LOGFILE);
    if ((p_log=fopen(string,"r"))==0)                    // check if file exist
    {
        if ((p_log=fopen(string,"a"))==0)
        {
            snprintf(errortext, ET_SIZE, "Error open file %s for appending",string);
            error(errortext, 500);
        }
        else                                              // Create header to new file
        {
            fprintf(p_log," ------------------------------------------------------------------------------------------\n");
            fprintf(p_log,"|  Decoder statistics. This file is made first time, later runs are appended               |\n");
            fprintf(p_log," ------------------------------------------------------------------------------------------ \n");
            fprintf(p_log,"| Date  | Time  |    Sequence        |#Img|Format|SNRY 1|SNRU 1|SNRV 1|SNRY N|SNRU N|SNRV N|\n");
            fprintf(p_log," ------------------------------------------------------------------------------------------\n");
        }
    }
    else
    {
        fclose(p_log);
        p_log=fopen(string,"a");                    // File exist,just open for appending
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
    fprintf(p_log,"| %1.5s |",string );
#endif

    fprintf(p_log,"%20.20s|",inp->infile);

    fprintf(p_log,"%3d |",img->number);

    fprintf(p_log,"%6.3f|",snr->snr_y1);
    fprintf(p_log,"%6.3f|",snr->snr_u1);
    fprintf(p_log,"%6.3f|",snr->snr_v1);
    fprintf(p_log,"%6.3f|",snr->snr_ya);
    fprintf(p_log,"%6.3f|",snr->snr_ua);
    fprintf(p_log,"%6.3f|\n",snr->snr_va);

    fclose(p_log);

    snprintf(string, OUTSTRING_SIZE,"%s", DATADECFILE);
    p_log=fopen(string,"a");

    if(Bframe_ctr != 0) // B picture used
    {
        fprintf(p_log, "%3d %2d %2d %2.2f %2.2f %2.2f %5d "
            "%2.2f %2.2f %2.2f %5d "
            "%2.2f %2.2f %2.2f %5d %.3f\n",
            img->number, 0, img->qp,
            snr->snr_y1,
            snr->snr_u1,
            snr->snr_v1,
            0,
            0.0,
            0.0,
            0.0,
            0,
            snr->snr_ya,
            snr->snr_ua,
            snr->snr_va,
            0,
            (double)0.001*tot_time/(img->number+Bframe_ctr-1));
    }
    else
    {
        fprintf(p_log, "%3d %2d %2d %2.2f %2.2f %2.2f %5d "
            "%2.2f %2.2f %2.2f %5d "
            "%2.2f %2.2f %2.2f %5d %.3f\n",
            img->number, 0, img->qp,
            snr->snr_y1,
            snr->snr_u1,
            snr->snr_v1,
            0,
            0.0,
            0.0,
            0.0,
            0,
            snr->snr_ya,
            snr->snr_ua,
            snr->snr_va,
            0,
            (double)0.001*tot_time/img->number);
    }
    fclose(p_log);
}

/*
*************************************************************************
* Function:Allocates a Bitstream
* Input:
* Output:allocated Bitstream point
* Return:
* Attention:
*************************************************************************
*/

Bitstream *AllocBitstream()
{
    Bitstream *bitstream;

    bitstream = (Bitstream *) calloc(1, sizeof(Bitstream));
    if (bitstream == NULL)
    {
        snprintf(errortext, ET_SIZE, "AllocBitstream: Memory allocation for Bitstream failed");
        error(errortext, 100);
    }
    bitstream->streamBuffer = (byte *) calloc(MAX_CODED_FRAME_SIZE, sizeof(byte));
    if (bitstream->streamBuffer == NULL)
    {
        snprintf(errortext, ET_SIZE, "AllocBitstream: Memory allocation for streamBuffer failed");
        error(errortext, 100);
    }

    return bitstream;
}


/*
*************************************************************************
* Function:Frees a partition structure (array).
* Input:Partition to be freed, size of partition Array (Number of Partitions)
* Output:
* Return:
* Attention:n must be the same as for the corresponding call of AllocPartition
*************************************************************************
*/

void FreeBitstream ()
{
    assert (currStream!= NULL);
    assert (currStream->streamBuffer != NULL);

    free (currStream->streamBuffer);
    free (currStream);
}

/*
*************************************************************************
* Function:Dynamic memory allocation of frame size related global buffers
buffers are defined in global.h, allocated memory must be freed in
void free_global_buffers()
* Input:Input Parameters struct inp_par *inp, Image Parameters struct img_par *img
* Output:Number of allocated bytes
* Return:
* Attention:
*************************************************************************
*/


int init_global_buffers(struct inp_par *inp, struct img_par *img)
{
    int i,j;
    int refnum;

    int memory_size=0;
    //############################################################
    //Carmen, 2007/12/20, Bug Fix: correct picture size for outputted reconstructed pictures
    //Changed all vertical_size to img->height
    int img_height=(vertical_size+img->auto_crop_bottom); //instead of vertical_size;
    //Carmen, 2007/12/20, Bug Fix: correct picture size for outputted reconstructed pictures
    //############################################################

    img->buf_cycle = inp->buf_cycle+1;

    img->buf_cycle *= 2;


    if(!progressive_sequence)
    {

        memory_size += get_mem2Dint(&refFrArr_top, img_height /*vertical_size*/ /(2*B8_SIZE), img->width/B8_SIZE);
        memory_size += get_mem2Dint(&refFrArr_bot, img_height /*vertical_size*/ /(2*B8_SIZE), img->width/B8_SIZE);

        memory_size += get_mem3Dint(&(img->mv_top),img->width/B8_SIZE +4, img_height /*vertical_size*/ /(2*B8_SIZE),3);
        memory_size += get_mem3Dint(&(img->mv_bot),img->width/B8_SIZE +4, img_height /*vertical_size*/ /(2*B8_SIZE),3);
        // int fw_refFrArr[72][88];
        memory_size += get_mem2Dint(&(img->fw_refFrArr_top),img_height /*vertical_size*/ /(2*B8_SIZE),img->width/B8_SIZE);
        // int bw_refFrArr[72][88];
        memory_size += get_mem2Dint(&(img->bw_refFrArr_top),img_height /*vertical_size*/ /(2*B8_SIZE),img->width/B8_SIZE);

        memory_size += get_mem2Dint(&(img->fw_refFrArr_bot),img_height /*vertical_size*/ /(2*B8_SIZE),img->width/B8_SIZE);
        // int bw_refFrArr[72][88];
        memory_size += get_mem2Dint(&(img->bw_refFrArr_bot),img_height /*vertical_size*/ /(2*B8_SIZE),img->width/B8_SIZE);

        memory_size += get_mem3Dint(&(img->fw_mv_top),img->width/B8_SIZE +4, img_height /*vertical_size*/ /(2*B8_SIZE),3);
        memory_size += get_mem3Dint(&(img->fw_mv_bot),img->width/B8_SIZE +4, img_height /*vertical_size*/ /(2*B8_SIZE),3);

        memory_size += get_mem3Dint(&(img->bw_mv_top),img->width/B8_SIZE +4, img_height /*vertical_size*/ /(2*B8_SIZE),3);
        memory_size += get_mem3Dint(&(img->bw_mv_bot),img->width/B8_SIZE +4, img_height /*vertical_size*/ /(2*B8_SIZE),3);

        memory_size += get_mem3Dint(&(img->dfMV_top),img->width/B8_SIZE +4, img_height /*vertical_size*/ /(2*B8_SIZE),3);
        memory_size += get_mem3Dint(&(img->dbMV_top),img->width/B8_SIZE +4, img_height /*vertical_size*/ /(2*B8_SIZE),3);

        memory_size += get_mem3Dint(&(img->dfMV_bot),img->width/B8_SIZE +4, img_height /*vertical_size*/ /(2*B8_SIZE),3);
        memory_size += get_mem3Dint(&(img->dbMV_bot),img->width/B8_SIZE +4, img_height /*vertical_size*/ /(2*B8_SIZE),3);
    }

    // allocate memory for imgY_prev
    memory_size += get_mem2D(&imgY_prev, img_height /*vertical_size*/ , img->width);
    memory_size += get_mem3D(&imgUV_prev, 2, img_height /*vertical_size*/ /(chroma_format==2?1:2), img->width_cr);

    // allocate memory for reference frames of each block: refFrArr
    memory_size += get_mem2Dint(&refFrArr_frm, img_height /*vertical_size*/ /B8_SIZE, img->width/B8_SIZE);

    // allocate memory for reference frame in find_snr
    memory_size += get_mem2D(&imgY_ref, img_height /*vertical_size*/ , img->width);
    memory_size += get_mem3D(&imgUV_ref, 2, img_height /*vertical_size*/ /(chroma_format==2?1:2), img->width_cr);

    // allocate memory in structure img
    if(((mb_data) = (Macroblock *) calloc((img->width/MB_BLOCK_SIZE) * (img_height /*vertical_size*/ /MB_BLOCK_SIZE),sizeof(Macroblock))) == NULL)
        no_mem_exit("init_global_buffers: mb_data");

    if(((img->intra_block) = (int**)calloc((j=(img->width/MB_BLOCK_SIZE) * (img_height /*vertical_size*/ /MB_BLOCK_SIZE)),sizeof(int))) == NULL)
        no_mem_exit("init_global_buffers: img->intra_block");
    for (i=0; i<j; i++)
    {
        if ((img->intra_block[i] = (int*)calloc(4, sizeof(int))) == NULL)
            no_mem_exit ("init_global_buffers: img->intra_block");
    }

    memory_size += get_mem3Dint(&(img->mv_frm),img->width/B8_SIZE +4, img_height /*vertical_size*/ /B8_SIZE,3);
    memory_size += get_mem2Dint(&(img->ipredmode),img->width/B8_SIZE +2 , img_height /*vertical_size*/ /B8_SIZE +2);
    memory_size += get_mem3Dint(&(img->dfMV),img->width/B8_SIZE +4, img_height /*vertical_size*/ /B8_SIZE,3);
    memory_size += get_mem3Dint(&(img->dbMV),img->width/B8_SIZE +4, img_height /*vertical_size*/ /B8_SIZE,3);
    memory_size += get_mem2Dint(&(img->fw_refFrArr_frm),img_height /*vertical_size*/ /B8_SIZE,img->width/B8_SIZE);
    memory_size += get_mem2Dint(&(img->bw_refFrArr_frm),img_height /*vertical_size*/ /B8_SIZE,img->width/B8_SIZE);
    memory_size += get_mem3Dint(&(img->fw_mv),img->width/B8_SIZE +4, img_height /*vertical_size*/ /B8_SIZE,3);
    memory_size += get_mem3Dint(&(img->bw_mv),img->width/B8_SIZE +4, img_height /*vertical_size*/ /B8_SIZE,3);

    // Prediction mode is set to -1 outside the frame, indicating that no prediction can be made from this part
    for (i=0; i < img->width/(B8_SIZE)+2; i++)
    {
        for (j=0; j < img_height /*vertical_size*/ /(B8_SIZE)+2; j++)
        {
            img->ipredmode[i][j]=-1;
        }
    }
    //by oliver 0512

    img->buf_cycle = inp->buf_cycle+1;

    // allocate frame buffer
    for(refnum=0; refnum<3; refnum++)
        for (i=0; i<3; i++)
        {
            if (i==0)
            {
                get_mem2D(&reference_frame[refnum][i],img_height /*vertical_size*/ ,img->width);
            }else
            {
                get_mem2D(&reference_frame[refnum][i],img_height /*vertical_size*/ /(chroma_format==2?1:2),img->width_cr);
            }
        }

        //allocate field buffer
        if(!progressive_sequence)
        {
            for(refnum=0; refnum<6; refnum++)
                for (i=0; i<3; i++)
                {
                    if (i==0)
                    {
                        get_mem2D(&reference_field[refnum][i],img_height/2,img->width);
                    }else
                    {
                        get_mem2D(&reference_field[refnum][i],img_height/(chroma_format==2?2:4),img->width_cr);
                    }
                }
        }

        //forward reference frame buffer
        ref_frm[0] = reference_frame[0]; //reference_frame[ref_index][yuv][height][width],ref_frm[ref_index][yuv][height][width]
        ref_frm[1] = reference_frame[1];
        current_frame = reference_frame[2];

        //luma for forward
        for (j=0;j<2;j++)//ref_index = 0
        {
            mref_frm[j] = ref_frm[j][0];
        }

        //chroma for forward
        for (j=0;j<2;j++)//ref_index = 0
            for (i=0;i<2;i++) // chroma uv =0,1; 1,2 for reference_frame
            {
                mcef_frm[j][i] = ref_frm[j][i+1];
            }

            //luma for backward
            //forward/backward reference buffer
            f_ref_frm[0] = ref_frm[1]; //f_ref_frm[ref_index][yuv][height(height/2)][width] ref_index=0 for B frame, ref_index = 0,1 for B field
            b_ref_frm[0] = ref_frm[0]; //b_ref_frm[ref_index][yuv][height(height/2)][width] ref_index=0 for B frame, ref_index = 0,1 for B field
            for (j=0;j<1;j++)//ref_index = 0 luma = 0
            {
                mref_fref_frm[j] = f_ref_frm[j][0];
                mref_bref_frm[j] = b_ref_frm[j][0];
            }

            //chroma for backward
            for (j=0;j<1;j++)//ref_index = 0
                for (i=0;i<2;i++) // chroma uv =0,1; 1,2 for reference_frame
                {
                    mcef_fref_frm[j][i] = f_ref_frm[j][i+1];
                    mcef_bref_frm[j][i] = b_ref_frm[j][i+1];
                }

                if(!progressive_sequence)
                {
                    //forward reference frame buffer
                    for (i=0; i<4; i++)
                        ref_fld[i] = reference_field[i];
                    ref_fld[4] = reference_field[5];
                    current_field = reference_field[4];

                    //luma for forward
                    for (j=0;j<4;j++)//ref_index = 0
                    {
                        mref_fld[j] = ref_fld[j][0];
                    }

                    //chroma for forward
                    for (j=0;j<4;j++)//ref_index = 0
                        for (i=0;i<2;i++) // chroma uv =0,1; 1,2 for reference_frame
                        {
                            mcef_fld[j][i] = ref_fld[j][i+1];
                        }

                        //luma for backward
                        //forward/backward reference buffer
                        for (i=0; i<2; i++)
                        {
                            f_ref_fld[i] = ref_fld[i+2]; //f_ref_frm[ref_index][yuv][height(height/2)][width] ref_index=0 for B frame, ref_index = 0,1 for B field
                            b_ref_fld[i] = ref_fld[1-i]; //b_ref_frm[ref_index][yuv][height(height/2)][width] ref_index=0 for B frame, ref_index = 0,1 for B field
                        }
                        for (j=0;j<2;j++)//ref_index = 0 luma = 0
                        {
                            mref_fref_fld[j] = f_ref_fld[j][0];
                            mref_bref_fld[j] = b_ref_fld[j][0];
                        }

                        //chroma for backward
                        for (j=0;j<2;j++)//ref_index = 0
                            for (i=0;i<2;i++) // chroma uv =0,1; 1,2 for reference_frame
                            {
                                mcef_fref_fld[j][i] = f_ref_fld[j][i+1];
                                mcef_bref_fld[j][i] = b_ref_fld[j][i+1];
                            }
                }

                if((mref = (byte***)calloc(4,sizeof(byte**))) == NULL)
                    no_mem_exit("alloc_mref: mcef");

                if((mcef = (byte****)calloc(4,sizeof(byte***))) == NULL)
                    no_mem_exit("alloc_mref: mcef");

                for (i=0; i<4; i++)
                    if((mcef[i] = (byte***)calloc(2,sizeof(byte**))) == NULL)
                        no_mem_exit("alloc_mref: mcef");

                return (memory_size);
}

/*
*************************************************************************
* Function:Free allocated memory of frame size related global buffers
buffers are defined in global.h, allocated memory is allocated in
int init_global_buffers()
* Input:Input Parameters struct inp_par *inp, Image Parameters struct img_par *img
* Output:
* Return:
* Attention:
*************************************************************************
*/

void free_global_buffers(struct inp_par *inp, struct img_par *img)
{
    int  i,j;

    free_mem2D(imgY_prev);
    free_mem3D(imgUV_prev,2);

    if(!progressive_sequence)
    {
        free (parity_fld);
        for (i=0; i<6; i++)
            for(j=0;j<3; j++)
            {
                free_mem2D(reference_field[i][j]);
            }
    }

    free (mref);
    for (i=0; i<2; i++)
        free (mcef[i]);
    free (mcef);


    free_mem2D (imgY_ref);
    free_mem3D (imgUV_ref,2);

    // free mem, allocated for structure img
    if (mb_data       != NULL) free(mb_data);

    j = (img->width/16)*(img->height/16);
    for (i=0; i<j; i++)
    {
        free (img->intra_block[i]);
    }

    free (img->intra_block);
    free_mem3Dint(img->mv_frm,img->width/B8_SIZE + 4);

    free_mem2Dint (img->ipredmode);

    free_mem3Dint(img->dfMV,img->width/B8_SIZE + 4);
    free_mem3Dint(img->dbMV,img->width/B8_SIZE + 4);


    free_mem2Dint(img->fw_refFrArr_frm);
    free_mem2Dint(img->bw_refFrArr_frm);

    free_mem3Dint(img->fw_mv,img->width/B8_SIZE + 4);
    free_mem3Dint(img->bw_mv,img->width/B8_SIZE + 4);

    for (i=0; i<3; i++)
        for(j=0;j<3; j++)
        {
            free_mem2D(reference_frame[i][j]);
        }

}

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
    int i,j;

    tmp = ref_frm[1];       //ref[ref_index][yuv][height(height/2)][width] ref_index = 0,1 for P frame, ref_index = 0,1,2,3 for B frame
    ref_frm[1] = ref_frm[0];    // ref_index = 0, for B frame, ref_index = 0,1 for B field
    ref_frm[0] = current_frame;

    current_frame = tmp;

    //forward/backward reference buffer
    f_ref_frm[0] = ref_frm[1]; //f_ref_frm[ref_index][yuv][height(height/2)][width] ref_index=0 for B frame, ref_index = 0,1 for B field
    b_ref_frm[0] = ref_frm[0]; //b_ref_frm[ref_index][yuv][height(height/2)][width] ref_index=0 for B frame, ref_index = 0,1 for B field

    //luma for forward
    for (j=0;j<2;j++)//ref_index = 0
    {
        mref_frm[j] = ref_frm[j][0];
    }

    //chroma for forward
    for (j=0;j<2;j++)//ref_index = 0
        for (i=0;i<2;i++) // chroma uv =0,1; 1,2 for reference_frame
        {
            mcef_frm[j][i] = ref_frm[j][i+1];
        }

        //luma for backward
        for (j=0;j<1;j++)//ref_index = 0 luma = 0
        {
            mref_fref_frm[j] = f_ref_frm[j][0];
            mref_bref_frm[j] = b_ref_frm[j][0];
        }

        //chroma for backward
        for (j=0;j<1;j++)//ref_index = 0
            for (i=0;i<2;i++) // chroma uv =0,1; 1,2 for reference_frame
            {
                mcef_fref_frm[j][i] = f_ref_frm[j][i+1];
                mcef_bref_frm[j][i] = b_ref_frm[j][i+1];
            }
}


void Update_Picture_top_field()
{
    unsigned char ***tmp;
    int i,j;

    //forward reference frame buffer
    tmp = ref_fld[4];

    for (i=4; i>0; i--)
        ref_fld[i] = ref_fld[i-1];

    ref_fld[0] = current_field;
    current_field = tmp;

    //forward/backward reference buffer
    for (i=0; i<2; i++)
    {
        f_ref_fld[i] = ref_fld[i+2]; //f_ref_frm[ref_index][yuv][height(height/2)][width] ref_index=0 for B frame, ref_index = 0,1 for B field
        b_ref_fld[i] = ref_fld[1-i]; //b_ref_frm[ref_index][yuv][height(height/2)][width] ref_index=0 for B frame, ref_index = 0,1 for B field
    }

    //luma for forward
    for (j=0;j<4;j++)//ref_index = 0
    {
        mref_fld[j] = ref_fld[j][0];
    }

    //chroma for forward
    for (j=0;j<4;j++)//ref_index = 0
        for (i=0;i<2;i++) // chroma uv =0,1; 1,2 for reference_frame
        {
            mcef_fld[j][i] = ref_fld[j][i+1];
        }

        //luma for backward
        for (j=0;j<2;j++)//ref_index = 0 luma = 0
        {
            mref_fref_fld[j] = f_ref_fld[j][0];
            mref_bref_fld[j] = b_ref_fld[j][0];
        }

        //chroma for backward
        for (j=0;j<2;j++)//ref_index = 0
            for (i=0;i<2;i++) // chroma uv =0,1; 1,2 for reference_frame
            {
                mcef_fref_fld[j][i] = f_ref_fld[j][i+1];
                mcef_bref_fld[j][i] = b_ref_fld[j][i+1];
            }
}

void Update_Picture_bot_field()
{
    unsigned char ***tmp;
    int i,j;

    //forward reference frame buffer
    tmp = ref_fld[4];

    for (i=4; i>0; i--)
        ref_fld[i] = ref_fld[i-1];

    ref_fld[0] = current_field;
    current_field = tmp;

    //forward/backward reference buffer
    for (i=0; i<2; i++)
    {
        f_ref_fld[i] = ref_fld[i+2]; //f_ref_frm[ref_index][yuv][height(height/2)][width] ref_index=0 for B frame, ref_index = 0,1 for B field
        b_ref_fld[i] = ref_fld[1-i]; //b_ref_frm[ref_index][yuv][height(height/2)][width] ref_index=0 for B frame, ref_index = 0,1 for B field
    }

    //luma for forward
    for (j=0;j<4;j++)//ref_index = 0
    {
        mref_fld[j] = ref_fld[j][0];
    }

    //chroma for forward
    for (j=0;j<4;j++)//ref_index = 0
    {
        for (i=0;i<2;i++) // chroma uv =0,1; 1,2 for reference_frame
        {
            mcef_fld[j][i] = ref_fld[j][i+1];
        }

        //luma for backward
        for (j=0;j<2;j++)//ref_index = 0 luma = 0
        {
            mref_fref_fld[j] = f_ref_fld[j][0];
            mref_bref_fld[j] = b_ref_fld[j][0];
        }
    }

    //chroma for backward
    for (j=0;j<2;j++)//ref_index = 0
    {
        for (i=0;i<2;i++) // chroma uv =0,1; 1,2 for reference_frame
        {
            mcef_fref_fld[j][i] = f_ref_fld[j][i+1];
            mcef_bref_fld[j][i] = b_ref_fld[j][i+1];
        }
    }
}
