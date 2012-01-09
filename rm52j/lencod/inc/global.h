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

#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#include <stdio.h>

#include "defines.h"

#ifndef WIN32
#include "minmax.h"
#else
#define  snprintf _snprintf
#endif

#define _BUGFIX_NON16MUL_PICSIZE


/***********************************************************************
* T y p e    d e f i n i t i o n s    f o r    T M L
***********************************************************************
*/
typedef unsigned char byte;    //!< byte type definition
typedef __int64 int64;
#define pel_t byte
#define MAX_V_SEARCH_RANGE 1024
#define MAX_V_SEARCH_RANGE_FIELD 512
#define MAX_H_SEARCH_RANGE 8192
byte *pic_buf;


#ifdef _OUTPUT_MB_QSTEP_BITS_
FILE *pf_mb_qstep_bits;
int glb_mb_coff_bits[3];
int glb_temp_coff_bits;
#endif


//! Boolean Type
typedef enum {
    FALSE,
    TRUE
} Boolean;

typedef enum {
    FRAME_CODING,
    FIELD_CODING,
    PAFF_CODING
} CodingType;

//! definition of AVS syntax elements
typedef enum {
    SE_HEADER,
    SE_PTYPE,
    SE_MBTYPE,
    SE_REFFRAME,
    SE_INTRAPREDMODE,
    SE_MVD,
    SE_CBP_INTRA,
    SE_LUM_DC_INTRA,
    SE_CHR_DC_INTRA,
    SE_LUM_AC_INTRA,
    SE_CHR_AC_INTRA,
    SE_CBP_INTER,
    SE_CBP01,  
    SE_LUM_DC_INTER,
    SE_CHR_DC_INTER,
    SE_LUM_AC_INTER,
    SE_CHR_AC_INTER,
    SE_DELTA_QUANT_INTER,
    SE_DELTA_QUANT_INTRA,
    SE_BFRAME,
    SE_EOS,
    SE_MAX_ELEMENTS  //!< number of maximum syntax elements
} SE_type;
//Lou

typedef enum {
    BITS_HEADER,
    BITS_TOTAL_MB,
    BITS_MB_MODE,
    BITS_INTER_MB,
    BITS_CBP_MB,
    BITS_CBP01_MB,
    BITS_COEFF_Y_MB,
    BITS_COEFF_UV_MB,
    BITS_DELTA_QUANT_MB,
    MAX_BITCOUNTER_MB
} BitCountType;

typedef enum {
    FRAME,
    TOP_FIELD,
    BOTTOM_FIELD
} PictureType;           //!< New enum for field processing

/*Lou 1016 Start*/
typedef enum{
    NS_BLOCK,
    VS_BLOCK
}SmbMode;
/*Lou 1016 End*/

//! Syntaxelement
typedef struct syntaxelement
{
    int                 type;           //!< type of syntax element for data part.
    int                 value1;         //!< numerical value of syntax element
    int                 value2;         //!< for blocked symbols, e.g. run/level
    int                 len;            //!< length of code
    int                 inf;            //!< info part of UVLC code
    unsigned int        bitpattern;     //!< UVLC bitpattern
    int                 context;        //!< CABAC context
    int                 k;              //!< CABAC context for coeff_count,uv
    int                 golomb_grad;    //needed if type is a golomb element (AVS)
    int                 golomb_maxlevels; // if this is zero, do not use the golomb coding. (AVS)

#if TRACE
#define             TRACESTRING_SIZE 100            //!< size of trace string
    char                tracestring[TRACESTRING_SIZE];  //!< trace string
#endif

    //!< for mapping of syntaxElement to UVLC
    void    (*mapping)(int value1, int value2, int* len_ptr, int* info_ptr);

} SyntaxElement;

