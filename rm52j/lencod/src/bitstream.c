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
* Function:  Annex B Byte Stream format NAL Unit writing routines
*
*************************************************************************************
*/


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "global.h"
#include "bitstream.h"
#define MAXHEADERSIZE 100
#include "vlc.h"

static FILE *f = NULL;    // the output file

CopyRight CopyRights, *cp = &CopyRights;
CameraParamters CameraParameter, *camera = &CameraParameter;

int seqstuffingflag=1; //added by cjw AVS Zhuhai 20070122

extern StatParameters  stats;

////////////////////////////////////////////////////////////////////////////////
#ifdef SVA_START_CODE_EMULATION

unsigned char bit[8] = {0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};
OutputStream  ORABS;
OutputStream *pORABS = &ORABS;

void OpenORABS(OutputStream *p,char *fname)
{
    p->f = fopen(fname,"wb");
    if(p->f==NULL){printf ("\nCan't open file %s",fname);exit(-1);}

    p->uPreBytes      = 0xffffffff;
    p->iBytePosition    = 0;
    p->iBitOffset      = 0;
    p->iNumOfStuffBits    = 0;
    p->iBitsCount      = 0;
}
void CloseORABS(OutputStream *p)
{
    if(p->iBitOffset)  fwrite(p->buf,1,p->iBytePosition+1,p->f);
    else        fwrite(p->buf,1,p->iBytePosition  ,p->f);
    fclose(p->f);
}
void FlushORABS(OutputStream *p)
{
    fflush(p->f);
}


int write_1_bit(OutputStream *p,int b)
{
    int i;

    if(p->iBytePosition == SVA_STREAM_BUF_SIZE)
    {
        i = fwrite(p->buf,1,SVA_STREAM_BUF_SIZE,p->f);
        if(i!=SVA_STREAM_BUF_SIZE)
        {
            printf ("Fatal: write file error, exit (-1)\n");
            exit (-1);
        }
        p->iBytePosition  = 0;
        p->iBitOffset    = 0;
    }
    p->uPreBytes <<= 1;
    if(b)
    {
        p->buf[p->iBytePosition] |= bit[p->iBitOffset];
        p->uPreBytes |= 1;
    }
    else
    {
        p->buf[p->iBytePosition] &= (~bit[p->iBitOffset]);
    }
    p->iBitOffset++;
    if(p->iBitOffset==8)
    {
        p->iBitOffset = 0;
        p->iBytePosition++;
    }
    p->iBitsCount++;
    return 0;
}
int write_n_bit(OutputStream *p,int b,int n)
{
    if(n>30) return 1;
    while(n>0)
    {
        write_1_bit(p,b&(0x01<<(n-1)));
        n--;
    }
    return 0;
}

// added by Yulj 2004.07.16
// one bit "1" is added to the end of stream, then some bits "0" are added to bytealigned position.
int write_align_stuff(OutputStream *p)
{
    unsigned char c;

    c = 0xff << ( 8 - p->iBitOffset );
    p->buf[p->iBytePosition] = ( c & p->buf[p->iBytePosition] ) | (0x80>>(p->iBitOffset));
    p->iBitsCount += 8 - p->iBitOffset;
    p->uPreBytes  = (p->uPreBytes << (8 - p->iBitOffset)) & c;
    p->iNumOfStuffBits  += 8 - p->iBitOffset;
    p->iBitOffset = 0;
    p->iBytePosition++;
    return 0;
}
//---end
int write_start_code(OutputStream *p,unsigned char code)
{
    int i;

    //modified by cjw&Zhenjunhao AVS Zhuhai 20070122
    if (p->iBitOffset || seqstuffingflag)  // 0x80 should inserted before the first seq-header
        write_align_stuff(p);

    seqstuffingflag = 0;                       // only work for the first seq-header

    if(p->iBytePosition >= SVA_STREAM_BUF_SIZE-4 && p->iBytePosition >0 )
    {
        i = fwrite(p->buf,1,p->iBytePosition,p->f);
        if(i != p->iBytePosition){printf ("\nWrite file error");exit (-1);}
        p->iBytePosition  = 0;
        p->iBitOffset    = 0;
    }
    p->buf[p->iBytePosition  ] = 0;
    p->buf[p->iBytePosition+1] = 0;
    p->buf[p->iBytePosition+2] = 1;
    p->buf[p->iBytePosition+3] = code;
    p->iBytePosition += 4;
    p->iBitsCount += 32;
    p->uPreBytes  = (unsigned int)code + 256;

    return 0;
}

