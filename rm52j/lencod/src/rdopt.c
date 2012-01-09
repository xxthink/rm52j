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

#include <stdlib.h>
#include <math.h>
#include <memory.h>
#include <assert.h>
#include "rdopt_coding_state.h"
#include "memalloc.h"
#include "refbuf.h"
#include "block.h"
#include "vlc.h"
#include "macroblock.h"
#include "ratectl.h"
#include "global.h"

#ifdef FastME
#include "fast_me.h"
#endif

//Rate control
extern       int  QP2QUANT  [40];
int QP,QP2;
int DELTA_QP,DELTA_QP2;
int diffy[16][16];
static int pred[16][16];

extern       int  QP2QUANT  [40];

//==== MODULE PARAMETERS ====
int   best_mode;
int   rec_mbY[16][16], rec_mbU[16][8], rec_mbV[16][8], rec_mbY8x8[16][16];    // reconstruction values
int   mpr8x8[16][16];
int   ****cofAC=NULL, ****cofAC8x8=NULL;        // [8x8block][4x4block][level/run][scan_pos]
int   ***cofDC=NULL;                       // [yuv][level/run][scan_pos]
int   ***cofAC4x4=NULL, ****cofAC4x4intern=NULL; // [level/run][scan_pos]
int   ****chromacofAC4x4=NULL;
int   cbp, cbp8x8, cnt_nonz_8x8,cbp01;  //wzm,422
int   cbp_blk, cbp_blk8x8;
int   frefframe[2][2], brefframe[2][2], b8mode[4], b8pdir[4];
int   best8x8mode [4];                // [block]
int   best8x8pdir [MAXMODE][4];       // [mode][block]
int   best8x8ref  [MAXMODE][4];       // [mode][block]
int   b8_ipredmode[4], b8_intra_pred_modes[4];
CSptr cs_mb=NULL, cs_b8=NULL, cs_cm=NULL;
int   best_c_imode;
int   best_c_imode_422;   //X ZHENG, 422
int   best_weight_flag ; // !!  shenyanfei 


int   best8x8bwref     [MAXMODE][4];       // [mode][block]
int   best8x8symref     [MAXMODE][4][2];       // [mode][block]

int   best_intra_pred_modes_tmp[4];
int   best_ipredmode_tmp[2][2];
int   best_mpr_tmp[16][16];
int   best_dct_mode;


#define IS_FW ((best8x8pdir[mode][k]==0 || best8x8pdir[mode][k]==2) && (mode!=P8x8 || best8x8mode[k]!=0 || !bframe))
#define IS_BW ((best8x8pdir[mode][k]==1 || best8x8pdir[mode][k]==2) && (mode!=P8x8 || best8x8mode[k]!=0))

/*
*************************************************************************
* Function:delete structure for RD-optimized mode decision
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void clear_rdopt ()
{
  free_mem_DCcoeff (cofDC);
  free_mem_ACcoeff (cofAC);
  free_mem_ACcoeff (cofAC8x8);
  free_mem_ACcoeff (cofAC4x4intern);
  free_mem4Dint(chromacofAC4x4, 4, 4 );  //wzm,422

  // structure for saving the coding state
  delete_coding_state (cs_mb);
  delete_coding_state (cs_b8);
  delete_coding_state (cs_cm);

}

/*
*************************************************************************
* Function:create structure for RD-optimized mode decision
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void init_rdopt ()
{
  get_mem_DCcoeff (&cofDC);
  get_mem_ACcoeff (&cofAC);
  get_mem_ACcoeff (&cofAC8x8);
  get_mem_ACcoeff (&cofAC4x4intern);
  cofAC4x4 = cofAC4x4intern[0];
  get_mem4Dint(&(chromacofAC4x4),4,8,2,17); //wzm,422

  // structure for saving the coding state
  cs_mb  = create_coding_state ();
  cs_b8  = create_coding_state ();
  cs_cm  = create_coding_state ();

}

/*
*************************************************************************
* Function:Get RD cost for AVS intra block
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

double RDCost_for_AVSIntraBlocks(int *nonzero,
                                 int b8,
                                 int ipmode,
                                 double lambda,
                                 double  min_rdcost,
                                 int mostProbableMode)
{
  int x,y,rate,tmp_cbp,tmp_cbp_blk;
  int distortion;
  int block_x;
  int block_y;
  int pic_pix_x;
  int pic_pix_y;
  int pic_block_x;
  int pic_block_y;
  int even_block;
  short tmp_block_88_inv[8][8];
  Macroblock  *currMB;
  SyntaxElement *currSE;
  int incr_y=1,off_y=0;/*lgp*/

  block_x    =8*(b8%2);
  block_y    =8*(b8/2);
  pic_pix_x  =img->pix_x+block_x;
  pic_pix_y  =img->pix_y+block_y;
  pic_block_x=pic_pix_x/4;
  pic_block_y=pic_pix_y/4;
  even_block =0;
  currMB   =&img->mb_data[img->current_mb_nr];
  currSE   =&img->MB_SyntaxElements[currMB->currSEnr];
  //===== perform DCT, Q, IQ, IDCT, Reconstruction =====
  
  for(y=0;y<8;y++)
    for(x=0;x<8;x++)
      tmp_block_88_inv[y][x]=img->m7[x][y];        //the subblock to be processed is in the top left corner of img->m7[][].

  tmp_cbp=tmp_cbp_blk=0;
  
  transform_B8(tmp_block_88_inv);
  //Lou 1013  scanquant_B8(img->qp+QP_OFS-MIN_QP,4,b8,tmp_block_88_inv,0,&tmp_cbp,&tmp_cbp_blk); // '|4' indicate intra for quantization
  scanquant_B8   (img->qp-MIN_QP,4,b8,tmp_block_88_inv,0,&tmp_cbp,&tmp_cbp_blk); // '|4' indicate intra for quantization    

    //digipro_0
  //scanquant returns a SCR value!.....
  *nonzero=(tmp_cbp!=0);

  //===== get distortion (SSD) of 4x4 block =====
  distortion =0;

  /*lgp*/
  for (y=0; y<8; y++)
    for (x=0; x<8; x++)
      distortion += img->quad [imgY_org[pic_pix_y+incr_y*y+off_y][pic_pix_x+x] - imgY[pic_pix_y+incr_y*y+off_y][pic_pix_x+x]];

  //===== RATE for INTRA PREDICTION MODE  (SYMBOL MODE MUST BE SET TO UVLC) =====
  currSE->value1 = (mostProbableMode == ipmode) ? -1 : ipmode < mostProbableMode ? ipmode : ipmode-1;

  //--- set position and type ---
  currSE->context = 4*b8;
  currSE->type    = SE_INTRAPREDMODE;

  //--- encode and update rate ---
  writeSyntaxElement_Intra4x4PredictionMode(currSE, currBitStream);
  
  rate = currSE->len;
  currSE++;
  currMB->currSEnr++;

  //===== RATE for LUMINANCE COEFFICIENTS =====
  
  x=currMB->cbp;
  currMB->cbp=tmp_cbp;//writeLumaCoeffAVS_B8 needs a correct Macroblock->cbp .
  rate+=writeLumaCoeffAVS_B8(b8,1);
  currMB->cbp=x;

  //calc RD and return it.
  return (double)distortion+lambda*(double)rate;
}

/*
*************************************************************************
* Function:Mode Decision for AVS intra blocks
     This might also be placed in rdopt.c behind Mode_Decision_for_4x4IntraBlocks().
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int Mode_Decision_for_AVS_IntraBlocks(int b8,double lambda,int *min_cost)
{
  int ipmode,best_ipmode,i,j,x,y,cost,loc_cbp,loc_cbp_blk;
  int c_nz, nonzero, rec4x4[8][8], diff[16][16];
  double rdcost;
  int block_x;
  int block_y;
  int pic_pix_x;
  int pic_pix_y;
  int pic_block_x;
  int pic_block_y;
  short tmp_block_88[8][8];
  int upMode;
  int leftMode;
  int mostProbableMode;
  Macroblock *currMB;
  double min_rdcost ;
  int incr_y=1,off_y=0;/*lgp*/
  int MBRowSize = img->width / MB_BLOCK_SIZE;/*lgp*/
  int k;

  currMB=img->mb_data+img->current_mb_nr;

  block_x    =8*(b8&1);
  block_y    =8*(b8>>1);
  pic_pix_x  =img->pix_x+block_x;
  pic_pix_y  =img->pix_y+block_y;
  pic_block_x=pic_pix_x>>3;
  pic_block_y=pic_pix_y>>3;
  min_rdcost =1e30;
  
  
  *min_cost = (1<<20);
  
  upMode           = img->ipredmode[pic_block_x+1][pic_block_y  ];
  leftMode         = img->ipredmode[pic_block_x  ][pic_block_y+1];
  mostProbableMode = (upMode < 0 || leftMode < 0) ? DC_PRED : upMode < leftMode ? upMode : leftMode;
    
  
  //===== INTRA PREDICTION FOR 4x4 BLOCK =====
  intrapred_luma_AVS(pic_pix_x,pic_pix_y);
  
  //===== LOOP OVER ALL INTRA PREDICTION MODES =====
  for (ipmode=0;ipmode<NO_INTRA_PMODE;ipmode++)
  {
    if(img->mprr[ipmode][0][0]>=0)        //changed this. the intrapred function marks the invalid modes. At least one is always valid (the DC).
    {
      if(!input->rdopt)
      {
        for (j=0;j<8;j++)
          for (i=0;i<8;i++)
            diff[j][i] = imgY_org[pic_pix_y+/*j*/incr_y*j+off_y/*lgp*/][pic_pix_x+i] - img->mprr[ipmode][j][i]; // bug fix: index of diff was flipped. mwi 020701

 //       cost  = (ipmode == mostProbableMode) ? 0 : (int)floor(4 * lambda );
          cost  = (ipmode == mostProbableMode) ? 0 : (int)floor(3 * lambda );   // jlzheng  7.20
          
        cost+=find_sad_8x8(input->hadamard,8,8,0,0,diff);
          
        if (cost < *min_cost)
        {
          best_ipmode = ipmode;
          *min_cost   = cost;
        }
      }
      else
      {
        // get prediction and prediction error
        for (j=0;j<8;j++)
          for (i=0;i<8;i++)
          {
            img->mpr[block_x+i][block_y+j]=img->mprr[ipmode][j][i];
            img->m7[i][j] = imgY_org[pic_pix_y+/*j*/incr_y*j+off_y/*lgp*/][pic_pix_x+i] - img->mpr[block_x+i][block_y+j] ;
          }
          
        //===== store the coding state =====
        store_coding_state (cs_cm);
          
        // get and check rate-distortion cost
        if ((rdcost = RDCost_for_AVSIntraBlocks(&c_nz,b8,ipmode,lambda,min_rdcost, mostProbableMode)) < min_rdcost)
        {
          //--- set coefficients ---
          for(j=0;j<2;j++)
            for(k=0;k<4;k++)
              for(i=0;i<65;i++)
                cofAC4x4[k][j][i]=img->cofAC[b8][k][j][i];
            
          //--- set reconstruction ---
          for(y=0; y<8; y++)
            for(x=0; x<8; x++)
              rec4x4[y][x]=imgY[pic_pix_y+/*y*/incr_y*y+off_y/*lgp*/][pic_pix_x+x];
                
          //--- flag if dct-coefficients must be coded ---
          nonzero = c_nz;
                
          //--- set best mode update minimum cost ---
          min_rdcost  = rdcost;
          *min_cost=(int)min_rdcost;
          best_ipmode = ipmode;
        }
        reset_coding_state (cs_cm);
      }
    }
  }
  
  //===== set intra mode prediction =====
  
  img->ipredmode[pic_block_x+1][pic_block_y+1] = best_ipmode;
  currMB->intra_pred_modes[b8] = mostProbableMode == best_ipmode ? -1 : best_ipmode < mostProbableMode ? best_ipmode : best_ipmode-1;