//! Macroblock
typedef struct macroblock
{
    int                 currSEnr;                   //!< number of current syntax element
    int                 slice_nr;
    int                 delta_qp;
    int                 qp ;
    int                 bitcounter[MAX_BITCOUNTER_MB];
    struct macroblock   *mb_available[3][3];        /*!< pointer to neighboring MBs in a 3x3 window of current MB, which is located at [1][1] \n
                                                    NULL pointer identifies neighboring MBs which are unavailable */
    // some storage of macroblock syntax elements for global access
    int                 mb_type;
    int                 mb_type_2;
    int                 mvd[2][BLOCK_MULTIPLE][BLOCK_MULTIPLE][2];          //!< indices correspond to [forw,backw][block_y][block_x][x,y]
    int                 intra_pred_modes[BLOCK_MULTIPLE*BLOCK_MULTIPLE];
    int                 cbp,scbp;
    int          cbp01;   
    int                 cbp_blk ;    //!< 1 bit set for every 4x4 block with coefs (not implemented for INTRA)
    int                 b8mode[4];
    int                 b8pdir[4];
    unsigned long       cbp_bits;

    int                 lf_disable;
    int                 lf_alpha_c0_offset;
    int                 lf_beta_offset;

    int                 c_ipred_mode;      //!< chroma intra prediction mode
    int                 IntraChromaPredModeFlag;
    int                                                                     mb_field;
    int                  ****cofAC;/*lgp*dct*modify*/         //!< AC coefficients [8x8block][4x4block][level/run][scan_pos]
    int                  ****chromacofAC;
    int                 c_ipred_mode_2;      //!< chroma intra prediction mode

    //rate control
    int                 prev_cbp;
    int                 prev_qp;
    int                 predict_qp;
    int                 predict_error;
} Macroblock;

//! Bitstream
typedef struct
{
    int             byte_pos;           //!< current position in bitstream;
    int             bits_to_go;         //!< current bitcounter
    byte            byte_buf;           //!< current buffer for last written byte
    int             stored_byte_pos;    //!< storage for position in bitstream;
    int             stored_bits_to_go;  //!< storage for bitcounter
    byte            stored_byte_buf;    //!< storage for buffer of last written byte

    byte            byte_buf_skip;      //!< current buffer for last written byte
    int             byte_pos_skip;      //!< storage for position in bitstream;
    int             bits_to_go_skip;    //!< storage for bitcounter

    byte            *streamBuffer;      //!< actual buffer for written bytes

} Bitstream;
Bitstream *currBitStream;

#define MAXSLICEPERPICTURE 100

typedef struct
{
    int   no_slices;
    int   bits_per_picture;
    float distortion_y;
    float distortion_u;
    float distortion_v;
} Picture;

typedef struct{
    int extension_id;
    int copyright_flag;
    int copyright_id;
    int original_or_copy;
    int reserved;
    int copyright_number;
} CopyRight;

typedef struct{
    int reserved;
    int camera_id;
    int height_of_image_device;
    int focal_length;
    int f_number;
    int vertical_angle_of_view;
    int camera_position_x;
    int camera_position_y;
    int camera_position_z;
    int camera_direction_x;
    int camera_direction_y;
    int camera_direction_z;
    int image_plane_vertical_x;
    int image_plane_vertical_y;
    int image_plane_vertical_z;
} CameraParamters;

extern CopyRight *cp;
extern CameraParamters *camera;

Picture *frame_pic;
Picture *top_pic;
Picture *bot_pic;

byte   *imgY_org_buffer;           //!< Reference luma image

// global picture format dependend buffers, mem allocation in image.c
byte   **imgY_frm;               //!< Encoded luma images
byte  ***imgUV_frm;              //!< Encoded croma images
byte   **imgY_org_frm;           //!< Reference luma image
byte  ***imgUV_org_frm;          //!< Reference croma image
int   ***tmp_mv_frm;             //!< motion vector buffer
int    **refFrArr_frm;           //!< Array for reference frames of each block

byte   **imgY;               //!< Encoded luma images
byte  ***imgUV;              //!< Encoded croma images
byte   **imgY_org;           //!< Reference luma image
byte  ***imgUV_org;          //!< Reference croma image
byte   **imgY_pf;            //!< Post filter luma image
byte  ***imgUV_pf;           //!< Post filter croma image
byte  **mref[4];               //!< 1/4 pix luma
byte **mcef[4][2];               //!< pix chroma
int    **img4Y_tmp;          //!< for quarter pel interpolation
int   ***tmp_mv;             //!< motion vector buffer

int    **refFrArr;           //!< Array for reference frames of each block

// mv_range, 20071009
int Min_V_MV;
int Max_V_MV;
int Min_H_MV;
int Max_H_MV;
// mv_range, 20071009

// B pictures
// motion vector : forward, backward, direct
int  ***tmp_fwMV;
int  ***tmp_bwMV;
int  ***tmp_fwMV_top;   //!< For MB level field/frame coding tools
int  ***tmp_fwMV_bot;   //!< For MB level field/frame coding tools
int  ***tmp_bwMV_top;   //!< For MB level field/frame coding tools
int  ***tmp_bwMV_bot;   //!< For MB level field/frame coding tools
int  **field_mb;      //!< For MB level field/frame coding tools
int  WriteFrameFieldMBInHeader; //! For MB level field/frame coding tools
int  ***tmp_fwMV_fld;   //!< For MB level field/frame coding tools
int  ***tmp_bwMV_fld;   //!< For MB level field/frame coding tools

