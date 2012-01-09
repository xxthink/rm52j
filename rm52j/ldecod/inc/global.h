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
* File name: global.h
* Function:  global definitions for for AVS decoder.
*
*************************************************************************************
*/

#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#include <stdio.h>                              //!< for FILE
#include "defines.h"

#ifdef WIN32
  #define  snprintf _snprintf
#endif

//digipro_1
#define TQ_16 0


#ifdef _OUTPUT_DISTANCE_
FILE *pf_distance;
#endif

#ifdef _OUTPUT_MV
FILE *pf_mv;
#endif

typedef enum {
  FIELD,
  FRAME
} PictureStrufcture; //cjw 20051219   //!< New enum for field processing


//! Boolean Type
typedef enum {
  FALSE,
    TRUE
} Boolean;

typedef unsigned char   byte;                   //!<  8 bit unsigned
typedef int             int32;
typedef unsigned int    u_int32;

// global picture format dependend buffers, mem allocation in decod.c ******************
int  **refFrArr;                                //!< Array for reference frames of each block
byte **imgY;                                    //!< array for the decoded luma component
byte ***imgUV;                                  //!< array for the chroma component

// B pictures
byte **imgY_prev;
byte ***imgUV_prev;

byte **imgY_ref;                                //!< reference frame find snr
byte ***imgUV_ref;

// B pictures
int  Bframe_ctr;
byte prevP_tr, nextP_tr, P_interval;
int  frame_no;

int  **refFrArr_frm;
byte **imgY_frm;
byte ***imgUV_frm;
int  **refFrArr_top;
int  **refFrArr_bot;


byte **imgY_top;
byte **imgY_bot;
byte ***imgUV_top;
byte ***imgUV_bot;

int  *parity_fld;

byte **mref_frm[2];                               //!< 1/1 pix luma for direct interpolation
byte **mcef_frm[2][2];                              //!< pix chroma

byte **mref_fld[4];                               //!< 1/1 pix luma for direct interpolation
byte **mcef_fld[4][2];     

byte nextP_tr_frm, nextP_tr_fld;

#define ET_SIZE 300      //!< size of error text buffer
char errortext[ET_SIZE]; //!< buffer for error message for exit with error()

/***********************************************************************
 * T y p e    d e f i n i t i o n s    f o r    T M L
 ***********************************************************************
 */

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
  SE_CBP01,     //wzm,422
  SE_LUM_DC_INTER,
  SE_CHR_DC_INTER,
  SE_LUM_AC_INTER,
  SE_CHR_AC_INTER,
  SE_DELTA_QUANT_INTER,
  SE_DELTA_QUANT_INTRA,
  SE_BFRAME,
  SE_EOS,
  SE_MAX_ELEMENTS //!< number of maximum syntax elements, this MUST be the last one!
} SE_type;        // substituting the definitions in element.h

typedef enum {
  INTER_MB,
  INTRA_MB_4x4,
  INTRA_MB_16x16
} IntraInterDecision;

typedef enum {
  BITS_TOTAL_MB,
  BITS_HEADER_MB,
  BITS_INTER_MB,
  BITS_CBP_MB,
  BITS_COEFF_Y_MB,
  BITS_COEFF_UV_MB,
  MAX_BITCOUNTER_MB
} BitCountType;

typedef enum {
  P_IMG = 0,
  B_IMG,
  I_IMG,
} PictureType;
/*Lou 1016 Start*/
typedef enum{
  NS_BLOCK,
    VS_BLOCK
}SmbMode;
/*Lou 1016 End*/

/***********************************************************************
 * N e w   D a t a    t y p e s   f o r    T M L
 ***********************************************************************
 */

struct img_par;
struct inp_par;
struct stat_par;

#ifdef _OUTPUT_CODENUM_
FILE *pf_code_num;
#endif

