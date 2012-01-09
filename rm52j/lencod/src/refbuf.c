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

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>

#include "refbuf.h"

#define CACHELINESIZE 32

static pel_t line[16];

/*
*************************************************************************
* Function:Reference buffer write routines
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

void PutPel_14 (pel_t **Pic, int y, int x, pel_t val)
{
    Pic [IMG_PAD_SIZE*4+y][IMG_PAD_SIZE*4+x] = val;
}

/*
*************************************************************************
* Function:Reference buffer read, Full pel
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

pel_t *FastLineX (int dummy, pel_t* Pic, int y, int x)
{
    return Pic + y*img->width + x;
}

/*
*************************************************************************
* Function:
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/
pel_t *UMVLineX (int size, pel_t* Pic, int y, int x)
{
    int i, maxx;
    pel_t *Picy;

    Picy = Pic + max(0,min(img->height-1,y)) * img->width;

    if (x < 0)                            // Left edge
    {
        maxx = min(0,x+size);

        for (i = x; i < maxx; i++)
        {
            line[i-x] = Picy [0];             // Replicate left edge pixel
        }

        maxx = x+size;

        for (i = 0; i < maxx; i++)          // Copy non-edge pixels
            line[i-x] = Picy [i];
    }
    else if (x > img->width-size)         // Right edge
    {
        maxx = img->width;

        for (i = x; i < maxx; i++)
        {
            line[i-x] = Picy [i];             // Copy non-edge pixels
        }

        maxx = x+size;

        for (i = max(img->width,x); i < maxx; i++)
        {
            line[i-x] = Picy [img->width-1];  // Replicate right edge pixel
        }
    }
    else                                  // No edge
    {
        return Picy + x;
    }

    return line;
}

/*
*************************************************************************
* Function:Reference buffer, 1/4 pel
* Input:
* Output:
* Return: 
* Attention:
*************************************************************************
*/

pel_t UMVPelY_14 (pel_t **Pic, int y, int x)
{
    int width4  = ((img->width+2*IMG_PAD_SIZE-1)<<2);
    int height4 = ((img->height+2*IMG_PAD_SIZE-1)<<2);

    x = x + IMG_PAD_SIZE*4;
    y = y + IMG_PAD_SIZE*4;

    if (x < 0)
    {
        if (y < 0)
            return Pic [y&3][x&3];
        if (y > height4)
            return Pic [height4+(y&3)][x&3];
        return Pic [y][x&3];
    }

    if (x > width4)
    {
        if (y < 0)
            return Pic [y&3][width4+(x&3)];
        if (y > height4)
            return Pic [height4+(y&3)][width4+(x&3)];
        return Pic [y][width4+(x&3)];
    }

    if (y < 0)    // note: corner pixels were already processed
        return Pic [y&3][x];
    if (y > height4)
        return Pic [height4+(y&3)][x];

    return Pic [y][x];
}

pel_t FastPelY_14 (pel_t **Pic, int y, int x)
{
    return Pic [IMG_PAD_SIZE*4+y][IMG_PAD_SIZE*4+x];
}


