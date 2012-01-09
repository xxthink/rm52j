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
#define INCLUDED_BY_CONFIGFILE_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "global.h"
#include "configfile.h"

#define MAX_ITEMS_TO_PARSE  10000

static char *GetConfigFileContent (char *Filename);
static void ParseContent (char *buf, int bufsize);
static int ParameterNameToMapIndex (char *s);
static void PatchInp ();

static void ProfileCheck();
static void LevelCheck();

/*
*************************************************************************
* Function:Parse the command line parameters and read the config files.
* Input: ac
number of command line parameters
av
command line parameters
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void Configure (int ac, char *av[])
{
    char *content;
    int CLcount, ContentLen, NumberParams;

    memset (&configinput, 0, sizeof (InputParameters));

    // Process default config file
    // Parse the command line

    CLcount = 1;
    while (CLcount < ac)
    {
        if (0 == strncmp (av[CLcount], "-f", 2))  // A file parameter?
        {
            content = GetConfigFileContent (av[CLcount+1]);
            printf ("Parsing Configfile %s", av[CLcount+1]);
            ParseContent (content, strlen (content));
            printf ("\n");
            free (content);
            CLcount += 2;
        } 
        else
        {
            if (0 == strncmp (av[CLcount], "-p", 2))  // A config change?
            {
                // Collect all data until next parameter (starting with -<x> (x is any character)),
                // put it into content, and parse content.
                CLcount++;
                ContentLen = 0;
                NumberParams = CLcount;

                // determine the necessary size for content
                while (NumberParams < ac && av[NumberParams][0] != '-')
                    ContentLen += strlen (av[NumberParams++]);        // Space for all the strings

                ContentLen += 1000;                     // Additional 1000 bytes for spaces and \0s

                if ((content = malloc (ContentLen))==NULL)
                    no_mem_exit("Configure: content");

                content[0] = '\0';

                // concatenate all parameters itendified before
                while (CLcount < NumberParams)
                {
                    char *source = &av[CLcount][0];
                    char *destin = &content[strlen (content)];

                    while (*source != '\0')
                    {
                        if (*source == '=')  // The Parser expects whitespace before and after '='
                        {
                            *destin++=' '; *destin++='='; *destin++=' ';  // Hence make sure we add it
                        } 
                        else
                            *destin++=*source;

                        source++;
                    }

                    *destin = '\0';
                    CLcount++;
                }
                printf ("Parsing command line string '%s'", content);
                ParseContent (content, strlen(content));
                free (content);
                printf ("\n");
            }
            else
            {
                snprintf (errortext, ET_SIZE, "Error in command line, ac %d, around string '%s', missing -f or -p parameters?", CLcount, av[CLcount]);
                error (errortext, 300);
            }
        }
    }

    printf ("\n");
}

/*
*************************************************************************
* Function: Alocate memory buf, opens file Filename in f, reads contents into
buf and returns buf
* Input:name of config file
* Output:
* Return: 
* Attention:
*************************************************************************
*/
char *GetConfigFileContent (char *Filename)
{
    unsigned FileSize;
    FILE *f;
    char *buf;

    if (NULL == (f = fopen (Filename, "r")))
    {
        snprintf (errortext, ET_SIZE, "Cannot open configuration file %s.\n", Filename);
        error (errortext, 300);
    }

    if (0 != fseek (f, 0, SEEK_END))
    {
        snprintf (errortext, ET_SIZE, "Cannot fseek in configuration file %s.\n", Filename);
        error (errortext, 300);
    }

    FileSize = ftell (f);

    if (FileSize < 0 || FileSize > 60000)
    {
        snprintf (errortext, ET_SIZE, "Unreasonable Filesize %d reported by ftell for configuration file %s.\n", FileSize, Filename);
        error (errortext, 300);
    }

    if (0 != fseek (f, 0, SEEK_SET))
    {
        snprintf (errortext, ET_SIZE, "Cannot fseek in configuration file %s.\n", Filename);
        error (errortext, 300);
    }

    if ((buf = malloc (FileSize + 1))==NULL)
        no_mem_exit("GetConfigFileContent: buf");

    // Note that ftell() gives us the file size as the file system sees it.  The actual file size,
    // as reported by fread() below will be often smaller due to CR/LF to CR conversion and/or
    // control characters after the dos EOF marker in the file.

    FileSize = fread (buf, 1, FileSize, f);
    buf[FileSize] = '\0';

    fclose (f);

    return buf;
}