//! Syntaxelement
typedef struct syntaxelement
{
  int           type;                  //!< type of syntax element for data part.
  int           value1;                //!< numerical value of syntax element
  int           value2;                //!< for blocked symbols, e.g. run/level
  int           len;                   //!< length of code
  int           inf;                   //!< info part of UVLC code
  unsigned int  bitpattern;            //!< UVLC bitpattern
  int           context;               //!< CABAC context
  int           k;                     //!< CABAC context for coeff_count,uv
  int           golomb_grad;    //!< Needed if type is a golomb element
  int           golomb_maxlevels; //!< If this is zero, do not use the golomb coding
#if TRACE
  #define       TRACESTRING_SIZE 100           //!< size of trace string
  char          tracestring[TRACESTRING_SIZE]; //!< trace string
#endif

  //! for mapping of UVLC to syntaxElement
  void    (*mapping)(int len, int info, int *value1, int *value2);
} SyntaxElement;

//! Macroblock
typedef struct macroblock
{
  int           qp;
  int           slice_nr;
  int           delta_quant;          //!< for rate control
  struct macroblock   *mb_available[3][3]; /*!< pointer to neighboring MBs in a 3x3 window of current MB, which is located at [1][1]
                                                NULL pointer identifies neighboring MBs which are unavailable */
  
  // some storage of macroblock syntax elements for global access
  int           mb_type;
  int           mvd[2][BLOCK_MULTIPLE][BLOCK_MULTIPLE][2];      //!< indices correspond to [forw,backw][block_y][block_x][x,y]
  int           cbp, cbp_blk,cbp01 ;  //wzm,422
  unsigned long cbp_bits;

  int           b8mode[4];
  int           b8pdir[4];
  int           mb_type_2;
  int           c_ipred_mode_2;       //!< chroma intra prediction mode
  int           dct_mode;

  int           c_ipred_mode;       //!< chroma intra prediction mode
  int           lf_disable;
  int           lf_alpha_c0_offset;
  int           lf_beta_offset;
} Macroblock;

//! Bitstream
typedef struct
{
  // CABAC Decoding
  int           read_len;           //!< actual position in the codebuffer, CABAC only
  int           code_len;           //!< overall codebuffer length, CABAC only
  // UVLC Decoding
  int           frame_bitoffset;    //!< actual position in the codebuffer, bit-oriented, UVLC only
  int           bitstream_length;   //!< over codebuffer lnegth, byte oriented, UVLC only
  // ErrorConcealment
  byte          *streamBuffer;      //!< actual codebuffer for read bytes
} Bitstream;

//****************************** ~DM ***********************************

