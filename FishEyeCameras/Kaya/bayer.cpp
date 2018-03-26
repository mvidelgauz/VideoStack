/*
 * 1394-Based Digital Camera Control Library
 *
 * Bayer pattern decoding functions
 *
 * Written by Damien Douxchamps and Frederic Devernay
 * The original VNG and AHD Bayer decoding are from Dave Coffin's DCRAW.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
//#include "conversions.h"
#include "bayer.h"


#define CLIP(in, out)\
   in = in < 0 ? 0 : in;\
   in = in > 255 ? 255 : in;\
   out=in;

#define CLIP16(in, out, bits)\
   in = in < 0 ? 0 : in;\
   in = in > ((1<<bits)-1) ? ((1<<bits)-1) : in;\
   out=in;

void ClearBorders(uint8_t *rgb, int sx, int sy, int w)
{
    int i, j;
    // black edges are added with a width w:
    i = 3 * sx * w - 1;
    j = 3 * sx * sy - 1;
    while (i >= 0) {
        rgb[i--] = 0;
        rgb[j--] = 0;
    }

    int low = sx * (w - 1) * 3 - 1 + w * 3;
    i = low + sx * (sy - w * 2 + 1) * 3;
    while (i > low) {
        j = 6 * w;
        while (j > 0) {
            rgb[i--] = 0;
            j--;
        }
        i -= (sx - 2 * w) * 3;
    }
}

void ClearBorders_uint16(uint16_t * rgb, int sx, int sy, int w)
{
    int i, j;

    // black edges:
    i = 3 * sx * w - 1;
    j = 3 * sx * sy - 1;
    while (i >= 0) {
        rgb[i--] = 0;
        rgb[j--] = 0;
    }

    int low = sx * (w - 1) * 3 - 1 + w * 3;
    i = low + sx * (sy - w * 2 + 1) * 3;
    while (i > low) {
        j = 6 * w;
        while (j > 0) {
            rgb[i--] = 0;
            j--;
        }
        i -= (sx - 2 * w) * 3;
    }

}

/**************************************************************
 *     Color conversion functions for cameras that can        *
 * output raw-Bayer pattern images, such as some Basler and   *
 * Point Grey camera. Most of the algos presented here come   *
 * from http://www-ise.stanford.edu/~tingchen/ and have been  *
 * converted from Matlab to C and extended to all elementary  *
 * patterns.                                                  *
 **************************************************************/

/* 8-bits versions */
/* insprired by OpenCV's Bayer decoding */

BayerToRgb::dc1394error_t BayerToRgb::dc1394_bayer_NearestNeighbor(const uint8_t *bayer, uint8_t *rgb, int sx, int sy, int tile)
{
    const int bayerStep = sx;
    const int rgbStep = 3 * sx;
    int width = sx;
    int height = sy;
    int blue = tile == DC1394_COLOR_FILTER_BGGR
        || tile == DC1394_COLOR_FILTER_GBRG ? -1 : 1;
    int start_with_green = tile == DC1394_COLOR_FILTER_GBRG
        || tile == DC1394_COLOR_FILTER_GRBG;
    int i, imax, iinc;

    if ((tile>DC1394_COLOR_FILTER_MAX)||(tile<DC1394_COLOR_FILTER_MIN))
      return DC1394_INVALID_COLOR_FILTER;

    /* add black border */
    imax = sx * sy * 3;
    for (i = sx * (sy - 1) * 3; i < imax; i++) {
        rgb[i] = 0;
    }
    iinc = (sx - 1) * 3;
    for (i = (sx - 1) * 3; i < imax; i += iinc) {
        rgb[i++] = 0;
        rgb[i++] = 0;
        rgb[i++] = 0;
    }

    rgb += 1;
    width -= 1;
    height -= 1;

    for (; height--; bayer += bayerStep, rgb += rgbStep) {
      //int t0, t1;
        const uint8_t *bayerEnd = bayer + width;

        if (start_with_green) {
            rgb[-blue] = bayer[1];
            rgb[0] = bayer[bayerStep + 1];
            rgb[blue] = bayer[bayerStep];
            bayer++;
            rgb += 3;
        }

        if (blue > 0) {
            for (; bayer <= bayerEnd - 2; bayer += 2, rgb += 6) {
                rgb[-1] = bayer[0];
                rgb[0] = bayer[1];
                rgb[1] = bayer[bayerStep + 1];

                rgb[2] = bayer[2];
                rgb[3] = bayer[bayerStep + 2];
                rgb[4] = bayer[bayerStep + 1];
            }
        } else {
            for (; bayer <= bayerEnd - 2; bayer += 2, rgb += 6) {
                rgb[1] = bayer[0];
                rgb[0] = bayer[1];
                rgb[-1] = bayer[bayerStep + 1];

                rgb[4] = bayer[2];
                rgb[3] = bayer[bayerStep + 2];
                rgb[2] = bayer[bayerStep + 1];
            }
        }

        if (bayer < bayerEnd) {
            rgb[-blue] = bayer[0];
            rgb[0] = bayer[1];
            rgb[blue] = bayer[bayerStep + 1];
            bayer++;
            rgb += 3;
        }

        bayer -= width;
        rgb -= width * 3;

        blue = -blue;
        start_with_green = !start_with_green;
    }

    return DC1394_SUCCESS;
}

