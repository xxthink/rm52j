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
#include <assert.h>
#include <string.h>

#include "global.h"
#include "header.h"
#include "bitstream.h"
#include "vlc.h"
#include "image.h"
#include "rdopt_coding_state.h"
#include "loopfilter.h"

/*
*************************************************************************
* Function:This function terminates a slice
* Input:
* Output:
* Return: 0 if OK,                                                         \n
1 in case of error
* Attention:                                                  by jlzheng
*************************************************************************
*/
extern StatParameters  stats;
int start_slice()
{
    Bitstream *currStream;

    currStream = currBitStream;

    if(((img->picture_structure && img->current_mb_nr!=0))||((!img->picture_structure)&&(img->current_mb_nr_fld!=0)))
        Demulate(currStream,current_slice_bytepos);                  //qhg 20060327  for de-emulation

    if (currStream->bits_to_go!=8) //   hzjia 2004-08-20 
    {

        currStream->byte_buf <<= currStream->bits_to_go;
        currStream->byte_buf |= ( 1 << (currStream->bits_to_go - 1) );

        currStream->streamBuffer[currStream->byte_pos++] = currStream->byte_buf;
        currStream->bits_to_go = 8;
        currStream->byte_buf = 0;
    } 
    else    // cjw 20060321 
    {
        currStream->streamBuffer[currStream->byte_pos++] = 0x80;
        currStream->bits_to_go = 8;
        currStream->byte_buf = 0; 
    }

    //qhg 20060327 for de-emulation 
    current_slice_bytepos=currStream->byte_pos;  

    return 0;   
}

/*
*************************************************************************
* Function:This function terminates a picture
* Input:
* Output:
* Return: 0 if OK,                                                         \n
1 in case of error
* Attention:
*************************************************************************
*/

int terminate_picture()
{
    Bitstream *currStream;

    currStream = currBitStream;

    //////////////////////////////////////////////////////////////////////////
    Demulate(currStream,current_slice_bytepos);                    //qhg 20060327 
    //////////////////////////////////////////////////////////////////////////


    //---deleted by Yulj 2004.07.16
    // '10..' should be filled even if currStream is bytealign. 
    //if (currStream->bits_to_go==8)  
    //  return 0;

    currStream->byte_buf <<= currStream->bits_to_go;
    currStream->byte_buf |= (1 << (currStream->bits_to_go - 1) );    // Yulj 2004.07.16

    currStream->streamBuffer[currStream->byte_pos++] = currStream->byte_buf;
    currStream->bits_to_go = 8;
    currStream->byte_buf = 0;

    return 0;   
}

/*
*************************************************************************
* Function:Encodes one slice
* Input:
* Output:
* Return: the number of coded MBs in the SLice 
* Attention:                                    
*************************************************************************
*/
void picture_data(Picture *pic)
{
    Boolean end_of_picture = FALSE;
    int CurrentMbNumber=0;
    int MBRowSize = img->width / MB_BLOCK_SIZE;
    int slice_nr = 0;
    int slice_qp = img->qp;
    int len;
    bot_field_mb_nr = -1;
    while (end_of_picture == FALSE) // loop over macroblocks
    {
        set_MB_parameters (CurrentMbNumber);
        if (input->slice_row_nr && (img->current_mb_nr ==0  
            ||(img->current_mb_nr>0 && img->mb_data[img->current_mb_nr].slice_nr != img->mb_data[img->current_mb_nr-1].slice_nr)))
        {
            start_slice (); 
            img->current_slice_qp = img->qp;
            img->current_slice_start_mb = img->current_mb_nr;
            len = SliceHeader(slice_nr, slice_qp);  
            stats.bit_slice += len;  
            slice_nr++; 
        }

        start_macroblock ();
        encode_one_macroblock ();
        write_one_macroblock (1);
        terminate_macroblock (&end_of_picture);
        proceed2nextMacroblock ();
        CurrentMbNumber++;
    }
    terminate_picture ();
    DeblockFrame (img, imgY, imgUV);
}