/*
*************************************************************************
* Function:Open the output file for the bytestream    
* Input: The filename of the file to be opened
* Output:
* Return: none.Function terminates the program in case of an error
* Attention:
*************************************************************************
*/


void OpenBitStreamFile(char *Filename)
{
    OpenORABS(pORABS,Filename);
}
void CloseBitStreamFile()
{
    CloseORABS(pORABS);
}

/*
*************************************************************************
* Function:Write video edit code
* Input:
* Output:
* Return: 32bit for video edit code
* Attention:
*************************************************************************
*/

int WriteVideoEditCode()
{
    Bitstream *bitstream;
    byte VideoEditCode[32];
    int  bitscount=0;

    if ((bitstream=calloc(1, sizeof(Bitstream)))==NULL)
        no_mem_exit("Seuqence Header: bitstream");

    bitstream->streamBuffer = VideoEditCode;
    bitstream->bits_to_go = 8;

    bitscount = u_v(32, "video_edit_code",0x1B7,bitstream);    

    write_start_code(pORABS, 0xb7);    //modified by cjw AVS Zhuhai 20070122

    free(bitstream);

    return bitscount;
}

/*
*************************************************************************
* Function:Write sequence header information
* Input:
* Output:
* Return: sequence header length, including stuffing bits
* Attention:
*************************************************************************
*/

int WriteSequenceHeader()
{
    Bitstream *bitstream;
    byte SequenceHeader[MAXHEADERSIZE];
    int  bitscount=0;
    int  stuffbits;
    int  i,j,k;
    if ((bitstream=calloc(1, sizeof(Bitstream)))==NULL)
        no_mem_exit("Seuqence Header: bitstream");

    bitstream->streamBuffer = SequenceHeader;
    bitstream->bits_to_go = 8;

    //  input->profile_id = 0x20;    // Commented by cjw, 20070327
    //  input->level_id   = 0x42;    // Commented by cjw, 20070327

    input->display_horizontal_size = img->width;  //add by wuzhongmou 0610
    input->display_vertical_size   = img->height; //add by wuzhongmou 0610
    input->sample_precision=1;//add by xfwang 2004.7.29
    input->bbv_buffer_size=122880;//here we random give a value,but in fact it is not true.  //add by wuzhongmou 200612
    input->aspect_ratio_information=1;    //add by wuzhongmou 0610
    input->bit_rate_lower=(input->bit_rate/400)&(0x3FFFF);  //add by wuzhongmou 0610
    input->bit_rate_upper=(input->bit_rate/400)>>18;    //add by wuzhongmou 0610

    //bitscount+=u_v(8,"stuffing bit",0x80,bitstream);    //add by wuzhongmou 200612 commented by cjw AVS Zhuhai 20070122
    bitscount+=u_v(32,"seqence start code",0x1b0,bitstream);
    bitscount+=u_v(8,"profile_id",input->profile_id,bitstream);
    bitscount+=u_v(8,"level_id",input->level_id,bitstream);

    bitscount+=u_v(1,"progressive_sequence",input->progressive_frame,bitstream); //add by wuzhongmou 0610

    //Carmen, 2007/12/20, Bug Fix: correct picture size for outputted reconstructed pictures
#ifdef _BUGFIX_NON16MUL_PICSIZE
    bitscount+=u_v(14,"picture width",(img->width-img->auto_crop_right),bitstream); //add by wuzhongmou 0610
    bitscount+=u_v(14,"picture height",(img->height-img->auto_crop_bottom),bitstream); //add by wuzhongmou 0610
#else
    bitscount+=u_v(14,"picture width",img->width,bitstream); //add by wuzhongmou 0610
    bitscount+=u_v(14,"picture height",img->height,bitstream); //add by wuzhongmou 0610
#endif
    //Carmen, 2007/12/20, Bug Fix: correct picture size for outputted reconstructed pictures

    bitscount+=u_v(2,"chroma foramt",input->chroma_format,bitstream);
    bitscount+=u_v(3,"sample precision",input->sample_precision,bitstream);
    bitscount+=u_v(4,"aspect ratio information",input->aspect_ratio_information,bitstream);
    bitscount+=u_v(4,"frame rate code",input->frame_rate_code,bitstream);

    //input->bit_rate_lower = input->bit_rate_upper = 0;
    //xyji 12.23
    bitscount+=u_v(18,"bit rate lower",input->bit_rate_lower,bitstream);
    bitscount+=u_v(1,"marker bit",1,bitstream);
    bitscount+=u_v(12,"bit rate upper",input->bit_rate_upper,bitstream);
    bitscount+=u_v(1,"low delay",input->low_delay,bitstream);
    bitscount+=u_v(1,"marker bit",1,bitstream);
    bitscount+=u_v(18,"bbv buffer size",input->bbv_buffer_size,bitstream);

    bitscount+=u_v(3,"reserved bits",0,bitstream);

    k = bitscount >> 3;
    j = bitscount % 8;

    stuffbits = 8-(bitscount%8);

    if (stuffbits<8)
        bitscount+=u_v(stuffbits,"stuff bits for byte align",0,bitstream);

    write_start_code(pORABS, 0xb0);    //modified by cjw AVS Zhuhai 20070122

    for(i=4;i<k;i++)  
        write_n_bit(pORABS,SequenceHeader[i],8); 

    write_n_bit(pORABS,SequenceHeader[k],j);
    write_align_stuff(pORABS);

    free(bitstream);

    return bitscount;
}