/* OpenCV's Bayer decoding */
BayerToRgb::dc1394error_t BayerToRgb::dc1394_bayer_Bilinear(const uint8_t *bayer, uint8_t *rgb, int sx, int sy, int tile)
{
    const int bayerStep = sx;
    const int rgbStep = 3 * sx;
    int width = sx;
    int height = sy;
    /*
       the two letters  of the OpenCV name are respectively
       the 4th and 3rd letters from the blinky name,
       and we also have to switch R and B (OpenCV is BGR)

       CV_BayerBG2BGR <-> DC1394_COLOR_FILTER_BGGR
       CV_BayerGB2BGR <-> DC1394_COLOR_FILTER_GBRG
       CV_BayerGR2BGR <-> DC1394_COLOR_FILTER_GRBG

       int blue = tile == CV_BayerBG2BGR || tile == CV_BayerGB2BGR ? -1 : 1;
       int start_with_green = tile == CV_BayerGB2BGR || tile == CV_BayerGR2BGR;
     */
    int blue = tile == DC1394_COLOR_FILTER_BGGR
        || tile == DC1394_COLOR_FILTER_GBRG ? -1 : 1;
    int start_with_green = tile == DC1394_COLOR_FILTER_GBRG
        || tile == DC1394_COLOR_FILTER_GRBG;

    if ((tile>DC1394_COLOR_FILTER_MAX)||(tile<DC1394_COLOR_FILTER_MIN))
        return DC1394_INVALID_COLOR_FILTER;

    ClearBorders(rgb, sx, sy, 1);
    rgb += rgbStep + 3 + 1;
    height -= 2;
    width -= 2;

    for (; height--; bayer += bayerStep, rgb += rgbStep) {
        int t0, t1;
        const uint8_t *bayerEnd = bayer + width;

        if (start_with_green) {
            /* OpenCV has a bug in the next line, which was
               t0 = (bayer[0] + bayer[bayerStep * 2] + 1) >> 1; */
            t0 = (bayer[1] + bayer[bayerStep * 2 + 1] + 1) >> 1;
            t1 = (bayer[bayerStep] + bayer[bayerStep + 2] + 1) >> 1;
            rgb[-blue] = (uint8_t) t0;
            rgb[0] = bayer[bayerStep + 1];
            rgb[blue] = (uint8_t) t1;
            bayer++;
            rgb += 3;
        }

        if (blue > 0) {
            for (; bayer <= bayerEnd - 2; bayer += 2, rgb += 6) {
                t0 = (bayer[0] + bayer[2] + bayer[bayerStep * 2] +
                      bayer[bayerStep * 2 + 2] + 2) >> 2;
                t1 = (bayer[1] + bayer[bayerStep] +
                      bayer[bayerStep + 2] + bayer[bayerStep * 2 + 1] +
                      2) >> 2;
                rgb[-1] = (uint8_t) t0;
                rgb[0] = (uint8_t) t1;
                rgb[1] = bayer[bayerStep + 1];

                t0 = (bayer[2] + bayer[bayerStep * 2 + 2] + 1) >> 1;
                t1 = (bayer[bayerStep + 1] + bayer[bayerStep + 3] +
                      1) >> 1;
                rgb[2] = (uint8_t) t0;
                rgb[3] = bayer[bayerStep + 2];
                rgb[4] = (uint8_t) t1;
            }
        } else {
            for (; bayer <= bayerEnd - 2; bayer += 2, rgb += 6) {
                t0 = (bayer[0] + bayer[2] + bayer[bayerStep * 2] +
                      bayer[bayerStep * 2 + 2] + 2) >> 2;
                t1 = (bayer[1] + bayer[bayerStep] +
                      bayer[bayerStep + 2] + bayer[bayerStep * 2 + 1] +
                      2) >> 2;
                rgb[1] = (uint8_t) t0;
                rgb[0] = (uint8_t) t1;
                rgb[-1] = bayer[bayerStep + 1];

                t0 = (bayer[2] + bayer[bayerStep * 2 + 2] + 1) >> 1;
                t1 = (bayer[bayerStep + 1] + bayer[bayerStep + 3] +
                      1) >> 1;
                rgb[4] = (uint8_t) t0;
                rgb[3] = bayer[bayerStep + 2];
                rgb[2] = (uint8_t) t1;
            }
        }

        if (bayer < bayerEnd) {
            t0 = (bayer[0] + bayer[2] + bayer[bayerStep * 2] +
                  bayer[bayerStep * 2 + 2] + 2) >> 2;
            t1 = (bayer[1] + bayer[bayerStep] +
                  bayer[bayerStep + 2] + bayer[bayerStep * 2 + 1] +
                  2) >> 2;
            rgb[-blue] = (uint8_t) t0;
            rgb[0] = (uint8_t) t1;
            rgb[blue] = bayer[bayerStep + 1];
            bayer++;
            rgb += 3;
        }

        bayer -= width;
        rgb -= width * 3;

        blue = -blue;
        start_with_green = !start_with_green;
    }
    return DC1394_SUCCESS;
}

/* High-Quality Linear Interpolation For Demosaicing Of
   Bayer-Patterned Color Images, by Henrique S. Malvar, Li-wei He, and
   Ross Cutler, in ICASSP'04 */