// image parameters
typedef struct img_par
{
  int number;                                 //<! frame number
  int current_mb_nr; // bitstream order
  int max_mb_nr;
  int current_slice_nr;
  int **intra_block;
  int tr;                                     //<! temporal reference, 8 bit, wrapps at 255
  int qp;                                     //<! quant for the current frame
  int type;                                   //<! image type INTER/INTRA
  int width;
  int height;
  int width_cr;                               //<! width chroma
  int height_cr;                              //<! height chroma
  int mb_y;
  int mb_x;
  int block_y;
  int pix_y;
  int pix_x;
  int pix_c_y;
  int block_x;
  int pix_c_x;

  int ***mv;                                  //<! [92][72][3]  // delete by xfwang 2004.7.29
  int mpr[16][16];                            //<! predicted block

  int m7[16][16];                             //<! final 4x4 block. Extended to 16x16 for ABT
  int m8[4][8][8];                             //<! final 4x4 block. Extended to 16x16 for AVS
  int cof[4][8][4][4];                        //<! correction coefficients from predicted
  int cofu[4];
  int **ipredmode;                            //<! prediction type [90][74]
  int quad[256];
  int cod_counter;                            //<! Current count of number of skipped macroblocks in a row

  int ***dfMV;                                //<! [92][72][3];
  int ***dbMV;                                //<! [92][72][3];
  int **fw_refFrArr;                          //<! [72][88];
  int **bw_refFrArr;                          //<! [72][88];

  int ***mv_frm;
  int **fw_refFrArr_frm;                          //<! [72][88];
  int **bw_refFrArr_frm;                          //<! [72][88];
  int imgtr_next_P;
  int imgtr_last_P;
  int tr_frm;
  int tr_fld;
  int imgtr_last_prev_P;//Lou 1016

  int no_forward_reference;            // xz zheng, 20071009
  int seq_header_indicate;            // xz zheng, 20071009
  int B_discard_flag;              // xz zheng, 20071009

  // B pictures
  int ***fw_mv;                                //<! [92][72][3];
  int ***bw_mv;                                //<! [92][72][3];
  int subblock_x;
  int subblock_y;

  int buf_cycle;

  int direct_type;

  int ***mv_top;
  int ***mv_bot;
  int **fw_refFrArr_top;                          //<! [72][88];
  int **bw_refFrArr_top;                          //<! [72][88];
  int **fw_refFrArr_bot;                          //<! [72][88];
  int **bw_refFrArr_bot;                          //<! [72][88];

  int **ipredmode_top;
  int **ipredmode_bot;
  int ***fw_mv_top;
  int ***fw_mv_bot;
  int ***bw_mv_top;
  int ***bw_mv_bot;
  int ***dfMV_top;                                //<! [92][72][3];
  int ***dbMV_top;                                //<! [92][72][3];
  int ***dfMV_bot;                                //<! [92][72][3];
  int ***dbMV_bot;                                //<! [92][72][3];

  int toppoc;      //poc for this top field // POC200301
  int bottompoc;   //poc of bottom field of frame
  int framepoc;    //poc of this frame // POC200301
  unsigned int frame_num;   //frame_num for this frame

  unsigned int pic_distance;
  int delta_pic_order_cnt_bottom;

  signed int PicDistanceMsb;
  unsigned int PrevPicDistanceLsb;
  signed int CurrPicDistanceMsb;
  unsigned int ThisPOC;

  int PicWidthInMbs;
  int PicHeightInMbs;
  int PicSizeInMbs;

  int block8_x,block8_y;
  int structure;
  int pn;
  int buf_used;
  int buf_size;
  int picture_structure;
  int advanced_pred_mode_disable;
  int types;
  int current_mb_nr_fld;
  
  // !! shenyanfei Weighted Prediction
  int slice_weighting_flag ; //cjw 20051219
  int lum_scale[4] ;
  int lum_shift[4] ;
  int chroma_scale[4] ;
  int chroma_shift[4] ;
  int mb_weighting_flag ;   //0  all the MB is weighted, 1 the weighting_prediction is used for each MB
  int weighting_prediction ;
  int mpr_weight[16][16];
  int top_bot;  // 0: top field / 1: bottom field / -1: frame / Yulj 2004.07.14 
  int Bframe_number;
//Carmen, 2007/12/20, Bug Fix: correct picture size for outputted reconstructed pictures
  int auto_crop_right;
  int auto_crop_bottom;
//Carmen, 2007/12/20, Bug Fix: correct picture size for outputted reconstructed pictures

} ImageParameters;
Macroblock          *mb_data;  

extern ImageParameters *img;
extern Bitstream *currStream;

// signal to noice ratio parameters
struct snr_par
{
  float snr_y;                                 //<! current Y SNR
  float snr_u;                                 //<! current U SNR
  float snr_v;                                 //<! current V SNR
  float snr_y1;                                //<! SNR Y(dB) first frame
  float snr_u1;                                //<! SNR U(dB) first frame
  float snr_v1;                                //<! SNR V(dB) first frame
  float snr_ya;                                //<! Average SNR Y(dB) remaining frames
  float snr_ua;                                //<! Average SNR U(dB) remaining frames
  float snr_va;                                //<! Average SNR V(dB) remaining frames
};

int tot_time;

// input parameters from configuration file
struct inp_par
{
  char infile[100];                       //<! Telenor AVS input
  char outfile[100];                      //<! Decoded YUV 4:2:0 output
  char reffile[100];                      //<! Optional YUV 4:2:0 reference file for SNR measurement
  int FileFormat;                         //<! File format of the Input file, PAR_OF_ANNEXB or PAR_OF_RTP
  int buf_cycle;                          //<! Frame buffer size
  int LFParametersFlag;                   //<! Specifies that loop filter parameters are included in bitstream

};

extern struct inp_par *input;