/*
*************************************************************************
* Function:Write sequence header information
* Input:
* Output:
* Return: sequence header length, including stuffing bits
* Attention:
*************************************************************************
*/


int WriteSliceHeader(int slice_nr, int slice_qp)
{
    Bitstream *bitstream;
    byte SequenceHeader[MAXHEADERSIZE];
    int  bitscount=0;
    int  stuffbits;
    int  i,j,k;

    if ((bitstream=calloc(1, sizeof(Bitstream)))==NULL)
        no_mem_exit("Seuqence Header: bitstream");

    bitstream->streamBuffer = SequenceHeader;
    bitstream->bits_to_go = 8;

    bitscount+=u_v(24,"start code prefix",1,bitstream);
    if(input->img_width > 2800)
        bitscount+=u_v(3, "slice vertical position extension",0,bitstream);

    bitscount+=u_v(8, "slice start code",slice_nr,bitstream);

    if (!input->fixed_picture_qp)
    {
        bitscount += u_v(1,"fixed_slice_qp",0,bitstream);//  [5/8/2007 Leeswan]
        bitscount += u_v(6,"slice_qp",slice_qp,bitstream);
    }

    k = bitscount >> 3;
    j = bitscount % 8;

    stuffbits = 8-(bitscount%8);

    if (stuffbits<8)
        bitscount+=u_v(stuffbits,"stuff bits for byte align",0,bitstream);

    write_start_code(pORABS, (unsigned char) slice_nr);
    for(i=4;i<k;i++)
        write_n_bit(pORABS,SequenceHeader[i],8);
    write_n_bit(pORABS,SequenceHeader[k],j);
    write_align_stuff(pORABS);

    //fwrite(SequenceHeader,1,(bitscount)/8,f);
    free(bitstream);

    return bitscount;
}


int WriteSequenceEnd()
{
    write_start_code(pORABS, 0xb1);
    return 32;
}