//  best_ipmode = mostProbableMode == best_ipmode ? best_ipmode : best_ipmode < mostProbableMode ? best_ipmode : best_ipmode-1;
/*  if(!pred_mode_flag)
  best_ipmode = best_ipmode < mostProbableMode ? best_ipmode : best_ipmode-1;
  else
   best_ipmode = mostProbableMode;*/
  
  
  /*lgp*/
  if (!input->rdopt)
  {
    
    // get prediction and prediction error
    for (j=0;j<8; j++)
      for (i=0;i<8; i++)
      {
        img->mpr[block_x+i][block_y+j] = img->mprr[best_ipmode][j][i];
        tmp_block_88[j][i] = imgY_org[pic_pix_y+/*j*/incr_y*j+off_y/*lgp*/][pic_pix_x+i] - img->mprr[best_ipmode][j][i];
      }

      loc_cbp=loc_cbp_blk=0;
      
      transform_B8(tmp_block_88);
        //Lou 1013      scanquant_B8(img->qp+QP_OFS-MIN_QP,4,b8,tmp_block_88,0,&loc_cbp,&loc_cbp_blk); // '|4' indicate intra for quantization
      scanquant_B8   (img->qp-MIN_QP,4,b8,tmp_block_88,0,&loc_cbp,&loc_cbp_blk); // '|4' indicate intra for quantization
        
      //digipro_1      
      //scanquant returns a SCR value!.....
      nonzero=(loc_cbp!=0);
      currMB->cbp|=loc_cbp;
      currMB->cbp_blk|=loc_cbp_blk;
  }
  else
  {/*lgp*/
    //===== restore coefficients =====
    for(j=0;j<2;j++)
      for(k=0;k<4;k++)
        for(i=0;i<65;i++)
          img->cofAC[b8][k][j][i] = cofAC4x4[k][j][i];

    //--- set reconstruction ---
    for(y=0; y<8; y++)
      for(x=0; x<8; x++)
      {
        imgY[pic_pix_y+incr_y*y+off_y][pic_pix_x+x] = rec4x4[y][x];
        img->mpr[block_x+i][block_y+j] = img->mprr[best_ipmode][j][i];
      }      
  }/*lgp*/

    
  return nonzero;
}


/*
*************************************************************************
* Function:Mode Decision for an 8x8 Intra block
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int Mode_Decision_for_8x8IntraBlocks(int b8,double lambda,int *cost)
{
  int  nonzero=0;
  int  cost4x4;
  int loc_cost0,loc_cost,best_AVS_cost;
  int cbp_blk_part,cbp_blk_mask;
  int best_nonzero;
  int b8_x,b8_y;

  *cost = (int)floor(6.0 * lambda + 0.4999);

  loc_cost0 = *cost;
  
  cbp_blk_mask=0x0033;
  if(b8&1)cbp_blk_mask<<=2;
  if(b8&2)cbp_blk_mask<<=8;
  
  b8_x=(b8&1)<<3;
  b8_y=(b8&2)<<2;
  
  best_AVS_cost=0x7FFFFFFFL;  //max int value
  cbp_blk_part=0;
  best_nonzero=0;
  
  nonzero=0;
  loc_cost=loc_cost0;
  
  //only one 8x8 mode
  if(Mode_Decision_for_AVS_IntraBlocks(b8,lambda,&cost4x4))
      nonzero=1;
  loc_cost+=cost4x4;
  
  if(loc_cost<best_AVS_cost)
  {
    //is better than last (or ist first try).
    best_AVS_cost=loc_cost;
    best_nonzero=nonzero;
  }
  
  loc_cost=best_AVS_cost;
  nonzero=best_nonzero;
  *cost = best_AVS_cost; 
  
  return nonzero;
}

/*
*************************************************************************
* Function:4x4 Intra mode decision for an macroblock
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int Mode_Decision_for_Intra4x4Macroblock (double lambda,  int* cost)
{
  int  cbp=0, b8, cost8x8;
  int  stage_block8x8_pos=0;/*lgp*/

  for (*cost=0, b8=stage_block8x8_pos; b8<4; b8++)
  {
    if (Mode_Decision_for_8x8IntraBlocks (b8, lambda, &cost8x8))
    {
      cbp |= (1<<b8);
    }
    *cost += cost8x8;
  }

  return cbp;
}

/*
*************************************************************************
* Function:R-D Cost for an 8x8 Partition
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

double RDCost_for_8x8blocks (int*    cnt_nonz,   // --> number of nonzero coefficients
                      int*    cbp_blk,    // --> cbp blk
                      double  lambda,     // <-- lagrange multiplier
                      int     block,      // <-- 8x8 block number
                      int     mode,       // <-- partitioning mode
                      int     pdir,       // <-- prediction direction
                      int     ref,        // <-- reference frame
                      int     bwd_ref)   // <-- abp type
{
  int  i, j;
  int  rate=0, distortion=0;
  int  dummy, mrate;
  int  fw_mode, bw_mode;
  int  cbp     = 0;
  int  pax     = 8*(block%2);
  int  pay     = 8*(block/2);
  int  i0      = pax/8;
  int  j0      = pay/8;
  int  bframe  = (img->type==B_IMG);
  int  direct  = (bframe && mode==0);
  int  b8value = B8Mode2Value (mode, pdir);

  Macroblock    *currMB    = &img->mb_data[img->current_mb_nr];
  SyntaxElement *currSE    = &img->MB_SyntaxElements[currMB->currSEnr];
  int  **frefarr = refFrArr;  // For MB level field/frame 
  int  block_y = img->block_y;
  int  block_x = img->block_x;
  int pix_y = img->pix_y;
  int pix_x = img->pix_x;
  byte **imgY_original  =  imgY_org;
  int  incr_y=1,off_y=0;/*lgp*/

  //=====  GET COEFFICIENTS, RECONSTRUCTIONS, CBP
  if (direct)
  { 
      *cnt_nonz = LumaResidualCoding8x8 (&cbp, cbp_blk, block, 0, 0, 
                          max(0,frefarr[block_y+j0][block_x+i0]), 0);      
  }
  else
  {
    fw_mode   = (pdir==0||pdir==2 ? mode : 0);
    bw_mode   = (pdir==1||pdir==2 ? mode : 0);
    *cnt_nonz = LumaResidualCoding8x8 (&cbp, cbp_blk, block, fw_mode, bw_mode, ref, bwd_ref);
  }

  for (j=0; j<8; j++)/*lgp*/
    for (i=pax; i<pax+8; i++)
    {
      distortion += img->quad [imgY_original[pix_y+pay+/*j*/incr_y*j+off_y/*lgp*/][pix_x+i] - imgY[img->pix_y+pay+/*j*/incr_y*j+off_y/*lgp*/][img->pix_x+i]];
    }

  //=====   GET RATE
  //----- block 8x8 mode -----
  ue_linfo (b8value, dummy, &mrate, &dummy);
  rate += mrate;

  //----- motion information -----
  if (!direct)
  {  
      if (pdir==0 || pdir==2)
      {
        rate  += writeMotionVector8x8 (i0, j0, i0+1, j0+1, ref, 0, 1, mode);
      }
      if (pdir==1 || pdir==2)
      {
        rate  += writeMotionVector8x8 (i0, j0, i0+1, j0+1, bwd_ref, 0, 0, mode);
      }
  }

  //----- luminance coefficients -----
  if (*cnt_nonz)
  {
    currMB->cbp = cbp; 
    rate += writeLumaCoeff8x8 (block, 0);
    
  }

  return (double)distortion + lambda * (double)rate;
}

/*
*************************************************************************
* Function:Sets modes and reference frames for an macroblock
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/


void SetModesAndRefframeForBlocks (int mode)
{
  int i,j,k;
  Macroblock *currMB = &img->mb_data[img->current_mb_nr];
  int  bframe  = (img->type==B_IMG);
  int  **fwrefarr = fw_refFrArr;
  int  **bwrefarr = bw_refFrArr;
  int  **frefarr = refFrArr;
  int  block_y = img->block_y;
  int  block_x = img->block_x;

  int  stage_block8x8_pos=0;/*lgp*/

  //--- macroblock type ---
  currMB->mb_type = mode;

  //--- block 8x8 mode and prediction direction ---
  switch (mode)
  {
    case 0:
      for(i=stage_block8x8_pos/*lgp*/;i<4;i++)
      {
        currMB->b8mode[i] = 0;
        currMB->b8pdir[i] = (bframe?2:0);
      }
      break;
    case 1:
    case 2:
    case 3:
      for(i=stage_block8x8_pos/*lgp*/;i<4;i++)
      {
        currMB->b8mode[i] = mode;
        currMB->b8pdir[i] = best8x8pdir[mode][i];
      }
      break;
    //case 4:
    case P8x8:
      for(i=stage_block8x8_pos/*lgp*/;i<4;i++)
      {
        currMB->b8mode[i]   = best8x8mode[i];
        currMB->b8pdir[i]   = best8x8pdir[mode][i];
      }
      break;
    case I4MB:
      for(i=stage_block8x8_pos/*lgp*/;i<4;i++)
      {
        currMB->b8mode[i] = IBLOCK;
        currMB->b8pdir[i] = -1;
     }
      break;
    default:
      printf ("Unsupported mode in SetModesAndRefframeForBlocks!\n");
      exit (1);
  }

  //--- reference frame arrays ---
  if (mode==0 || mode==I4MB)
  {
    if (bframe)
    { 
      for (j=stage_block8x8_pos/2/*lgp*/;j<2;j++)
        for (i=0;i<2;i++)
        {
      k = 2*j+i;
          if(!mode)
          {
       if(!img->picture_structure)
       {
         fwrefarr[(block_y>>1)+j][(block_x>>1)+i] = best8x8symref[mode][k][0];
         bwrefarr[(block_y>>1)+j][(block_x>>1)+i] = best8x8symref[mode][k][1];
       }
       else
       {
         fwrefarr[(block_y>>1)+j][(block_x>>1)+i] = 0;
         //sw 11.23
         if(img->picture_structure)
           bwrefarr[(block_y>>1)+j][(block_x>>1)+i] = 0;// min(frefarr[ipdirect_y][ipdirect_x],0);
         else
          bwrefarr[(block_y>>1)+j][(block_x>>1)+i] = 0; 
       }
          }
          else
          {
            fwrefarr[(block_y>>1)+j][(block_x>>1)+i] = -1;
            bwrefarr[(block_y>>1)+j][(block_x>>1)+i] = -1;          
          }
        }
    }
    else
    {
      for (j=stage_block8x8_pos/2;j<2;j++)
        for (i=0;i<2;i++)
        {
          frefarr [(block_y>>1)+j][(block_x>>1)+i] = (mode==0?0:-1);
        }
    }
  }
  else
  {
    if (bframe)
    {
      for (j=stage_block8x8_pos/2;j<2;j++)
        for (i=0;i<2;i++)
        {
          k = 2*j+i;
          
          if((mode==P8x8) && (best8x8mode[k]==0))
          {
       if(!img->picture_structure)
       {
         fwrefarr[(block_y>>1)+j][(block_x>>1)+i] = best8x8symref[0][k][0];
         bwrefarr[(block_y>>1)+j][(block_x>>1)+i] = best8x8symref[0][k][1];
       }
       else
       {
         fwrefarr[(block_y>>1)+j][(block_x>>1)+i] = 0;
         bwrefarr[(block_y>>1)+j][(block_x>>1)+i] = 0;
       }
          }
          else
          {
        if(IS_FW&&IS_BW)
        {
          fwrefarr[(block_y>>1)+j][(block_x>>1)+i] = best8x8symref[mode][k][0];
          bwrefarr[(block_y>>1)+j][(block_x>>1)+i] = best8x8symref[mode][k][1];
        }
        else
        {
          fwrefarr[(block_y>>1)+j][(block_x>>1)+i] = (IS_FW ? best8x8ref[mode][k] : -1);
          bwrefarr[(block_y>>1)+j][(block_x>>1)+i] = (IS_BW ? best8x8bwref[mode][k] : -1);
        }
          }
        }
    }
    else
    {
      for (j=stage_block8x8_pos/2/*lgp*/;j<2;j++)
        for (i=0;i<2;i++)
        {
          k = 2*j+i;
          frefarr [(block_y>>1)+j][(block_x>>1)+i] = (IS_FW ? best8x8ref[mode][k] : -1);
        }
    }
  }

}

/*
*************************************************************************
* Function:Sets Coefficients and reconstruction for an 8x8 block
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void SetCoeffAndReconstruction8x8 (Macroblock* currMB)
{
  int block, k, j, i;
  int block_x = img->block_x;
  int block_y = img->block_y;
  int **ipredmodes = img->ipredmode;
  int  stage_block8x8_pos=0; /*lgp*/
  int  incr_y=1,off_y=0;/*lgp*/

  //--- restore coefficients ---
  for (block=0; block<6; block++)
    for (k=0; k<4; k++)
      for (j=0; j<2; j++)
        for (i=0; i<65; i++)
          img->cofAC[block][k][j][i] = cofAC8x8[block][k][j][i];

  if (cnt_nonz_8x8<=5)/*lgp*/
  {
    currMB->cbp     = 0;
    currMB->cbp_blk = 0;

    for (j=0; j<16; j++)
      for (i=0; i<16; i++)
        imgY[img->pix_y+j][img->pix_x+i] = mpr8x8[j][i];

  }
  else
  {
    currMB->cbp     = cbp8x8;
    currMB->cbp_blk = cbp_blk8x8;

    for(k=stage_block8x8_pos/2;k<2;k++)
    {
      for (j=0; j<8; j++)
        for (i=0; i<16; i++)
          imgY[img->pix_y+k*8+incr_y*j+off_y][img->pix_x+i] = rec_mbY8x8[k*8+incr_y*j+off_y][i];
    }
  }

  //===== restore intra prediction modes for 8x8+ macroblock mode =====
  for (j=img->block8_y+1+stage_block8x8_pos/2; j<img->block8_y+3; j++)
    for (     i=img->block8_x+1; i<img->block8_x+3; i++)
    {
      k = (j-img->block8_y-1)*2 + i -img->block8_x -1;
      ipredmodes    [i][j] = b8_ipredmode       [k];
      currMB->intra_pred_modes[k] = b8_intra_pred_modes[k];
    }

}