BayerToRgb::dc1394error_t BayerToRgb::dc1394_bayer_HQLinear(const uint8_t *bayer, uint8_t *rgb, int sx, int sy, int tile)
{
    const int bayerStep = sx;
    const int rgbStep = 3 * sx;
    int width = sx;
    int height = sy;
    int blue = tile == DC1394_COLOR_FILTER_BGGR
        || tile == DC1394_COLOR_FILTER_GBRG ? -1 : 1;
    int start_with_green = tile == DC1394_COLOR_FILTER_GBRG
        || tile == DC1394_COLOR_FILTER_GRBG;

    if ((tile>DC1394_COLOR_FILTER_MAX)||(tile<DC1394_COLOR_FILTER_MIN))
      return DC1394_INVALID_COLOR_FILTER;

    ClearBorders(rgb, sx, sy, 2);
    rgb += 2 * rgbStep + 6 + 1;
    height -= 4;
    width -= 4;

    /* We begin with a (+1 line,+1 column) offset with respect to bilinear decoding, so start_with_green is the same, but blue is opposite */
    blue = -blue;

    for (; height--; bayer += bayerStep, rgb += rgbStep) {
        int t0, t1;
        const uint8_t *bayerEnd = bayer + width;
        const int bayerStep2 = bayerStep * 2;
        const int bayerStep3 = bayerStep * 3;
        const int bayerStep4 = bayerStep * 4;

        if (start_with_green) {
            /* at green pixel */
            rgb[0] = bayer[bayerStep2 + 2];
            t0 = rgb[0] * 5
                + ((bayer[bayerStep + 2] + bayer[bayerStep3 + 2]) << 2)
                - bayer[2]
                - bayer[bayerStep + 1]
                - bayer[bayerStep + 3]
                - bayer[bayerStep3 + 1]
                - bayer[bayerStep3 + 3]
                - bayer[bayerStep4 + 2]
                + ((bayer[bayerStep2] + bayer[bayerStep2 + 4] + 1) >> 1);
            t1 = rgb[0] * 5 +
                ((bayer[bayerStep2 + 1] + bayer[bayerStep2 + 3]) << 2)
                - bayer[bayerStep2]
                - bayer[bayerStep + 1]
                - bayer[bayerStep + 3]
                - bayer[bayerStep3 + 1]
                - bayer[bayerStep3 + 3]
                - bayer[bayerStep2 + 4]
                + ((bayer[2] + bayer[bayerStep4 + 2] + 1) >> 1);
            t0 = (t0 + 4) >> 3;
            CLIP(t0, rgb[-blue]);
            t1 = (t1 + 4) >> 3;
            CLIP(t1, rgb[blue]);
            bayer++;
            rgb += 3;
        }

        if (blue > 0) {
            for (; bayer <= bayerEnd - 2; bayer += 2, rgb += 6) {
                /* B at B */
                rgb[1] = bayer[bayerStep2 + 2];
                /* R at B */
                t0 = ((bayer[bayerStep + 1] + bayer[bayerStep + 3] +
                       bayer[bayerStep3 + 1] + bayer[bayerStep3 + 3]) << 1)
                    -
                    (((bayer[2] + bayer[bayerStep2] +
                       bayer[bayerStep2 + 4] + bayer[bayerStep4 +
                                                     2]) * 3 + 1) >> 1)
                    + rgb[1] * 6;
                /* G at B */
                t1 = ((bayer[bayerStep + 2] + bayer[bayerStep2 + 1] +
                       bayer[bayerStep2 + 3] + bayer[bayerStep3 + 2]) << 1)
                    - (bayer[2] + bayer[bayerStep2] +
                       bayer[bayerStep2 + 4] + bayer[bayerStep4 + 2])
                    + (rgb[1] << 2);
                t0 = (t0 + 4) >> 3;
                CLIP(t0, rgb[-1]);
                t1 = (t1 + 4) >> 3;
                CLIP(t1, rgb[0]);
                /* at green pixel */
                rgb[3] = bayer[bayerStep2 + 3];
                t0 = rgb[3] * 5
                    + ((bayer[bayerStep + 3] + bayer[bayerStep3 + 3]) << 2)
                    - bayer[3]
                    - bayer[bayerStep + 2]
                    - bayer[bayerStep + 4]
                    - bayer[bayerStep3 + 2]
                    - bayer[bayerStep3 + 4]
                    - bayer[bayerStep4 + 3]
                    +
                    ((bayer[bayerStep2 + 1] + bayer[bayerStep2 + 5] +
                      1) >> 1);
                t1 = rgb[3] * 5 +
                    ((bayer[bayerStep2 + 2] + bayer[bayerStep2 + 4]) << 2)
                    - bayer[bayerStep2 + 1]
                    - bayer[bayerStep + 2]
                    - bayer[bayerStep + 4]
                    - bayer[bayerStep3 + 2]
                    - bayer[bayerStep3 + 4]
                    - bayer[bayerStep2 + 5]
                    + ((bayer[3] + bayer[bayerStep4 + 3] + 1) >> 1);
                t0 = (t0 + 4) >> 3;
                CLIP(t0, rgb[2]);
                t1 = (t1 + 4) >> 3;
                CLIP(t1, rgb[4]);
            }
        } else {
            for (; bayer <= bayerEnd - 2; bayer += 2, rgb += 6) {
                /* R at R */
                rgb[-1] = bayer[bayerStep2 + 2];
                /* B at R */
                t0 = ((bayer[bayerStep + 1] + bayer[bayerStep + 3] +
                       bayer[bayerStep3 + 1] + bayer[bayerStep3 + 3]) << 1)
                    -
                    (((bayer[2] + bayer[bayerStep2] +
                       bayer[bayerStep2 + 4] + bayer[bayerStep4 +
                                                     2]) * 3 + 1) >> 1)
                    + rgb[-1] * 6;
                /* G at R */
                t1 = ((bayer[bayerStep + 2] + bayer[bayerStep2 + 1] +
                       bayer[bayerStep2 + 3] + bayer[bayerStep * 3 +
                                                     2]) << 1)
                    - (bayer[2] + bayer[bayerStep2] +
                       bayer[bayerStep2 + 4] + bayer[bayerStep4 + 2])
                    + (rgb[-1] << 2);
                t0 = (t0 + 4) >> 3;
                CLIP(t0, rgb[1]);
                t1 = (t1 + 4) >> 3;
                CLIP(t1, rgb[0]);

                /* at green pixel */
                rgb[3] = bayer[bayerStep2 + 3];
                t0 = rgb[3] * 5
                    + ((bayer[bayerStep + 3] + bayer[bayerStep3 + 3]) << 2)
                    - bayer[3]
                    - bayer[bayerStep + 2]
                    - bayer[bayerStep + 4]
                    - bayer[bayerStep3 + 2]
                    - bayer[bayerStep3 + 4]
                    - bayer[bayerStep4 + 3]
                    +
                    ((bayer[bayerStep2 + 1] + bayer[bayerStep2 + 5] +
                      1) >> 1);
                t1 = rgb[3] * 5 +
                    ((bayer[bayerStep2 + 2] + bayer[bayerStep2 + 4]) << 2)
                    - bayer[bayerStep2 + 1]
                    - bayer[bayerStep + 2]
                    - bayer[bayerStep + 4]
                    - bayer[bayerStep3 + 2]
                    - bayer[bayerStep3 + 4]
                    - bayer[bayerStep2 + 5]
                    + ((bayer[3] + bayer[bayerStep4 + 3] + 1) >> 1);
                t0 = (t0 + 4) >> 3;
                CLIP(t0, rgb[4]);
                t1 = (t1 + 4) >> 3;
                CLIP(t1, rgb[2]);
            }
        }

        if (bayer < bayerEnd) {
            /* B at B */
            rgb[blue] = bayer[bayerStep2 + 2];
            /* R at B */
            t0 = ((bayer[bayerStep + 1] + bayer[bayerStep + 3] +
                   bayer[bayerStep3 + 1] + bayer[bayerStep3 + 3]) << 1)
                -
                (((bayer[2] + bayer[bayerStep2] +
                   bayer[bayerStep2 + 4] + bayer[bayerStep4 +
                                                 2]) * 3 + 1) >> 1)
                + rgb[blue] * 6;
            /* G at B */
            t1 = (((bayer[bayerStep + 2] + bayer[bayerStep2 + 1] +
                    bayer[bayerStep2 + 3] + bayer[bayerStep3 + 2])) << 1)
                - (bayer[2] + bayer[bayerStep2] +
                   bayer[bayerStep2 + 4] + bayer[bayerStep4 + 2])
                + (rgb[blue] << 2);
            t0 = (t0 + 4) >> 3;
            CLIP(t0, rgb[-blue]);
            t1 = (t1 + 4) >> 3;
            CLIP(t1, rgb[0]);
            bayer++;
            rgb += 3;
        }

        bayer -= width;
        rgb -= width * 3;

        blue = -blue;
        start_with_green = !start_with_green;
    }

    return DC1394_SUCCESS;

}

/* coriander's Bayer decoding */
/* Edge Sensing Interpolation II from http://www-ise.stanford.edu/~tingchen/ */
/*   (Laroche,Claude A.  "Apparatus and method for adaptively
     interpolating a full color image utilizing chrominance gradients"
     U.S. Patent 5,373,322) */
BayerToRgb::dc1394error_t BayerToRgb::dc1394_bayer_EdgeSense(const uint8_t *bayer, uint8_t *rgb, int sx, int sy, int tile)
{
    /* Removed due to patent concerns */
    return DC1394_FUNCTION_NOT_SUPPORTED;
}

