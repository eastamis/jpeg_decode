#include <stdlib.h>
#include <stdio.h>

/* extern function */
extern int32_t round_up(int32_t a, int32_t b);

/* constant definition */
#define OVER_RANGE		256
#define DATA_RANGE		256
#define TOTAL_RANGE DATA_RANGE + OVER_RANGE * 2

static unsigned char CLIP[TOTAL_RANGE];
static long CY1[256], CU1[256], CU2[256], CV1[256], CV2[256];

void create_yuv_table (void)
{
  int i;

  for (i=0; i<256; i++) {
	  CY1[i] = (long) (i<<16);
	  CV1[i] = (long) (1371*(1<<16)/1000 * (i-128));
	  CV2[i] = (long) ( 698*(1<<16)/1000 * (i-128));
	  CU1[i] = (long) ( 336*(1<<16)/1000 * (i-128));
	  CU2[i] = (long) (1732*(1<<16)/1000 * (i-128));
  }

  for (i = 0; i < OVER_RANGE; i++)
	  CLIP[i] = 16; //0;

  for (i = OVER_RANGE; i < (OVER_RANGE + DATA_RANGE); i++)
	  CLIP[i] = (unsigned char)(i - OVER_RANGE);

  for (i = (OVER_RANGE + DATA_RANGE); i < TOTAL_RANGE; i++)
	  CLIP[i] = 240; //(unsigned char)(DATA_RANGE - 1);
}

void yuv444torgb (unsigned char *pYUV, unsigned char *pRGB, int width, int height)
{
  int x, y;
  long len;
  unsigned char * pY;
  unsigned char * pU;
  unsigned char * pV;
  unsigned char * pRGB2;
  int w;
  int rnd_width, rnd_height;
  long red, green ,blue;
  int  r1, g1, b1;

  rnd_width = round_up(width, 2);
  rnd_height = round_up(height, 2);

  w = round_up(width*3, 4);

  len = rnd_width * rnd_height;
  pY = pYUV;
  pU = pYUV + len;
  pV = pU + len;

  pRGB2 = pRGB + w * height;		// RGB buffer

  for (y=0; y<height; y++) {
	  pRGB2 -= w;
	  pRGB = pRGB2;			// RGB buffer

	  for (x=0; x<width; x++) {
      r1 = CV1[pV[x]];
	    g1 = CU1[pU[x]] + CV2[pV[x]];
	    b1 = CU2[pU[x]];

	    // calculate the (R,G,B) for (x,y)
	    red	  = ( CY1[pY[x]] + r1 ) >> 16;
	    green = ( CY1[pY[x]] - g1 ) >> 16;
	    blue  = ( CY1[pY[x]] + b1 ) >> 16;

	    pRGB[2] = CLIP[red + OVER_RANGE];
	    pRGB[1] = CLIP[green + OVER_RANGE];
	    pRGB[0] = CLIP[blue + OVER_RANGE];

	    pRGB += 3;
	  }

	  pY += width;
	  pU += width;
	  pV += width;
  }
}

void yuv422torgb (unsigned char *pYUV, unsigned char *pRGB, int width, int height)
{
  int x, y, ux;
  long len;
  unsigned char * pY;
  unsigned char * pU;
  unsigned char * pV;
  unsigned char * pRGB2;
  int w1, w2;
  int rnd_width, rnd_height;
  long red, green ,blue;
  int  r1, g1, b1, y0, y1;

  rnd_width = round_up(width, 2);
  rnd_height = round_up(height, 2);

  w1 = round_up(width*3, 4);
  w2 = rnd_width >> 1;

  len = rnd_width * rnd_height;
  pY = pYUV;
  pU = pYUV + len;
  pV = pU + (len >> 1);

  pRGB2 = pRGB + w1 * height;		// RGB buffer

  for (y=0; y<height; y++) {
	  pRGB2 -= w1;
	  pRGB = pRGB2;			// RGB buffer

	  for (x=0; x<width; x+=2) {
      ux = x >> 1;

	    r1 = CV1[pV[ux]];
	    g1 = CU1[pU[ux]] + CV2[pV[ux]];
	    b1 = CU2[pU[ux]];
	    y0 = CY1[pY[x]];
	    if (x+1 != width)
	      y1 = CY1[pY[x+1]];
	    else
	      y1 = y0;

	    // calculate the (R,G,B) for (x,y)
	    red	  = ( y0 + r1 ) >> 16;
	    green = ( y0 - g1 ) >> 16;
	    blue  = ( y0 + b1 ) >> 16;

	    pRGB[2] = CLIP[red + OVER_RANGE];
	    pRGB[1] = CLIP[green + OVER_RANGE];
	    pRGB[0] = CLIP[blue + OVER_RANGE];

	    // calculate the (R,G,B) for (x+1,y)
	    if (x+1 != width) {
        red	  = ( y1 + r1 ) >> 16;
        green = ( y1 - g1 ) >> 16;
        blue  = ( y1 + b1 ) >> 16;

        pRGB[5] = CLIP[red + OVER_RANGE];
        pRGB[4] = CLIP[green + OVER_RANGE];
        pRGB[3] = CLIP[blue + OVER_RANGE];
      }
      pRGB += 6;
	  }

	  pY += width;
	  pU += w2;
	  pV += w2;
  }
}