/*
*************************************************************************
* Function:Sets motion vectors for an macroblock
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void SetMotionVectorsMB (Macroblock* currMB, int bframe)
{
  int i, j, k, mode8, pdir8, ref, by, bx, bxr, dref;
  int ***mv        = tmp_mv;
  int *****all_mv  = img->all_mv;
  int *****all_bmv = img->all_bmv;
  int ***fwMV      = tmp_fwMV;
  int ***bwMV      = tmp_bwMV;
  int *****imgmv   = img->mv;
  int *****p_fwMV  = img->p_fwMV;
  int *****p_bwMV  = img->p_bwMV;
  int **refar      = refFrArr;
  int **frefar     = fw_refFrArr;
  int block_y      = img->block_y>>1;
  int block_x      = img->block_x>>1;
  int **brefar     = bw_refFrArr;
  int  bw_ref;
  int stage_block8x8_pos=0;/*lgp*/

  for (j=stage_block8x8_pos/2/*lgp*/;j<2;j++)
    for (i=0; i<2; i++)
    {
      mode8 = currMB->b8mode[k=2*j+i];
      pdir8 = currMB->b8pdir[k];
      if(pdir8 == 2 && mode8 != 0) //mode8 != 0 added by xyji 07.11
      {
        //sw 10.1
        all_mv = img->all_omv;
        p_fwMV = img->omv;
      }
      else
      {
        all_mv = img->all_mv;
        imgmv = img->mv;
      }

      by    = block_y+j;
      bxr   = block_x+i;
      bx    = block_x+i+4;
      ref   = (bframe?frefar:refar)[by][bxr];
      bw_ref = (bframe?brefar:refar)[by][bxr];
      
      if (!bframe)
      {
        if (mode8!=IBLOCK && ref != -1)
        {
          mv   [0][by][bx] = all_mv [i][j][ ref][mode8][0];
          mv   [1][by][bx] = all_mv [i][j][ ref][mode8][1];
        }
        else
        {
          mv   [0][by][bx] = 0;
          mv   [1][by][bx] = 0;
        }
      }
      else
      {
        if (pdir8==-1) // intra
        {
          fwMV [0][by][bx]     = 0;
          fwMV [1][by][bx]     = 0;
          bwMV [0][by][bx]     = 0;
          bwMV [1][by][bx]     = 0;
          dfMV     [0][by][bx] = 0;
          dfMV     [1][by][bx] = 0;
          dbMV     [0][by][bx] = 0;
          dbMV     [1][by][bx] = 0;
        }
        else if (pdir8==0) // forward
        {
          fwMV [0][by][bx]     = all_mv [i][j][ ref][mode8][0];
          fwMV [1][by][bx]     = all_mv [i][j][ ref][mode8][1];
          bwMV [0][by][bx]     = 0;
          bwMV [1][by][bx]     = 0;
          dfMV     [0][by][bx] = 0;
          dfMV     [1][by][bx] = 0;
          dbMV     [0][by][bx] = 0;
          dbMV     [1][by][bx] = 0;
        }
        else if (pdir8==1) // backward
        {
          fwMV [0][by][bx] = 0;
          fwMV [1][by][bx] = 0;
          {
            bwMV [0][by][bx] = all_bmv[i][j][   bw_ref][mode8][0];/*lgp*13*/
            bwMV [1][by][bx] = all_bmv[i][j][   bw_ref][mode8][1];/*lgp*13*/
          }
          dfMV     [0][by][bx] = 0;
          dfMV     [1][by][bx] = 0;
          dbMV     [0][by][bx] = 0;
          dbMV     [1][by][bx] = 0;
        }
        else if (mode8!=0) // bidirect
        {
          fwMV [0][by][bx] = all_mv [i][j][ ref][mode8][0];
          fwMV [1][by][bx] = all_mv [i][j][ ref][mode8][1];
      {
        int delta_P,TRp,DistanceIndexFw,DistanceIndexBw,refframe,delta_PB;
        refframe = ref;
        delta_P = 2*(img->imgtr_next_P_frm - img->imgtr_last_P_frm);
        delta_P = (delta_P + 512)%512;   // Added by Xiaozhen ZHENG, 2007.05.05
        if(img->picture_structure)
          TRp = (refframe+1)*delta_P;  //the lates backward reference
        else
        {
          TRp = delta_P;//refframe == 0 ? delta_P-1 : delta_P+1;
        }
        delta_PB = 2*(picture_distance - img->imgtr_last_P_frm);    // Tsinghua 200701
        TRp = (TRp + 512)%512;
        delta_PB = (delta_PB + 512)%512;  // Added by Xiaozhen ZHENG, 2007.05.05
        if(!img->picture_structure)
        {
          if(img->current_mb_nr_fld < img->total_number_mb) //top field
            DistanceIndexFw =  refframe == 0 ? delta_PB-1:delta_PB;
          else
            DistanceIndexFw =  refframe == 0 ? delta_PB:delta_PB+1;
        }
        else
          DistanceIndexFw = delta_PB;
        
        //DistanceIndexBw  = TRp - DistanceIndexFw;         
        DistanceIndexBw    = (TRp - DistanceIndexFw+512)%512; // Added by Zhijie Yang, 20070419, Broadcom

          
      bwMV [0][by][bx] = - ((all_mv[i][j][ref][mode8][0]*DistanceIndexBw*(512/DistanceIndexFw)+256)>>9); 
      bwMV [1][by][bx] = - ((all_mv[i][j][ref][mode8][1]*DistanceIndexBw*(512/DistanceIndexFw)+256)>>9); 
      }

          dfMV     [0][by][bx] = 0;
          dfMV     [1][by][bx] = 0;
          dbMV     [0][by][bx] = 0;
          dbMV     [1][by][bx] = 0;
        }
        else // direct
        {
      if(!img->picture_structure)
      {
        dref = max(0,fw_refFrArr[by][bxr]);
        fwMV [0][by][bx] = dfMV     [0][by][bx] = all_mv [i][j][dref][    0][0];
        fwMV [1][by][bx] = dfMV     [1][by][bx] = all_mv [i][j][dref][    0][1];
        dref = max(0,bw_refFrArr[by][bxr]);
        bwMV [0][by][bx] = dbMV     [0][by][bx] = all_bmv[i][j][   dref][    0][0];
        bwMV [1][by][bx] = dbMV     [1][by][bx] = all_bmv[i][j][   dref][    0][1];
      }
      else
      {
        dref = 0;
      //sw
      /*
      fwMV [0][by][bx] = dfMV     [0][by][bx] = all_mv [i][j][dref][    0][0];
      fwMV [1][by][bx] = dfMV     [1][by][bx] = all_mv [i][j][dref][    0][1];*/       
      fwMV [0][by][bx] = dfMV     [0][by][bx] = all_mv [i][j][dref][    0][0];
      fwMV [1][by][bx] = dfMV     [1][by][bx] = all_mv [i][j][dref][    0][1];
      
      bwMV [0][by][bx] = dbMV     [0][by][bx] = all_bmv[i][j][   0][    0][0];
      bwMV [1][by][bx] = dbMV     [1][by][bx] = all_bmv[i][j][   0][    0][1];
      }
        }
      }
  }
}

/*
*************************************************************************
* Function:R-D Cost for a macroblock
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

int RDCost_for_macroblocks (double   lambda,      // <-- lagrange multiplier
                            int      mode,        // <-- modus (0-COPY/DIRECT, 1-16x16, 2-16x8, 3-8x16, 4-8x8(+), 5-Intra4x4, 6-Intra16x16)
                            double*  min_rdcost)  // <-> minimum rate-distortion cost
{
  int        i, j; //, k, ****ip4;
  int         rate=0, distortion=0;
  double      rdcost;
  Macroblock  *currMB       = &img->mb_data[img->current_mb_nr];
  int         bframe        = (img->type==B_IMG);
  int         tmp_cc;
  int         use_of_cc     =  (img->type!=INTRA_IMG);
  int         cc_rate, dummy;
  byte        **imgY_orig   = imgY_org;
  byte        ***imgUV_orig = imgUV_org;
  int         pix_y         = img->pix_y;
  int         pix_c_y       = img->pix_c_y;
  int         pix_x         = img->pix_x;
  int         pix_c_x       = img->pix_c_x;
  /*lgp*/
  int         rate_tmp,rate_top=0,rate_bot=0,distortion_blk,distortion_top=0,distortion_bot=0;
  double      rdcost_top,rdcost_bot;
  int         incr_y=1,off_y=0;
  int         block8x8,block_x,block_y;
  int         stage_block8x8_pos=0;
  int         c_distortion=0,lum_bits;
  int         cp_table[2][8] = {{1,1,1,1,1,1,1,1},{0,0,1,1,0,1,0,1}};  //wzm,422

  //cjw 20060321 for MV limit
//  mv_out_of_range=0;    // 20071009
  mv_out_of_range = 1;  // mv_range, 20071009

  //=====  SET REFERENCE FRAMES AND BLOCK MODES
  SetModesAndRefframeForBlocks (mode);

  //=====  GET COEFFICIENTS, RECONSTRUCTIONS, CBP
  if (mode==I4MB)
  {
    currMB->cbp = Mode_Decision_for_Intra4x4Macroblock (lambda, &dummy);
  }
/*  else if (mode==P8x8)
  {
    SetCoeffAndReconstruction8x8 (currMB);
  }
*/  else
  {
    LumaResidualCoding ();
  }

  //Rate control
  for(i=0; i<16; i++)
  for(j=0; j<16; j++)
    pred[j][i] = img->mpr[i][j];

  dummy = 0;
  ChromaResidualCoding (&dummy);
  //wzm,422
  if(input->chroma_format==2)
  {
    img->mb_data[img->current_mb_nr].cbp01=(img->mb_data[img->current_mb_nr].cbp)>>6;
    img->mb_data[img->current_mb_nr].cbp=(img->mb_data[img->current_mb_nr].cbp&63);
  }
  //wzm,422


  //////////////////////////////////////////////////////////////////////////
  ////////////////////////****cjw  qhg add 20060327*************////////////
    ///Just for Direct mode MB's weighting prediction/////////////////////////
    ///if the direct MB has a zero cbp, it will be coded as a skip MB ////////
    ///then the weighting prediction is unnecessary //////////////////////////
  //////////////////////////////////////////////////////////////////////////
  if(img->LumVarFlag&&img->mb_weighting_flag&&img->weighting_prediction&&(IS_DIRECT(currMB))&&(currMB->cbp==0)) //qhg add
  {
    int bw_ref;
    int sum_cnt_nonz=0;
    int fw_mode, bw_mode, refframe,coeff_cost;
    int mb_x;
    int mb_y;
    
    img->weighting_prediction=0;
    for (block8x8=stage_block8x8_pos/*lgp*/; block8x8<4; block8x8++)
    {
                mb_x = (block8x8 % 2) << 3;
              mb_y = (block8x8 / 2) << 3;
      SetModesAndRefframe (block8x8, &fw_mode, &bw_mode, &refframe, &bw_ref);
      sum_cnt_nonz += LumaResidualCoding8x8 (&(currMB->cbp), &(currMB->cbp_blk), block8x8,fw_mode, bw_mode, refframe, bw_ref);
    }
    for(i=0; i<16; i++)
      for(j=0; j<16; j++)
        pred[j][i] = img->mpr[i][j];
      ChromaResidualCoding(&dummy);
  }