/* coriander's Bayer decoding */
BayerToRgb::dc1394error_t BayerToRgb::dc1394_bayer_Downsample(const uint8_t *bayer, uint8_t *rgb, int sx, int sy, int tile)
{
    uint8_t *outR, *outG, *outB;
    int i, j;
    int tmp;

    switch (tile) {
    case DC1394_COLOR_FILTER_GRBG:
    case DC1394_COLOR_FILTER_BGGR:
        outR = &rgb[0];
        outG = &rgb[1];
        outB = &rgb[2];
        break;
    case DC1394_COLOR_FILTER_GBRG:
    case DC1394_COLOR_FILTER_RGGB:
        outR = &rgb[2];
        outG = &rgb[1];
        outB = &rgb[0];
        break;
    default:
      return DC1394_INVALID_COLOR_FILTER;
    }

    switch (tile) {
    case DC1394_COLOR_FILTER_GRBG:        //---------------------------------------------------------
    case DC1394_COLOR_FILTER_GBRG:
        for (i = 0; i < sy*sx; i += (sx<<1)) {
            for (j = 0; j < sx; j += 2) {
                tmp = ((bayer[i + j] + bayer[i + sx + j + 1]) >> 1);
                CLIP(tmp, outG[((i >> 2) + (j >> 1)) * 3]);
                tmp = bayer[i + sx + j + 1];
                CLIP(tmp, outR[((i >> 2) + (j >> 1)) * 3]);
                tmp = bayer[i + sx + j];
                CLIP(tmp, outB[((i >> 2) + (j >> 1)) * 3]);
            }
        }
        break;
    case DC1394_COLOR_FILTER_BGGR:        //---------------------------------------------------------
    case DC1394_COLOR_FILTER_RGGB:
        for (i = 0; i < sy*sx; i += (sx<<1)) {
            for (j = 0; j < sx; j += 2) {
                tmp = ((bayer[i + sx + j] + bayer[i + j + 1]) >> 1);
                CLIP(tmp, outG[((i >> 2) + (j >> 1)) * 3]);
                tmp = bayer[i + sx + j + 1];
                CLIP(tmp, outR[((i >> 2) + (j >> 1)) * 3]);
                tmp = bayer[i + j];
                CLIP(tmp, outB[((i >> 2) + (j >> 1)) * 3]);
            }
        }
        break;
    }

    return DC1394_SUCCESS;

}

/* this is the method used inside AVT cameras. See AVT docs. */
BayerToRgb::dc1394error_t BayerToRgb::dc1394_bayer_Simple(const uint8_t *bayer, uint8_t *rgb, int sx, int sy, int tile)
{
    const int bayerStep = sx;
    const int rgbStep = 3 * sx;
    int width = sx;
    int height = sy;
    int blue = tile == DC1394_COLOR_FILTER_BGGR
        || tile == DC1394_COLOR_FILTER_GBRG ? -1 : 1;
    int start_with_green = tile == DC1394_COLOR_FILTER_GBRG
        || tile == DC1394_COLOR_FILTER_GRBG;
    int i, imax, iinc;

    if ((tile>DC1394_COLOR_FILTER_MAX)||(tile<DC1394_COLOR_FILTER_MIN))
      return DC1394_INVALID_COLOR_FILTER;

    /* add black border */
    imax = sx * sy * 3;
    for (i = sx * (sy - 1) * 3; i < imax; i++) {
        rgb[i] = 0;
    }
    iinc = (sx - 1) * 3;
    for (i = (sx - 1) * 3; i < imax; i += iinc) {
        rgb[i++] = 0;
        rgb[i++] = 0;
        rgb[i++] = 0;
    }

    rgb += 1;
    width -= 1;
    height -= 1;

    for (; height--; bayer += bayerStep, rgb += rgbStep) {
        const uint8_t *bayerEnd = bayer + width;

        if (start_with_green) {
            rgb[-blue] = bayer[1];
            rgb[0] = (bayer[0] + bayer[bayerStep + 1] + 1) >> 1;
            rgb[blue] = bayer[bayerStep];
            bayer++;
            rgb += 3;
        }

        if (blue > 0) {
            for (; bayer <= bayerEnd - 2; bayer += 2, rgb += 6) {
                rgb[-1] = bayer[0];
                rgb[0] = (bayer[1] + bayer[bayerStep] + 1) >> 1;
                rgb[1] = bayer[bayerStep + 1];

                rgb[2] = bayer[2];
                rgb[3] = (bayer[1] + bayer[bayerStep + 2] + 1) >> 1;
                rgb[4] = bayer[bayerStep + 1];
            }
        } else {
            for (; bayer <= bayerEnd - 2; bayer += 2, rgb += 6) {
                rgb[1] = bayer[0];
                rgb[0] = (bayer[1] + bayer[bayerStep] + 1) >> 1;
                rgb[-1] = bayer[bayerStep + 1];

                rgb[4] = bayer[2];
                rgb[3] = (bayer[1] + bayer[bayerStep + 2] + 1) >> 1;
                rgb[2] = bayer[bayerStep + 1];
            }
        }

        if (bayer < bayerEnd) {
            rgb[-blue] = bayer[0];
            rgb[0] = (bayer[1] + bayer[bayerStep] + 1) >> 1;
            rgb[blue] = bayer[bayerStep + 1];
            bayer++;
            rgb += 3;
        }

        bayer -= width;
        rgb -= width * 3;

        blue = -blue;
        start_with_green = !start_with_green;
    }

    return DC1394_SUCCESS;

}

/* 16-bits versions */

