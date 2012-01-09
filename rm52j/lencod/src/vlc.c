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

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "golomb.h"
#include "vlc.h"

extern StatParameters  stats;

const int NCBP[64][2]=                                     // jlzheng 7.20
{
    { 4, 0},{16, 19},{17, 16},{19, 15},{14, 18},{9, 11},{22,31},{ 8,13},
    {11, 17},{21,30},{10,12},{ 7,9}, {12,10},{ 6,7},{ 5,8},{ 1,1},
    {35, 4},{47,42},{48,38},{38,27},{46,39},{36,33},{50,59},{26,26},
    {45,40},{52,58},{41,35},{28,25},{37,29},{23,24},{31,28},{ 2,3},
    {43, 5},{51,51},{56,52},{39,37}, {55,50},{33,43},{62,63},{27,44},
    {54,53},{60,62},{40,48},{32,47},{42,34},{24,45},{29,49},{ 3,6},
    {49, 14},{53,55},{57,56},{25,36},{58,54},{30,41},{59,60},{ 15,21},
    {61,57},{63,61},{44,46},{18,22}, {34,32},{13,20},{20,23},{ 0,2}
};

/*
*************************************************************************
* Function:ue_v, writes an ue(v) syntax element, returns the length in bits
* Input:
tracestring
the string for the trace file
value
the value to be coded
bitstream
the Bitstream the value should be coded into
* Output:
* Return:  Number of bits used by the coded syntax element
* Attention: This function writes always the bit buffer for the progressive scan flag, and
should not be used (or should be modified appropriately) for the interlace crap
When used in the context of the Parameter Sets, this is obviously not a
problem.
*************************************************************************
*/

int ue_v (char *tracestring, int value, Bitstream *bitstream)
{
    SyntaxElement symbol, *sym=&symbol;
    sym->type = SE_HEADER;
    sym->mapping = ue_linfo;               // Mapping rule: unsigned integer
    sym->value1 = value;

#if TRACE
    strncpy(sym->tracestring,tracestring,TRACESTRING_SIZE);
#endif

    return writeSyntaxElement_UVLC (sym, bitstream);
}

/*
*************************************************************************
* Function:ue_v, writes an ue(v) syntax element, returns the length in bits
* Input:
tracestring
the string for the trace file
value
the value to be coded
bitstream
the Bitstream the value should be coded into
* Output:
* Return: Number of bits used by the coded syntax element
* Attention:
This function writes always the bit buffer for the progressive scan flag, and
should not be used (or should be modified appropriately) for the interlace crap
When used in the context of the Parameter Sets, this is obviously not a
problem.
*************************************************************************
*/

int se_v (char *tracestring, int value, Bitstream *bitstream)
{
    SyntaxElement symbol, *sym=&symbol;
    sym->type = SE_HEADER;
    sym->mapping = se_linfo;               // Mapping rule: signed integer
    sym->value1 = value;

#if TRACE
    strncpy(sym->tracestring,tracestring,TRACESTRING_SIZE);
#endif

    return writeSyntaxElement_UVLC (sym, bitstream);
}

/*
*************************************************************************
* Function: u_1, writes a flag (u(1) syntax element, returns the length in bits, 
always 1
* Input:
tracestring
the string for the trace file
value
the value to be coded
bitstream
the Bitstream the value should be coded into
* Output:
* Return: Number of bits used by the coded syntax element (always 1)
* Attention:This function writes always the bit buffer for the progressive scan flag, and
should not be used (or should be modified appropriately) for the interlace crap
When used in the context of the Parameter Sets, this is obviously not a
problem.
*************************************************************************
*/

int u_1 (char *tracestring, int value, Bitstream *bitstream)
{
    SyntaxElement symbol, *sym=&symbol;

    sym->bitpattern = value;
    sym->len = 1;
    sym->type = SE_HEADER;
    sym->value1 = value;

#if TRACE
    strncpy(sym->tracestring,tracestring,TRACESTRING_SIZE);
#endif

    return writeSyntaxElement_fixed(sym, bitstream);
}

/*
*************************************************************************
* Function:u_v, writes a a n bit fixed length syntax element, returns the length in bits, 
* Input:
tracestring
the string for the trace file
value
the value to be coded
bitstream
the Bitstream the value should be coded into
* Output:
* Return: Number of bits used by the coded syntax element 
* Attention:This function writes always the bit buffer for the progressive scan flag, and
should not be used (or should be modified appropriately) for the interlace crap
When used in the context of the Parameter Sets, this is obviously not a
problem.
*************************************************************************
*/
int u_v (int n, char *tracestring, int value, Bitstream *bitstream)
{
    SyntaxElement symbol, *sym=&symbol;

    sym->bitpattern = value;
    sym->len = n;
    sym->type = SE_HEADER;
    sym->value1 = value;

#if TRACE
    strncpy(sym->tracestring,tracestring,TRACESTRING_SIZE);
#endif

    return writeSyntaxElement_fixed(sym, bitstream);
}


