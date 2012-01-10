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
* File name: defines.h
* Function: Headerfile containing some useful global definitions
*
*************************************************************************************
*/


#ifndef _DEFINES_H_
#define _DEFINES_H_

//#define _OUTPUT_MV
//#define _OUTPUT_DEC_REC
//#define _OUTPUT_DISTANCE_
#define _OUTPUT_ERROR_MB_
//#define _OUTPUT_CODENUM_
#ifdef _OUTPUT_CODENUM_
  #define _MBNR_ 0
  #define _FRAME_NUM_ 35
  #define _B8_        0
#endif

//#define _OUTPUT_RES_

//#define _OUTPUT_PRED_
#ifdef _OUTPUT_PRED_
//#define _MBNR_ 11
//#define _FRAME_NUM_ 0
//#define _B8_        1
#endif

//#define  _OUTPUT_RECON_
#define _MBNR_ 11
#define _FRAME_NUM_ 0
#define _B8_        1


#define  MAX_NO_POC_FRAMES  10  //size of poc ref array = max no ref frames+1
#define  MAX_LENGTH_POC_CYCLE   10              //max no in type 1 poc cycle
#define NONREFFRAME 0           // used with push_poc
#define REFFRAME 1

#define DCT_BLOCK_SIZE  8

// CAVLC
#define LUMA              0
#define LUMA_INTRA16x16DC 1
#define LUMA_INTRA16x16AC 2

#define TOTRUN_NUM    15
#define RUNBEFORE_NUM  7

//--- block types for CABAC ----
#define LUMA_16DC       0
#define LUMA_16AC       1
#define LUMA_8x8        2
#define LUMA_8x4        3
#define LUMA_4x8        4
#define LUMA_4x4        5
#define CHROMA_DC       6
#define CHROMA_AC       7
#define NUM_BLOCK_TYPES 8

#define MAX_INFO_WORD  300000               //!< for one frame
#define MAX_CODED_FRAME_SIZE 1500000         //!< bytes for one frame
#define MAXIMUM_UVLC_CODEWORD_PER_HEADER 20 //!< UVLC codewords per combined picture/slice header maximum

#define TRACE           0                   //!< 0:Trace off 1:Trace on

#define _LEAKYBUCKET_

#define absm(A) ((A)<(0) ? (-(A)):(A))      //!< abs macro, faster than procedure
#define MAX_VALUE       999999              //!< used for start value for some variables
#define Clip1(a)            ((a)>255?255:((a)<0?0:(a)))
#define Clip3(min,max,val) (((val)<(min))?(min):(((val)>(max))?(max):(val)))

// FLAGS and DEFINES.
#define B8_SIZE         8       // block size of transformed block
#define QUANT_PERIOD    8       // mantissa/exponent quantization, step size doubles every QUANT_PERIOD qp
#define _ALT_SCAN_              // use GI scan field coding
#define _CD_4x4VALUES_          // use baseline 4x4 quantization values

#define P8x8    8
#define I4MB    9
#define I16MB   10
#define IBLOCK  11
#define SI4MB   12
#define MAXMODE 13

#define IS_INTRA(MB)    ((MB)->mb_type==I4MB  || (MB)->mb_type==I16MB)
#define IS_NEWINTRA(MB) ((MB)->mb_type==I16MB)
#define IS_OLDINTRA(MB) ((MB)->mb_type==I4MB)
#define IS_INTER(MB)    ((MB)->mb_type!=I4MB  && (MB)->mb_type!=I16MB)
#define IS_INTERMV(MB)  ((MB)->mb_type!=I4MB  && (MB)->mb_type!=I16MB       && (MB)->mb_type!=0)
#define IS_DIRECT(MB)   ((MB)->mb_type==0     && (img->   type==    B_IMG ))
#define IS_COPY(MB)     ((MB)->mb_type==0     && (img->   type==P_IMG))
#define IS_P8x8(MB)     ((MB)->mb_type==P8x8)

// Quantization parameter range
#define MIN_QP          0
#define MAX_QP          63

#define BLOCK_SIZE      4
#define B8_SIZE         8
#define MB_BLOCK_SIZE   16

#define NO_INTRA_PMODE  5        //!< #intra prediction modes
/* 4x4 intra prediction modes */
#define VERT_PRED             0
#define HOR_PRED              1
#define DC_PRED               2
#define DOWN_LEFT_PRED   3
#define DOWN_RIGHT_PRED  4


// 16x16 intra prediction modes
#define VERT_PRED_16    0
#define HOR_PRED_16     1
#define DC_PRED_16      2
#define PLANE_16        3

// 8x8 chroma intra prediction modes
#define DC_PRED_8       0
#define HOR_PRED_8      1
#define VERT_PRED_8     2
#define PLANE_8         3

// QCIF format
#define IMG_WIDTH       176
#define IMG_HEIGHT      144
#define IMG_WIDTH_CR    88
#define IMG_HEIGHT_CR   72

#define INIT_FRAME_RATE 30
#define EOS             1                       //!< End Of Sequence
#define SOP             2                       //!< Start Of Picture
#define SOS             3                       //!< Start Of Slice

#define EOS_MASK        0x01                    //!< mask for end of sequence (bit 1)
#define ICIF_MASK       0x02                    //!< mask for image format (bit 2)
#define QP_MASK         0x7C                    //!< mask for quant.parameter (bit 3->7)
#define TR_MASK         0x7f80                  //!< mask for temporal referance (bit 8->15)

#define DECODING_OK     0
#define SEARCH_SYNC     1
#define PICTURE_DECODED 2

#define MIN_PIX_VAL     0                       //!< lowest legal values for 8 bit sample
#define MAX_PIX_VAL     255                     //!< highest legal values for 8 bit sample

#define MAX_REFERENCE_PICTURES 15

#ifndef WIN32
#define max(a, b)      ((a) > (b) ? (a) : (b))  //!< Macro returning max value
#define min(a, b)      ((a) < (b) ? (a) : (b))  //!< Macro returning min value
#endif
#define mmax(a, b)      ((a) > (b) ? (a) : (b)) //!< Macro returning max value
#define mmin(a, b)      ((a) < (b) ? (a) : (b)) //!< Macro returning min value
#define clamp(a,b,c) ( (a)<(b) ? (b) : ((a)>(c)?(c):(a)) )    //!< clamp a to the range of [b;c]

#define MVPRED_MEDIAN   0
#define MVPRED_L        1
#define MVPRED_U        2
#define MVPRED_UR       3

#define DECODE_COPY_MB  0
#define DECODE_MB       1

#define BLOCK_MULTIPLE      (MB_BLOCK_SIZE/(BLOCK_SIZE*2))

#define MAX_SYMBOLS_PER_MB  1200  //!< Maximum number of different syntax elements for one MB

#define MAX_PART_NR     3        /*!< Maximum number of different data partitions.
                                      Some reasonable number which should reflect
                                      what is currently defined in the SE2Partition
                                      map (elements.h) */

// Interim File Format: define the following macro to identify which version is 
//                      used in the implementation

#define WORKING_DRAFT_MAJOR_NO 0    // indicate the working draft version number
#define WORKING_DRAFT_MINOR_NO 4
#define INTERIM_FILE_MAJOR_NO 0     // indicate interim file format version number
#define INTERIM_FILE_MINOR_NO 1

//Start code and Emulation Prevention need this to be defined in identical manner at encoder and decoder
#define ZEROBYTES_SHORTSTARTCODE 2 //indicates the number of zero bytes in the short start-code prefix

#endif

