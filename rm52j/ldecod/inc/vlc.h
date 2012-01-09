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
* File name: vlc.h
* Function: header for (CA)VLC coding functions
*
*************************************************************************************
*/


#ifndef _VLC_H_
#define _VLC_H_

int se_v (char *tracestring);
int ue_v (char *tracestring);
int u_1 (char *tracestring);
int u_v (int LenInBits, char *tracestring);
int i_8(char *tracestring);

// UVLC mapping
void linfo_ue(int len, int info, int *value1, int *dummy);
void linfo_se(int len, int info, int *value1, int *dummy);

void linfo_cbp_intra(int len,int info,int *cbp, int *dummy);
void linfo_cbp_inter(int len,int info,int *cbp, int *dummy);

int  readSyntaxElement_VLC (SyntaxElement *sym);
int  readSyntaxElement_UVLC(SyntaxElement *sym, struct inp_par *inp);
int  readSyntaxElement_Intra8x8PredictionMode(SyntaxElement *sym);

int  GetVLCSymbol (byte buffer[],int totbitoffset,int *info, int bytecount);
int  GetVLCSymbol_IntraMode (byte buffer[],int totbitoffset,int *info, int bytecount);
int  GetVLCSymbol_refidx (byte buffer[],int totbitoffset,int *info, int bytecount);

int readSyntaxElement_FLC(SyntaxElement *sym);
int GetBits (byte buffer[],int totbitoffset,int *info, int bytecount, 
             int numbits);
int ShowBits (byte buffer[],int totbitoffset,int bytecount, int numbits);

int get_uv (int LenInBits, char*tracestring);  // Yulj 2004.07.15  for decision of slice end.
int GetSyntaxElement_FLC(SyntaxElement *sym); // Yulj 2004.07.15  for decision of slice end.

#endif