void top_field(Picture *pic)
{
    Boolean end_of_picture = FALSE;
    int CurrentMbNumber=0;
    int MBRowSize = img->width / MB_BLOCK_SIZE;
    int slice_nr =0;
    int slice_qp = img->qp;
    int len;

    img->top_bot = 0;
    bot_field_mb_nr = -1;
    while (end_of_picture == FALSE) // loop over macroblocks
    {
        set_MB_parameters (CurrentMbNumber);

        img->current_mb_nr_fld = img->current_mb_nr;
        if (input->slice_row_nr && (img->current_mb_nr ==0  
            ||(img->current_mb_nr>0 && img->mb_data[img->current_mb_nr].slice_nr != img->mb_data[img->current_mb_nr-1].slice_nr)))
        {
            start_slice ();  
            img->current_slice_qp = img->qp;
            img->current_slice_start_mb = img->current_mb_nr;
            len = SliceHeader(slice_nr, slice_qp);
            stats.bit_slice += len;
            slice_nr++;
        }
        start_macroblock ();
        encode_one_macroblock ();
        write_one_macroblock (1);
        terminate_macroblock (&end_of_picture);
        proceed2nextMacroblock ();
        CurrentMbNumber++;
    }

    DeblockFrame (img, imgY, imgUV);

    bot_field_mb_nr = img->current_mb_nr;  // record the last mb index in the top field,  Xiaozhen Zheng HiSilicon, 20070327

    //rate control
    pic->bits_per_picture = 8 * (currBitStream->byte_pos);
}

void bot_field(Picture *pic)
{
    Boolean end_of_picture = FALSE;
    int CurrentMbNumber=0;
    int MBRowSize = img->width / MB_BLOCK_SIZE;
    int slice_nr =0;
    int slice_qp = img->qp;
    int len;

    img->top_bot = 1; //Yulj 2004.07.20
    while (end_of_picture == FALSE) // loop over macroblocks
    {
        set_MB_parameters (CurrentMbNumber);
        if (input->slice_row_nr && (img->current_mb_nr ==0  
            ||(img->current_mb_nr>0 && img->mb_data[img->current_mb_nr].slice_nr != img->mb_data[img->current_mb_nr-1].slice_nr)))
        {
            // slice header start  jlzheng 7.11
            start_slice ();  
            img->current_slice_qp = img->qp;
            img->current_slice_start_mb = img->current_mb_nr;
            len = SliceHeader(slice_nr, slice_qp);
            stats.bit_slice += len;
            slice_nr++;
            // slice header end
        }

        img->current_mb_nr_fld = img->current_mb_nr+img->total_number_mb;
        start_macroblock ();
        encode_one_macroblock ();
        write_one_macroblock (1);
        terminate_macroblock (&end_of_picture);
        proceed2nextMacroblock ();
        CurrentMbNumber++;
    }

    //  terminate_picture ();    // 20071009
    DeblockFrame (img, imgY, imgUV);

    pic->bits_per_picture = 8 * (currBitStream->byte_pos);
}

void combine_field()
{
    int i;

    for (i=0; i<img->height; i++)
    {
        memcpy(imgY_com[i*2], imgY_top[i], img->width);     // top field
        memcpy(imgY_com[i*2 + 1], imgY_bot[i], img->width); // bottom field
    }

    for (i=0; i<img->height_cr; i++)
    {
        memcpy(imgUV_com[0][i*2], imgUV_top[0][i], img->width_cr);
        memcpy(imgUV_com[0][i*2 + 1], imgUV_bot[0][i], img->width_cr);
        memcpy(imgUV_com[1][i*2], imgUV_top[1][i], img->width_cr);
        memcpy(imgUV_com[1][i*2 + 1], imgUV_bot[1][i], img->width_cr);
    }
}


