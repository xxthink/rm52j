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
* File name:  output.c
* Function: Output an image and Trance support
*
*************************************************************************************
*/


#include "contributors.h"

#include "global.h"

#include <stdlib.h>

/*
*************************************************************************
* Function:Write decoded frame to output file
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

void write_frame(
struct img_par *img,  //!< image parameter
    FILE *p_out)          //!< filestream to output file
{
    int i,j;
    //Carmen, 2007/12/20, Bug Fix: correct picture size for outputted reconstructed pictures
    int img_width = (img->width-img->auto_crop_right);
    int img_height = (img->height-img->auto_crop_bottom);
    int img_width_cr = (img_width/2);
    int img_height_cr = (img_height/ /*2*/(chroma_format==2?1:2));  //X ZHENG, 422
    //Carmen, 2007/12/20, Bug Fix: correct picture size for outputted reconstructed pictures

    for(i=0;i<img_height/*img->height*/;i++)
        for(j=0;j<img_width/*img->width*/;j++)
        {
            fputc(imgY[i][j],p_out);
        }

        for(i=0;i<img_height_cr/*img->height_cr*/;i++)
            for(j=0;j<img_width_cr/*img->width_cr*/;j++)
            {
                fputc(imgUV[0][i][j],p_out);
            }

            for(i=0;i<img_height_cr/*img->height_cr*/;i++)
                for(j=0;j<img_width_cr/*img->width_cr*/;j++)
                {
                    fputc(imgUV[1][i][j],p_out);
                }

                fflush(p_out);

}

/*
*************************************************************************
* Function:Write previous decoded P frame to output file
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

void write_prev_Pframe(struct img_par *img, FILE *p_out)
{
    int i,j;
    //Carmen, 2007/12/20, Bug Fix: correct picture size for outputted reconstructed pictures
    int img_width = (img->width-img->auto_crop_right);
    int img_height = (img->height-img->auto_crop_bottom);
    int img_width_cr = (img_width/2);
    int img_height_cr = (img_height/(chroma_format==2?1:2));  //X ZHENG, 422
    //Carmen, 2007/12/20, Bug Fix: correct picture size for outputted reconstructed pictures

    for(i=0;i<img_height/*img->height*/;i++)
        for(j=0;j<img_width/*img->width*/;j++)
            fputc(imgY_prev[i][j],p_out);

    for(i=0;i<img_height_cr/*img->height_cr*/;i++)
        for(j=0;j<img_width_cr/*img->width_cr*/;j++)
            fputc(imgUV_prev[0][i][j],p_out);

    for(i=0;i<img_height_cr/*img->height_cr*/;i++)
        for(j=0;j<img_width_cr/*img->width_cr*/;j++)
            fputc(imgUV_prev[1][i][j],p_out);

    fflush( p_out  );

}

#if TRACE
static int bitcounter = 0;

/*
*************************************************************************
* Function:Tracing bitpatterns for symbols
A code word has the following format: 0 Xn...0 X2 0 X1 0 X0 1
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

void tracebits(
               const char *trace_str,  //!< tracing information, char array describing the symbol
               int len,                //!< length of syntax element in bits
               int info,               //!< infoword of syntax element
               int value1)
{

    int i, chars;

    if(len>=34)
    {
        snprintf(errortext, ET_SIZE, "Length argument to put too long for trace to work");
        error (errortext, 600);
    }

    putc('@', p_trace);
    chars = fprintf(p_trace, "%i", bitcounter);
    while(chars++ < 6)
        putc(' ',p_trace);

    chars += fprintf(p_trace, "%s", trace_str);
    while(chars++ < 55)
        putc(' ',p_trace);

    // Align bitpattern
    if(len<15)
    {
        for(i=0 ; i<15-len ; i++)
            fputc(' ', p_trace);
    }

    // Print bitpattern
    for(i=0 ; i<len/2 ; i++)
    {
        fputc('0', p_trace);
    }
    // put 1
    fprintf(p_trace, "1");

    // Print bitpattern
    for(i=0 ; i<len/2 ; i++)
    {
        if (0x01 & ( info >> ((len/2-i)-1)))
            fputc('1', p_trace);
        else
            fputc('0', p_trace);
    }

    fprintf(p_trace, "  (%3d)\n", value1);
    bitcounter += len;

    fflush (p_trace);

}

/*
*************************************************************************
* Function:Tracing bitpatterns
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/


void tracebits2(
                const char *trace_str,  //!< tracing information, char array describing the symbol
                int len,                //!< length of syntax element in bits
                int info)
{

    int i, chars;

    if(len>=45)
    {
        snprintf(errortext, ET_SIZE, "Length argument to put too long for trace to work");
        error (errortext, 600);
    }

    putc('@', p_trace);
    chars = fprintf(p_trace, "%i", bitcounter);
    while(chars++ < 6)
        putc(' ',p_trace);
    chars += fprintf(p_trace, "%s", trace_str);
    while(chars++ < 55)
        putc(' ',p_trace);

    // Align bitpattern
    if(len<15)
        for(i=0 ; i<15-len ; i++)
            fputc(' ', p_trace);

    bitcounter += len;
    while (len >= 32)
    {
        for(i=0 ; i<8 ; i++)
        {
            fputc('0', p_trace);
        }
        len -= 8;

    }
    // Print bitpattern
    for(i=0 ; i<len ; i++)
    {
        if (0x01 & ( info >> (len-i-1)))
            fputc('1', p_trace);
        else
            fputc('0', p_trace);
    }

    fprintf(p_trace, "  (%3d)\n", info);

    fflush (p_trace);

}

/*
*************************************************************************
* Function:Tracing bitpatterns for symbols
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/


void tracebits3(
                const char *trace_str,  //!< tracing information, char array describing the symbol
                int len,                //!< length of syntax element in bits
                int info,               //!< infoword of syntax element
                int value1)
{
    int i, chars;

    if(len>=34)
    {
        snprintf(errortext, ET_SIZE, "Length argument to put too long for trace to work");
        error (errortext, 600);
    }

    putc('@', p_trace);
    chars = fprintf(p_trace, "%i", bitcounter);
    while(chars++ < 6)
        putc(' ',p_trace);

    chars += fprintf(p_trace, "%s", trace_str);
    while(chars++ < 55)
        putc(' ',p_trace);

    // Align bitpattern
    if(len<15)
    {
        for(i=0 ; i<15-len ; i++)
            fputc(' ', p_trace);
    }

    // Print bitpattern
    for(i=0 ; i<len ; i++)
    {
        if (0x01 & ( info >> (len-i-1)))
            fputc('1', p_trace);
        else
            fputc('0', p_trace);
    }

    fprintf(p_trace, "  (%3d)\n", value1);
    bitcounter += len;

    fflush (p_trace);

}
#endif

