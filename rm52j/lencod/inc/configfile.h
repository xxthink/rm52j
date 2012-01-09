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

#ifndef _CONFIGFILE_H_
#define _CONFIGFILE_H_

typedef struct {
    char *TokenName;
    void *Place;
    int Type;
} Mapping;

InputParameters configinput;

#ifdef INCLUDED_BY_CONFIGFILE_C

Mapping Map[] = {
    {"ProfileID",                &configinput.profile_id,              0},
    {"LevelID",           &configinput.level_id,                0},
    {"IntraPeriod",              &configinput.intra_period,            0},
    {"VECPeriod",                &configinput.vec_period,              0},
    {"SeqHeaderPeriod",          &configinput.seqheader_period,        0},
    {"FramesToBeEncoded",        &configinput.no_frames,               0},
    {"QPFirstFrame",             &configinput.qp0,                     0},
    {"QPRemainingFrame",         &configinput.qpN,                     0},
    {"FrameSkip",                &configinput.jumpd,                   0},
    {"UseHadamard",              &configinput.hadamard,                0},
    {"SearchRange",              &configinput.search_range,            0},
    {"NumberReferenceFrames",    &configinput.no_multpred,             0},
    {"SourceWidth",              &configinput.img_width,               0},
    {"SourceHeight",             &configinput.img_height,              0},
    {"InputFile",                &configinput.infile,                  1},
    {"InputHeaderLength",        &configinput.infile_header,           0},
    {"OutputFile",               &configinput.outfile,                 1},
    {"ReconFile",                &configinput.ReconFile,               1},
    {"TraceFile",                &configinput.TraceFile,               1},
    {"NumberBFrames",            &configinput.successive_Bframe,       0},
    {"QPBPicture",               &configinput.qpB,                     0},
    {"InterSearch16x16",         &configinput.InterSearch16x16,        0},
    {"InterSearch16x8",          &configinput.InterSearch16x8 ,        0},
    {"InterSearch8x16",          &configinput.InterSearch8x16,         0},
    {"InterSearch8x8",           &configinput.InterSearch8x8 ,         0},
    {"RDOptimization",           &configinput.rdopt,                   0},
    {"InterlaceCodingOption",    &configinput.InterlaceCodingOption,   0},
    {"repeat_first_field",       &configinput.repeat_first_field,      0},
    {"top_field_first",          &configinput.top_field_first,         0},
    {"LoopFilterDisable",        &configinput.loop_filter_disable,     0},
    {"LoopFilterParameter",      &configinput.loop_filter_parameter_flag,     0},
    {"LoopFilterAlphaOffset",    &configinput.alpha_c_offset,       0},
    {"LoopFilterBetaOffset",     &configinput.beta_offset,         0},
    {"Progressive_frame",        &configinput.progressive_frame,       0},
    {"Dct_Adaptive_Flag",        &configinput.dct_adaptive_flag,       0},
    //    {"SliceEnable",              &configinput.slice_enable,             0},

    {"NumberOfRowsInSlice",      &configinput.slice_row_nr,            0},

    {"SliceParameter",           &configinput.slice_parameter,         0},
    {"WeightEnable",             &configinput.slice_weighting_flag,    0},
    {"FrameRate",         &configinput.frame_rate_code,       0},
    {"ChromaFormat",             &configinput.chroma_format,       0},
    // Rate Control on AVS Standard
    {"RateControlEnable",        &configinput.RCEnable,                0},
    {"Bitrate",                  &configinput.bit_rate,                0},
    {"InitialQP",                &configinput.SeinitialQP,         0},
    {"BasicUnit",                &configinput.basicunit,         0},
    {"ChannelType",              &configinput.channel_type,           0},

    {NULL,                       NULL,                                -1}
};

#endif

#ifndef INCLUDED_BY_CONFIGFILE_C
extern Mapping Map[];
#endif

void Configure (int ac, char *av[]);
void DecideMvRange();  // 20071009

#endif