int  ***dfMV;
int  ***dbMV;
int   **fw_refFrArr;
int   **bw_refFrArr;
byte  **nextP_imgY;
byte ***nextP_imgUV;
pel_t *Refbuf11[4];            //!< 1/1th pel (full pel) reference frame buffer

// global picture format dependend buffers, mem allocation in image.c (field picture)
byte   **imgY_org_top;
byte  ***imgUV_org_top;
byte   **imgY_org_bot;
byte  ***imgUV_org_bot;
byte   **imgY_top;               //!< Encoded luma images
byte  ***imgUV_top;              //!< Encoded croma images
byte   **imgY_bot;               //!< Encoded luma images
byte  ***imgUV_bot;              //!< Encoded croma images
pel_t **Refbuf11_fld;            //!< 1/1th pel (full pel) reference frame buffer
int    **refFrArr_top;           //!< Array for reference frames of each block
int    **refFrArr_bot;           //!< Array for reference frames of each block
byte   **imgY_com;               //!< Encoded luma images
byte  ***imgUV_com;              //!< Encoded croma images
int    **refFrArr_fld;           //!< Array for reference frames of each block
int *parity_fld;


// global picture format dependend buffers, mem allocation in image.c (field picture)
byte  **mref_fld[4];               //!< 1/4 pix luma
byte ***mref_mbfld;        //!< For MB level field/frame coding tools

// global picture format dependend buffers, mem allocation in image.c (frame buffer)
byte  **mref_frm[2];               //!< 1/4 pix luma //[2:ref_index]
// B pictures
// motion vector : forward, backward, direct
int   **fw_refFrArr_top;
int   **bw_refFrArr_top;
int   **fw_refFrArr_bot;
int   **bw_refFrArr_bot;
int   ***tmp_mv_top;             //!< motion vector buffer
int   ***tmp_mv_bot;             //!< motion vector buffer

int   **fwdir_refFrArr;         //!< direct mode forward reference buffer
int   **bwdir_refFrArr;         //!< direct mode backward reference buffer

// global picture format dependend buffers, mem allocation in image.c (frame buffer)
byte   **imgY_org_frm;
byte  ***imgUV_org_frm;
byte   **imgY_frm;               //!< Encoded luma images
byte  ***imgUV_frm;              //!< Encoded croma images

int    **refFrArr_frm;           //!< Array for reference frames of each block
int   direct_mode;

// B pictures
// motion vector : forward, backward, direct
int   **fw_refFrArr_frm;
int   **bw_refFrArr_frm;

int   **fw_refFrArr_fld;
int   **bw_refFrArr_fld;
int   ***tmp_mv_fld;             //!< motion vector buffer

int intras;         //!< Counts the intra updates in each frame.

int  Bframe_ctr, frame_no, nextP_tr_frm,nextP_tr;
int  tot_time;
int  tmp_buf_cycle;    // jlzheng 7.21
int  temp_vecperiod;

int bot_field_mb_nr;  // record the relative mb index in the top field,  Xiaozhen Zheng HiSilicon, 20070327

#define ET_SIZE 300      //!< size of error text buffer
char errortext[ET_SIZE]; //!< buffer for error message for exit with error()

//! SNRParameters
typedef struct
{
    float snr_y;               //!< current Y SNR
    float snr_u;               //!< current U SNR
    float snr_v;               //!< current V SNR
    float snr_y1;              //!< SNR Y(dB) first frame
    float snr_u1;              //!< SNR U(dB) first frame
    float snr_v1;              //!< SNR V(dB) first frame
    float snr_ya;              //!< Average SNR Y(dB) remaining frames
    float snr_ua;              //!< Average SNR U(dB) remaining frames
    float snr_va;              //!< Average SNR V(dB) remaining frames
} SNRParameters;