//////////////////////////////////////////////////////////////////////////
////////////////********************* end *****************************///
//////////////////////////////////////////////////////////////////////////

  //=====   GET DISTORTION
  // LUMA
  /*lgp*/
  for(block8x8=stage_block8x8_pos; block8x8<4;block8x8++)
  {
    block_y = (block8x8/2)*8;
    block_x = (block8x8%2)*8;

    distortion_blk =0;

    for (j=0; j<8; j++)
      for (i=0; i<8; i++)
      {
        distortion_blk += img->quad [imgY_orig[block_y+incr_y*j+off_y+pix_y][block_x+i+pix_x] - imgY[img->pix_y+block_y+incr_y*j+off_y][img->pix_x+block_x+i]];
      }
      
      block_y = (block8x8/2)*4;
      block_x = (block8x8%2)*4;

      // CHROMA
    if(input->chroma_format==2) //wzm,422
    {
       for (j=0; j<8; j++)     
        for (i=0; i<4; i++)
        {
          distortion_blk += img->quad [imgUV_orig[0][block_y+incr_y*j+off_y+pix_c_y][block_x+i+pix_c_x] - imgUV[0][img->pix_c_y+block_y+incr_y*j+off_y][img->pix_c_x+block_x+i]];
          distortion_blk += img->quad [imgUV_orig[1][block_y+incr_y*j+off_y+pix_c_y][block_x+i+pix_c_x] - imgUV[1][img->pix_c_y+block_y+incr_y*j+off_y][img->pix_c_x+block_x+i]];

          c_distortion += img->quad [imgUV_orig[0][block_y+incr_y*j+off_y+pix_c_y][block_x+i+pix_c_x] - imgUV[0][img->pix_c_y+block_y+incr_y*j+off_y][img->pix_c_x+block_x+i]];
          c_distortion += img->quad [imgUV_orig[1][block_y+incr_y*j+off_y+pix_c_y][block_x+i+pix_c_x] - imgUV[1][img->pix_c_y+block_y+incr_y*j+off_y][img->pix_c_x+block_x+i]];

        }
     }
     else if(input->chroma_format==1)//wzm,422
     {
      for (j=0; j<4; j++)
        for (i=0; i<4; i++)
        {
          distortion_blk += img->quad [imgUV_orig[0][block_y+incr_y*j+off_y+pix_c_y][block_x+i+pix_c_x] - imgUV[0][img->pix_c_y+block_y+incr_y*j+off_y][img->pix_c_x+block_x+i]];
          distortion_blk += img->quad [imgUV_orig[1][block_y+incr_y*j+off_y+pix_c_y][block_x+i+pix_c_x] - imgUV[1][img->pix_c_y+block_y+incr_y*j+off_y][img->pix_c_x+block_x+i]];

          c_distortion += img->quad [imgUV_orig[0][block_y+incr_y*j+off_y+pix_c_y][block_x+i+pix_c_x] - imgUV[0][img->pix_c_y+block_y+incr_y*j+off_y][img->pix_c_x+block_x+i]];
          c_distortion += img->quad [imgUV_orig[1][block_y+incr_y*j+off_y+pix_c_y][block_x+i+pix_c_x] - imgUV[1][img->pix_c_y+block_y+incr_y*j+off_y][img->pix_c_x+block_x+i]];

        }
     }

      if(block8x8<2)
        distortion_top += distortion_blk;
      else
        distortion_bot += distortion_blk;

  }

  //=====   S T O R E   C O D I N G   S T A T E   =====
  store_coding_state (cs_cm);

  //=====   GET RATE
  //----- macroblock header -----
if (input->chroma_format==2) //wzm,422
{
  if (use_of_cc)
  {
    if (currMB->mb_type!=0 || (bframe && (currMB->cbp!=0||currMB->cbp01!=0)))
    {
      // cod counter and macroblock mode are written ==> do not consider code counter
      tmp_cc = img->cod_counter;
      rate_top   = writeMBHeader (1); 
      ue_linfo (tmp_cc, dummy, &cc_rate, &dummy);
      rate_top  -= cc_rate;/*lgp*/
      img->cod_counter = tmp_cc;
    }
    else
    {
      // cod counter is just increased  ==> get additional rate
      ue_linfo (img->cod_counter+1, dummy, &rate_top,    &dummy);
      ue_linfo (img->cod_counter,   dummy, &cc_rate, &dummy);
      rate_top -= cc_rate;/*lgp*/
    }
  }
  else if (!(stage_block8x8_pos/2))/*lgp*/
  {
    rate_top = writeMBHeader (1); /*lgp*/
  }
  if (mode)
  {
    //----- motion information -----
    storeMotionInfo (stage_block8x8_pos/2);
    writeReferenceIndex(stage_block8x8_pos/2,&rate_top,&rate_bot);
    writeMVD(stage_block8x8_pos/2,&rate_top,&rate_bot);
  writeCBPandDqp(0,&rate_top,&rate_bot);
  }

  if (mode || (bframe && (currMB->cbp!=0||currMB->cbp01!=0)))
  {
    for(block8x8=stage_block8x8_pos; block8x8<8;block8x8++)  //modified by wuzhongmou 422
      if(cp_table[stage_block8x8_pos/2][block8x8])
      {
        rate_tmp = writeBlockCoeff (block8x8);
        
        if(block8x8 < 2)
          rate_top +=rate_tmp;
        else if(block8x8< 4)
          rate_bot +=rate_tmp;
        if(block8x8 == 4)
          rate_top +=rate_tmp;
        if(block8x8 == 5)
          rate_bot +=rate_tmp;
        if(block8x8 == 6)  //X ZHENG,422
          rate_top +=rate_tmp;
        if(block8x8 == 7)  //X ZHENG,422
          rate_bot +=rate_tmp;
        
        if(block8x8 < 4)
          lum_bits = rate_bot+rate_top;
      }
      
  }
}
else if(input->chroma_format==1) //wzm,422
{
  if (use_of_cc)
  {
    if (currMB->mb_type!=0 || (bframe && currMB->cbp!=0))
    {
      // cod counter and macroblock mode are written ==> do not consider code counter
      tmp_cc = img->cod_counter;
      rate_top   = writeMBHeader (1); 
      ue_linfo (tmp_cc, dummy, &cc_rate, &dummy);
      rate_top  -= cc_rate;/*lgp*/
      img->cod_counter = tmp_cc;
    }
    else
    {
      // cod counter is just increased  ==> get additional rate
      ue_linfo (img->cod_counter+1, dummy, &rate_top,    &dummy);
      ue_linfo (img->cod_counter,   dummy, &cc_rate, &dummy);
      rate_top -= cc_rate;/*lgp*/
    }
  }
  else if (!(stage_block8x8_pos/2))/*lgp*/
  {
    rate_top = writeMBHeader (1); /*lgp*/
  }

  if (mode)
  {
    //----- motion information -----
    storeMotionInfo (stage_block8x8_pos/2);
    writeReferenceIndex(stage_block8x8_pos/2,&rate_top,&rate_bot);
    writeMVD(stage_block8x8_pos/2,&rate_top,&rate_bot);
    writeCBPandDqp(0,&rate_top,&rate_bot);
  }

  if (mode || (bframe && (currMB->cbp!=0)))
  {
    for(block8x8=stage_block8x8_pos; block8x8<6;block8x8++)
      if(cp_table[stage_block8x8_pos/2][block8x8])
      {
        rate_tmp = writeBlockCoeff (block8x8);
        
        if(block8x8 < 2)
          rate_top +=rate_tmp;
        else if(block8x8< 4)
          rate_bot +=rate_tmp;
        if(block8x8 == 4)
          rate_top +=rate_tmp;
        if(block8x8 == 5)
          rate_bot +=rate_tmp;
        
        if(block8x8 < 4)
          lum_bits = rate_bot+rate_top;
      }
      
  }
}
  
  //=====   R E S T O R E   C O D I N G   S T A T E   =====
  reset_coding_state (cs_cm);
  
  rdcost_top = (double)distortion_top + lambda * (double)rate_top; 
  rdcost_bot = (double)distortion_bot + lambda * (double)rate_bot;
    
  rdcost = rdcost_top + rdcost_bot;
    
  if (rdcost >= *min_rdcost)
  {
    return 0;   
  }

  //cjw 20060321 for MV limit
//  if ( mv_out_of_range == 1 )      // 20071009
    if (!mv_out_of_range)        // mv_range, 20071009
  {
    return 0;   //return 0 means it is not the best mode
  }

  //=====   U P D A T E   M I N I M U M   C O S T   =====
  *min_rdcost = rdcost;

  return 1;
}

/*
*************************************************************************
* Function:Store macroblock parameters
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void store_macroblock_parameters (int mode)
{
  int  i, j, k,l,****i4p, ***i3p;
  Macroblock *currMB  = &img->mb_data[img->current_mb_nr];
  int        bframe   = (img->type==B_IMG);
  int        **frefar = ((img->type==B_IMG)? fw_refFrArr : refFrArr);
  int        **brefar = bw_refFrArr;
  int    block_x  = img->block_x;
  int    pix_x    = img->pix_x;
  int    pix_c_x  = img->pix_c_x;
  int    block_y  = img->block_y;
  int    pix_y    = img->pix_y;
  int    pix_c_y  = img->pix_c_y;
  int    stage_block8x8_pos=0;/*lgp*/
  int    temp_chroma;

  //--- store best mode ---
  best_mode = mode;
  best_c_imode = currMB->c_ipred_mode;
  best_c_imode_422 = currMB->c_ipred_mode_2; //X ZHENG,422

  // !! shenyanfei 
  best_weight_flag = img->weighting_prediction ;

  for (i=/*0*/stage_block8x8_pos/*lgp*/; i<4; i++)
  {
    b8mode[i]   = currMB->b8mode[i];
    b8pdir[i]   = currMB->b8pdir[i];
  }

  //--- reconstructed blocks ----
  for (j=0; j<16; j++)
    for (i=0; i<16; i++)
    {
      rec_mbY[j][i] = imgY[img->pix_y+j][img->pix_x+i];
    }

  if(input->chroma_format==2) //wzm,422
  {
    for (j=0; j<16; j++)   
      for (i=0; i<8; i++)
    {
        rec_mbU[j][i] = imgUV[0][img->pix_c_y+j][img->pix_c_x+i];
        rec_mbV[j][i] = imgUV[1][img->pix_c_y+j][img->pix_c_x+i];
    }
  }
  else //wzm,422
  {
  for (j=0; j<8; j++)
    for (i=0; i<8; i++)
    {
      rec_mbU[j][i] = imgUV[0][img->pix_c_y+j][img->pix_c_x+i];
      rec_mbV[j][i] = imgUV[1][img->pix_c_y+j][img->pix_c_x+i];
    }
  }

  if(best_mode == I4MB)    //add by qwang
  {
    for(i=stage_block8x8_pos/*lgp*/; i<4; i++)
      best_intra_pred_modes_tmp[i]=currMB->intra_pred_modes[i];
    
    for(j=stage_block8x8_pos/2/*lgp*/;j<2;j++)
      for(i=0;i<2;i++)
        best_ipredmode_tmp[i][j]=img->ipredmode[ 1 + img->block8_x + i ][ 1 + img->block8_y + j ];
      
      for(j=0;j<16;j++)
        for(i=0;i<16;i++)
        {
          best_mpr_tmp[i][j]=img->mpr[i][j];
        }
  }


  //--- coeff, cbp, kac ---
  if (mode || bframe)
  {
    i4p=cofAC; cofAC=img->cofAC; img->cofAC=i4p;
    i3p=cofDC; cofDC=img->cofDC; img->cofDC=i3p;
    cbp     = currMB->cbp;
    cbp_blk = currMB->cbp_blk;
  cbp01   = currMB->cbp01;   //add by wuzhongmou 422
    
    if(1)//input->InterlaceCodingOption == SMB_CODING)
    {
      for(j=0;j<4;j++)
        for(k=0;k<2;k++)
          for(l=0;l<17;l++)
          {
            temp_chroma = chromacofAC4x4[0][j][k][l];
            chromacofAC4x4[0][j][k][l] = img->chromacofAC[0][j][k][l];
            img->chromacofAC[0][j][k][l] = temp_chroma;
            
            temp_chroma = chromacofAC4x4[1][j][k][l];
            chromacofAC4x4[1][j][k][l] = img->chromacofAC[1][j][k][l];
            img->chromacofAC[1][j][k][l] = temp_chroma;
          }
    }
  }
  else
  {
    cbp = cbp_blk = 0;
  }

  for(j=stage_block8x8_pos/2/*lgp*/;j<2;j++)
    for (i=0; i<2; i++)
    {  
      frefframe[j][i] = frefar[(block_y>>1)+j  ][(block_x>>1) +i ];
      if (bframe)
      {
        brefframe[j][i] = brefar[(block_y>>1) +j ][(block_x>>1)+i  ];
      }
    }

}