/*
*************************************************************************
* Function: Parses the character array buf and writes global variable input, which is defined in
configfile.h.  This hack will continue to be necessary to facilitate the addition of
new parameters through the Map[] mechanism (Need compiler-generated addresses in map[]).
* Input:  buf
buffer to be parsed
bufsize
buffer size of buffer
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void ParseContent (char *buf, int bufsize)
{
    char *items[MAX_ITEMS_TO_PARSE];
    int MapIdx;
    int item     = 0;
    int InString = 0;
    int InItem   = 0;
    char *p      = buf;
    char *bufend = &buf[bufsize];
    int IntContent;
    int i;

    // Stage one: Generate an argc/argv-type list in items[], without comments and whitespace.
    // This is context insensitive and could be done most easily with lex(1).

    while (p < bufend)
    {
        switch (*p)
        {
        case 13:
            p++;
            break;
        case '#':                 // Found comment
            *p = '\0';              // Replace '#' with '\0' in case of comment immediately following integer or string
            while (*p != '\n' && p < bufend)  // Skip till EOL or EOF, whichever comes first
                p++;
            InString = 0;
            InItem = 0;
            break;
        case '\n':
            InItem = 0;
            InString = 0;
            *p++='\0';
            break;
        case ' ':
        case '\t':              // Skip whitespace, leave state unchanged
            if (InString)
                p++;
            else
            {                     // Terminate non-strings once whitespace is found
                *p++ = '\0';
                InItem = 0;
            }
            break;
        case '"':               // Begin/End of String
            *p++ = '\0';
            if (!InString)
            {
                items[item++] = p;
                InItem = ~InItem;
            }
            else
                InItem = 0;
            InString = ~InString; // Toggle
            break;
        default:
            if (!InItem)
            {
                items[item++] = p;
                InItem = ~InItem;
            }
            p++;
        }
    }

    item--;

    for (i=0; i<item; i+= 3)
    {
        if (0 > (MapIdx = ParameterNameToMapIndex (items[i])))
        {
            snprintf (errortext, ET_SIZE, " Parsing error in config file: Parameter Name '%s' not recognized.", items[i]);
            error (errortext, 300);
        }
        if (strcmp ("=", items[i+1]))
        {
            snprintf (errortext, ET_SIZE, " Parsing error in config file: '=' expected as the second token in each line.");
            error (errortext, 300);
        }

        // Now interprete the Value, context sensitive...
        switch (Map[MapIdx].Type)
        {
        case 0:           // Numerical
            if (1 != sscanf (items[i+2], "%d", &IntContent))
            {
                snprintf (errortext, ET_SIZE, " Parsing error: Expected numerical value for Parameter of %s, found '%s'.", items[i], items[i+2]);
                error (errortext, 300);
            }
            * (int *) (Map[MapIdx].Place) = IntContent;
            printf (".");
            break;
        case 1:
            strcpy ((char *) Map[MapIdx].Place, items [i+2]);
            printf (".");
            break;
        default:
            assert ("Unknown value type in the map definition of configfile.h");
        }
    }

    memcpy (input, &configinput, sizeof (InputParameters));
    PatchInp();
}

/*
*************************************************************************
* Function:Return the index number from Map[] for a given parameter name.
* Input:parameter name string
* Output:
* Return: the index number if the string is a valid parameter name,         \n
-1 for error
* Attention:
*************************************************************************
*/

static int ParameterNameToMapIndex (char *s)
{
    int i = 0;

    while (Map[i].TokenName != NULL)
        if (0==strcmp (Map[i].TokenName, s))
            return i;
        else
            i++;

    return -1;
};

// Added by LiShao, Tsinghua, 20070327
static void ProfileCheck()
{
    switch (input->profile_id)
    {
    case 0x20:
        {
            if(input->level_id!=0x10&&input->level_id!=0x20&&input->level_id!=0x22&&input->level_id!=0x40&&input->level_id!=0x42)
            {
                printf("\n Level_ID invalid or Unsupported. \n");
                exit(-1);
            }

            if(input->InterlaceCodingOption>2)
            {
                printf("\n The value of 'InterlaceCodingOption' is invalid! \n");
                exit(-1);
            }

            if(input->chroma_format!=1&&input->chroma_format!=2)
            {
                printf("\n Chroma_format invaild.\n");
                exit(-1);
            }
            break;
        }

    case 0x40: printf("\n Current Profile_ID is not 1.0 version. \n");  /*break*/exit(-1); //X ZHENG,20080515

    default: 
        {
            printf("\n Profile_ID invaild.\n"); 
            exit(-1);
        }
    }

}