/* insprired by OpenCV's Bayer decoding */
BayerToRgb::dc1394error_t BayerToRgb::dc1394_bayer_NearestNeighbor_uint16(const uint16_t *bayer, uint16_t *rgb, int sx, int sy, int tile, int bits)
{
    const int bayerStep = sx;
    const int rgbStep = 3 * sx;
    int width = sx;
    int height = sy;
    int blue = tile == DC1394_COLOR_FILTER_BGGR
        || tile == DC1394_COLOR_FILTER_GBRG ? -1 : 1;
    int start_with_green = tile == DC1394_COLOR_FILTER_GBRG
        || tile == DC1394_COLOR_FILTER_GRBG;
    int i, iinc, imax;

    if ((tile>DC1394_COLOR_FILTER_MAX)||(tile<DC1394_COLOR_FILTER_MIN))
      return DC1394_INVALID_COLOR_FILTER;

    /* add black border */
    imax = sx * sy * 3;
    for (i = sx * (sy - 1) * 3; i < imax; i++) {
        rgb[i] = 0;
    }
    iinc = (sx - 1) * 3;
    for (i = (sx - 1) * 3; i < imax; i += iinc) {
        rgb[i++] = 0;
        rgb[i++] = 0;
        rgb[i++] = 0;
    }

    rgb += 1;
    height -= 1;
    width -= 1;

    for (; height--; bayer += bayerStep, rgb += rgbStep) {
      //int t0, t1;
        const uint16_t *bayerEnd = bayer + width;

        if (start_with_green) {
            rgb[-blue] = bayer[1];
            rgb[0] = bayer[bayerStep + 1];
            rgb[blue] = bayer[bayerStep];
            bayer++;
            rgb += 3;
        }

        if (blue > 0) {
            for (; bayer <= bayerEnd - 2; bayer += 2, rgb += 6) {
                rgb[-1] = bayer[0];
                rgb[0] = bayer[1];
                rgb[1] = bayer[bayerStep + 1];

                rgb[2] = bayer[2];
                rgb[3] = bayer[bayerStep + 2];
                rgb[4] = bayer[bayerStep + 1];
            }
        } else {
            for (; bayer <= bayerEnd - 2; bayer += 2, rgb += 6) {
                rgb[1] = bayer[0];
                rgb[0] = bayer[1];
                rgb[-1] = bayer[bayerStep + 1];

                rgb[4] = bayer[2];
                rgb[3] = bayer[bayerStep + 2];
                rgb[2] = bayer[bayerStep + 1];
            }
        }

        if (bayer < bayerEnd) {
            rgb[-blue] = bayer[0];
            rgb[0] = bayer[1];
            rgb[blue] = bayer[bayerStep + 1];
            bayer++;
            rgb += 3;
        }

        bayer -= width;
        rgb -= width * 3;

        blue = -blue;
        start_with_green = !start_with_green;
    }

    return DC1394_SUCCESS;

}
/* OpenCV's Bayer decoding */
BayerToRgb::dc1394error_t BayerToRgb::dc1394_bayer_Bilinear_uint16(const uint16_t *bayer, uint16_t *rgb, int sx, int sy, int tile, int bits)
{
    const int bayerStep = sx;
    const int rgbStep = 3 * sx;
    int width = sx;
    int height = sy;
    int blue = tile == DC1394_COLOR_FILTER_BGGR
        || tile == DC1394_COLOR_FILTER_GBRG ? -1 : 1;
    int start_with_green = tile == DC1394_COLOR_FILTER_GBRG
        || tile == DC1394_COLOR_FILTER_GRBG;

    if ((tile>DC1394_COLOR_FILTER_MAX)||(tile<DC1394_COLOR_FILTER_MIN))
      return DC1394_INVALID_COLOR_FILTER;

    rgb += rgbStep + 3 + 1;
    height -= 2;
    width -= 2;

    for (; height--; bayer += bayerStep, rgb += rgbStep) {
        int t0, t1;
        const uint16_t *bayerEnd = bayer + width;

        if (start_with_green) {
            /* OpenCV has a bug in the next line, which was
               t0 = (bayer[0] + bayer[bayerStep * 2] + 1) >> 1; */
            t0 = (bayer[1] + bayer[bayerStep * 2 + 1] + 1) >> 1;
            t1 = (bayer[bayerStep] + bayer[bayerStep + 2] + 1) >> 1;
            rgb[-blue] = (uint16_t) t0;
            rgb[0] = bayer[bayerStep + 1];
            rgb[blue] = (uint16_t) t1;
            bayer++;
            rgb += 3;
        }

        if (blue > 0) {
            for (; bayer <= bayerEnd - 2; bayer += 2, rgb += 6) {
                t0 = (bayer[0] + bayer[2] + bayer[bayerStep * 2] +
                      bayer[bayerStep * 2 + 2] + 2) >> 2;
                t1 = (bayer[1] + bayer[bayerStep] +
                      bayer[bayerStep + 2] + bayer[bayerStep * 2 + 1] +
                      2) >> 2;
                rgb[-1] = (uint16_t) t0;
                rgb[0] = (uint16_t) t1;
                rgb[1] = bayer[bayerStep + 1];

                t0 = (bayer[2] + bayer[bayerStep * 2 + 2] + 1) >> 1;
                t1 = (bayer[bayerStep + 1] + bayer[bayerStep + 3] +
                      1) >> 1;
                rgb[2] = (uint16_t) t0;
                rgb[3] = bayer[bayerStep + 2];
                rgb[4] = (uint16_t) t1;
            }
        } else {
            for (; bayer <= bayerEnd - 2; bayer += 2, rgb += 6) {
                t0 = (bayer[0] + bayer[2] + bayer[bayerStep * 2] +
                      bayer[bayerStep * 2 + 2] + 2) >> 2;
                t1 = (bayer[1] + bayer[bayerStep] +
                      bayer[bayerStep + 2] + bayer[bayerStep * 2 + 1] +
                      2) >> 2;
                rgb[1] = (uint16_t) t0;
                rgb[0] = (uint16_t) t1;
                rgb[-1] = bayer[bayerStep + 1];

                t0 = (bayer[2] + bayer[bayerStep * 2 + 2] + 1) >> 1;
                t1 = (bayer[bayerStep + 1] + bayer[bayerStep + 3] +
                      1) >> 1;
                rgb[4] = (uint16_t) t0;
                rgb[3] = bayer[bayerStep + 2];
                rgb[2] = (uint16_t) t1;
            }
        }

        if (bayer < bayerEnd) {
            t0 = (bayer[0] + bayer[2] + bayer[bayerStep * 2] +
                  bayer[bayerStep * 2 + 2] + 2) >> 2;
            t1 = (bayer[1] + bayer[bayerStep] +
                  bayer[bayerStep + 2] + bayer[bayerStep * 2 + 1] +
                  2) >> 2;
            rgb[-blue] = (uint16_t) t0;
            rgb[0] = (uint16_t) t1;
            rgb[blue] = bayer[bayerStep + 1];
            bayer++;
            rgb += 3;
        }

        bayer -= width;
        rgb -= width * 3;

        blue = -blue;
        start_with_green = !start_with_green;
    }

    return DC1394_SUCCESS;

}

/* High-Quality Linear Interpolation For Demosaicing Of
   Bayer-Patterned Color Images, by Henrique S. Malvar, Li-wei He, and
   Ross Cutler, in ICASSP'04 */