//! all input parameters
typedef struct
{
    int no_frames;                //!< number of frames to be encoded
    int qp0;                      //!< QP of first frame
    int qpN;                      //!< QP of remaining frames
    int jumpd;                    //!< number of frames to skip in input sequence (e.g 2 takes frame 0,3,6,9...)
    int hadamard;                 /*!< 0: 'normal' SAD in 1/3 pixel search.  1: use 4x4 Haphazard transform and '
                                  Sum of absolute transform difference' in 1/3 pixel search                   */
    int search_range;             /*!< search range - integer pel search and 16x16 blocks.  The search window is
                                  generally around the predicted vector. Max vector is 2xmcrange.  For 8x8
                                  and 4x4 block sizes the search range is 1/2 of that for 16x16 blocks.       */
    int no_multpred;              /*!< 1: prediction from the last frame only. 2: prediction from the last or
                                  second last frame etc.  Maximum 5 frames                                    */
    int img_width;                //!< GH: if CUSTOM image format is chosen, use this size
    int img_height;               //!< GH: width and height must be a multiple of 16 pels
    int yuv_format;               //!< GH: YUV format (0=4:0:0, 1=4:2:0, 2=4:2:2, 3=4:4:4,currently only 4:2:0 is supported)
    int color_depth;              //!< GH: YUV color depth per component in bit/pel (currently only 8 bit/pel is supported)
    int intra_upd;                /*!< For error robustness. 0: no special action. 1: One GOB/frame is intra coded
                                  as regular 'update'. 2: One GOB every 2 frames is intra coded etc.
                                  In connection with this intra update, restrictions is put on motion vectors
                                  to prevent errors to propagate from the past                                */
    int blc_size[8][2];           //!< array for different block sizes
    int  infile_header;           //!< If input file has a header set this to the length of the header
    char infile[100];             //!< YUV 4:2:0 input format
    char outfile[100];            //!< AVS compressed output bitstream
    char ReconFile[100];          //!< Reconstructed Pictures
    char TraceFile[100];          //!< Trace Outputs
    int intra_period;

    // B pictures
    int successive_Bframe;        //!< number of B frames that will be used
    int qpB;                      //!< QP of B frames
    int SequenceHeaderType;

    int InterSearch16x16;
    int InterSearch16x8;
    int InterSearch8x16;
    int InterSearch8x8;

    char PictureTypeSequence[MAXPICTURETYPESEQUENCELEN];

    int rdopt;

    int InterlaceCodingOption;

    //AVS
    int aspect_ratio_information;
    int frame_rate_code;//xfwang  2004.7.28
    //int bit_rate;
    int bit_rate_lower;
    int bit_rate_upper;
    int slice_weighting_flag;
    int mb_weighting_flag;

    int vec_period;
    int seqheader_period;  // Random Access period though sequence header

    int bbv_buffer_size;
    int video_format;
    int color_description;
    int color_primaries;
    int transfer_characteristics;
    int matrix_coefficients;
    int hour;
    int minute;
    int second;
    int frame_offset;
    int profile_id;
    int level_id;
    int progressive_sequence;
    int repeat_first_field;
    int top_field_first;
    int low_delay;
    int chroma_format;
    int sample_precision;
    int video_range;
    int stream_length_flag;
    int picture_decoder_order_flag;
    int frame_pred_frame_dct;
    int progressive_frame;
    int fixed_picture_qp;
    int time_code_flag;
    int display_horizontal_size;
    int display_vertical_size;
    int dct_adaptive_flag;
    //  int slice_enable;
    int slice_parameter;
    int slice_row_nr;
    int skip_mode_flag;
    int loop_filter_disable;
    int loop_filter_parameter_flag;
    int alpha_c_offset;
    int beta_offset;
    //! Rate Control on AVS standard
    int RCEnable;
    int bit_rate;
    int SeinitialQP;
    int basicunit;
    int channel_type;
    int frame_rate;
    int stuff_height;
} InputParameters;