/*
*************************************************************************
* Function:Set stored macroblock parameters
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void set_stored_macroblock_parameters ()
{
  int  i, j, k, ****i4p, ***i3p,l;
  Macroblock  *currMB  = &img->mb_data[img->current_mb_nr];
  int         mode     = best_mode;
  int         bframe   = (img->type==B_IMG);
  int         **frefar = ((img->type==B_IMG)? fw_refFrArr : refFrArr);
  int         **brefar = bw_refFrArr;
  int     **ipredmodes = img->ipredmode;
  int     block_x = img->block_x; 
  int     block_y = img->block_y; 

  int         cp_table[2][6] = {{1,1,1,1,1,1},{0,0,1,1,0,1}};/*lgp*/
  int         stage_block8x8_pos=0;/*lgp*/
  int    temp_chroma;

  //===== reconstruction values =====
  for (j=0; j<16; j++)
    for (i=0; i<16; i++)
    {
      imgY[img->pix_y+j][img->pix_x+i] = rec_mbY[j][i];
    }

  if(input->chroma_format==2) //wzm,422
  {
    for (j=0; j<16; j++)   
      for (i=0; i<8; i++)
    {
        imgUV[0][img->pix_c_y+j][img->pix_c_x+i] = rec_mbU[j][i];
        imgUV[1][img->pix_c_y+j][img->pix_c_x+i] = rec_mbV[j][i];
    }
  }
  else //wzm,422
  {
  for (j=0; j<8; j++)
    for (i=0; i<8; i++)
    {
      imgUV[0][img->pix_c_y+j][img->pix_c_x+i] = rec_mbU[j][i];
      imgUV[1][img->pix_c_y+j][img->pix_c_x+i] = rec_mbV[j][i];
    }
  }

  //===== coefficients and cbp =====
  i4p=cofAC; cofAC=img->cofAC; img->cofAC=i4p;
  i3p=cofDC; cofDC=img->cofDC; img->cofDC=i3p;
  currMB->cbp      = cbp;
  currMB->cbp_blk = cbp_blk;
  currMB->cbp01      = cbp01;  //wzm,422
  //==== macroblock type ====
  currMB->mb_type = mode;

  // !! shenyanfei 
  img->weighting_prediction = best_weight_flag ;

    for(j=0;j<4;j++)
      for(k=0;k<2;k++)
        for(l=0;l<17;l++)
        {
          temp_chroma = chromacofAC4x4[0][j][k][l];
          chromacofAC4x4[0][j][k][l] = img->chromacofAC[0][j][k][l];
          img->chromacofAC[0][j][k][l] = temp_chroma;

          temp_chroma = chromacofAC4x4[1][j][k][l];
          chromacofAC4x4[1][j][k][l] = img->chromacofAC[1][j][k][l];
          img->chromacofAC[1][j][k][l] = temp_chroma;
        }

  for (i=stage_block8x8_pos; i<4; i++)
  {
    currMB->b8mode[i]   = b8mode[i];
    currMB->b8pdir[i]   = b8pdir[i];
  }

  //==== reference frames =====
  for (j=stage_block8x8_pos/2; j<2; j++)
    for (i=0; i<2; i++)
    {
      frefar[(block_y>>1)+j][(block_x>>1)+i] = frefframe[j][i];
    }

  if (bframe)
  {
    for (j=stage_block8x8_pos/2; j<2; j++)
      for (i=0; i<2; i++)
      {
        brefar[(block_y>>1)+j][(block_x>>1)+i] = brefframe[j][i];
      }
  }

  if(currMB->mb_type == I4MB)    //add by qwang
  {
    for(i=stage_block8x8_pos; i<4; i++)
      currMB->intra_pred_modes[i]=best_intra_pred_modes_tmp[i];
    
    for(j=stage_block8x8_pos/2;j<2;j++)
      for(i=0;i<2;i++)
        img->ipredmode[ 1 + (img->mb_x<<1) + i ][ 1 + (img->mb_y<<1) + j ]=best_ipredmode_tmp[i][j];
      
    for(j=0;j<16;j++)
      for(i=0;i<16;i++)
      {
        img->mpr[i][j]=best_mpr_tmp[i][j];
      }
  }

  //==== intra prediction modes ====
  currMB->c_ipred_mode = best_c_imode;
  currMB->c_ipred_mode_2 = best_c_imode_422; //X ZHENG,422

  if (mode==P8x8)
  {
    for (j=img->block8_y+stage_block8x8_pos/2+1; j<img->block8_y+3; j++)
      for ( i=img->block8_x+1; i<img->block8_x+3; i++)
      {
        k = 2*(j-img->block8_y-1)+i-img->block8_x-1;
        ipredmodes[i][j] = b8_ipredmode[k];
        currMB->intra_pred_modes[k] = b8_intra_pred_modes[k];
      }
  }
  else if (mode!=I4MB)
  {
    for (j=img->block8_y+stage_block8x8_pos/2+1; j<img->block8_y+3; j++)
      for ( i=img->block8_x+1; i<img->block8_x+3; i++)
      {
        k = 2*(j-img->block8_y-1)+i-img->block8_x-1;
  
//        ipredmodes    [i][j] = DC_PRED;
        currMB->intra_pred_modes[k] = DC_PRED;

    ipredmodes    [i][j] = -1;
    //    currMB->intra_pred_modes[k] = -1;

    
      }
  }

  //==== motion vectors =====
  SetMotionVectorsMB (currMB, bframe);
  storeMotionInfo(stage_block8x8_pos/2);
}

/*
*************************************************************************
* Function:Set reference frames and motion vectors
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void SetRefAndMotionVectors (int block, int mode, int ref, int bw_ref,int pdir)/*lgp*13*/
{
  int i, j;
  int     bframe       = (img->type==B_IMG);
  int     pmode        = (mode==1||mode==2||mode==3?mode:4);
  int     j0           = (block/2);
  int     i0           = (block%2);
  int     j1           = j0 + (input->blc_size[pmode][1]>>3);
  int     i1           = i0 + (input->blc_size[pmode][0]>>3);
  int**   frefArr      = (bframe ? fw_refFrArr : refFrArr);
  int**   brefArr      = bw_refFrArr;
  //int***  fmvArr       = (bframe ? tmp_fwMV    : tmp_mv);
  int***  bmvArr       = tmp_bwMV;
  //int***** all_mv_arr  = img->all_mv;
  int***** all_bmv_arr = img->all_bmv;
  int   block_x        = img->block_x>>1;
  int   block_y        = img->block_y>>1; 

  int***  fmvArr  = (bframe ? tmp_fwMV    : tmp_mv);
  int***** all_mv_arr = (pdir==2 && mode != 0 /*&&mode ! =IBLOCK*/)?img->all_omv:img->all_mv; //mode != 0 added by xyji 7.11
  
  if ((pdir==0 || pdir==2) && (mode!=IBLOCK && mode!=0))
  {
    for (j=j0; j<j1; j++)
      for (i=i0; i<i1; i++)
      {
        fmvArr[0][block_y+j][block_x+i+4] = all_mv_arr[i][j][ref][mode][0];
        fmvArr[1][block_y+j][block_x+i+4] = all_mv_arr[i][j][ref][mode][1];
        frefArr  [block_y+j][block_x+i  ] = ref;
      }
  }
  else
  {
    if(!mode&&bframe)
    {
      for (j=j0; j<j0+1; j++)
        for (i=i0; i<i0+1; i++)
        {
      if(img->picture_structure)
          ref = 0;
          if(ref==-1)
          {
            fmvArr[0][block_y+j][block_x+i+4] = 0;
            fmvArr[1][block_y+j][block_x+i+4] = 0;
            frefArr  [block_y+j][block_x+i  ] = -1;
          }
          else
          {
            fmvArr[0][block_y+j][block_x+i+4] = all_mv_arr[i][j][ref][mode][0];
            fmvArr[1][block_y+j][block_x+i+4] = all_mv_arr[i][j][ref][mode][1];
      if(img->picture_structure)
        ref = 0;
            frefArr  [block_y+j][block_x+i  ] = ref;//???????for direct mode with ref;
          }
        }
    }
    else    
      for (j=j0; j<j0+1; j++)
        for (i=i0; i<i0+1; i++)
        {
          fmvArr[0][block_y+j][block_x+i+4] = 0;
          fmvArr[1][block_y+j][block_x+i+4] = 0;
          frefArr  [block_y+j][block_x+i  ] = -1;
        }
  }
  
  if ((pdir==1 || pdir==2) && (mode!=IBLOCK && mode!=0))
  {
    for (j=j0; j<j0+1; j++)
    for (i=i0; i<i0+1; i++)
    {
      if(pdir == 2)
      {
        {
          int delta_P,TRp,DistanceIndexFw,DistanceIndexBw,refframe,delta_PB;  
          refframe = ref;
          delta_P = 2*(img->imgtr_next_P_frm - img->imgtr_last_P_frm);
          delta_P = (delta_P + 512)%512;   // Added by Xiaozhen ZHENG, 2007.05.05
          if(img->picture_structure)
            TRp = (refframe+1)*delta_P;  //the lates backward reference
          else
          {
            TRp = delta_P;//refframe == 0 ? delta_P-1 : delta_P+1;
          }
          delta_PB = 2*(picture_distance - img->imgtr_last_P_frm);    // Tsinghua  200701
          TRp = (TRp + 512)%512;
          delta_PB = (delta_PB + 512)%512;   // Added by Xiaozhen ZHENG, 2007.05.05
          if(!img->picture_structure)
          {
            if(img->current_mb_nr_fld < img->total_number_mb) //top field
              DistanceIndexFw =  refframe == 0 ? delta_PB-1:delta_PB;
            else
              DistanceIndexFw =  refframe == 0 ? delta_PB:delta_PB+1;
          }
          else
            DistanceIndexFw = delta_PB;
          
          //DistanceIndexBw    = TRp - DistanceIndexFw;               
          DistanceIndexBw    = (TRp - DistanceIndexFw+512)%512; // Added by Zhijie Yang, 20070419, Broadcom
              
        bmvArr[0][block_y+j][block_x+i+4] = - ((all_mv_arr[i][j][ref][mode][0]*DistanceIndexBw*(512/DistanceIndexFw)+256)>>9);
        bmvArr[1][block_y+j][block_x+i+4] = - ((all_mv_arr[i][j][ref][mode][1]*DistanceIndexBw*(512/DistanceIndexFw)+256)>>9);
        }        
      }
      else
      {
        bmvArr[0][block_y+j][block_x+i+4] = all_bmv_arr[i][j][bw_ref][mode][0];
        bmvArr[1][block_y+j][block_x+i+4] = all_bmv_arr[i][j][bw_ref][mode][1];
      }
      brefArr  [block_y+j][block_x+i  ] = bw_ref;/*lgp*13*/
    }
  }
  else if (bframe)
  {
    if(!mode)
    {
      for (j=j0; j<j0+1; j++)
        for (i=i0; i<i0+1; i++)
        {
          {
            if(!img->picture_structure)
            {
              if(refFrArr[block_y+j][block_x+i] >= 0)
              {
                bmvArr[0][block_y+j][block_x+i+4] = all_bmv_arr[i][j][bw_ref][mode][0];
                bmvArr[1][block_y+j][block_x+i+4] = all_bmv_arr[i][j][bw_ref][mode][1];
              }
              else//intra prediction
              {
                bmvArr[0][block_y+j][block_x+i+4] = all_bmv_arr[i][j][1][mode][0];
                bmvArr[1][block_y+j][block_x+i+4] = all_bmv_arr[i][j][1][mode][1];
              }              
              
              if(img->current_mb_nr_fld < img->total_number_mb)
                brefArr  [block_y+j][block_x+i  ] = 1;
              else
                brefArr  [block_y+j][block_x+i  ] = 0;
              if(refFrArr[block_y+j][block_x+i] < 0)
                brefArr  [block_y+j][block_x+i  ] = 1;
              
            }
            else
            {
              bmvArr[0][block_y+j][block_x+i+4] = all_bmv_arr[i][j][0][mode][0];
              bmvArr[1][block_y+j][block_x+i+4] = all_bmv_arr[i][j][0][mode][1];
              ref = refFrArr[block_y+j][block_x+i];             
              //sw 11.23
              if(img->picture_structure)
                brefArr  [block_y+j][block_x+i  ] = 0;// min(ref,0); 
              else
                brefArr  [block_y+j][block_x+i  ] = 0;
            }
          }
        }   
    }
    else
    {
      for (j=j0; j<j0+1; j++)
        for (i=i0; i<i0+1; i++)
        {
          bmvArr[0][block_y+j][block_x+i+4] = 0;
          bmvArr[1][block_y+j][block_x+i+4] = 0;
          brefArr  [block_y+j][block_x+i  ] = -1;
        }
    }
  }
}
/*
*************************************************************************
* Function:Mode Decision for a macroblock
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void encode_one_macroblock ()
{
  static const int  b8_mode_table[2]  = {0, 4};         // DO NOT CHANGE ORDER !!!
  static const int  mb_mode_table[7]  = {0, 1, 2, 3, P8x8, I16MB, I4MB}; // DO NOT CHANGE ORDER !!!
  
  int         valid[MAXMODE];
  int         block, index, mode, i0, i1, j0, j1, pdir, ref, i, j, k, ctr16x16, dummy;
  double      qp, lambda_mode, lambda_motion, min_rdcost, rdcost = 0, max_rdcost=1e30;
  int         lambda_motion_factor;
  int         fw_mcost, bw_mcost, bid_mcost, mcost, max_mcost=(1<<30);
  int         curr_cbp_blk, cnt_nonz = 0, best_cnt_nonz = 0, best_fw_ref = 0, best_pdir;
  int         cost, min_cost, cost8x8, cost_direct=0, have_direct=0;
  int         intra       = img->type==INTRA_IMG;
  int         bframe      = img->type==B_IMG;
  int         write_ref   = input->no_multpred>1;
  int         max_ref     = img->nb_references;
  int         adjust_ref;
  Macroblock* currMB      = &img->mb_data[img->current_mb_nr];
  int     **ipredmodes = img->ipredmode;
  int     block_x   = img->block_x;/*lgp*/
  int     block_y   = img->block_y;
  int         best_bw_ref;
  int     ***tmpmvs = tmp_mv;
  int     *****allmvs = img->all_mv;
  int     **refar     = refFrArr;
  int     incr_y=1,off_y=0;/*lgp*/
  int     stage_block8x8_pos=0;/*lgp*/
  int    best_bid_ref = 0;
  int    bid_best_fw_ref = 0, bid_best_bw_ref = 0;
  int    scale_refframe;
  int     min_cost_8x8;
  
  
  adjust_ref  = (img->type==B_IMG? (mref==mref_fld)? 2 : 1 : 0);
  adjust_ref  = min(adjust_ref, max_ref-1);

  // 20071009
  img->mv_range_flag = 1;
  
  if(!img->picture_structure)
  {
    max_ref = min (img->nb_references, img->buf_cycle);
    adjust_ref = 0;
    if (max_ref > 2 && img->type == B_IMG)
      max_ref = 2;
    if(img->old_type!=img->type)
      max_ref = 1;
  }else
  {
    max_ref = min (img->nb_references, img->buf_cycle);
    adjust_ref = 0;
    if (max_ref > 1 && img->type == B_IMG)
      max_ref = 1;
  }
  
  //FAST MOTION ESTIMATION. ZHIBO CHEN 2003.3
  //in case the neighboring block is intra block