BayerToRgb::dc1394error_t BayerToRgb::dc1394_bayer_HQLinear_uint16(const uint16_t *bayer, uint16_t *rgb, int sx, int sy, int tile, int bits)
{
    const int bayerStep = sx;
    const int rgbStep = 3 * sx;
    int width = sx;
    int height = sy;
    /*
       the two letters  of the OpenCV name are respectively
       the 4th and 3rd letters from the blinky name,
       and we also have to switch R and B (OpenCV is BGR)

       CV_BayerBG2BGR <-> DC1394_COLOR_FILTER_BGGR
       CV_BayerGB2BGR <-> DC1394_COLOR_FILTER_GBRG
       CV_BayerGR2BGR <-> DC1394_COLOR_FILTER_GRBG

       int blue = tile == CV_BayerBG2BGR || tile == CV_BayerGB2BGR ? -1 : 1;
       int start_with_green = tile == CV_BayerGB2BGR || tile == CV_BayerGR2BGR;
     */
    int blue = tile == DC1394_COLOR_FILTER_BGGR
        || tile == DC1394_COLOR_FILTER_GBRG ? -1 : 1;
    int start_with_green = tile == DC1394_COLOR_FILTER_GBRG
        || tile == DC1394_COLOR_FILTER_GRBG;

    if ((tile>DC1394_COLOR_FILTER_MAX)||(tile<DC1394_COLOR_FILTER_MIN))
      return DC1394_INVALID_COLOR_FILTER;

    ClearBorders_uint16(rgb, sx, sy, 2);
    rgb += 2 * rgbStep + 6 + 1;
    height -= 4;
    width -= 4;

    /* We begin with a (+1 line,+1 column) offset with respect to bilinear decoding, so start_with_green is the same, but blue is opposite */
    blue = -blue;

    for (; height--; bayer += bayerStep, rgb += rgbStep) {
        int t0, t1;
        const uint16_t *bayerEnd = bayer + width;
        const int bayerStep2 = bayerStep * 2;
        const int bayerStep3 = bayerStep * 3;
        const int bayerStep4 = bayerStep * 4;

        if (start_with_green) {
            /* at green pixel */
            rgb[0] = bayer[bayerStep2 + 2];
            t0 = rgb[0] * 5
                + ((bayer[bayerStep + 2] + bayer[bayerStep3 + 2]) << 2)
                - bayer[2]
                - bayer[bayerStep + 1]
                - bayer[bayerStep + 3]
                - bayer[bayerStep3 + 1]
                - bayer[bayerStep3 + 3]
                - bayer[bayerStep4 + 2]
                + ((bayer[bayerStep2] + bayer[bayerStep2 + 4] + 1) >> 1);
            t1 = rgb[0] * 5 +
                ((bayer[bayerStep2 + 1] + bayer[bayerStep2 + 3]) << 2)
                - bayer[bayerStep2]
                - bayer[bayerStep + 1]
                - bayer[bayerStep + 3]
                - bayer[bayerStep3 + 1]
                - bayer[bayerStep3 + 3]
                - bayer[bayerStep2 + 4]
                + ((bayer[2] + bayer[bayerStep4 + 2] + 1) >> 1);
            t0 = (t0 + 4) >> 3;
            CLIP16(t0, rgb[-blue], bits);
            t1 = (t1 + 4) >> 3;
            CLIP16(t1, rgb[blue], bits);
            bayer++;
            rgb += 3;
        }

        if (blue > 0) {
            for (; bayer <= bayerEnd - 2; bayer += 2, rgb += 6) {
                /* B at B */
                rgb[1] = bayer[bayerStep2 + 2];
                /* R at B */
                t0 = ((bayer[bayerStep + 1] + bayer[bayerStep + 3] +
                       bayer[bayerStep3 + 1] + bayer[bayerStep3 + 3]) << 1)
                    -
                    (((bayer[2] + bayer[bayerStep2] +
                       bayer[bayerStep2 + 4] + bayer[bayerStep4 +
                                                     2]) * 3 + 1) >> 1)
                    + rgb[1] * 6;
                /* G at B */
                t1 = ((bayer[bayerStep + 2] + bayer[bayerStep2 + 1] +
                       bayer[bayerStep2 + 3] + bayer[bayerStep * 3 +
                                                     2]) << 1)
                    - (bayer[2] + bayer[bayerStep2] +
                       bayer[bayerStep2 + 4] + bayer[bayerStep4 + 2])
                    + (rgb[1] << 2);
                t0 = (t0 + 4) >> 3;
                CLIP16(t0, rgb[-1], bits);
                t1 = (t1 + 4) >> 3;
                CLIP16(t1, rgb[0], bits);
                /* at green pixel */
                rgb[3] = bayer[bayerStep2 + 3];
                t0 = rgb[3] * 5
                    + ((bayer[bayerStep + 3] + bayer[bayerStep3 + 3]) << 2)
                    - bayer[3]
                    - bayer[bayerStep + 2]
                    - bayer[bayerStep + 4]
                    - bayer[bayerStep3 + 2]
                    - bayer[bayerStep3 + 4]
                    - bayer[bayerStep4 + 3]
                    +
                    ((bayer[bayerStep2 + 1] + bayer[bayerStep2 + 5] +
                      1) >> 1);
                t1 = rgb[3] * 5 +
                    ((bayer[bayerStep2 + 2] + bayer[bayerStep2 + 4]) << 2)
                    - bayer[bayerStep2 + 1]
                    - bayer[bayerStep + 2]
                    - bayer[bayerStep + 4]
                    - bayer[bayerStep3 + 2]
                    - bayer[bayerStep3 + 4]
                    - bayer[bayerStep2 + 5]
                    + ((bayer[3] + bayer[bayerStep4 + 3] + 1) >> 1);
                t0 = (t0 + 4) >> 3;
                CLIP16(t0, rgb[2], bits);
                t1 = (t1 + 4) >> 3;
                CLIP16(t1, rgb[4], bits);
            }
        } else {
            for (; bayer <= bayerEnd - 2; bayer += 2, rgb += 6) {
                /* R at R */
                rgb[-1] = bayer[bayerStep2 + 2];
                /* B at R */
                t0 = ((bayer[bayerStep + 1] + bayer[bayerStep + 3] +
                       bayer[bayerStep * 3 + 1] + bayer[bayerStep3 +
                                                        3]) << 1)
                    -
                    (((bayer[2] + bayer[bayerStep2] +
                       bayer[bayerStep2 + 4] + bayer[bayerStep4 +
                                                     2]) * 3 + 1) >> 1)
                    + rgb[-1] * 6;
                /* G at R */
                t1 = ((bayer[bayerStep + 2] + bayer[bayerStep2 + 1] +
                       bayer[bayerStep2 + 3] + bayer[bayerStep3 + 2]) << 1)
                    - (bayer[2] + bayer[bayerStep2] +
                       bayer[bayerStep2 + 4] + bayer[bayerStep4 + 2])
                    + (rgb[-1] << 2);
                t0 = (t0 + 4) >> 3;
                CLIP16(t0, rgb[1], bits);
                t1 = (t1 + 4) >> 3;
                CLIP16(t1, rgb[0], bits);

                /* at green pixel */
                rgb[3] = bayer[bayerStep2 + 3];
                t0 = rgb[3] * 5
                    + ((bayer[bayerStep + 3] + bayer[bayerStep3 + 3]) << 2)
                    - bayer[3]
                    - bayer[bayerStep + 2]
                    - bayer[bayerStep + 4]
                    - bayer[bayerStep3 + 2]
                    - bayer[bayerStep3 + 4]
                    - bayer[bayerStep4 + 3]
                    +
                    ((bayer[bayerStep2 + 1] + bayer[bayerStep2 + 5] +
                      1) >> 1);
                t1 = rgb[3] * 5 +
                    ((bayer[bayerStep2 + 2] + bayer[bayerStep2 + 4]) << 2)
                    - bayer[bayerStep2 + 1]
                    - bayer[bayerStep + 2]
                    - bayer[bayerStep + 4]
                    - bayer[bayerStep3 + 2]
                    - bayer[bayerStep3 + 4]
                    - bayer[bayerStep2 + 5]
                    + ((bayer[3] + bayer[bayerStep4 + 3] + 1) >> 1);
                t0 = (t0 + 4) >> 3;
                CLIP16(t0, rgb[4], bits);
                t1 = (t1 + 4) >> 3;
                CLIP16(t1, rgb[2], bits);
            }
        }

        if (bayer < bayerEnd) {
            /* B at B */
            rgb[blue] = bayer[bayerStep2 + 2];
            /* R at B */
            t0 = ((bayer[bayerStep + 1] + bayer[bayerStep + 3] +
                   bayer[bayerStep3 + 1] + bayer[bayerStep3 + 3]) << 1)
                -
                (((bayer[2] + bayer[bayerStep2] +
                   bayer[bayerStep2 + 4] + bayer[bayerStep4 +
                                                 2]) * 3 + 1) >> 1)
                + rgb[blue] * 6;
            /* G at B */
            t1 = (((bayer[bayerStep + 2] + bayer[bayerStep2 + 1] +
                    bayer[bayerStep2 + 3] + bayer[bayerStep3 + 2])) << 1)
                - (bayer[2] + bayer[bayerStep2] +
                   bayer[bayerStep2 + 4] + bayer[bayerStep4 + 2])
                + (rgb[blue] << 2);
            t0 = (t0 + 4) >> 3;
            CLIP16(t0, rgb[-blue], bits);
            t1 = (t1 + 4) >> 3;
            CLIP16(t1, rgb[0], bits);
            bayer++;
            rgb += 3;
        }

        bayer -= width;
        rgb -= width * 3;

        blue = -blue;
        start_with_green = !start_with_green;
    }

    return DC1394_SUCCESS;
}

