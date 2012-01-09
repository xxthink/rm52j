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



/*!
***************************************************************************
* \file
*    ratectl.h
*
* \author
*    Zhengguo LI
*
* \date
*    14 Jan 2003
*
* \brief
*    Headerfile for rate control 
**************************************************************************
*/
#ifndef _RATE_CTL_H_
#define _RATE_CTL_H_


#define MIN(a,b)  (((a)<(b)) ? (a) : (b))//LIZG 28/10/2002
#define MAX(a,b)  (((a)<(b)) ? (b) : (a))//LIZG 28/10/2002

double bit_rate; 
double GAMMAP;//LIZG, JVT019r1
double BETAP;//LIZG, JVT019r1

int RC_MAX_QUANT;//LIZG 28/10/2002
int RC_MIN_QUANT;//LIZG 28/10/2002

double BufferSize; //LIZG 25/10/2002
double GOPTargetBufferLevel;
double CurrentBufferFullness; //LIZG 25/10/2002
double TargetBufferLevel;//LIZG 25/10/2002
double PreviousBit_Rate;//LIZG  25/10/2002
double AWp;
double AWb;
int MyInitialQp;
int PAverageQp;

/*LIZG JVT50V2 distortion prediction model*/
/*coefficients of the prediction model*/
double PreviousPictureMAD;
double MADPictureC1;
double MADPictureC2;
double PMADPictureC1;
double PMADPictureC2;
/* LIZG JVT50V2 picture layer MAD */
Boolean PictureRejected[21];
double PPictureMAD[21];
double PictureMAD[21];
double ReferenceMAD[21];

/*quadratic rate-distortion model*/
Boolean   m_rgRejected[21];
double  m_rgQp[21];
double m_rgRp[21];
double m_X1;
double m_X2;
int m_Qc;
double m_Qstep;
int m_Qp;
int Pm_Qp;
int PreAveMBHeader;
int CurAveMBHeader;
int PPreHeader;
int PreviousQp1;
int PreviousQp2;
int NumberofBFrames;
/*basic unit layer rate control*/
int TotalFrameQP;
int NumberofBasicUnit;
int PAveHeaderBits1;
int PAveHeaderBits2;
int PAveHeaderBits3;
int PAveFrameQP;
int TotalNumberofBasicUnit;
int CodedBasicUnit;
double MINVALUE;
double CurrentFrameMAD;
double CurrentBUMAD;
double TotalBUMAD;
double PreviousFrameMAD;
int m_Hp;
int m_windowSize;
int MADm_windowSize;
int DDquant;
int MBPerRow;
double AverageMADPreviousFrame;
int TotalBasicUnitBits;
int QPLastPFrame;
int QPLastGOP;
//int MADn_windowSize;
//int n_windowSize;

double Pm_rgQp[20];
double Pm_rgRp[20];
double Pm_X1;
double Pm_X2;
int Pm_Hp;
/* adaptive field/frame coding*/
int FieldQPBuffer;
int FrameQPBuffer;
int FrameAveHeaderBits;
int FieldAveHeaderBits;
double BUPFMAD[6336];//LIZG
double BUCFMAD[6336];//LIZG
double FCBUCFMAD[6336];
double FCBUPFMAD[6336];

Boolean GOPOverdue;


//comput macroblock activity for rate control
int diffy[16][16];
int diffyy[16][16];
int diffy8[16][16];//for P8X8 mode 

extern int Iprev_bits;
extern int Pprev_bits;

void rc_init_seq();
void rc_init_GOP(int np, int nb);
void rc_update_pict_frame(int nbits);
void rc_init_pict(int fieldpic,int topfield, int targetcomputation);
void rc_update_pict(int nbits);
void setbitscount(int nbits);

int updateQuantizationParameter(int topfield);/*LIZG*/
void updateRCModel ();/*LIZG*/
void updateMADModel ();/*LIZG*/
Boolean skipThisFrame (); /*LIZG*/
void RCModelEstimator (int n_windowSize);/*LIZG*/
void MADModelEstimator (int n_windowSize);/*LIZG*/
double calc_MAD();/*LIZG*/
double ComputeFrameMAD();
int Qstep2QP( double Qstep );
double QP2Qstep( int QP );

#endif