//! ImageParameters
typedef struct
{
    int number;                  //!< current image number to be encoded
    int pn;                      //!< picture number
    int lindex;                  //!< next long term index to be used
    int max_lindex;              //!< max long term index
    int nb_references;
    int current_mb_nr;
    int total_number_mb;
    int current_slice_nr;
    int type;
    int ptype;
    int types;                   /*!< This is for SP-Pictures, since all the syntax elements for SP-Pictures
                                 are the same as P-pictures, we keep the img->type as P_IMG but indicate
                                 SP-Pictures by img->types */
    int no_multpred;             /*!< 1: prediction from the last frame only.
                                 2: prediction from the last or second last frame etc. */
    int qp;                      //!< quant for the current frame
    int framerate;

    int width;                   //!< Number of pels
    int width_cr;                //!< Number of pels chroma
    int height;                  //!< Number of lines
    int height_cr;               //!< Number of lines  chroma
    int mb_y;                    //!< current MB vertical
    int mb_x;                    //!< current MB horizontal
    int block_y;                 //!< current block vertical
    int block_x;                 //!< current block horizontal
    int pix_y;                   //!< current pixel vertical
    int pix_x;                   //!< current pixel horizontal
    int pix_c_y;                 //!< current pixel chroma vertical
    int block_c_x;               //!< current block chroma vertical
    int pix_c_x;                 //!< current pixel chroma horizontal
    int **ipredmode;             //!< GH ipredmode[90][74];prediction mode for inter frames */ /* fix from ver 4.1
    int cod_counter;             //!< Current count of number of skipped macroblocks in a row
    //int ****nz_coeff;            //!< number of coefficients per block (CAVLC)
    //int ****n_coeff_avs;         //!< number of coefficients per 8x8 block

    // some temporal buffers
    int mprr[9][16][16];         //!< all 9 prediction modes? // enlarged from 4 to 16 for ABT (is that neccessary?)

    int mprr_2[5][16][16];       //!< all 4 new intra prediction modes
    int mprr_c[2][4][8][16];      //!< new chroma 8x8 intra prediction modes
    int***** mv;                 //!< motion vectors for all block types and all reference frames
    int mpr[16][16];             //!< current best prediction mode
    int m7[16][16];              //!< the diff pixel values between orginal image and prediction

    int ****chromacofAC;         //!< AC coefficients [uv][4x4block][level/run][scan_pos]
    int ****cofAC;               //!< AC coefficients [8x8block][4x4block][level/run][scan_pos]
    int ***cofDC;                //!< DC coefficients [yuv][level/run][scan_pos]

    Macroblock    *mb_data;                                   //!< array containing all MBs of a whole frame
    // SyntaxElement   MB_SyntaxElements[MAX_SYMBOLS_PER_MB];    //!< temporal storage for all chosen syntax elements of one MB
    SyntaxElement   *MB_SyntaxElements; //!< by oliver 0612
    int *quad;               //!< Array containing square values,used for snr computation  */                                         /* Values are limited to 5000 for pixel differences over 70 (sqr(5000)).
    int **intra_block;

    int tr;
    int fld_type;                        //!< top or bottom field
    unsigned int fld_flag;
    int direct_intraP_ref[4][4];
    int imgtr_next_P_frm;
    int imgtr_last_P_frm;
    int imgtr_next_P_fld;
    int imgtr_last_P_fld;
    int imgtr_last_prev_P_frm;//Lou 1016
    // B pictures
    int b_interval;
    int p_interval;
    int b_frame_to_code;
    int fw_mb_mode;
    int bw_mb_mode;
    int***** p_fwMV;       //!< for MVDFW
    int***** p_bwMV;       //!< for MVDBW

    int***** all_mv;       //!< replaces local all_mv
    int***** all_bmv;      //!< replaces local all_mv

    int num_ref_pic_active_fwd_minus1;
    int num_ref_pic_active_bwd_minus1;

    
    int *****mv_fld;
    int *****p_fwMV_fld;
    int *****p_bwMV_fld;
    int *****all_mv_fld;
    int *****all_bmv_fld;

    int mv_range_flag;      // xiaozhen zheng, mv_range, 20071009

    int field_mb_y;   // Macroblock number of a field MB
    int field_block_y;  // Vertical block number for the first block of a field MB
    int field_pix_y;    // Co-ordinates of current macroblock in terms of field pixels (luma)
    int field_pix_c_y;  // Co-ordinates of current macroblock in terms of field pixels (chroma)
    int *****mv_top;    //!< For MB level field/frame coding tools
    int *****mv_bot;    //!< For MB level field/frame coding tools
    int *****p_fwMV_top;    //!< For MB level field/frame coding tools
    int *****p_fwMV_bot;    //!< For MB level field/frame coding tools
    int *****p_bwMV_top;    //!< For MB level field/frame coding tools
    int *****p_bwMV_bot;    //!< For MB level field/frame coding tools
    int *****all_mv_top;    //!< For MB level field/frame coding tools
    int *****all_mv_bot;    //!< For MB level field/frame coding tools
    int *****all_bmv_top;   //!< For MB level field/frame coding tools
    int *****all_bmv_bot;   //!< For MB level field/frame coding tools
    int **ipredmode_top;    //!< For MB level field/frame coding tools
    int **ipredmode_bot;    //!< For MB level field/frame coding tools
    int field_mode;     //!< For MB level field/frame -- field mode on flag
    int top_field;      //!< For MB level field/frame -- top field flag
    int auto_crop_right;
    int auto_crop_bottom;
    int buf_cycle;

    unsigned int frame_num;   //frame_num for this frame

    //the following are sent in the slice header
    int NoResidueDirect;

    int coding_stage;
    int block8_x;
    int block8_y;
    int coded_mb_nr;


    int***** omv;
    int***** all_omv;       //!< replaces local all_mv
    int***** omv_fld;
    int***** all_omv_fld;       //!< replaces local all_mv

    int current_slice_start_mb;
    int current_slice_qp;
    int progressive_frame;
    int picture_structure;
    int dropflag;
    int advanced_pred_mode_disable;
    int old_type;
    int current_mb_nr_fld;

    // !! for weighting prediction
    int LumVarFlag ;
    int lum_scale[4] ;
    int lum_shift[4] ;
    int chroma_scale[4] ;
    int chroma_shift[4] ;
    int mb_weighting_flag ;   //0 all MB in one frame should be weighted, 1 use the mb_weight_flag
    int weighting_prediction ;
    int mpr_weight[16][16];
    int top_bot;           // -1: frame / 0: top field / 1: bottom field / Yulj 2004.07.14
    int mb_no_currSliceLastMB; // the last MB no in current slice.      Yulj 2004.07.15


    /*rate control*/
    int NumberofHeaderBits;
    int NumberofTextureBits;
    int NumberofBasicUnitHeaderBits;
    int NumberofBasicUnitTextureBits;
    double TotalMADBasicUnit;
    int NumberofMBTextureBits;
    int NumberofMBHeaderBits;
    int NumberofCodedBFrame;
    int NumberofCodedPFrame;
    int NumberofGOP;
    int TotalQpforPPicture;
    int NumberofPPicture;
    double MADofMB[10000];
    int BasicUnitQP;
    int TopFieldFlag;
    int FieldControl;
    int FieldFrame;
    int Frame_Total_Number_MB;
    int IFLAG;
    int NumberofCodedMacroBlocks;
    int BasicUnit;
    int bot_MB;
    //  int VEC_FLAG;          // Commented by cjw, 20070327
    int Seqheader_flag;      // Added by cjw, 20070327
    int EncodeEnd_flag;          // Carmen, 2007/12/19    
    int curr_picture_distance;  // Added by Xiaozhen Zheng, 20070405
    int last_picture_distance;  // Added by Xiaozhen Zheng, 20070405
    int count;
    int count_PAFF;
} ImageParameters;