/* coriander's Bayer decoding */
BayerToRgb::dc1394error_t BayerToRgb::dc1394_bayer_EdgeSense_uint16(const uint16_t *bayer, uint16_t *rgb, int sx, int sy, int tile, int bits)
{
    /* Removed due to patent concerns */
    return DC1394_FUNCTION_NOT_SUPPORTED;
}

/* coriander's Bayer decoding */
BayerToRgb::dc1394error_t BayerToRgb::dc1394_bayer_Downsample_uint16(const uint16_t *bayer, uint16_t *rgb, int sx, int sy, int tile, int bits)
{
    uint16_t *outR, *outG, *outB;
    int i, j;
    int tmp;

    switch (tile) {
    case DC1394_COLOR_FILTER_GRBG:
    case DC1394_COLOR_FILTER_BGGR:
        outR = &rgb[0];
        outG = &rgb[1];
        outB = &rgb[2];
        break;
    case DC1394_COLOR_FILTER_GBRG:
    case DC1394_COLOR_FILTER_RGGB:
        outR = &rgb[2];
        outG = &rgb[1];
        outB = &rgb[0];
        break;
    default:
      return DC1394_INVALID_COLOR_FILTER;
    }

    switch (tile) {
    case DC1394_COLOR_FILTER_GRBG:        //---------------------------------------------------------
    case DC1394_COLOR_FILTER_GBRG:
        for (i = 0; i < sy*sx; i += (sx<<1)) {
            for (j = 0; j < sx; j += 2) {
                tmp =
                    ((bayer[i + j] + bayer[i + sx + j + 1]) >> 1);
                CLIP16(tmp, outG[((i >> 2) + (j >> 1)) * 3], bits);
                tmp = bayer[i + sx + j + 1];
                CLIP16(tmp, outR[((i >> 2) + (j >> 1)) * 3], bits);
                tmp = bayer[i + sx + j];
                CLIP16(tmp, outB[((i >> 2) + (j >> 1)) * 3], bits);
            }
        }
        break;
    case DC1394_COLOR_FILTER_BGGR:        //---------------------------------------------------------
    case DC1394_COLOR_FILTER_RGGB:
        for (i = 0; i < sy*sx; i += (sx<<1)) {
            for (j = 0; j < sx; j += 2) {
                tmp =
                    ((bayer[i + sx + j] + bayer[i + j + 1]) >> 1);
                CLIP16(tmp, outG[((i >> 2) + (j >> 1)) * 3], bits);
                tmp = bayer[i + sx + j + 1];
                CLIP16(tmp, outR[((i >> 2) + (j >> 1)) * 3], bits);
                tmp = bayer[i + j];
                CLIP16(tmp, outB[((i >> 2) + (j >> 1)) * 3], bits);
            }
        }
        break;
    }

    return DC1394_SUCCESS;

}