void store_field_MV ()
{
    int i, j;

    if (img->type != B_IMG)     //all I- and P-frames
    {
        if (!img->picture_structure)
        {
            for (i = 0; i < img->width / 8 + 4; i++)
            {
                for (j = 0; j < img->height / 16; j++)
                {
                    tmp_mv_frm[0][2 * j][i] = tmp_mv_frm[0][2 * j + 1][i] = tmp_mv_top[0][j][i];
                    tmp_mv_frm[0][2 * j][i] = tmp_mv_frm[0][2 * j + 1][i] = tmp_mv_top[0][j][i];        // ??
                    tmp_mv_frm[1][2 * j][i] = tmp_mv_frm[1][2 * j + 1][i] = tmp_mv_top[1][j][i] * 2;
                    tmp_mv_frm[1][2 * j][i] = tmp_mv_frm[1][2 * j + 1][i] = tmp_mv_top[1][j][i] * 2;    // ??

                    if (refFrArr_top[j][i] == -1)
                    {
                        refFrArr_frm[2 * j][i] =
                            refFrArr_frm[2 * j + 1][i] = -1;
                    }
                    else
                    {
                        refFrArr_frm[2 * j][i] =
                            refFrArr_frm[2 * j + 1][i] =
                            (int) (refFrArr_top[j][i] / 2);
                    }
                }
            }
        }
        else
        {
            for (i = 0; i < img->width / 8 + 4; i++)
            {
                for (j = 0; j < img->height / 16; j++)
                {
                    tmp_mv_top[0][j][i] = tmp_mv_bot[0][j][i] =  (int) (tmp_mv_frm[0][2 * j][i]);
                    tmp_mv_top[1][j][i] = tmp_mv_bot[1][j][i] =  (int) ((tmp_mv_frm[1][2 * j][i]) / 2);

                    if (refFrArr_frm[2 * j][i] == -1)
                    {
                        refFrArr_top[j][i] = refFrArr_bot[j][i] = -1;
                    }
                    else
                    {
                        //   refFrArr_top[j][i] = refFrArr_bot[j][i] =
                        //   refFrArr_frm[2 * j][i] * 2;
                        refFrArr_top[j][i] = refFrArr_frm[2*j][i]*2;
                        refFrArr_bot[j][i] = refFrArr_frm[2*j][i]*2 + 1;
                        //by oliver 0512
                    }
                }
            }
        }
    }
}

/*!
************************************************************************
* \brief
*    Picture Structure Decision
************************************************************************
*/
int picture_structure_decision (Picture *frame, Picture *top, Picture *bot)
{
    double lambda_picture;
    int bframe = (img->type == B_IMG);
    float snr_frame, snr_field;
    int bit_frame, bit_field;

    lambda_picture = 0.85 * pow (2, (img->qp - SHIFT_QP) / 4.0) * (bframe?4 : 1);

    snr_frame = frame->distortion_y + frame->distortion_u + frame->distortion_v;
    //! all distrortions of a field picture are accumulated in the top field
    snr_field = bot->distortion_y + bot->distortion_u + bot->distortion_v;
    bit_field = bot->bits_per_picture;
    bit_frame = frame->bits_per_picture;

    return decide_fld_frame (snr_frame, snr_field, bit_field, bit_frame, lambda_picture);
}




/*
*************************************************************************
* Function: allocates the memory for the coded picture data
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/
void AllocateBitstream()
{
    const int buffer_size = (img->width * img->height * 4); 

    if ((currBitStream = (Bitstream *) calloc(1, sizeof(Bitstream))) == NULL)
        no_mem_exit ("malloc_slice: Bitstream");
    if ((currBitStream->streamBuffer = (byte *) calloc(buffer_size, sizeof(byte))) == NULL)
        no_mem_exit ("malloc_slice: StreamBuffer");

    currBitStream->bits_to_go = 8;
}
/*
*************************************************************************
* Function:free the allocated memory for the coded picture data
* Input:
* Output:
* Return: 
*************************************************************************
*/
void FreeBitstream()
{
    const int buffer_size = (img->width * img->height * 4); 

    if (currBitStream->streamBuffer)
        free(currBitStream->streamBuffer);
    if (currBitStream)
        free(currBitStream);
}