//!< statistics
typedef struct
{
    int   quant0;                 //!< quant for the first frame
    int   quant1;                 //!< average quant for the remaining frames
    float bitr;                   //!< bit rate for current frame, used only for output til terminal
    float bitr0;                  //!< stored bit rate for the first frame
    float bitrate;                //!< average bit rate for the sequence except first frame
    int64   bit_ctr;                //!< counter for bit usage
    int64   bit_ctr_0;              //!< stored bit use for the first frame
    int64   bit_ctr_n;              //!< bit usage for the current frame
    int64   bit_slice;              //!< number of bits in current slice
    int64   bit_use_mode_inter[2][MAXMODE]; //!< statistics of bit usage
    int64   bit_ctr_emulationprevention; //!< stored bits needed to prevent start code emulation
    int64   mode_use_intra[25];     //!< Macroblock mode usage for Intra frames
    int64   mode_use_inter[2][MAXMODE];

    int64   mb_use_mode[2];

    // B pictures
    int   *mode_use_Bframe;
    int   *bit_use_mode_Bframe;
    int64   bit_ctr_P;
    int64   bit_ctr_B;
    float bitrate_P;
    float bitrate_B;

#define NUM_PIC_TYPE 5
    int64   bit_use_stuffingBits[NUM_PIC_TYPE];
    int64   bit_use_mb_type[NUM_PIC_TYPE];
    int64   bit_use_header[NUM_PIC_TYPE];
    int64   tmp_bit_use_cbp[NUM_PIC_TYPE];
    int64   bit_use_coeffY[NUM_PIC_TYPE];
    int64   bit_use_coeffC[NUM_PIC_TYPE];
    int64   bit_use_delta_quant[NUM_PIC_TYPE];

    int64   em_prev_bits_frm;
    int64   em_prev_bits_fld;
    int64   bit_ctr_parametersets;
} StatParameters;