/*
*************************************************************************
* Function:Write sequence display extension information
* Input:
* Output:
* Return: sequence display extension information lenght
* Attention:
*************************************************************************
*/


int WriteSequenceDisplayExtension()
{
    Bitstream *bitstream;
    byte SequenceDisplayExtension[MAXHEADERSIZE];
    int  bitscount=0;
    int  stuffbits;
    int  i,j,k;

    if ((bitstream=calloc(1, sizeof(Bitstream)))==NULL)
        no_mem_exit("Sequence Display Extension: bitstream");
    input->video_format=1;
    input->video_range=1;
    input->display_horizontal_size=1920;
    input->display_vertical_size=1280;

    bitstream->streamBuffer = SequenceDisplayExtension;
    bitstream->bits_to_go = 8;

    bitscount+=u_v(32,"sequence display extension start code",0x1b5,bitstream);
    bitscount+=u_v(4,"extension id",2,bitstream);
    bitscount+=u_v(3,"video format",input->video_format,bitstream);
    bitscount+=u_v(1,"video range",input->video_range,bitstream);
    bitscount+=u_v(1,"color description",input->color_description,bitstream);

    if(input->color_description)
    {
        bitscount+=u_v(8,"color primaries",input->color_primaries,bitstream);
        bitscount+=u_v(8,"transfer characteristics",input->transfer_characteristics,bitstream);
        bitscount+=u_v(8,"matrix coefficients",input->matrix_coefficients,bitstream);
    }

    bitscount+=u_v(14,"display horizontal size",input->display_horizontal_size,bitstream);
    //xyji 12.23
    bitscount+=u_v(1,"marker bit",1,bitstream);
    bitscount+=u_v(14,"display vertical size", input->display_vertical_size,bitstream);
    //xyji 12.23
    bitscount+=u_v(2,"reserved bits",0,bitstream);

    k = bitscount >> 3;
    j = bitscount % 8;

    stuffbits = 8-(bitscount%8);

    if (stuffbits<8)
    {
        bitscount+=u_v(stuffbits,"stuff bits for byte align",0,bitstream);
    }

    write_start_code(pORABS, 0xb5);
    for(i=4;i<k;i++)
        write_n_bit(pORABS,SequenceDisplayExtension[i],8);
    write_n_bit(pORABS,SequenceDisplayExtension[k],j);
    write_align_stuff(pORABS);
    //  fwrite(SequenceDisplayExtension,1,bitscount/8,f);

    free(bitstream);

    return bitscount;
}

/*
*************************************************************************
* Function:Write copyright extension information
* Input:
* Output:
* Return: copyright extension information lenght
* Attention:
*************************************************************************
*/
int WriteCopyrightExtension()
{
    Bitstream *bitstream;
    byte CopyrightExtension[MAXHEADERSIZE];
    int  bitscount=0;
    int  stuffbits;
    int  i,j,k;

    if ((bitstream=calloc(1, sizeof(Bitstream)))==NULL)
        no_mem_exit("Copyright Extension: bitstream");

    bitstream->streamBuffer = CopyrightExtension;
    bitstream->bits_to_go = 8;

    bitscount+=u_v(32,"copyright extension start code",0x1b5,bitstream);
    bitscount+=u_v(4,"extension id",4,bitstream);
    bitscount+=u_v(1,"copyright flag",cp->copyright_flag,bitstream);
    bitscount+=u_v(8,"copyright id",cp->copyright_id,bitstream);
    bitscount+=u_v(1,"original or copy",cp->original_or_copy,bitstream);

    bitscount+=u_v(7,"reserved_bits",0,bitstream);
    bitscount+=u_v(1,"marker bit", 1,bitstream);

    bitscount+=u_v(20,"copyright number 1",cp->copyright_number,bitstream);
    bitscount+=u_v(1,"marker bit", 1,bitstream);
    bitscount+=u_v(22,"copyright number 2",cp->copyright_number,bitstream);
    bitscount+=u_v(1,"marker bit", 1,bitstream);
    bitscount+=u_v(22,"copyright number 3",cp->copyright_number,bitstream);

    k = bitscount >> 3;
    j = bitscount % 8;

    stuffbits = 8-(bitscount%8);

    if (stuffbits<8)
    {
        bitscount+=u_v(stuffbits,"stuff bits for byte align",0,bitstream);
    }

    write_start_code(pORABS, 0xb5);
    for(i=4;i<k;i++)
        write_n_bit(pORABS,CopyrightExtension[i],8);
    write_n_bit(pORABS,CopyrightExtension[k],j);
    write_align_stuff(pORABS);
    //  fwrite(CopyrightExtension,1,bitscount/8,f);

    free(bitstream);

    return bitscount;
}