//////////////////////////////////////////////////////////////////////////
////////////********************* qhg add 20060327*****************///////
//////////////////////////////////////////////////////////////////////////
void Demulate(Bitstream *currStream, int current_slice_bytepos)
{
    int i, j;
    unsigned int rawbitsequence=-1;
    int bitvalue, nonzero, bitnum; 
    if(!(currBitStream->streamBuffer[current_slice_bytepos]==0 && currBitStream->streamBuffer[current_slice_bytepos+1]==0 && currBitStream->streamBuffer[current_slice_bytepos+2]==1))
        printf ("Fatal bitstream error");
    AllocatetmpBitstream();

    bitnum=8;
    tmpStream->bits_to_go=0;
    currStream->streamBuffer[currStream->byte_pos]=currStream->byte_buf<<currStream->bits_to_go;  

    for(tmpStream->byte_pos=i=current_slice_bytepos+3 ; i<=currStream->byte_pos; i++)
    { 
        for(j=8; j>(i==currStream->byte_pos? currStream->bits_to_go:0); j--)
        {
            bitvalue=currBitStream->streamBuffer[i]&(0x01<<(j-1));
            if(bitnum==2)
            {
                nonzero = rawbitsequence & 0x003fffff;
                if(!nonzero)
                {
                    tmpStream->streamBuffer[tmpStream->byte_pos] = 0x02;
                    tmpStream->byte_pos++;
                    tmpStream->bits_to_go += 2;
                    rawbitsequence = 0x00000002;
                    bitnum = 8;
                }
            }

            rawbitsequence <<= 1;
            if(bitvalue)
            {
                tmpStream->streamBuffer[tmpStream->byte_pos] |= 1<<(bitnum-1);
                rawbitsequence |= 1;
            }
            else
            {
                tmpStream->streamBuffer[tmpStream->byte_pos] &= (~(1<<(bitnum-1)));
            }
            bitnum--;
            tmpStream->bits_to_go++;
            if(bitnum==0)
            {
                bitnum = 8;
                tmpStream->byte_pos++;
            }
        }

    }
    for(i=current_slice_bytepos+3; i<=tmpStream->byte_pos; i++)
    {
        currStream->streamBuffer[i]=tmpStream->streamBuffer[i];
    }

    currStream->byte_pos=tmpStream->byte_pos;  
    currStream->bits_to_go=8-tmpStream->bits_to_go%8;
    currStream->byte_buf=tmpStream->streamBuffer[tmpStream->byte_pos]>>currStream->bits_to_go;
    FreetmpBitstream();
}

void AllocatetmpBitstream()
{
    const int buffer_size = (img->width * img->height * 4);  //add by wuzhongmou 0610

    if ((tmpStream = (Bitstream *) calloc(1, sizeof(Bitstream))) == NULL)
        no_mem_exit ("malloc_slice: Bitstream");
    if ((tmpStream->streamBuffer = (byte *) calloc(buffer_size, sizeof(byte))) == NULL)
        no_mem_exit ("malloc_slice: StreamBuffer");

    tmpStream->bits_to_go = 8;
}

void FreetmpBitstream()
{
    const int buffer_size = (img->width * img->height * 4); 

    if (tmpStream->streamBuffer)
        free(tmpStream->streamBuffer);
    if (tmpStream)
        free(tmpStream);
}

//////////////////////////////////////////////////////////////////////////
////////////////*********************end  qhg *************************///
//////////////////////////////////////////////////////////////////////////