// files
FILE *p_out;                    //<! pointer to output YUV file
FILE *p_ref;                    //<! pointer to input original reference YUV file file
FILE *p_log;                    //<! SNR file

#if TRACE
FILE *p_trace;
#endif

void read_ipred_block_modes(struct img_par *img,struct inp_par *inp,int b8);
void set_MB_parameters (struct img_par *img,struct inp_par *inp, int mb);
// prototypes
//void init_conf(struct inp_par *inp, char *config_filename);    // 20071224
void init_conf(struct inp_par *inp,int numpar,char **config_str);  // 20071224, command line
void report(struct inp_par *inp, struct img_par *img, struct snr_par *snr);
void find_snr(struct snr_par *snr,struct img_par *img, FILE *p_ref);
void init(struct img_par *img);

int  decode_one_frame(struct img_par *img,struct inp_par *inp, struct snr_par *snr);
void init_frame(struct img_par *img, struct inp_par *inp);
void init_frame_buffer();
void init_top(struct img_par *img, struct inp_par *inp);
void init_bot(struct img_par *img, struct inp_par *inp);
void init_top_buffer();
void init_bot_buffer();
void split_field_top();
void split_field_bot();

void write_frame(struct img_par *img, FILE *p_out);
void write_prev_Pframe(struct img_par *img,FILE *p_out);// B pictures
void copy_Pframe(struct img_par *img);// B pictures


int  read_new_slice();
void decode_one_slice(struct img_par *img,struct inp_par *inp);
void picture_data(struct img_par *img,struct inp_par *inp);
void top_field(struct img_par *img,struct inp_par *inp);
void bot_field(struct img_par *img,struct inp_par *inp);
void combine_field(struct img_par *img);
void start_macroblock(struct img_par *img,struct inp_par *inp);
void init_macroblock_Bframe(struct img_par *img);// B pictures
int  read_one_macroblock(struct img_par *img,struct inp_par *inp);

int  read_one_macroblock_Bframe(struct img_par *img,struct inp_par *inp);// B pictures
int  decode_one_macroblock(struct img_par *img,struct inp_par *inp);
int  decode_one_macroblock_Bframe(struct img_par *img);// B pictures
int  exit_macroblock(struct img_par *img,struct inp_par *inp, int eos_bit);

int  sign(int a , int b);

int Header();

void Update_Picture_top_field();
void Update_Picture_bot_field();
void DeblockFrame(ImageParameters *img, byte **imgY, byte ***imgUV);

// Direct interpolation
void get_block(int ref_frame,int x_pos, int y_pos, struct img_par *img, int block[8][8],unsigned char **ref_pic);
void CheckAvailabilityOfNeighbors(struct img_par *img);

void error(char *text, int code);

// dynamic mem allocation
int  init_global_buffers(struct inp_par *inp, struct img_par *img);
void free_global_buffers(struct inp_par *inp, struct img_par *img);

void Update_Picture_Buffers();
void frame_postprocessing(struct img_par *img);

void report_frame(struct snr_par    *snr,int tmp_time);

#define PAYLOAD_TYPE_IDERP 8

Bitstream *AllocBitstream();
void FreeBitstream();
void tracebits2(const char *trace_str, int len, int info);

int get_direct_mv (int****** mv,int mb_x,int mb_y);
void free_direct_mv (int***** mv,int mb_x,int mb_y);

int  *****direct_mv; // only to verify result  
//int    direct_mv[45][80][4][4][3];  // only to verify result  
int   ipdirect_x,ipdirect_y;
int demulate_enable;  //X ZHENG, 20080515, enable demulation mechanism
/* Sequence_header() */
int aspect_ratio_information;
int frame_rate_code; 
int bit_rate_value; 
int bbv_buffer_size;
//int slice_enable;
int slice_row_nr;
int currentbitoffset;
int currentBytePosition;
int chroma_format;
int profile_id;
int level_id;
int progressive_sequence;
int horizontal_size;
int vertical_size;
int chroma_format;
int sample_precision;
int aspect_ratio_information;
int frame_rate_code;
int bit_rate;
int low_delay;
int bit_rate_lower;
int bit_rate_upper;
//int slice_weighting_flag;
//int mb_weighting_flag;
/* Sequence_display_extension() */
int video_format;
int video_range;  
int color_description;
int color_primaries;
int transfer_characteristics;
int matrix_coefficients;
int display_horizontal_size;
int display_vertical_size;


