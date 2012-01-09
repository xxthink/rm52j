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
#ifndef _DEFINES_H_
#define _DEFINES_H_
#define Frame_Rate_Num   8  //xfwang 2004.7.28  kinds of frame rate
#define AVS
#define TRACE           0 //!< 0:Trace off 1:Trace on

#define FastME
#define SSE_ME
//#define _DISABLE_SUB_ME_
//#define _OUTPUT_MB_QSTEP_BITS_

/*lgp*/
#define RD_EST_FACTOR_I   0.2
#define RD_EST_FACTOR_P   0.005
/*lgp*/
#define  DIRECTSCALE

// CAVLC
#define LUMA              0
#define LUMA_INTRA16x16DC 1
#define LUMA_INTRA16x16AC 2

#define LEVEL_NUM      6
#define TOTRUN_NUM    15
#define RUNBEFORE_NUM  7

#define LUMA_16DC       0
#define LUMA_16AC       1
#define LUMA_8x8        2
#define LUMA_8x4        3
#define LUMA_4x8        4
#define LUMA_4x4        5
#define CHROMA_DC       6
#define CHROMA_AC       7
#define NUM_BLOCK_TYPES 8

#define clamp(a,b,c) ( (a)<(b) ? (b) : ((a)>(c)?(c):(a)) )    //!< clamp a to the range of [b;c]

#define LOG2_MAX_FRAME_NUM_MINUS4   4           // POC200301 moved from defines.h
#define LOG2_MAX_PIC_ORDER_CNT_LSB_MINUS4 4     // POC200301 newly added

// Constants for the interim file format
#define WORKING_DRAFT_MAJOR_NO 0    // inidicate the working draft version number
#define WORKING_DRAFT_MINOR_NO 4
#define INTERIM_FILE_MAJOR_NO 0     // indicate interim file format version number
#define INTERIM_FILE_MINOR_NO 1

// ---------------------------------------------------------------------------------
// FLAGS and DEFINES for new chroma intra prediction, Dzung Hoang
// Threshold values to zero out quantized transform coefficients.
// Recommend that _CHROMA_COEFF_COST_ be low to improve chroma quality
#define _LUMA_COEFF_COST_       4 //!< threshold for luma coeffs
#define _CHROMA_COEFF_COST_     4 //!< threshold for chroma coeffs, used to be 7

#define IMG_PAD_SIZE    4   //!< Number of pixels padded around the reference frame (>=4)

#define absm(A) ((A)<(0) ? (-(A)):(A)) //!< abs macro, faster than procedure
#define MAX_VALUE       999999   //!< used for start value for some variables

#define Clip1(a)            ((a)>255?255:((a)<0?0:(a)))
#define Clip3(min,max,val) (((val)<(min))?(min):(((val)>(max))?(max):(val)))

// ---------------------------------------------------------------------------------
// FLAGS and DEFINES for AVS.
#define INI_CTX         1       //!< use initialization values for all CABAC contexts. 0=off, 1=on
#define INICNT_AVS      64      // max_count for AVS contexts if INI_CTX = 0

#define B8_SIZE         8       // block size of block transformed by AVS
//Lou 1014 #define QUANT_PERIOD    6       // mantissa/exponent quantization, step size doubles every QUANT_PERIOD qp
#define QUANT_PERIOD    8       // mantissa/exponent quantization, step size doubles every QUANT_PERIOD qp
#define _ALT_SCAN_              // use GI scan for field coding
//Lou 1013 #define QP_OFS         -12      // workaround to use old qp-design for AVS routines
#define _CD_4x4VALUES_          // use baseline 4x4 quantization values
//#define _AVS_FLAG_IN_SLICE_HEADER_ // write AVS flag to slice header (needs fix)
// ---------------------------------------------------------------------------------


#define P8x8    8
#define I4MB    9
#define I16MB   10
#define IBLOCK  11
#define SI4MB   12
#define MAXMODE 13

#define  LAMBDA_ACCURACY_BITS         16
#define  LAMBDA_FACTOR(lambda)        ((int)((double)(1<<LAMBDA_ACCURACY_BITS)*lambda+0.5))
#define  WEIGHTED_COST(factor,bits)   (((factor)*(bits))>>LAMBDA_ACCURACY_BITS)
#define  MV_COST(f,s,cx,cy,px,py)     (WEIGHTED_COST(f,mvbits[((cx)<<(s))-px]+mvbits[((cy)<<(s))-py]))
#define  REF_COST(f,ref)              (WEIGHTED_COST(f,refbits[(ref)]))

#define  BWD_IDX(ref)                 (((ref)<2)? 1-(ref): (ref))
#define  REF_COST_FWD(f,ref)          (WEIGHTED_COST(f,((img->num_ref_pic_active_fwd_minus1==0)? 0:refbits[(ref)])))
#define  REF_COST_BWD(f,ref)          (WEIGHTED_COST(f,((img->num_ref_pic_active_bwd_minus1==0)? 0:BWD_IDX(refbits[ref]))))

#define IS_INTRA(MB)    ((MB)->mb_type==I4MB)
#define IS_OLDINTRA(MB) ((MB)->mb_type==I4MB)
#define IS_INTER(MB)    ((MB)->mb_type!=I4MB)
#define IS_INTERMV(MB)  ((MB)->mb_type!=I4MB && (MB)->mb_type!=0)
#define IS_DIRECT(MB)   ((MB)->mb_type==0     && (img ->   type==B_IMG))
#define IS_COPY(MB)     ((MB)->mb_type==0     && img ->   type==INTER_IMG)
#define IS_P8x8(MB)     ((MB)->mb_type==P8x8)

// Quantization parameter range
//Lou 1014#define MIN_QP          0
//Lou 1014#define MAX_QP          51
//Lou 1014#define SHIFT_QP        12
#define MIN_QP          0
#define MAX_QP          63
#define SHIFT_QP        11

// Picture types
#define INTRA_IMG       0   //!< I frame
#define INTER_IMG       1   //!< P frame
#define B_IMG           2   //!< B frame

// Direct Mode types
#define DIR_TEMPORAL    0   //!< Temporal Direct Mode
#define DIR_SPATIAL     1    //!< Spatial Direct Mode
#define BLOCK_SIZE      4
#define DCT_BLOCK_SIZE  8 /*lgp*/
#define MB_BLOCK_SIZE   16
#define BLOCK_MULTIPLE      (MB_BLOCK_SIZE/(2*BLOCK_SIZE))
#define NO_INTRA_PMODE  5        //!< #intra prediction modes

//!< 4x4 intra prediction modes
#define VERT_PRED             0
#define HOR_PRED              1
#define DC_PRED               2
#define DOWN_LEFT_PRED   3
#define DOWN_RIGHT_PRED  4


// 8x8 chroma intra prediction modes
#define DC_PRED_8       0
#define HOR_PRED_8      1
#define VERT_PRED_8     2
#define PLANE_8         3

#define INIT_FRAME_RATE 30
#define EOS             1         //!< End Of Sequence

#define MVPRED_MEDIAN   0
#define MVPRED_L        1
#define MVPRED_U        2
#define MVPRED_UR       3


#define MAX_SYMBOLS_PER_MB  1200  //!< Maximum number of different syntax elements for one MB
// CAVLC needs more symbols per MB
#define MAXPICTURETYPESEQUENCELEN 100   /*!< Maximum size of the string that defines the picture
types to be coded, e.g. "IBBPBBPBB" */
#endif