/*
*************************************************************************
* Function:mapping for ue(v) syntax elements
* Input:
ue
value to be mapped
info
returns mapped value
len
returns mapped value length
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void ue_linfo(int ue, int dummy, int *len,int *info)
{
    int i,nn;

    nn=(ue+1)/2;

    for (i=0; i < 16 && nn != 0; i++)
    {
        nn /= 2;
    }

    *len= 2*i + 1;
    *info=ue+1-(int)pow(2,i);

}

/*
*************************************************************************
* Function:mapping for ue(v) syntax elements
* Input:
ue
value to be mapped
info
returns mapped value
len
returns mapped value length
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void se_linfo(int se, int dummy, int *len,int *info)
{

    int i,n,sign,nn;

    sign=0;

    if (se <= 0)
    {
        sign=1;
    }
    n=abs(se) << 1;

    //n+1 is the number in the code table.  Based on this we find length and info

    nn=n/2;
    for (i=0; i < 16 && nn != 0; i++)
    {
        nn /= 2;
    }
    *len=i*2 + 1;
    *info=n - (int)pow(2,i) + sign;

}

/*
*************************************************************************
* Function:
* Input:Number in the code table
* Output:lenght and info
* Return: 
* Attention:
*************************************************************************
*/

void cbp_linfo_intra(int cbp, int dummy, int *len,int *info)
{
    //extern const int NCBP[48][2];
    extern const int NCBP[64][2];
    ue_linfo(NCBP[cbp][0], dummy, len, info);
}


/*
*************************************************************************
* Function:
* Input:Number in the code table
* Output:lenght and info
* Return: 
* Attention:
*************************************************************************
*/

void cbp_linfo_inter(int cbp, int dummy, int *len,int *info)
{
    extern const int NCBP[64][2];
    ue_linfo(NCBP[cbp][1], dummy, len, info);
}

/*
*************************************************************************
* Function:Makes code word and passes it back
A code word has the following format: 0 0 0 ... 1 Xn ...X2 X1 X0.
* Input: Info   : Xn..X2 X1 X0                                             \n
Length : Total number of bits in the codeword
* Output:
* Return: 
* Attention:this function is called with sym->inf > (1<<(sym->len/2)). 
The upper bits of inf are junk
*************************************************************************
*/

int symbol2uvlc(SyntaxElement *sym)
{
    int suffix_len=sym->len/2;  
    sym->bitpattern = (1<<suffix_len)|(sym->inf&((1<<suffix_len)-1));
    return 0;
}