// Added by LiShao, Tsinghua, 20070327
static void LevelCheck()
{
    float framerate[8]={24000/1001,24,25,30000/1001,30,50,60000/1001,60};
    if(input->frame_rate_code<1||input->frame_rate_code>8)
    {
        printf("\n Undefined frame_rate in all levels. \n");
        exit(0);
    }
    switch (input->level_id)
    {
    case 0x10:
        {
            if(input->img_width>352)
            {
                printf("\n Image Width exceeds level 2.0 restriction.\n");
                exit(-1);
            }
            else 
                if(input->img_height>288)
                {
                    printf("\n Image height exceeds level 2.0 restriction.\n");
                    exit(-1);
                }
                else
                {
                    if(input->frame_rate_code>5)
                    {
                        printf("\n Current frame_rate is not support in Level_2.0 .\n");
                        exit(-1);
                    }
                    else 
                    {
                        if((long)(input->img_width)*(long)(input->img_height)*(framerate[input->frame_rate_code-1])>2534400)
                        {
                            printf("\n Luma Sampling Rate invalid for level 2.0.\n");
                            exit(-1);
                        }
                    }
                }

                if((long)(input->img_width)*(long)(input->img_height)/256>396)
                {
                    printf("\n The number of macroblock per frame exceeds 396 in level 2.0.\n");
                    exit(-1);
                }

                if((long)(input->img_width)*(long)(input->img_height)*framerate[input->frame_rate_code-1]/256>11880)
                {
                    printf(" \n The number of macroblock per second exceeds 11880 in level 2.0.\n");
                    exit(-1);
                }

                if(input->chroma_format!=1)
                {
                    printf("\n In level 2.0 only format 4:2:0 is supported.\n");
                    exit(-1);
                }

                break;
        }

    case 0x20:
        {
            if(input->img_width>720)
            {
                printf("\n Image Width exceeds level 4.0's restriction.\n");
                exit(-1);
            }
            else 
                if(input->img_height>576)
                {
                    printf("\n Imgage Height exceeds level 4.0's restriction.\n");
                    exit(-1);
                }
                else
                {
                    if(input->frame_rate_code>5)
                    {
                        printf("\n Current frame_rate is not support in Level_4.0 .\n");
                        exit(-1);
                    }
                    else
                    {
                        if((long)(input->img_width)*(long)(input->img_height)*(long)(framerate[input->frame_rate_code-1])>10368000)
                        {
                            printf("\n Luma Sampling Rate invalid for level 4.0.\n");
                            exit(-1);
                        }    
                    }
                }

                if((long)(input->img_width)*(long)(input->img_height)/256>4050)
                {
                    printf(" \n The number of macroblock per frame exceeds 1,620 for Level_4.0.\n");
                    exit(-1);
                }

                if((long)(input->img_width)*(long)(input->img_height)*framerate[input->frame_rate_code-1]/256>40500)
                {
                    printf(" \n The number of macroblock per second exceeds 40500 in Level_4.0.\n");
                    exit(-1);
                }

                if(input->chroma_format!=1)
                {
                    printf("\n In level 4.0 only format 4:2:0 is supported.\n");
                    exit(-1);
                }

                break;
        }

    case 0x22:
        {
            if(input->img_width>720)
            {
                printf("\n Imgage Width exceeds level 4.2's restriction.\n");
                exit(-1);
            }
            else 
                if(input->img_height>576)
                {
                    printf("\n Image Height exceeds level 4.2's restriction.\n");
                    exit(-1);
                }
                else
                {
                    if(input->frame_rate_code>5)
                    {
                        printf("\n Current frame_rate is not support in Level_4.2 .\n");
                        exit(-1);
                    }
                    else
                    {
                        if((long)(input->img_width)*(long)(input->img_height)*(long)(framerate[input->frame_rate_code-1])>10368000)
                        {
                            printf("\n Luma Sampling Rate invalid for level 4.2.\n");
                            exit(-1);
                        }
                    }
                }
                if((long)(input->img_width)*(long)(input->img_height)/256>1620)
                {
                    printf(" \n The number of macroblock per frame exceeds 1,620 for Level_4.2.\n");
                    exit(-1);
                }

                if((long)(input->img_width)*(long)(input->img_height)*framerate[input->frame_rate_code-1]/256>40500)
                {
                    printf(" \n The number of macroblock per second exceeds 40500 in Level_4.2.\n");
                    exit(-1);
                }

                if(input->chroma_format!=1&&input->chroma_format!=2)
                {
                    printf("\n In level 4.2 only format 4:2:0 and 4:2:2 are supported.\n");
                    exit(-1);
                }

                break;
        }

    case 0x40:
        {
            if(input->img_width>1920)
            {
                printf("\n Imgage Width exceeds level 6.0's restriction.\n");
                exit(-1);
            }
            else 
                if(input->img_height>1152)
                {
                    printf("\n Image Height exceeds level 6.0's restriction.\n");
                    exit(-1);
                }
                else
                {
                    if((long)(input->img_width)*(long)(input->img_height)*(long)(framerate[input->frame_rate_code-1])>62668800)
                    {
                        printf("\n Luma Sampling Rate invalid for level 6.0.\n");
                        exit(-1);
                    }
                }  

                if((long)(input->img_width)*(long)(input->img_height)/256>8160)
                {
                    printf(" \n The number of macroblock per frame exceeds 8160 for Level_6.0.\n");
                    exit(-1);
                }

                if((long)(input->img_width)*(long)(input->img_height)*framerate[input->frame_rate_code-1]/256>244800)
                {
                    printf(" \n The number of macroblock per second exceeds 244800 in Level_6.0.\n");
                    exit(-1);
                }

                if(input->chroma_format!=1)
                {
                    printf("\n In level 6.0 only format 4:2:0 is supported.\n");
                    exit(-1);
                }

                break;
        }

    case 0x42:
        {
            if(input->img_width>1920)
            {
                printf("\n Imgage Width exceeds level 6.2's restriction.\n");
                exit(-1);
            }
            else 
                if(input->img_height>1152)
                {
                    printf("\n Imgage Height exceeds level 6.2's restriction.\n");
                    exit(-1);
                }
                else
                {
                    if((long)(input->img_width)*(long)(input->img_height)*(long)(framerate[input->frame_rate_code-1])>62668800)
                    {
                        printf("\n Luma Sampling Rate invalid for level 6.2.\n");
                        exit(-1);
                    }
                }

                if((long)(input->img_width)*(long)(input->img_height)/256>8160)
                {
                    printf(" \n The number of macroblock per frame exceeds 8160 for Level_6.2.\n");
                    exit(-1);
                }

                if((long)(input->img_width)*(long)(input->img_height)*framerate[input->frame_rate_code-1]/256>244800)
                {
                    printf(" \n The number of macroblock per second exceeds 244800 in Level_6.2.\n");
                    exit(-1);
                }

                if(input->chroma_format!=1&&input->chroma_format!=2)
                {
                    printf("\n In level 6.2 only format 4:2:0 and 4:2:2 are supported.\n");
                    exit(-1);
                }

                break;
        }

    default: 
        {
            printf("\n No such level_ID. \n");
            exit(-1);
        }
    }

}
/*
*************************************************************************
* Function:Checks the input parameters for consistency.
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

static void PatchInp ()
{
    // consistency check of QPs
    input->fixed_picture_qp = 1;
    if (input->qp0 > MAX_QP || input->qp0 < MIN_QP)
    {
        snprintf(errortext, ET_SIZE, "Error input parameter quant_0,check configuration file");
        error (errortext, 400);
    }

    if (input->qpN > MAX_QP || input->qpN < MIN_QP)
    {
        snprintf(errortext, ET_SIZE, "Error input parameter quant_n,check configuration file");
        error (errortext, 400);
    }

    if (input->qpB > MAX_QP || input->qpB < MIN_QP)
    {
        snprintf(errortext, ET_SIZE, "Error input parameter quant_B,check configuration file");
        error (errortext, 400);
    }

    // consistency check no_multpred
    if (input->no_multpred<1) 
        input->no_multpred=1;

    if (input->img_width % 16 != 0)   //add by wuzhongmou 0610
    {
        img->auto_crop_right = 16-(input->img_width % 16);
    }
    else
    {
        img->auto_crop_right=0;
    }
    if (input->InterlaceCodingOption!=FRAME_CODING)  //add by wuzhongmou 0610
    {
        if (input->img_height % 32 != 0)
        {
            img->auto_crop_bottom = 32-(input->img_height % 32);
        }
        else
        {
            img->auto_crop_bottom=0;
        }
    }
    else              
    {
        if (input->img_height % 16 != 0)     //add by wuzhongmou 0610
        {
            img->auto_crop_bottom = 16-(input->img_height % 16);
        }
        else
        {
            img->auto_crop_bottom=0;
        }
    }                                   //add by wuzhongmou 0610
    // check range of filter offsets
    if (input->alpha_c_offset > 6 || input->alpha_c_offset < -6)
    {
        snprintf(errortext, ET_SIZE, "Error input parameter LFAlphaC0Offset, check configuration file");
        error (errortext, 400);
    }

    if (input->beta_offset > 6 || input->beta_offset < -6)
    {
        snprintf(errortext, ET_SIZE, "Error input parameter LFBetaOffset, check configuration file");
        error (errortext, 400);
    }

    // Set block sizes
    input->blc_size[0][0]=16;
    input->blc_size[0][1]=16;
    input->blc_size[1][0]=16;
    input->blc_size[1][1]=16;
    input->blc_size[2][0]=16;
    input->blc_size[2][1]= 8;
    input->blc_size[3][0]= 8;
    input->blc_size[3][1]=16;
    input->blc_size[4][0]= 8;
    input->blc_size[4][1]= 8;

    // B picture consistency check
    if(input->successive_Bframe > input->jumpd)
    {
        snprintf(errortext, ET_SIZE, "Number of B-frames %d can not exceed the number of frames skipped", input->successive_Bframe);
        error (errortext, 400);
    }

    if(input->successive_Bframe==0)  //low_delay added by cjw 20070414
        input->low_delay=1;
    else
        input->low_delay=0;

    // Open Files
    if (strlen (input->infile) > 0 && (p_in=fopen(input->infile,"rb"))==NULL)
    {
        snprintf(errortext, ET_SIZE, "Input file %s does not exist",input->infile);    
    }

    if (strlen (input->ReconFile) > 0 && (p_dec=fopen(input->ReconFile, "wb"))==NULL)
    {
        snprintf(errortext, ET_SIZE, "Error open file %s", input->ReconFile);    
    }

    if (strlen (input->TraceFile) > 0 && (p_trace=fopen(input->TraceFile,"w"))==NULL)
    {
        snprintf(errortext, ET_SIZE, "Error open file %s", input->TraceFile);    
    }


    // frame/field consistency check

    // input intra period and Seqheader check Add cjw, 20070327
    if(input->seqheader_period!=0 && input->intra_period==0)
    {
        if(input->intra_period==0)
            snprintf(errortext, ET_SIZE, "\nintra_period  should not equal %d when seqheader_period equal %d", input->intra_period,input->seqheader_period);
        error (errortext, 400);
    }

    // input intra period and Seqheader check Add cjw, 20070327
    if(input->seqheader_period==0 && input->vec_period!=0)
    {
        snprintf(errortext, ET_SIZE, "\nvec_period  should not equal %d when seqheader_period equal %d", input->intra_period,input->seqheader_period);
        error (errortext, 400);
    }

    // Added by LiShao, Tsinghua, 20070327
    ProfileCheck();
    LevelCheck();
}


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
void DecideMvRange()
{
    if(input->profile_id==0x20)
    {
        switch(input->level_id) {
    case 0x10:
        Min_V_MV = -512;
        Max_V_MV =  511;
        Min_H_MV = -8192;
        Max_H_MV =  8191;
        break;
    case 0x20:
        if (img->picture_structure) {
            Min_V_MV = -1024;
            Max_V_MV =  1023;
        }
        else{
            Min_V_MV = -512;
            Max_V_MV =  511;
        }
        Min_H_MV = -8192;
        Max_H_MV =  8191;
        break;
    case 0x22:
        if (img->picture_structure) {
            Min_V_MV = -1024;
            Max_V_MV =  1023;
        }
        else{
            Min_V_MV = -512;
            Max_V_MV =  511;
        }
        Min_H_MV = -8192;
        Max_H_MV =  8191;
        break;
    case 0x40:
        if (img->picture_structure) {
            Min_V_MV = -2048;
            Max_V_MV =  2047;
        }
        else{
            Min_V_MV = -1024;
            Max_V_MV =  1023;
        }
        Min_H_MV = -8192;
        Max_H_MV =  8191;
        break;
    case 0x42:
        if (img->picture_structure) {
            Min_V_MV = -2048;
            Max_V_MV =  2047;
        }
        else{
            Min_V_MV = -1024;
            Max_V_MV =  1023;
        }
        Min_H_MV = -8192;
        Max_H_MV =  8191;
        break;
        }
    }
}