#ifdef FastME
  decide_intrabk_SAD();
#endif
  
  //===== SET VALID MODES =====
  valid[I4MB]   = 1;
  valid[I16MB]  = 0;//Lou
  
  valid[0]      =  (!intra );
  valid[1]      =  (!intra && input->InterSearch16x16);
  valid[2]      =  (!intra && input->InterSearch16x8);
  valid[3]      =  (!intra && input->InterSearch8x16);
  valid[4]      =  (!intra && input->InterSearch8x8);
  valid[5]      = 0;
  valid[6]      = 0;
  valid[7]      = 0;
  valid[P8x8]   = (valid[4] || valid[5] || valid[6] || valid[7]);
  
  //===== SET LAGRANGE PARAMETERS =====
  if (input->rdopt)
  {
    qp            = (double)img->qp - SHIFT_QP;
    
    if (input->successive_Bframe>0)
      //Lou 1014 lambda_mode   = 0.68 * pow (2, qp/3.0) * (img->type==B_IMG? max(2.00,min(4.00,(qp / 6.0))):1.0);  
      lambda_mode   = 0.68 * pow (2, qp/4.0) * (img->type==B_IMG? max(2.00,min(4.00,(qp / 8.0))):1.0);  
    else
      //Lou 1014 lambda_mode   = 0.85 * pow (2, qp/3.0) * (img->type==B_IMG? 4.0:1.0);  
      lambda_mode   = 0.85 * pow (2, qp/4.0) * (img->type==B_IMG? 4.0:1.0);  
    
    lambda_motion = sqrt (lambda_mode);
  }
  else
  {
    lambda_mode = lambda_motion = QP2QUANT[max(0,img->qp-SHIFT_QP)];
  }
  
  lambda_motion_factor = LAMBDA_FACTOR (lambda_motion);
  
  // reset chroma intra predictor to default
  currMB->c_ipred_mode = DC_PRED_8;
  currMB->c_ipred_mode_2 = DC_PRED_8; //X ZHENG,422
  
  if (!intra)
  {
    //===== set direct motion vectors =====
    
    if (bframe)
    {
      Get_IP_direct();
    }
    
#ifdef FastME
//    cost8x8 = 1<<20;  // 20071009
    cost8x8 = 1<<30;  // mv_range, 20071009
    //===== MOTION ESTIMATION FOR 16x16, 16x8, 8x16 BLOCKS =====
    for (min_cost=cost8x8, best_mode=P8x8, mode=1; mode<4; mode++)
    {
      if (valid[mode])
      {
        for (cost=0, block=stage_block8x8_pos/2; block<(mode==1?1:2); block++)
          //for (cost=0, block=0; block<(mode==1?1:2); block++)
        {    
          PartitionMotionSearch (mode, block, lambda_motion);
          //--- set 4x4 block indizes (for getting MV) ---
          j = (block==1 && mode==2 ? 2 : 0);
          i = (block==1 && mode==3 ? 2 : 0);
          
          //--- get cost and reference frame for forward prediction ---
          if(img->type==INTER_IMG)/*lgp*/
          {
            for (fw_mcost=max_mcost, ref=0; ref<max_ref-adjust_ref; ref++)
            {
              //if (!checkref || ref==0)/*lgp*13*/
              {
                mcost  = (input->rdopt ? write_ref ? REF_COST_FWD (lambda_motion_factor, ref) : 0 : (int)(2*lambda_motion*min(ref,1)));
                mcost += motion_cost[mode][ref+/*1*/1/*lgp*13*/][block];
                if (mcost < fw_mcost)
                {
                  fw_mcost    = mcost;
                  best_fw_ref = ref;
                }
              }
            }
            best_bw_ref = 0;//xyji
          }
          else
          {            
            for (fw_mcost=max_mcost, ref=0; ref<max_ref-adjust_ref; ref++)
            {
              //if (!checkref || ref==0)/*lgp*13*/
              {
                //mcost  = (input->rdopt ? write_ref ? REF_COST_FWD (lambda_motion_factor, ref) : 0 : (int)(2*lambda_motion*min(ref,1)));
                mcost = motion_cost[mode][img->picture_structure ? ref+1 : ref+2][block];
                if (mcost < fw_mcost)
                {
                  fw_mcost    = mcost;
                  best_fw_ref = ref;
                }
              }
            }
            best_bw_ref = 0;
          }
          
          if (bframe)
          {
            //--- get cost for bidirectional prediction ---
            PartitionMotionSearch_bid (mode, block, lambda_motion);
            best_bw_ref = 0;
            bw_mcost   = motion_cost[mode][0][block];
            
            
            for (bw_mcost=max_mcost, ref=0; ref<max_ref-adjust_ref; ref++)
            {
              //if (!checkref || ref==0)/*lgp*13*/
              {
                //mcost  = (input->rdopt ? write_ref ? REF_COST_FWD (lambda_motion_factor, ref) : 0 : (int)(2*lambda_motion*min(ref,1)));
                mcost = motion_cost[mode][ref][block];
                if (mcost < bw_mcost)
                {
                  bw_mcost    = mcost;
                  best_bw_ref = ref;
                }
              }
            }
            
            for (bid_mcost=max_mcost, ref=0; ref<max_ref-adjust_ref; ref++)
            {
              //if (!checkref || ref==0)/*lgp*13*/
              {
                //mcost  = (input->rdopt ? write_ref ? REF_COST_FWD (lambda_motion_factor, ref) : 0 : (int)(2*lambda_motion*min(ref,1)));
                mcost = motion_cost_bid[mode][img->picture_structure ? ref+1 : ref+2][block];
                if (mcost < bid_mcost)
                {
                  bid_mcost    = mcost;
                  bid_best_fw_ref = ref;
                  bid_best_bw_ref = ref;
                }
              }
            }
            
            //--- get prediction direction ----
            if (fw_mcost<=bw_mcost && fw_mcost<=bid_mcost)
            {
              best_pdir = 0;
              best_bw_ref = 0;
              cost += fw_mcost;    
            }
            else if (bw_mcost<=fw_mcost && bw_mcost<=bid_mcost)
            {
              best_pdir = 1;
              cost += bw_mcost;    
              best_fw_ref = 0;/*lgp*13*/
            }
            else
            {
              best_pdir = 2;
              cost += bid_mcost;    
            }
          }
          else // if (bframe)
          {
            best_pdir  = 0;
            cost      += fw_mcost;  
          }
          
          //----- set reference frame and direction parameters -----
          if (mode==3)
          {
            best8x8ref [3][  block] = best8x8ref [3][  block+2] = best_fw_ref;
            best8x8pdir[3][  block] = best8x8pdir[3][  block+2] = best_pdir;
            best8x8bwref   [3][block] = best8x8bwref   [3][block+2] = best_bw_ref;      
            best8x8symref  [3][block][0] = best8x8symref   [3][block+2][0] = bid_best_fw_ref;      
            best8x8symref  [3][block][1] = best8x8symref   [3][block+2][1] = bid_best_bw_ref;
          }
          else if (mode==2)
          {
            best8x8ref [2][2*block] = best8x8ref [2][2*block+1] = best_fw_ref;
            best8x8pdir[2][2*block] = best8x8pdir[2][2*block+1] = best_pdir;
            best8x8bwref   [2][2*block] = best8x8bwref   [2][2*block+1] = best_bw_ref;      
            best8x8symref   [2][2*block][0] = best8x8symref   [2][2*block+1][0] = bid_best_fw_ref;
            best8x8symref   [2][2*block][1] = best8x8symref   [2][2*block+1][1] = bid_best_bw_ref;      
          }
          else
          {
            best8x8ref [1][0] = best8x8ref [1][1] = best8x8ref [1][2] = best8x8ref [1][3] = best_fw_ref;
            best8x8pdir[1][0] = best8x8pdir[1][1] = best8x8pdir[1][2] = best8x8pdir[1][3] = best_pdir;
            best8x8bwref   [1][0] = best8x8bwref   [1][1] = best8x8bwref   [1][2] = best8x8bwref   [1][3] = best_bw_ref;      
            best8x8symref [1][0][0] = best8x8symref [1][1][0] = best8x8symref [1][2][0] = best8x8symref [1][3][0] = bid_best_fw_ref;
            best8x8symref [1][0][1] = best8x8symref [1][1][1] = best8x8symref [1][2][1] = best8x8symref [1][3][1] = bid_best_bw_ref;
          }
          
          //--- set reference frames and motion vectors ---
          if (mode>1 && block==0)   
            SetRefAndMotionVectors (block, mode, best_pdir==2?bid_best_fw_ref:best_fw_ref,
            best_pdir==2?bid_best_bw_ref:best_bw_ref, best_pdir);  
      } 
        
      // mv_range, 20071009      
      if(cost<0)
        cost=1<<30;
      
      if (cost < min_cost)
      {
        best_mode = mode;
        min_cost  = cost;
      }
      
      } // if (valid[mode])
    } // for (mode=3; mode>0; mode--)*/
#endif
    
    if (valid[P8x8])
    {
    cost8x8 = 0;
    //===== store coding state of macroblock =====
    store_coding_state (cs_mb);
    //=====  LOOP OVER 8x8 SUB-PARTITIONS  (Motion Estimation & Mode Decision) =====
    for (cbp8x8=cbp_blk8x8=cnt_nonz_8x8=0, block=stage_block8x8_pos; block<4; block++)
    {
      //--- set coordinates ---
      j0 = ((block/2)<<3);    j1 = (j0>>2);
      i0 = ((block%2)<<3);    i1 = (i0>>2);
      //=====  LOOP OVER POSSIBLE CODING MODES FOR 8x8 SUB-PARTITION  =====
      for (min_cost_8x8=/*(1<<20)*/(1<<30), min_rdcost=1e30, index=(bframe?0:1); index<2; index++) // mv_range, 20071009
      {
        if (valid[mode=b8_mode_table[index]])
        {
//           if(mode==0 && img->type==B_IMG && img->VEC_FLAG==1 && input->vec_ext==3)// M1956 by Grandview 2006.12.12
//             continue;
          curr_cbp_blk = 0;
          if (mode==0)
          {
            //--- Direct Mode ---
            if (!input->rdopt)
            {
              // mv_range, 20071009
              mv_out_of_range = 1;
              
              cost_direct += (cost = Get_Direct_Cost8x8 ( block, lambda_mode ));
              have_direct ++;

              // xiaozhen zheng, mv_range, 20071009
              if (!mv_out_of_range) 
              {
                cost_direct = 1<<30;
                cost = 1<<30;
              }
            }
            
            //for B frame
            if(!img->picture_structure)
            {
              scale_refframe = refFrArr[img->block8_y+block/2][img->block8_x+block%2];
              if (img->current_mb_nr_fld < img->total_number_mb) //top field
              {
                scale_refframe =   scale_refframe == 0 ? 0 : 1;
              }
              else
              {
                scale_refframe =   scale_refframe == 1 ? 0 : 1; //?????xyji
              }
              bid_best_fw_ref = scale_refframe;
              if(img->current_mb_nr_fld < img->total_number_mb)
              {
                bid_best_bw_ref = 1;
              }
              else
                bid_best_bw_ref = 0;
              best8x8symref[0][block][0] = scale_refframe;
              best8x8symref[0][block][1] = bid_best_bw_ref;
              if(refFrArr[img->block8_y+block/2][img->block8_x+block%2] < 0)
              {
                best8x8symref[0][block][0] = 0;
                best8x8symref[0][block][1] = 1;
                bid_best_bw_ref = 1;
                bid_best_fw_ref = 0;        
              }            
            }
            
            best_fw_ref = -1;
            best_pdir   =  2;
          } // if (mode==0)
          else
          {
            //--- motion estimation for all reference frames ---
            PartitionMotionSearch (mode, block, lambda_motion);
            //--- get cost and reference frame for forward prediction ---
            //--- get cost and reference frame for forward prediction ---
            if(img->type == INTER_IMG)/*lgp*/
            {
              for (fw_mcost=max_mcost, ref=0; ref<max_ref-adjust_ref; ref++) // Tian Dong. PLUS1. -adjust_ref. June 06, 2002
              {  /*lgp*13*/                
                {
                  mcost  = (input->rdopt ? write_ref ? REF_COST_FWD (lambda_motion_factor, ref) : 0 : (int)(2*lambda_motion*min(ref,1)));
                  
                  mcost += motion_cost[mode][ref+/*1*/1/*lgp*13*/][block];
                  
                  // mv_range, 20071009
                  if(mcost<0)
                    mcost=1<<30;

                  if (mcost < fw_mcost)
                  {
                    fw_mcost    = mcost;
                    best_fw_ref = ref;
                  }
                }
              }
            }
            else
            {
              //xyji
              for (fw_mcost=max_mcost, ref=0; ref<max_ref-adjust_ref; ref++)
              {
                //if (!checkref || ref==0)/*lgp*13*/
                {
                  //mcost  = (input->rdopt ? write_ref ? REF_COST_FWD (lambda_motion_factor, ref) : 0 : (int)(2*lambda_motion*min(ref,1)));
                  mcost = motion_cost[mode][img->picture_structure ? ref+1 : ref+2][block];

                  if (mcost < fw_mcost)
                  {
                    fw_mcost    = mcost;
                    best_fw_ref = ref;
                  }
                }
              }
              best_bw_ref = 0;
            }
            
            if (bframe)
            {
              PartitionMotionSearch_bid (mode, block, lambda_motion);
              
              best_bw_ref = 0;
              bw_mcost   = motion_cost[mode][0][block];
              //xyji 11.26
              for (bw_mcost=max_mcost, ref=0; ref<max_ref-adjust_ref; ref++)
              {
                {
                  //mcost  = (input->rdopt ? write_ref ? REF_COST_FWD (lambda_motion_factor, ref) : 0 : (int)(2*lambda_motion*min(ref,1)));
                  mcost = motion_cost[mode][ref][block];
                  if (mcost < bw_mcost)
                  {
                    bw_mcost    = mcost;
                    best_bw_ref = ref;
                  }
                }
              }
              
              for (bid_mcost=max_mcost, ref=0; ref<max_ref-adjust_ref; ref++)
              {
                //if (!checkref || ref==0)/*lgp*13*/
                {
                  //mcost  = (input->rdopt ? write_ref ? REF_COST_FWD (lambda_motion_factor, ref) : 0 : (int)(2*lambda_motion*min(ref,1)));
                  mcost = motion_cost_bid[mode][img->picture_structure ? ref+1 : ref+2][block];
                  if (mcost < bid_mcost)
                  {
                    bid_mcost    = mcost;
                    bid_best_fw_ref = ref;
                    bid_best_bw_ref = ref;
                  }
                }
              }        
              
              //--- get prediction direction ----
//              if(fw_mcost<=bw_mcost && fw_mcost<=bid_mcost)
              if(fw_mcost<bw_mcost && fw_mcost<bid_mcost)
              {
                best_pdir = 0;
                cost = fw_mcost;
                best_bw_ref = 0;
              }
//              else if (bw_mcost<=fw_mcost && bw_mcost<=bid_mcost)
              else if (bw_mcost<fw_mcost && bw_mcost<bid_mcost)
              {
                best_pdir = 1;
                cost = bw_mcost;
                best_fw_ref = 0;/*lgp*13*/
              }
              else
              {
                best_pdir = 2;
                cost = bid_mcost;                        
              }
            }
            else // if (bframe)
            {
              best_pdir = 0;
              cost      = fw_mcost;
            }
          } // if (mode!=0)
      
            //--- store coding state before coding with current mode ---
            store_coding_state (cs_cm);
      
            if (input->rdopt)
            {
        //--- get and check rate-distortion cost ---        
        rdcost = RDCost_for_8x8blocks (&cnt_nonz, &curr_cbp_blk, lambda_mode,block, mode, best_pdir,
          best_pdir==2?bid_best_fw_ref:best_fw_ref,best_pdir==2?bid_best_bw_ref:best_bw_ref);/*lgp*13*/
            }
            else
            {
        cost += (REF_COST (lambda_motion_factor, B8Mode2Value (mode, best_pdir)) - 1);
            }

      // xiaozhen zheng, mv_range, 20071009
      if (!img->mv_range_flag &&!mode && input->rdopt) {
        rdcost = (1<<30);
        img->mv_range_flag = 1;
      }
      else if (!img->mv_range_flag && !mode && !input->rdopt) {
        cost   = (1<<30);
        img->mv_range_flag = 1;
      }
      // xiaozhen zheng, mv_range, 20071009
      
            //--- set variables if best mode has changed ---
            if (( input->rdopt && rdcost < min_rdcost) ||
        (!input->rdopt && cost   < min_cost_8x8  )   )
            {
        min_cost_8x8               = cost;
        min_rdcost                 = rdcost;
        best8x8mode        [block] = mode;
        best8x8pdir  [P8x8][block] = best_pdir;
        best8x8ref   [P8x8][block] = best_fw_ref;
        best8x8bwref   [P8x8][block] = best_bw_ref;
        best8x8symref[P8x8][block][0] = bid_best_fw_ref;
        best8x8symref[P8x8][block][1] = bid_best_bw_ref;
        //--- store number of nonzero coefficients ---
        best_cnt_nonz  = cnt_nonz;
        
        if (input->rdopt)
        {
          //--- store block cbp ---
          cbp_blk8x8    &= (~(0x33 << (((block>>1)<<3)+((block%2)<<1)))); // delete bits for block
          cbp_blk8x8    |= curr_cbp_blk;
        }
        
        //--- store coding state ---
        store_coding_state (cs_b8);

            } // if (rdcost <= min_rdcost)
      
            //--- re-set coding state as it was before coding with current mode was performed ---
            reset_coding_state (cs_cm);
          } // if (valid[mode=b8_mode_table[index]])
        } // for (min_rdcost=1e30, index=(bframe?0:1); index<6; index++)
    
        cost8x8 += min_cost_8x8;
        
        if (!input->rdopt)
        {
      mode = best8x8mode[block];
      pdir = best8x8pdir[P8x8][block];
      
      curr_cbp_blk  = 0;
      //by oliver 
      if(pdir==2)
        best_cnt_nonz = LumaResidualCoding8x8 (&dummy, &curr_cbp_blk, block,
        (pdir==0||pdir==2?mode:0),
        (pdir==1||pdir==2?mode:0),
        (mode!=0?best8x8symref[P8x8][block][0]:max(0,refar[block_y+j1][block_x+i1])),
        (mode!=0?best8x8symref[P8x8][block][1]:0));
      else//0502
        best_cnt_nonz = LumaResidualCoding8x8 (&dummy, &curr_cbp_blk, block,
        (pdir==0||pdir==2?mode:0),
        (pdir==1||pdir==2?mode:0),
        (mode!=0?best8x8ref[P8x8][block]:max(0,refar[block_y+j1][block_x+i1])),
        (mode!=0?best8x8bwref[P8x8][block]:0));
      
      cbp_blk8x8   &= (~(0x33 << (((block>>1)<<3)+((block%2)<<1)))); // delete bits for block
      cbp_blk8x8   |= curr_cbp_blk;
      
      //--- store coefficients ---
      for (k=0; k< 4; k++)
        for (j=0; j< 2; j++)
          for (i=0; i<65; i++)  cofAC8x8[block][k][j][i] = img->cofAC[block][k][j][i]; // 18->65 for ABT
          
      //--- store reconstruction and prediction ---
      for (j=0; j<8; j++)
        for (i=i0; i<i0+8; i++)
        {
          rec_mbY8x8[j0+incr_y*j+off_y/*lgp*/][i] = imgY[img->pix_y+j0+incr_y*j+off_y/*lgp*/][img->pix_x+i];
          mpr8x8    [j+j0][i] = img->mpr[i][j+j0];/*lgp*/
        }
        }
    
        //----- set cbp and count of nonzero coefficients ---
        if (best_cnt_nonz)
        {
      cbp8x8        |= (1<<block);
      cnt_nonz_8x8  += best_cnt_nonz;
        }
    
        mode=best8x8mode[block];
        //===== reset intra prediction modes (needed for prediction, must be stored after 8x8 mode dec.) =====
        j0 = block_y+1+2*(block/2);
        i0 = block_x+1+2*(block%2);
    
    for (j=img->block8_y+stage_block8x8_pos/2+1; j<img->block8_y+3; j++)
      for (i=img->block8_x+1; i<img->block8_x+3; i++)
      {            
    //    ipredmodes    [i][j] = DC_PRED;
        ipredmodes    [i][j] = -1;
      }
      currMB->intra_pred_modes[block]  = DC_PRED;
    //  currMB->intra_pred_modes[block]  = -1;
      
    if (block<3)
    {
      //===== set motion vectors and reference frames (prediction) =====      
      SetRefAndMotionVectors (block, mode,best8x8pdir[P8x8][block]==2?best8x8symref[P8x8][block][0]:
      best8x8ref[P8x8][block],best8x8pdir[P8x8][block]==2?best8x8symref[P8x8][block][1]:
      best8x8bwref[P8x8][block],best8x8pdir[P8x8][block]);
    } // if (block<3)
      
      //===== set the coding state after current block =====
    reset_coding_state (cs_b8);
      } // for (cbp8x8=cbp_blk8x8=cnt_nonz_8x8=0, block=0; block<4; block++)
    
      //===== store intra prediction modes for 8x8+ macroblock mode =====
    for (j=img->block8_y+stage_block8x8_pos/2+1; j<img->block8_y+3; j++)
      for (i=img->block8_x+1; i<img->block8_x+3; i++)
      {
        k = 2*(j-img->block8_y-1)+i-img->block8_x-1;
        b8_ipredmode    [k] = ipredmodes    [i][j];
        b8_intra_pred_modes[k] = currMB->intra_pred_modes[k];
      }
      
      //--- re-set coding state (as it was before 8x8 block coding) ---
      reset_coding_state (cs_mb);
    
    //Rate control
    for (i=0; i<16; i++)
      for(j=0; j<16; j++)
        diffy[j][i] = imgY_org[img->pix_y+j][img->pix_y+i]-img->mpr[j][i];
        
      if(cost8x8<min_cost)
    {
     best_mode = P8x8;
       //min_cost = best_mode;
     min_cost=cost8x8;  //by oliver 
    }
  }
    else // if (valid[P8x8])
    {
    cost8x8 = (1<<20);
    }
    
#ifndef FastME
    //===== MOTION ESTIMATION FOR 16x16, 16x8, 8x16 BLOCKS =====
    for (min_cost=cost8x8, best_mode=P8x8, mode=3; mode>0; mode--)
    {
    if (valid[mode])
    {
      for (cost=0, block=stage_block8x8_pos/2; block<(mode==1?1:2); block++)
      {

        PartitionMotionSearch (mode, block, lambda_motion);
        //--- set 4x4 block indizes (for getting MV) ---
        j = (block==1 && mode==2 ? 2 : 0);
        i = (block==1 && mode==3 ? 2 : 0);
        
        //--- get cost and reference frame for forward prediction ---
        if(img->type == INTER_IMG)/*lgp*/
        {
          for (fw_mcost=max_mcost, ref=0; ref<max_ref-adjust_ref; ref++)   // Tian Dong. PLUS1. -adjust_ref. June 06, 2002
          {
            //if (!checkref || ref==0)/*lgp*13*/
            {
              mcost  = (input->rdopt ? write_ref ? REF_COST_FWD (lambda_motion_factor, ref) : 0 : (int)(2*lambda_motion*min(ref,1)));
              
              mcost += motion_cost[mode][ref+/*1*/1/*lgp*13*/][block];
              
              // mv_range, 20071009
              if(mcost>(1<<30))
                mcost=1<<30;

              if (mcost < fw_mcost)
              {
                fw_mcost    = mcost;
                best_fw_ref = ref;
              }
            }
          }
        }
        else
        {
        /*
          mcost  = (input->rdopt ? write_ref ? REF_COST_FWD (lambda_motion_factor, 0) : 0 : (int)(2*lambda_motion*min(0,1)));
                  
                  mcost += motion_cost[mode][1][block];
                  fw_mcost    = mcost;
                  best_fw_ref = 0;*/
                      
            for (fw_mcost=max_mcost, ref=0; ref<max_ref-adjust_ref; ref++)
            {
              //if (!checkref || ref==0)/*lgp*13*/
              {
                //mcost  = (input->rdopt ? write_ref ? REF_COST_FWD (lambda_motion_factor, ref) : 0 : (int)(2*lambda_motion*min(ref,1)));
                mcost = motion_cost[mode][img->picture_structure ? ref+1 : ref+2][block];
                if (mcost < fw_mcost)
                {
                  fw_mcost    = mcost;
                  best_fw_ref = ref;
                }
              }
            }
            best_bw_ref = 0;
          //by oliver 0511
        
        }
        
        if (bframe)
        {
          //--- get cost for bidirectional prediction ---
          PartitionMotionSearch_bid (mode, block, lambda_motion);
          
          best_bw_ref = 0;/*lgp*13*/
          bw_mcost   = motion_cost[mode][0][block];
          /***********************by oliver 0511***************************/
          for (bw_mcost=max_mcost, ref=0; ref<max_ref-adjust_ref; ref++)
            {
              //if (!checkref || ref==0)/*lgp*13*/
              {
                //mcost  = (input->rdopt ? write_ref ? REF_COST_FWD (lambda_motion_factor, ref) : 0 : (int)(2*lambda_motion*min(ref,1)));
                mcost = motion_cost[mode][ref][block];
                if (mcost < bw_mcost)
                {
                  bw_mcost    = mcost;
                  best_bw_ref = ref;
                }
              }
            }

            for (bid_mcost=max_mcost, ref=0; ref<max_ref-adjust_ref; ref++)
            {
              //if (!checkref || ref==0)/*lgp*13*/
              {
                //mcost  = (input->rdopt ? write_ref ? REF_COST_FWD (lambda_motion_factor, ref) : 0 : (int)(2*lambda_motion*min(ref,1)));
                mcost = motion_cost_bid[mode][img->picture_structure ? ref+1 : ref+2][block];
                if (mcost < bid_mcost)
                {
                  bid_mcost    = mcost;
                  bid_best_fw_ref = ref;
                  bid_best_bw_ref = ref;
                }
              }
            }

                  //--- get prediction direction ----
                  if (fw_mcost<=bw_mcost && fw_mcost<=bid_mcost)
                  {
                    best_pdir = 0;
                    best_bw_ref = 0;
                    cost += fw_mcost;    
                  }
                  else if (bw_mcost<=fw_mcost && bw_mcost<=bid_mcost)
                  {
                    best_pdir = 1;
                    cost += bw_mcost;    
                    best_fw_ref = 0;/*lgp*13*/
                  }
                  else
                  {
                    best_pdir = 2;
                    cost += bid_mcost;  
                  }
        
        }
        else // if (bframe)
        {
          best_pdir  = 0;
          cost      += fw_mcost;  
        }
        
          //----- set reference frame and direction parameters -----
          //----- set reference frame and direction parameters -----
          //----- set reference frame and direction parameters -----
          if (mode==3)  //8x16
          {
            best8x8ref [3][  block] = best8x8ref [3][  block+2] = best_fw_ref;
            best8x8pdir[3][  block] = best8x8pdir[3][  block+2] = best_pdir;
            best8x8bwref   [3][block] = best8x8bwref   [3][block+2] = best_bw_ref;
            /*********************/
            best8x8symref  [3][block][0] = best8x8symref   [3][block+2][0] = bid_best_fw_ref;      
            best8x8symref  [3][block][1] = best8x8symref   [3][block+2][1] = bid_best_bw_ref;
            //added by oliver 0512
          }
          else if (mode==2) //16x8
          {
            best8x8ref [2][2*block] = best8x8ref [2][2*block+1] = best_fw_ref;
            best8x8pdir[2][2*block] = best8x8pdir[2][2*block+1] = best_pdir;
            best8x8bwref   [2][2*block] = best8x8bwref   [2][2*block+1] = best_bw_ref;
            /*********************/
            best8x8symref   [2][2*block][0] = best8x8symref   [2][2*block+1][0] = bid_best_fw_ref;
            best8x8symref   [2][2*block][1] = best8x8symref   [2][2*block+1][1] = bid_best_bw_ref;  
            //added by oliver 0512
          }
          else        //16x16
          {
            best8x8ref [1][0] = best8x8ref [1][1] = best8x8ref [1][2] = best8x8ref [1][3] = best_fw_ref;
            best8x8pdir[1][0] = best8x8pdir[1][1] = best8x8pdir[1][2] = best8x8pdir[1][3] = best_pdir;
            best8x8bwref   [1][0] = best8x8bwref   [1][1] = best8x8bwref   [1][2] = best8x8bwref   [1][3] = best_bw_ref;      
            /*********************/
            best8x8symref [1][0][0] = best8x8symref [1][1][0] = best8x8symref [1][2][0] = best8x8symref [1][3][0] = bid_best_fw_ref;
            best8x8symref [1][0][1] = best8x8symref [1][1][1] = best8x8symref [1][2][1] = best8x8symref [1][3][1] = bid_best_bw_ref;
            //added by oliver 0512
          }        
        
        //--- set reference frames and motion vectors ---
        if (mode>1 && block==0)   
          SetRefAndMotionVectors (block, mode, best_pdir==2? bid_best_fw_ref: best_fw_ref,
          best_pdir==2? bid_best_bw_ref: best_bw_ref, best_pdir);/*lgp*13*/
      }       
      
      if (cost < min_cost)
      {
        best_mode = mode;
        min_cost  = cost;
      }
      } // if (valid[mode])
    } // for (mode=3; mode>0; mode--)*/
#endif
    
    // Find a motion vector for the Skip mode
    if(img->type == INTER_IMG)
      FindSkipModeMotionVector ();
  }
  else // if (img->type!=INTRA_IMG)
  {
//    min_cost = (1<<20);  // 20071009
  min_cost = (1<<30);    // 20071009
  }

  if (input->rdopt)
  {
    int mb_available_up;
    int mb_available_left;
    int mb_available_up_left;
    
    min_rdcost = max_rdcost;
    
    // precompute all new chroma intra prediction modes
    IntraChromaPrediction8x8(&mb_available_up, &mb_available_left, &mb_available_up_left);
    
    for (currMB->c_ipred_mode=DC_PRED_8; currMB->c_ipred_mode<=PLANE_8; currMB->c_ipred_mode++)
    {
    // bypass if c_ipred_mode is not allowed
    if ((currMB->c_ipred_mode==VERT_PRED_8 && !mb_available_up) ||
      (currMB->c_ipred_mode==HOR_PRED_8 && !mb_available_left) ||
      (currMB->c_ipred_mode==PLANE_8 && (!mb_available_left || !mb_available_up || !mb_available_up_left)))
      continue;
    
    //===== GET BEST MACROBLOCK MODE =====
    for (ctr16x16=0, index=0; index<7; index++)
    {
      mode = mb_mode_table[index];
//       if(mode==0 && img->type==B_IMG && img->VEC_FLAG==1 && input->vec_ext==3)// M1956 by Grandview 2006.12.12
//         continue;

      // xiaozhen zheng, mv_range, 20071009
      if (img->type==INTER_IMG && !mode && !img->mv_range_flag) {
        img->mv_range_flag = 1;
        continue;
      }
      // xiaozhen zheng, mv_range, 20071009
      
      //--- for INTER16x16 check all prediction directions ---
      if (mode==1 && img->type==B_IMG) 
//       if (mode==1 && img->type==B_IMG && // M1956 by Grandview 2006.12.12
//         ((img->VEC_FLAG==1 && input->vec_ext!=3) || img->VEC_FLAG==0)) 
      {
        best8x8pdir[1][0] = best8x8pdir[1][1] = best8x8pdir[1][2] = best8x8pdir[1][3] = ctr16x16;
        if (ctr16x16 < 2) index--;
        ctr16x16++;
      }
      
      img->NoResidueDirect = 0;
      
      if (valid[mode])
      {
        // bypass if c_ipred_mode not used
        SetModesAndRefframeForBlocks (mode);
        if (currMB->c_ipred_mode == DC_PRED_8 ||  (IS_INTRA(currMB) ))
        {
          if (RDCost_for_macroblocks (lambda_mode, mode, &min_rdcost))
          {
            //Rate control
            if(mode == P8x8)
            {
              for (i=0; i<16; i++)
                for(j=0; j<16; j++)
                  diffy[j][i] = imgY_org[img->pix_y+j][img->pix_x+i] - mpr8x8[j][i];
            }else
            {
              for (i=0; i<16; i++)
                for(j=0; j<16; j++)
                  diffy[j][i] = imgY_org[img->pix_y+j][img->pix_x+i] - pred[j][i];
            }
            store_macroblock_parameters (mode);
          }
        }
      }
      
      //if (bframe && mode == 0 && currMB->cbp && (currMB->cbp&15) != 15)
      //{
      //  img->NoResidueDirect = 1;
      //  if (RDCost_for_macroblocks (lambda_mode, mode, &min_rdcost))
      //  {
      //    //Rate control
      //    for (i=0; i<16; i++)
      //      for(j=0; j<16; j++)
      //        diffy[j][i] = imgY_org[img->pix_y+j][img->pix_x+i] - pred[j][i];
      //      
      //      store_macroblock_parameters (mode);
      //  }
      //}
    }
    }
  }
  else
  {
    if (valid[0] && bframe) // check DIRECT MODE
    //&&((img->VEC_FLAG==1 && input->vec_ext!=3) || img->VEC_FLAG==0)) // M1956 by Grandview 2006.12.12
    {
      cost  = (have_direct?cost_direct:Get_Direct_CostMB (lambda_mode));
      cost -= (int)floor(16*lambda_motion+0.4999);
      
      if (cost <= min_cost)
      {
        min_cost  = cost;
        best_mode = 0;
      }
    }
    if (valid[I4MB]) // check INTRA4x4
    {
      currMB->cbp = Mode_Decision_for_Intra4x4Macroblock (lambda_mode, &cost);
      
      if (cost <= min_cost)
      {
        min_cost  = cost;
        best_mode = I4MB;
      }
    }
  }
  
  if (input->rdopt)
  {
        // Rate control
    if(input->RCEnable)
    {   
      if(img->type==INTER_IMG)
      {
        img->MADofMB[img->current_mb_nr] = calc_MAD();
        
        if(input->basicunit<img->Frame_Total_Number_MB)
        {
          img->TotalMADBasicUnit +=img->MADofMB[img->current_mb_nr];
          
          /* delta_qp is present only for non-skipped macroblocks*/
          if ((cbp!=0 || best_mode==I16MB))
            currMB->prev_cbp = 1;
          else
          {
            img->qp -= currMB->delta_qp; 
            currMB->delta_qp = 0;
            currMB->qp = img->qp;
            currMB->prev_cbp = 0;
          }     
        }
      }                        
    }

    set_stored_macroblock_parameters ();
  }
  else
  {
    //===== set parameters for chosen mode =====
    SetModesAndRefframeForBlocks (best_mode);
  storeMotionInfo (stage_block8x8_pos/2);          //fix for rdo off   jlzheng 6.25
    if (best_mode==P8x8)
    {
      SetCoeffAndReconstruction8x8 (currMB);
    }
    else
    {
      if (best_mode!=I4MB)
      {
        for (k=0, j=(block_y>>1)+1; j<(block_y>>1)+3; j++)
          for (     i=(block_x>>1)+1; i<(block_x>>1)+3; i++, k++)
          {
  //        ipredmodes    [i][j] = DC_PRED;
            currMB->intra_pred_modes[k] = DC_PRED;
      ipredmodes    [i][j] = -1;
//          currMB->intra_pred_modes[k] = -1;
          }
          
    if (best_mode!=I16MB)
    {
      LumaResidualCoding ();
    }
      }
    }
    // precompute all chroma intra prediction modes
    IntraChromaPrediction8x8(NULL, NULL, NULL);
    
    dummy = 0;
  img->NoResidueDirect = 0;
    ChromaResidualCoding (&dummy);
    SetMotionVectorsMB (currMB, bframe);
    
  //wzm,422
  if(input->chroma_format==2)
  {
    img->mb_data[img->current_mb_nr].cbp01=(img->mb_data[img->current_mb_nr].cbp)>>6;
    img->mb_data[img->current_mb_nr].cbp=(img->mb_data[img->current_mb_nr].cbp&63);
  }
    //wzm,422
    
    //===== check for SKIP mode =====
    if (img->type==INTER_IMG && best_mode==1 && currMB->cbp==0 &&
      refFrArr [block_y>>1][block_x>>1  ]==0 &&
      tmpmvs[0][block_y>>1][(block_x>>1)+4]==allmvs[0][0][0][0][0] &&
      tmpmvs[1][block_y>>1][(block_x>>1)+4]==allmvs[0][0][0][0][1])
    {
      currMB->mb_type=currMB->b8mode[0]=currMB->b8mode[1]=currMB->b8mode[2]=currMB->b8mode[3]=0;/*lgp*/
    }

  // RC 20071224
  if(input->RCEnable)
  {
    if(img->type==INTER_IMG)
    {
      if ((currMB->cbp!=0 || currMB->mb_type==I16MB))
        currMB->prev_cbp = 1;
      else
      {
        img->qp -= currMB->delta_qp;
        
        currMB->delta_qp = 0;
        currMB->qp = img->qp;
        
        currMB->prev_cbp = 0;
      }
    }    
  }  
  // RC 20071224
  }
  
  if (img->current_mb_nr==0)
    intras=0;
  if (img->type==INTER_IMG && IS_INTRA(currMB))
    intras++;
  
  //FAST MOTION ESTIMATION. ZHIBO CHEN 2003.3
  //for intra block consideration
#ifdef FastME
  skip_intrabk_SAD(best_mode, max_ref-adjust_ref);
#endif  
}