/*
*************************************************************************
* Function:Write camera parameter extension information
* Input:
* Output:
* Return: camera parameter  extension information lenght
* Attention:
*************************************************************************
*/
int WriteCameraParametersExtension()
{
    Bitstream *bitstream;
    byte CameraParametersExtension[MAXHEADERSIZE];
    int  bitscount=0;
    int  stuffbits;
    int  i,j,k;

    if ((bitstream=calloc(1, sizeof(Bitstream)))==NULL)
        no_mem_exit("Camera Parameters Extension: bitstream");

    bitstream->streamBuffer = CameraParametersExtension;
    bitstream->bits_to_go = 8;

    bitscount+=u_v(32,"camera parameters extension start code",0x1b5,bitstream);
    bitscount+=u_v(4,"extension id",11,bitstream);
    bitscount+=u_v(1,"reserved_bits",0,bitstream);
    bitscount+=u_v(7,"camera id",camera->camera_id, bitstream);
    bitscount+=u_v(1,"marker bit",1, bitstream);

    bitscount+=u_v(22,"height_of_image_device",camera->height_of_image_device,bitstream);
    bitscount+=u_v(1,"marker bit",1, bitstream);

    bitscount+=u_v(22,"focal_length",camera->focal_length,bitstream);
    bitscount+=u_v(1,"marker bit",1, bitstream);

    bitscount+=u_v(22,"f_number",camera->f_number,bitstream);
    bitscount+=u_v(1,"marker bit",1, bitstream);

    bitscount+=u_v(22,"vertical_angle_of_view",camera->vertical_angle_of_view,bitstream);
    bitscount+=u_v(1,"marker bit",1, bitstream);

    bitscount+=u_v(16,"camera_position_x_upper",camera->camera_direction_x,bitstream);
    bitscount+=u_v(1,"marker bit",1, bitstream);
    bitscount+=u_v(16,"camera_position_x_lower",camera->camera_direction_x,bitstream);
    bitscount+=u_v(1,"marker bit",1, bitstream);

    bitscount+=u_v(16,"camera_position_y_upper",camera->camera_direction_y,bitstream);
    bitscount+=u_v(1,"marker bit",1, bitstream);

    bitscount+=u_v(16,"camera_position_y_lower",camera->camera_direction_y,bitstream);
    bitscount+=u_v(1,"marker bit",1, bitstream);

    bitscount+=u_v(16,"camera_position_z_upper",camera->camera_direction_z,bitstream);
    bitscount+=u_v(1,"marker bit",1, bitstream);
    bitscount+=u_v(16,"camera_position_z_lower",camera->camera_direction_z,bitstream);
    bitscount+=u_v(1,"marker bit",1, bitstream);

    bitscount+=u_v(22,"camera_direction_x",camera->camera_direction_x,bitstream);
    bitscount+=u_v(1,"marker bit",1, bitstream);

    bitscount+=u_v(22,"camera_direction_y",camera->camera_direction_y,bitstream);
    bitscount+=u_v(1,"marker bit",1, bitstream);

    bitscount+=u_v(22,"camera_direction_z",camera->camera_direction_z,bitstream);
    bitscount+=u_v(1,"marker bit",1, bitstream);

    bitscount+=u_v(22,"image_plane_vertical_x",camera->image_plane_vertical_x,bitstream);
    bitscount+=u_v(1,"marker bit",1, bitstream);

    bitscount+=u_v(22,"image_plane_vertical_y",camera->image_plane_vertical_y,bitstream);
    bitscount+=u_v(1,"marker bit",1, bitstream);

    bitscount+=u_v(22,"image_plane_vertical_z",camera->image_plane_vertical_z,bitstream);
    bitscount+=u_v(1,"marker bit",1, bitstream);

    bitscount+=u_v(32,"reserved_bits",0,bitstream);

    k = bitscount >> 3;
    j = bitscount % 8;

    stuffbits = 8-(bitscount%8);

    if (stuffbits<8)
    {
        bitscount+=u_v(stuffbits,"stuff bits for byte align",0,bitstream);
    }

    write_start_code(pORABS, 0xb5);
    for(i=4;i<k;i++)
        write_n_bit(pORABS,CameraParametersExtension[i],8);
    write_n_bit(pORABS,CameraParametersExtension[k],j);
    write_align_stuff(pORABS);
    //  fwrite(CameraParametersExtension,1,bitscount/8,f);

    free(bitstream);

    return bitscount;
}