// Zheng Xiaozhen, HiSilicon, 2007.03.21
int FrameNum;  // Used to count the current frame number 
int eos;
int pre_img_type;
int pre_img_types;
//int pre_str_vec;
int pre_img_tr;
int pre_img_qp;
int pre_tmp_time;   
int RefPicExist;   // 20071224
struct snr_par    *snr;         //!< statistics    

//char str_vec[5];   // added by XiaoZhen Zheng, Hilisicon just for output display


/* video edit code */ // M1956 by Grandview 2006.12.12
int vec_flag;

/* Sopyright_extension() header */
int copyright_flag;
int copyright_identifier;
int original_or_copy;
int copyright_number_1;
int copyright_number_2;
int copyright_number_3;

/* I_pictures_header() */ 
int stream_length_flag;
int stream_length;
int picture_distance;
int picture_decode_order_flag;
int picture_decode_order;
int top_field_first;
int frame_pred_frame_dct;
int repeat_first_field;
int progressive_frame;

int fixed_picture_qp;
int picture_qp;
int time_code_flag;
int time_code;
int skip_mode_flag;
int loop_filter_disable;
int loop_filter_parameter_flag;
int alpha_offset;
int beta_offset;
int bby_delay;
int hour;
int minute;
int sec;
int frame_offset;
int bbv_check_times;


//int slice_weighting_flag;
//int mb_weighting_flag;
int luma_scale;
int luma_shift;
int chroma_scale;
int chroma_shift;
int second_IField;   //cjw 20051230


/* Pb_picture_header() */
int picture_coding_type;
int bbv_delay;
int picture_reference_flag;
int dct_adaptive_flag;
float singlefactor;
int  marker_bit;

/*picture_display_extension()*/
int frame_centre_horizontal_offset[4];
int frame_centre_vertical_offset[4];

/* slice_header() */
int start_code_prefix;
int img_width;
int slice_vertical_position;  //cjw 20060327
int slice_vertical_position_extension;
int slice_start_code;
int fixed_picture_qp;
int fixed_slice_qp;
int slice_qp;
int StartCodePosition;



#define I_PICTURE_START_CODE    0xB3
#define PB_PICTURE_START_CODE   0xB6
#define SLICE_START_CODE_MIN    0x00
#define SLICE_START_CODE_MAX    0xAF
#define USER_DATA_START_CODE    0xB2
#define SEQUENCE_HEADER_CODE    0xB0
#define EXTENSION_START_CODE    0xB5
#define SEQUENCE_END_CODE       0xB1
#define VIDEO_EDIT_CODE         0xB7


#define SEQUENCE_DISPLAY_EXTENSION_ID            2
#define COPYRIGHT_EXTENSION_ID                   4
#define CAMERAPARAMETERS_EXTENSION_ID            11
#define PICTURE_DISPLAY_EXTENSION_ID             7

// reference frame buffer
unsigned char **reference_frame[3][3];  //[refnum][yuv][height][width]
unsigned char **reference_field[6][3];  //[refnum][yuv][height/2][width]

unsigned char ***ref[4];  //[refnum(4 for filed)][yuv][height(height/2)][width]
unsigned char ***ref_frm[2];  //[refnum(5 for filed)][yuv][height(height/2)][width]
unsigned char ***ref_fld[5];  //[refnum(5 for filed)][yuv][height(height/2)][width]
unsigned char ***b_ref[2],***f_ref[2];
unsigned char ***b_ref_frm[2],***f_ref_frm[2];
unsigned char ***b_ref_fld[2],***f_ref_fld[2];
unsigned char ***current_frame;//[yuv][height][width]
unsigned char ***current_field;//[yuv][height/2][width]

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

extern CameraParamters *camera;

unsigned long int bn;  // 08.16.2007

#endif