/* coriander's Bayer decoding */
BayerToRgb::dc1394error_t BayerToRgb::dc1394_bayer_Simple_uint16(const uint16_t *bayer, uint16_t *rgb, int sx, int sy, int tile, int bits)
{
    uint16_t *outR, *outG, *outB;
    int i, j;
    int tmp, base;

    // sx and sy should be even
    switch (tile) {
    case DC1394_COLOR_FILTER_GRBG:
    case DC1394_COLOR_FILTER_BGGR:
        outR = &rgb[0];
        outG = &rgb[1];
        outB = &rgb[2];
        break;
    case DC1394_COLOR_FILTER_GBRG:
    case DC1394_COLOR_FILTER_RGGB:
        outR = &rgb[2];
        outG = &rgb[1];
        outB = &rgb[0];
        break;
    default:
      return DC1394_INVALID_COLOR_FILTER;
    }

    switch (tile) {
    case DC1394_COLOR_FILTER_GRBG:
    case DC1394_COLOR_FILTER_BGGR:
        outR = &rgb[0];
        outG = &rgb[1];
        outB = &rgb[2];
        break;
    case DC1394_COLOR_FILTER_GBRG:
    case DC1394_COLOR_FILTER_RGGB:
        outR = &rgb[2];
        outG = &rgb[1];
        outB = &rgb[0];
        break;
    default:
        outR = NULL;
        outG = NULL;
        outB = NULL;
        break;
    }

    switch (tile) {
    case DC1394_COLOR_FILTER_GRBG:        //---------------------------------------------------------
    case DC1394_COLOR_FILTER_GBRG:
        for (i = 0; i < sy - 1; i += 2) {
            for (j = 0; j < sx - 1; j += 2) {
                base = i * sx + j;
                tmp = ((bayer[base] + bayer[base + sx + 1]) >> 1);
                CLIP16(tmp, outG[base * 3], bits);
                tmp = bayer[base + 1];
                CLIP16(tmp, outR[base * 3], bits);
                tmp = bayer[base + sx];
                CLIP16(tmp, outB[base * 3], bits);
            }
        }
        for (i = 0; i < sy - 1; i += 2) {
            for (j = 1; j < sx - 1; j += 2) {
                base = i * sx + j;
                tmp = ((bayer[base + 1] + bayer[base + sx]) >> 1);
                CLIP16(tmp, outG[(base) * 3], bits);
                tmp = bayer[base];
                CLIP16(tmp, outR[(base) * 3], bits);
                tmp = bayer[base + 1 + sx];
                CLIP16(tmp, outB[(base) * 3], bits);
            }
        }
        for (i = 1; i < sy - 1; i += 2) {
            for (j = 0; j < sx - 1; j += 2) {
                base = i * sx + j;
                tmp = ((bayer[base + sx] + bayer[base + 1]) >> 1);
                CLIP16(tmp, outG[base * 3], bits);
                tmp = bayer[base + sx + 1];
                CLIP16(tmp, outR[base * 3], bits);
                tmp = bayer[base];
                CLIP16(tmp, outB[base * 3], bits);
            }
        }
        for (i = 1; i < sy - 1; i += 2) {
            for (j = 1; j < sx - 1; j += 2) {
                base = i * sx + j;
                tmp = ((bayer[base] + bayer[base + 1 + sx]) >> 1);
                CLIP16(tmp, outG[(base) * 3], bits);
                tmp = bayer[base + sx];
                CLIP16(tmp, outR[(base) * 3], bits);
                tmp = bayer[base + 1];
                CLIP16(tmp, outB[(base) * 3], bits);
            }
        }
        break;
    case DC1394_COLOR_FILTER_BGGR:        //---------------------------------------------------------
    case DC1394_COLOR_FILTER_RGGB:
        for (i = 0; i < sy - 1; i += 2) {
            for (j = 0; j < sx - 1; j += 2) {
                base = i * sx + j;
                tmp = ((bayer[base + sx] + bayer[base + 1]) >> 1);
                CLIP16(tmp, outG[base * 3], bits);
                tmp = bayer[base + sx + 1];
                CLIP16(tmp, outR[base * 3], bits);
                tmp = bayer[base];
                CLIP16(tmp, outB[base * 3], bits);
            }
        }
        for (i = 1; i < sy - 1; i += 2) {
            for (j = 0; j < sx - 1; j += 2) {
                base = i * sx + j;
                tmp = ((bayer[base] + bayer[base + 1 + sx]) >> 1);
                CLIP16(tmp, outG[(base) * 3], bits);
                tmp = bayer[base + 1];
                CLIP16(tmp, outR[(base) * 3], bits);
                tmp = bayer[base + sx];
                CLIP16(tmp, outB[(base) * 3], bits);
            }
        }
        for (i = 0; i < sy - 1; i += 2) {
            for (j = 1; j < sx - 1; j += 2) {
                base = i * sx + j;
                tmp = ((bayer[base] + bayer[base + sx + 1]) >> 1);
                CLIP16(tmp, outG[base * 3], bits);
                tmp = bayer[base + sx];
                CLIP16(tmp, outR[base * 3], bits);
                tmp = bayer[base + 1];
                CLIP16(tmp, outB[base * 3], bits);
            }
        }
        for (i = 1; i < sy - 1; i += 2) {
            for (j = 1; j < sx - 1; j += 2) {
                base = i * sx + j;
                tmp = ((bayer[base + 1] + bayer[base + sx]) >> 1);
                CLIP16(tmp, outG[(base) * 3], bits);
                tmp = bayer[base];
                CLIP16(tmp, outR[(base) * 3], bits);
                tmp = bayer[base + 1 + sx];
                CLIP16(tmp, outB[(base) * 3], bits);
            }
        }
        break;
    }

    /* add black border */
    for (i = sx * (sy - 1) * 3; i < sx * sy * 3; i++) {
        rgb[i] = 0;
    }
    for (i = (sx - 1) * 3; i < sx * sy * 3; i += (sx - 1) * 3) {
        rgb[i++] = 0;
        rgb[i++] = 0;
        rgb[i++] = 0;
    }

    return DC1394_SUCCESS;

}

BayerToRgb::dc1394error_t BayerToRgb::dc1394_bayer_decoding_8bit(const uint8_t *bayer, uint8_t *rgb, uint32_t sx, uint32_t sy, dc1394color_filter_t tile, dc1394bayer_method_t method)
{
    switch (method) {
    case DC1394_BAYER_METHOD_NEAREST:
        return dc1394_bayer_NearestNeighbor(bayer, rgb, sx, sy, tile);
    case DC1394_BAYER_METHOD_SIMPLE:
        return dc1394_bayer_Simple(bayer, rgb, sx, sy, tile);
    case DC1394_BAYER_METHOD_BILINEAR:
        return dc1394_bayer_Bilinear(bayer, rgb, sx, sy, tile);
    case DC1394_BAYER_METHOD_HQLINEAR:
        return dc1394_bayer_HQLinear(bayer, rgb, sx, sy, tile);
    case DC1394_BAYER_METHOD_DOWNSAMPLE:
        return dc1394_bayer_Downsample(bayer, rgb, sx, sy, tile);
    case DC1394_BAYER_METHOD_EDGESENSE:
        return dc1394_bayer_EdgeSense(bayer, rgb, sx, sy, tile);
    default:
        return DC1394_INVALID_BAYER_METHOD;
  }

}

BayerToRgb::dc1394error_t BayerToRgb::dc1394_bayer_decoding_16bit(const uint16_t *bayer, uint16_t *rgb, uint32_t sx, uint32_t sy, dc1394color_filter_t tile, dc1394bayer_method_t method, uint32_t bits)
{
    switch (method) {
    case DC1394_BAYER_METHOD_NEAREST:
        return dc1394_bayer_NearestNeighbor_uint16(bayer, rgb, sx, sy, tile, bits);
    case DC1394_BAYER_METHOD_SIMPLE:
        return dc1394_bayer_Simple_uint16(bayer, rgb, sx, sy, tile, bits);
    case DC1394_BAYER_METHOD_BILINEAR:
        return dc1394_bayer_Bilinear_uint16(bayer, rgb, sx, sy, tile, bits);
    case DC1394_BAYER_METHOD_HQLINEAR:
        return dc1394_bayer_HQLinear_uint16(bayer, rgb, sx, sy, tile, bits);
    case DC1394_BAYER_METHOD_DOWNSAMPLE:
        return dc1394_bayer_Downsample_uint16(bayer, rgb, sx, sy, tile, bits);
    case DC1394_BAYER_METHOD_EDGESENSE:
        return dc1394_bayer_EdgeSense_uint16(bayer, rgb, sx, sy, tile, bits);
    default:
        return DC1394_INVALID_BAYER_METHOD;
    }
}

bool BayerToRgb::convert(uint8_t *bayer, uint8_t *rgb)
{
  dc1394error_t ret = DC1394_SUCCESS;
  switch(bits)
  {
    case 8:
      ret = dc1394_bayer_decoding_8bit(bayer, rgb, sx, sy, tile, method);
      break;
    case 16:
      if(swap){
          uint8_t tmp=0;
          uint32_t i=0;
          long in_size = sx*sy*2/*16 bit*/;
          for(i=0;i<in_size;i+=2){
              tmp = *((bayer)+i);
              *((bayer)+i) = *((bayer)+i+1);
              *((bayer)+i+1) = tmp;
          }
      }
      ret = dc1394_bayer_decoding_16bit(reinterpret_cast<const uint16_t*>(bayer), reinterpret_cast<uint16_t*>(rgb), sx, sy, tile, method, bits);
      break;
    default:
      return false;
  }
  return ret == DC1394_SUCCESS;
}