/*
*************************************************************************
* Function:generates UVLC code and passes the codeword to the buffer
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int writeSyntaxElement_UVLC(SyntaxElement *se, Bitstream *bitstream)
{
    if( se->golomb_maxlevels && (se->type==SE_LUM_DC_INTRA||se->type==SE_LUM_AC_INTRA||se->type==SE_LUM_DC_INTER||se->type==SE_LUM_AC_INTER) )
        return writeSyntaxElement_GOLOMB(se,bitstream);

    if(se->type == SE_REFFRAME)
    {
        se->bitpattern = se->value1;
        se->len = 1;
    }
    else
    {
        se->mapping(se->value1,se->value2,&(se->len),&(se->inf));
        symbol2uvlc(se);
    }

    writeUVLC2buffer(se, bitstream);

#if TRACE
    if(se->type <= 1)
        trace2out (se);
#endif

    return (se->len);
}

/*
*************************************************************************
* Function:passes the fixed codeword to the buffer
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/
int writeSyntaxElement_fixed(SyntaxElement *se, Bitstream *bitstream)
{  
    writeUVLC2buffer(se, bitstream);

#if TRACE
    if(se->type <= 1)
        trace2out (se);
#endif

    return (se->len);
}

/*
*************************************************************************
* Function:generates code and passes the codeword to the buffer
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/
int writeSyntaxElement_Intra4x4PredictionMode(SyntaxElement *se, Bitstream *bitstream)
{
    if (se->value1 == -1)
    {
        se->len = 1;
        se->inf = 1;
    }
    else 
    {
        se->len = 3;  
        se->inf = se->value1;
    }

    se->bitpattern = se->inf;
    writeUVLC2buffer(se, bitstream);

#if TRACE
    if(se->type <= 1)
        trace2out (se);
#endif

    return (se->len);
}

/*
*************************************************************************
* Function:writes UVLC code to the appropriate buffer
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void  writeUVLC2buffer(SyntaxElement *se, Bitstream *currStream)
{
    int i;
    unsigned int mask = 1 << (se->len-1);

    // Add the new bits to the bitstream.
    // Write out a byte if it is full
    for (i=0; i<se->len; i++)
    {
        currStream->byte_buf <<= 1;
        if (se->bitpattern & mask)
            currStream->byte_buf |= 1;
        currStream->bits_to_go--;
        mask >>= 1;
        if (currStream->bits_to_go==0)
        {
            currStream->bits_to_go = 8;
            currStream->streamBuffer[currStream->byte_pos++]=currStream->byte_buf;
            currStream->byte_buf = 0;
        }
    }

}

/*
*************************************************************************
* Function:generates UVLC code and passes the codeword to the buffer
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int writeSyntaxElement2Buf_Fixed(SyntaxElement *se, Bitstream* this_streamBuffer )
{
    writeUVLC2buffer(se, this_streamBuffer );

#if TRACE
    if(se->type <= 1)
        trace2out (se);
#endif

    return (se->len);
}

/*
*************************************************************************
* Function:Makes code word and passes it back
* Input:
Info   : Xn..X2 X1 X0                                             \n
Length : Total number of bits in the codeword
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int symbol2vlc(SyntaxElement *sym)
{
    int info_len = sym->len;

    // Convert info into a bitpattern int
    sym->bitpattern = 0;

    // vlc coding
    while(--info_len >= 0)
    {
        sym->bitpattern <<= 1;
        sym->bitpattern |= (0x01 & (sym->inf >> info_len));
    }

    return 0;
}

/*
*************************************************************************
* Function:write VLC for Run Before Next Coefficient, VLC0
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int writeSyntaxElement_Run(SyntaxElement *se, Bitstream *bitstream)
{
    int lentab[TOTRUN_NUM][16] = 
    {
        {1,1},
        {1,2,2},
        {2,2,2,2},
        {2,2,2,3,3},
        {2,2,3,3,3,3},
        {2,3,3,3,3,3,3},
        {3,3,3,3,3,3,3,4,5,6,7,8,9,10,11},
    };

    int codtab[TOTRUN_NUM][16] = 
    {
        {1,0},
        {1,1,0},
        {3,2,1,0},
        {3,2,1,1,0},
        {3,2,3,2,1,0},
        {3,0,1,3,2,5,4},
        {7,6,5,4,3,2,1,1,1,1,1,1,1,1,1},
    };
    int vlcnum;

    vlcnum = se->len;

    // se->value1 : run
    se->len = lentab[vlcnum][se->value1];
    se->inf = codtab[vlcnum][se->value1];

    if (se->len == 0)
    {
        printf("ERROR: (run) not valid: (%d)\n",se->value1);
        exit(-1);
    }

    symbol2vlc(se);

    writeUVLC2buffer(se, bitstream);

#if TRACE
    if (se->type <= 1)
        trace2out (se);
#endif

    return (se->len);
}

/*
*************************************************************************
* Function:Write out a trace string on the trace file
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

#if TRACE
void trace2out(SyntaxElement *sym)
{
    static int bitcounter = 0;
    int i, chars;

    if (p_trace != NULL)
    {
        putc('@', p_trace);
        chars = fprintf(p_trace, "%i", bitcounter);
        while(chars++ < 6)
            putc(' ',p_trace);

        chars += fprintf(p_trace, "%s", sym->tracestring);
        while(chars++ < 55)
            putc(' ',p_trace);

        // Align bitpattern
        if(sym->len<15)
        {
            for(i=0 ; i<15-sym->len ; i++)
                fputc(' ', p_trace);
        }

        // Print bitpattern
        bitcounter += sym->len;
        for(i=1 ; i<=sym->len ; i++)
        {
            if((sym->bitpattern >> (sym->len-i)) & 0x1)
                fputc('1', p_trace);
            else
                fputc('0', p_trace);
        }
        fprintf(p_trace, " (%3d) \n",sym->value1);
    }
    fflush (p_trace);
}
#endif

/*
*************************************************************************
* Function:puts the less than 8 bits in the byte buffer of the Bitstream into
the streamBuffer.  
* Input:
currStream: the Bitstream the alignment should be established
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void writeVlcByteAlign(Bitstream* currStream)
{
    if (currStream->bits_to_go < 8)
    { // trailing bits to process
        currStream->byte_buf = (currStream->byte_buf <<currStream->bits_to_go) | (0xff >> (8 - currStream->bits_to_go));
        stats.bit_use_stuffingBits[img->type]+=currStream->bits_to_go;
        currStream->streamBuffer[currStream->byte_pos++]=currStream->byte_buf;
        currStream->bits_to_go = 8;
    }
}