//!< For MB level field/frame coding tools
//!< temporary structure to store MB data for field/frame coding
typedef struct
{
    double min_rdcost;
    int tmp_mv[2][2][2];        // to hold the motion vectors for each block
    int tmp_fwMV[2][2][2];      // to hold forward motion vectors for B MB's
    int tmp_bwMV[2][2][2];      // to hold backward motion vectors for B MBs
    int dfMV[2][2][2], dbMV[2][2][2]; // to hold direct motion vectors for B MB's
    int    rec_mbY[16][16];       // hold the Y component of reconstructed MB
    int    rec_mbU[16][8], rec_mbV[16][8];
    int    ****cofAC;
    int    ****chromacofAC;
    int    ***cofDC;
    int    mvd[2][BLOCK_MULTIPLE][BLOCK_MULTIPLE][2];
    int    mb_type;
    int    b8mode[4], b8pdir[4];
    int    frefar[2][2], brefar[2][2];
    int    **ipredmode;
    int    intra_pred_modes[4];
    int    cbp, cbp_blk,cbp01;  
    int    mode;
    int    *****mv, *****p_fwMV, *****p_bwMV ;
    int    *****all_mv;
    int    *****all_bmv;
    int    c_ipred_mode;
    int    c_ipred_mode_2; //X ZHENG,422
} RD_DATA;

RD_DATA *rdopt;


float singlefactor;
/*
RD_DATA rddata_top_frame_mb, rddata_bot_frame_mb; //!< For MB level field/frame coding tools
RD_DATA rddata_top_field_mb, rddata_bot_field_mb; //!< For MB level field/frame coding tools
*/

extern InputParameters *input;
extern ImageParameters *img;
extern StatParameters *stat;
extern SNRParameters *snr;


// cjw for WP prediction
int *allalpha_lum, *allbelta_lum;
int second_IField;

// cjw for mv limit
int mv_out_of_range;

// Tsinghua for picture_distance  200701
int picture_distance;

// files
FILE *p_dec,*p_dec_u,*p_dec_v;   //!< internal decoded image for debugging
FILE *p_stat;                    //!< status file for the last encoding session
FILE *p_log;                     //!< SNR file
FILE *p_in;                      //!< YUV
FILE *p_datpart;                 //!< file to write bitlength and id of all partitions
FILE *p_trace;                   //!< Trace file

int  sign(int a,int b);
void init_img();
void report();
void information_init();

void  LumaPrediction4x4 (int, int, int, int, int, int);
int   SATD (int*, int);

pel_t* FastLineX (int, pel_t*, int, int);
pel_t* UMVLineX  (int, pel_t*, int, int);

void LumaResidualCoding ();
void ChromaResidualCoding (int*);
void IntraChromaPrediction8x8 (int*, int*, int*);
void IntraChromaPrediction8x8_422 (int*, int*, int*); //X ZHENG,422
int  writeMBHeader   (int rdopt);

extern int*   refbits;
extern int*** motion_cost;
/*Lou 1016 Start*/
//int calculate_distance(int blkref, int smbtype, int coding_stage, int ref);
int scale_motion_vector(int motion_vector, int currblkref, int neighbourblkref, int currsmbtype, int neighboursmbtype, int block_y_pos, int curr_block_y, int ref, int direct_mv);
/*Lou 1016 End*/
int calculate_distance(int blkref, int fw_bw ); // Yulj 2004.07.14
void  PartitionMotionSearch     (int, int, double);
void  PartitionMotionSearch_bid (int, int, double);
int   BIDPartitionCost          (int, int, int, int,int);
int   LumaResidualCoding8x8     (int*, int*, int, int, int, int, int);
int   writeLumaCoeff8x8         (int, int);
int   writeMotionVector8x8      (int, int, int, int, int, int, int, int);
int   writeIntra4x4Modes        (int);
int   writeChromaIntraPredMode  ();
int    Get_Direct_Cost8x8 (int, double);
int    Get_Direct_CostMB  (double);
int    B8Mode2Value (int b8mode, int b8pdir);
int    GetSkipCostMB (double lambda);
void  FindSkipModeMotionVector ();

// dynamic mem allocation
int  init_global_buffers();
void free_global_buffers();
void no_mem_exit  (char *where);

int  get_mem_mv  (int******);
void free_mem_mv (int*****);
void free_img    ();

