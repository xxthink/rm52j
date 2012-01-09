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
* File name: mbuffer.h
* Function: Frame buffer functions
*
*************************************************************************************
*/

#ifndef _BUFFER_H_
#define _BUFFER_H_

#include "global.h"

/*! one single frame */
typedef struct 
{
  int  used;
  int  valid;           //<! To indicate if this frame is inserted to fill in the PN gap
                        //<! 1: normal reference frame, 0: frame inserted to the buffer
  int  picID;
  int  lt_picID;
  byte **mref;
  byte ***mcef;
  int  islong;            //<! field is needed for reordering

  unsigned int layerNumber;  //<! to tell which layer this frame belongs to.
  unsigned int subSequenceIdentifier;  //<! to tell which subsequence this frame belongs to.
  int parity;
} Frame;

/* the whole frame buffer structure containing both long and short term frames */
typedef struct
{
  Frame **picbuf_short;
  Frame **picbuf_long;
  int   short_size;
  int   long_size;
  int   short_used;
  int   long_used;
} FrameBuffer;


FrameBuffer *fb;
FrameBuffer *frm;
FrameBuffer *fld;


byte ***mref;                                   //<! these are pointer arrays to the actual structures
byte ****mcef;                                  //<! these are pointer arrays to the actual structures

byte **mref_fref_frm[2]; //mref_fref[ref_index][height(height/2)][width] ref_index=0 for B frame, ref_index=0,1 for B field
byte **mref_bref_frm[2]; //mref_bref[ref_index][height(height/2)][width] ref_index=0 for B frame, ref_index=0,1 for B field
byte **mcef_fref_frm[2][2];//mcef_fref[ref_index][uv][height/2][width] ref_index=0 for B frame, ref_index=0,1 for B field
byte **mcef_bref_frm[2][2];//mcef_bref[ref_index][uv][height/2][width] ref_index=0 for B frame, ref_index=0,1 for B field

byte **mref_fref_fld[2]; //mref_fref[ref_index][height(height/2)][width] ref_index=0 for B frame, ref_index=0,1 for B field
byte **mref_bref_fld[2]; //mref_bref[ref_index][height(height/2)][width] ref_index=0 for B frame, ref_index=0,1 for B field
byte **mcef_fref_fld[2][2];//mcef_fref[ref_index][uv][height/2][width] ref_index=0 for B frame, ref_index=0,1 for B field
byte **mcef_bref_fld[2][2];//mcef_bref[ref_index][uv][height/2][width] ref_index=0 for B frame, ref_index=0,1 for B field

byte **mref_fref[2]; //mref_fref[ref_index][height(height/2)][width] ref_index=0 for B frame, ref_index=0,1 for B field
byte **mref_bref[2]; //mref_bref[ref_index][height(height/2)][width] ref_index=0 for B frame, ref_index=0,1 for B field
byte **mcef_fref[2][2];//mcef_fref[ref_index][uv][height/2][width] ref_index=0 for B frame, ref_index=0,1 for B field
byte **mcef_bref[2][2];//mcef_bref[ref_index][uv][height/2][width] ref_index=0 for B frame, ref_index=0,1 for B field
#endif

