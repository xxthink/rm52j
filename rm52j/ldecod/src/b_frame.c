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
* File name: b_frame.c
* Function: B picture decoding
*
*************************************************************************************
*/


#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "global.h"
#include "b_frame.h"

/*
*************************************************************************
* Function:Copy decoded P frame to temporary image array
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/
void copy_Pframe(struct img_par *img)
{
  int i,j;

  /*
   * the mmin, mmax macros are taken out
   * because it makes no sense due to limited range of data type
   */
  
  for(i=0;i<img->height;i++)
           for(j=0;j<img->width;j++)
       {
            imgY_prev[i][j] = imgY[i][j];
    }
    
  for(i=0;i<img->height_cr;i++)
    for(j=0;j<img->width_cr;j++)
    {
      imgUV_prev[0][i][j] = imgUV[0][i][j];
    }

  for(i=0;i<img->height_cr;i++)
    for(j=0;j<img->width_cr;j++)
    {
      imgUV_prev[1][i][j] = imgUV[1][i][j];
    }
}

/*
*************************************************************************
* Function:init macroblock B frames
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void init_macroblock_Bframe(struct img_par *img)
{
  int i,j,k;
  Macroblock *currMB = &mb_data[img->current_mb_nr];//GB current_mb_nr];
  
  // reset vectors and pred. modes
  for (i=0;i<2;i++)
  {
    for(j=0;j<2;j++)
    {
      img->dfMV [img->block_x+i+4][img->block_y+j][0]=img->dfMV[img->block_x+i+4][img->block_y+j][1]=0;
      img->dbMV [img->block_x+i+4][img->block_y+j][0]=img->dbMV[img->block_x+i+4][img->block_y+j][1]=0;
      
      img->fw_mv[img->block_x+i+4][img->block_y+j][0]=img->fw_mv[img->block_x+i+4][img->block_y+j][1]=0;
      img->bw_mv[img->block_x+i+4][img->block_y+j][0]=img->bw_mv[img->block_x+i+4][img->block_y+j][1]=0;
    }
  }
  
  for (i=0;i<2;i++)
  {
    for(j=0;j<2;j++)
    {
//      img->ipredmode[img->block_x+i+1][img->block_y+j+1] = DC_PRED;
  img->ipredmode[img->block_x+i+1][img->block_y+j+1] = -1;   //cjw the default value should be -1
    }
  }
  
  // Set the reference frame information for motion vector prediction
  if (IS_INTRA (currMB) || IS_DIRECT (currMB))
  {
    for (j=0; j<2; j++)
      for (i=0; i<2; i++)
      {
        img->fw_refFrArr[img->block_y+j][img->block_x+i] = -1;
        img->bw_refFrArr[img->block_y+j][img->block_x+i] = -1;
      }
  }
  else
  {
    for(j=0;j<2;j++)
      for(i=0;i<2;i++)
      {
        k=2*j+i;
        
        img->fw_refFrArr[img->block_y+j][img->block_x+i] = ((currMB->b8pdir[k]==0||currMB->b8pdir[k]==2)&&currMB->b8mode[k]!=0?0:-1);
        img->bw_refFrArr[img->block_y+j][img->block_x+i] = ((currMB->b8pdir[k]==1||currMB->b8pdir[k]==2)&&currMB->b8mode[k]!=0?0:-1);
      }
  }
}