void yuv411torgb (unsigned char *pYUV, unsigned char *pRGB, int width, int height)
{
  int x, y, ux;
  long len;
  unsigned char * pY;
  unsigned char * pU;
  unsigned char * pV;
  unsigned char * pRGB2;
  int w1, w2;
  int rnd_width, rnd_height;
  long red, green ,blue;
  int  r1, g1, b1, y0, y1, y2, y3;
  rnd_width = round_up(width, 2);
  rnd_height = round_up(height, 2);

  w1 = round_up(width*3, 4);
  w2 = rnd_width >> 1;

  len = rnd_width * rnd_height;
  pY = pYUV;
  pU = pY + len;
  pV = pU + (len >> 2);

  pRGB2 = pRGB + w1 * height;		// RGB buffer

  for (y=0; y<height; y+=2) {
    pRGB2 -= w1;
    pRGB = pRGB2;			// RGB buffer

    for (x=0; x<width; x+=2) {
      ux = x >> 1;

      r1 = CV1[pV[ux]];
      g1 = CU1[pU[ux]] + CV2[pV[ux]];
      b1 = CU2[pU[ux]];
      y0 = CY1[pY[x]];
      if (x+1 != width)
        y1 = CY1[pY[x+1]];
      else
        y1 = y0;

      if (y+1 != height) {
        y2 = CY1[pY[rnd_width+x]];
        if (x+1 != width)
          y3 = CY1[pY[rnd_width+x+1]];
        else
          y3 = y2;
      } else {
        y2 = y0;
        y3 = y1;
      }

      // calculate the (R,G,B) for (x,y)
      red   = ( y0 + r1 ) >> 16;
      green = ( y0 - g1 ) >> 16;
      blue  = ( y0 + b1 ) >> 16;

      pRGB[2] = CLIP[red + OVER_RANGE];
      pRGB[1] = CLIP[green + OVER_RANGE];
      pRGB[0] = CLIP[blue + OVER_RANGE];

      // calculate the (R,G,B) for (x+1,y)
      if (x+1 != width) {
        red	  = ( y1 + r1 ) >> 16;
        green = ( y1 - g1 ) >> 16;
        blue  = ( y1 + b1 ) >> 16;

        pRGB[5] = CLIP[red + OVER_RANGE];
        pRGB[4] = CLIP[green + OVER_RANGE];
        pRGB[3] = CLIP[blue + OVER_RANGE];
      }

      if (y+1 != height) {
        // calculate the (R,G,B) for (x,y+1)
        red	  = ( y2 + r1 ) >> 16;
        green = ( y2 - g1 ) >> 16;
        blue  = ( y2 + b1 ) >> 16;

        pRGB[2-w1] = CLIP[red + OVER_RANGE];
        pRGB[1-w1] = CLIP[green + OVER_RANGE];
        pRGB[0-w1] = CLIP[blue + OVER_RANGE];

        // calculate the (R,G,B) for (x+1,y+1)
        if (x+1 != width) {
          red	  = ( y3 + r1 ) >> 16;
          green = ( y3 - g1 ) >> 16;
          blue  = ( y3 + b1 ) >> 16;

          pRGB[5-w1] = CLIP[red + OVER_RANGE];
          pRGB[4-w1] = CLIP[green + OVER_RANGE];
          pRGB[3-w1] = CLIP[blue + OVER_RANGE];
        }
      }

      pRGB += 6;
    }

    if (y+1 != height) {
      pY += (rnd_width<<1);
      pU += w2;
      pV += w2;
      pRGB2 -= w1;
    }
  }
}