int  get_mem_ACcoeff  (int*****);
int  get_mem_DCcoeff  (int****);
void free_mem_ACcoeff (int****);
void free_mem_DCcoeff (int***);

void split_field_top();
void split_field_bot();

int  writeBlockCoeff (int block8x8);
int  storeMotionInfo (int pos);

void intrapred_luma_AVS(int img_x,int img_y);

// Added for (re-) structuring the TML soft
Picture *malloc_picture();
void    free_picture (Picture *pic);
int      encode_one_slice(Picture *pic);   //! returns the number of MBs in the slice

void  encode_one_macroblock();
void  start_macroblock();
void  set_MB_parameters (int mb);           //! sets up img-> according to input-> and currSlice->

void  terminate_macroblock(Boolean *end_of_picture);
void  write_one_macroblock(int eos_bit);
void  proceed2nextMacroblock();

void  CheckAvailabilityOfNeighbors();

void free_slice_list(Picture *currPic);

#if TRACE
void  trace2out(SyntaxElement *se);
#endif

void error(char *text, int code);
int  start_sequence();
int  terminate_sequence();
int  start_slice();

void stuffing_byte(int n);

int   writeCBPandLumaCoeff  ();
int   writeChromaCoeff      ();

//============= rate-distortion optimization ===================
void  clear_rdopt      ();
void  init_rdopt       ();

void SetImgType();

#define IMG_NUMBER (img->number)

extern int*** motion_cost_bid;
int   ipdirect_x,ipdirect_y;
void  Get_IP_direct();
void FreeBitstream();
void AllocateBitstream();

// reference frame buffer
unsigned char **reference_frame[3][3];  //[refnum][yuv][height][width]
unsigned char **reference_field[6][3];  //[refnum][yuv][height/2][width]

unsigned char ***ref[4];  //[refnum(4 for filed)][yuv][height(height/2)][width]
unsigned char ***ref_frm[2];  //[refnum(4 for filed)][yuv][height(height/2)][width]
unsigned char ***ref_fld[6];  //[refnum(4 for filed)][yuv][height(height/2)][width]
unsigned char ***b_ref[2],***f_ref[2];
unsigned char ***b_ref_frm[2],***f_ref_frm[2];
unsigned char ***b_ref_fld[2],***f_ref_fld[2];
unsigned char ***current_frame;//[yuv][height][width]
unsigned char ***current_field;//[yuv][height/2][width]


//qhg for demulation
Bitstream *tmpStream;
int current_slice_bytepos;
//qhg 20060327 for de-emulation
void AllocatetmpBitstream();
void FreetmpBitstream();
void Demulate(Bitstream *currStream, int current_slice_bytepos);

void SetModesAndRefframeForBlocks (int mode);

void SetModesAndRefframe (int b8, int* fw_mode, int* bw_mode, int* fw_ref, int* bw_ref);

int (*PartCalMad)(pel_t *ref_pic,pel_t** orig_pic,pel_t *(*get_ref_line)(int, pel_t*, int, int), int blocksize_y,int blocksize_x, int blocksize_x4,int mcost,int min_mcost,int cand_x,int cand_y);
int PartCalMad_c(pel_t *ref_pic,pel_t** orig_pic,pel_t *(*get_ref_line)(int, pel_t*, int, int), int blocksize_y,int blocksize_x, int blocksize_x4,int mcost,int min_mcost,int cand_x,int cand_y);
int PartCalMad_sse(pel_t *ref_pic,pel_t** orig_pic,pel_t *(*get_ref_line)(int, pel_t*, int, int), int blocksize_y,int blocksize_x, int blocksize_x4,int mcost,int min_mcost,int cand_x,int cand_y);

int (*FullPelBlockMotionSearch) (pel_t** orig_pic, int ref, int pic_pix_x, int pic_pix_y, int blocktype, int pred_mv_x, int pred_mv_y, int*mv_x, int*mv_y, int search_range, int min_mcost, double lambda);
int FullPelBlockMotionSearch_c (pel_t** orig_pic, int ref, int pic_pix_x, int pic_pix_y, int blocktype, int pred_mv_x, int pred_mv_y, int*mv_x, int*mv_y, int search_range, int min_mcost, double lambda);
int FullPelBlockMotionSearch_sse (pel_t** orig_pic, int ref, int pic_pix_x, int pic_pix_y, int blocktype, int pred_mv_x, int pred_mv_y, int*mv_x, int*mv_y, int search_range, int min_mcost, double lambda);
void report_stats_on_error(void);
int total_enc_frame[4];
#endif