/*
*************************************************************************
* Function:Write user data
* Input:
* Output:
* Return: user data length
* Attention:
*************************************************************************
*/

int WriteUserData(char *userdata)
{
    Bitstream *bitstream;
    byte UserData[MAXHEADERSIZE];
    int  bitscount=0;

    if ((bitstream=calloc(1, sizeof(Bitstream)))==NULL) no_mem_exit("User data: bitstream");
    bitstream->streamBuffer = UserData;
    bitstream->bits_to_go = 8;

    bitscount += u_v(32,"user data start code", 0x1b2,bitstream);
    write_start_code(pORABS, 0xb2);
    while (*userdata)
    {
        write_n_bit(pORABS,*userdata,8);
        bitscount += u_v(8,"user data", *userdata++,bitstream);
    }
    write_align_stuff(pORABS);
    //  fwrite(UserData,1,bitscount/8,f);
    free(bitstream);

    return bitscount;
}

/*
*************************************************************************
* Function:Write bit steam to file
* Input:
* Output:
* Return: none
* Attention:
*************************************************************************
*/

void WriteBitstreamtoFile()
{
    int n, i;
    n = currBitStream->byte_pos;

    // added by jlzheng 6.30
    for(i=0;i<n;i++)
    {
        if(currBitStream->streamBuffer[i]==0 && currBitStream->streamBuffer[i+1]==0 && currBitStream->streamBuffer[i+2]==1)  
        {
            write_start_code(pORABS, currBitStream->streamBuffer[i+3]);
            i=i+4;
        }                                      

        write_n_bit(pORABS, currBitStream->streamBuffer[i],8);
    }    


    //   write_align_stuff(pORABS);       //commented by cjw AVS 20070204

    //bytecount = fwrite(currBitStream->streamBuffer,1,currBitStream->byte_pos,f);
    stats.bit_ctr += 8*n;
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////////
/*
*************************************************************************
* Function:
* Input:
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
/*
*************************************************************************
* Function:
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/
int start_sequence()
{
    int len = 0;
    char id_string[255] = "AVS test stream";
    if(img->number==0)    //Added by cjw, 20070327
        OpenBitStreamFile(input->outfile);
    len = WriteSequenceHeader();
    len += WriteSequenceDisplayExtension();
    len += WriteCopyrightExtension();
    len += WriteCameraParametersExtension();
    if (strlen(id_string) > 1)
        len += WriteUserData(id_string);
    return len;
}
/*
*************************************************************************
* Function:
* Input:
* Output:
* Return: 
* Attention:Mainly flushing of everything Add termination symbol, etc.
*************************************************************************
*/
int terminate_sequence()
{
    int len;
    len = WriteSequenceEnd();

    //Carmen, 2007/12/19, Bug Fix: No sequence end codes when stream has multiple sequences
    if (img->EncodeEnd_flag==1)      
        CloseBitStreamFile();
    return len;   // make lint happy
}

