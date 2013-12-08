#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <unistd.h>
#include <errno.h>
#include "jpginfo.h"

/* intdct.c */
extern void iDCT(int *src, int *dst);

/* color.c */
extern void create_yuv_table(void);
extern void yuv444torgb (unsigned char *pYUV,
                unsigned char *pRGB, int width, int height);
extern void yuv422torgb(unsigned char *pYUV,
                unsigned char *pRGB, int width, int height);
extern void yuv411torgb(unsigned char *pYUV,
                unsigned char *pRGB, int width, int height);

/* huffdec.c */
extern int Decode_DC(JPEG_INFO *ji, int cpno, int *dc_coff);
extern int Decode_AC(JPEG_INFO *ji, int cpno, int *ac_coff);

/* readbits.c */
extern int get1byte(FILE *f, unsigned char *data);
extern int get2byte(FILE *f, unsigned char *data);
extern int getnbyte(FILE *f, unsigned char *data, int size);
extern int ReadBits(JPEG_INFO *ji, int count, int *value);

/* zig-zag */
const int natural_order[64] = {
  0,  1,  8, 16,  9,  2,  3, 10,
 17, 24, 32, 25, 18, 11,  4,  5,
 12, 19, 26, 33, 40, 48, 41, 34,
 27, 20, 13,  6,  7, 14, 21, 28,
 35, 42, 49, 56, 57, 50, 43, 36,
 29, 22, 15, 23, 30, 37, 44, 51,
 58, 59, 52, 45, 38, 31, 39, 46,
 53, 60, 61, 54, 47, 55, 62, 63
};


// CODE SEGMENT
long div_round_up (long a, long b)
// Compute a/b rounded up to next integer
{
  return (a + b - 1L) / b;
}

long round_up (long a, long b)
// Compute a rounded up to next multiple of b
{
  a += b - 1L;
  return a - (a % b);
}

int ParseQuantTable (JPEG_INFO *ji)
{
  int size = 0;
  unsigned char *dq_table;
  unsigned char tmp, no;
  int n, offset = 0;

  if (!ReadBits(ji, 16, &size))
    return 0;

  printf("    Quantization Table size is %d bytes\n", size);

  size -= 2;

  dq_table = malloc(size);

  // run out left bits
  switch (ji->DataStream.leftbits) {
  case 0:
    if (!getnbyte(ji->f, dq_table, size))
      return 0;
    break;
  case 8:
    dq_table[0] = (ji->DataStream.data >> 8) & 0xff;
    if (!getnbyte(ji->f, &dq_table[1], size-1))
      return 0;
    ji->DataStream.leftbits = ji->DataStream.data = 0;
    break;
  default:
    return 0;
  }

  while (offset < size) {
    no = dq_table[offset] & 0x0f;
    tmp = (dq_table[offset] >> 4) & 0x0f;
    printf("    QT No.: %x, bits: %d\n", no, (tmp==0) ? 8 : 16);
    offset++;

    if (tmp != 0) {
      printf("Currently do support 16 bits!\n");
      return 0;
    }

    ji->Quan_Table[no] = malloc(sizeof(QUAN_TABLE));
    if (ji->Quan_Table[no] == NULL)
      return 0;
    ji->quan_Tables++;

    printf("    Quantization table:\n");
    for (n = 0; n < 64; n++, offset++) {
      if ((n % 8) == 0)
        printf("    ");

      ji->Quan_Table[no]->quan_data[n] = dq_table[offset];
      printf(" %d", ji->Quan_Table[no]->quan_data[n]);

      if ((n % 8) == 7)
        printf("\n");
    }
  }

  printf("\n");
  free(dq_table);
  return 1;
}

int ParseHuffmanTable (JPEG_INFO *ji)
{
  int size = 0;
  unsigned char *dh_table;
  unsigned char type, no, tmp;
  int i, n, offset = 0;
  int count;
  unsigned short si, code;
  unsigned short *huffsize;
  unsigned short *huffcode;
  unsigned short *huffval;
  unsigned long *huffmax;
  unsigned long *huff1st;

  if (!ReadBits(ji, 16, &size))
    return 0;

  printf("    Huffman Table size is %d bytes\n", size);

  size -= 2;
  dh_table = malloc(size);

  // run out left bits
  switch (ji->DataStream.leftbits) {
  case 0:
    if (!getnbyte(ji->f, dh_table, size))
      return 0;
    break;
  case 8:
    dh_table[0] = (ji->DataStream.data >> 8) & 0xff;
    if (!getnbyte(ji->f, &dh_table[1], size-1))
      return 0;
    ji->DataStream.leftbits = ji->DataStream.data = 0;
    break;
  default:
    return 0;
  }

  while (offset < size) {
    no = dh_table[offset] & 0x0f;
    type = (dh_table[offset] >> 4) & 0x01;
    printf("    %s table, no is %d\n", (type==0) ? "DC" : "AC", no);
    offset++;

    if (type == 0) {
      ji->DC_Huff_Table[no] = malloc(sizeof(HUFFMAN_TABLE));
      if (ji->DC_Huff_Table[no] == NULL) return 0;

      huffsize = ji->DC_Huff_Table[no]->codesize;
      huffcode = ji->DC_Huff_Table[no]->codeword;
      huffval  = ji->DC_Huff_Table[no]->symbols;
      huffmax  = ji->DC_Huff_Table[no]->maxcode;
      huff1st  = ji->DC_Huff_Table[no]->firstdata;
    } else {
      ji->AC_Huff_Table[no] = malloc(sizeof(HUFFMAN_TABLE));
      if (ji->AC_Huff_Table[no] == NULL) return 0;

      huffsize = ji->AC_Huff_Table[no]->codesize;
      huffcode = ji->AC_Huff_Table[no]->codeword;
      huffval  = ji->AC_Huff_Table[no]->symbols;
      huffmax  = ji->AC_Huff_Table[no]->maxcode;
      huff1st  = ji->AC_Huff_Table[no]->firstdata;
    }

    printf("    codesize count: \n");
    printf("    ");
    for (count = 0, i = 0, n = 0; n < 16; n++, offset++) {
      tmp = dh_table[offset];
      while (tmp) {
        huffsize[i++] = n+1;
        tmp--;
      }
      printf(" %d", dh_table[offset]);
      count += dh_table[offset];
    }
    huffsize[i] = 0; // terminate

    printf("\n     huffsize: \n");
    for (n = 0; n < 256; n++) {
      if (huffsize[n] == 0)
          break;
      if ((n % 16) == 0)
        printf("    ");
      printf(" %d", huffsize[n]);
      if ((n % 16) == 15)
        printf("\n");
    }

    /* generate the Huffman codes */
    memset(huffmax, -1, sizeof(long)*16); /* -1 if no codes of this length */
    memset(huff1st, -1, sizeof(long)*16);
    code = 0;
    si = huffsize[0];
    i = 0;
    while (huffsize[i]) {
      if (huffsize[i] == si) {
        huff1st[si-1] = i; // index of the first code in each codesize
      }
      while ((huffsize[i]) == si) {
        huffcode[i++] = code;
        huffmax[si-1] = code;
        code++;
      }
      code <<= 1;
      si++;
    }

    printf("\n     huffcode: \n");
    for (n = 0; n < count; n++) {
      if ((n % 8) == 0)
        printf("    ");
      printf(" %d", huffcode[n]);
      if ((n % 8) == 7)
        printf("\n");
    }

    printf("\n     maxcode:\n     ");
    for (n = 0; n < 16; n++)
      printf(" %d", (int)huffmax[n]);
    printf("\n     index of firstdata:\n     ");
    for (n = 0; n < 16; n++)
      printf(" %d", (int)huff1st[n]);

    printf("\n    total symbol is %d, codeword: \n", count);
    for (n = 0; n < count; n++, offset++) {
      if ((n % 16) == 0)
        printf("    ");
      huffval[n] = dh_table[offset];
      printf(" %d", huffval[n]);
      if ((n % 16) == 15)
        printf("\n");
    }
    if (count % 16)
        printf("\n");
  }

  printf("\n");
  free(dh_table);
  return 1;
}

int ParseStartofFrame (JPEG_INFO *ji)
{
  int size = 0;
  unsigned char *sof_buf;
  int n, offset = 0;
  int round_width, round_height, round_linesize;

  if (!ReadBits(ji, 16, &size))
    return 0;
  printf("    Start of Frame size is %d bytes, 0x%x\n", size, size);

  size -= 2;
  sof_buf = malloc(size);

  // run out left bits
  switch (ji->DataStream.leftbits) {
  case 0:
    if (!getnbyte(ji->f, sof_buf, size))
      return 0;
    break;
  case 8:
    sof_buf[0] = (ji->DataStream.data >> 8) & 0xff;
    if (!getnbyte(ji->f, &sof_buf[1], size-1))
      return 0;
    ji->DataStream.leftbits = ji->DataStream.data = 0;
    break;
  default:
    return 0;
  }

  // Sample precision
  printf("    Sample precision is %d bits\n", sof_buf[offset++]);

  // image width and height
  ji->Height = (sof_buf[offset] << 8) | sof_buf[offset+1];
  offset += 2;
  ji->Width  = (sof_buf[offset] << 8) | sof_buf[offset+1];
  offset += 2;
  printf("    image width is %d, height is %d\n", ji->Width, ji->Height);

  // Number of components
  ji->components = sof_buf[offset++];
  printf("    %d components\n", ji->components);

  // allocate buffer to store conponent infomation
  ji->CompInfo = malloc(ji->components*sizeof(COMPONENT_INFO));
  if (ji->CompInfo == NULL)
    return 0;
  ji->LastDC = malloc(ji->components*sizeof(int));
  if (ji->LastDC == NULL)
    return 0;
  memset(ji->LastDC, 0, ji->components*sizeof(int));

  // account how many blocks in a MCU
  ji->MCU_Blocks = 0;

  for (n = 0; n < ji->components; n++) {
    switch (sof_buf[offset]) {
    case 1:
      printf("      Y: ");
      break;
    case 2:
      printf("     Cb: ");
      break;
    case 3:
      printf("     Cr: ");
      break;
    }
    offset++;

    // horizontal and vertical sampling
    ji->CompInfo[n].horz_sampling = (sof_buf[offset]>>4) & 0x0f;
    ji->CompInfo[n].vert_sampling = sof_buf[offset] & 0x0f;
    printf("horizontal sampling is %d, vertical sampling is %d\n",
           ji->CompInfo[n].horz_sampling, ji->CompInfo[n].vert_sampling);
    offset++;

    // MCU blocks
    ji->MCU_Blocks += (ji->CompInfo[n].horz_sampling * ji->CompInfo[n].vert_sampling);

    // quantization table idenfifier
    ji->CompInfo[n].quan_table_id = sof_buf[offset];
    printf("         quantization table idenfifier is %d\n", ji->CompInfo[n].quan_table_id);
    offset++;
  }

  printf("\n");
  free(sof_buf);

  // caculate mcu variables
  ji->MCU_Width = ji->CompInfo[0].horz_sampling / ji->CompInfo[1].horz_sampling * 8;
  ji->MCU_Height = ji->CompInfo[0].vert_sampling / ji->CompInfo[1].vert_sampling * 8;
  ji->Row_MCUs = div_round_up(ji->Width, ji->MCU_Width);
  //printf("MCU_Width=%d, MCU_Height=%d, MCUs in row=%d\n\n",
  //       ji->MCU_Width, ji->MCU_Height, ji->Row_MCUs);

  // allocate image buffer
  round_width = round_up(ji->Width, 2);
  round_height = round_up(ji->Height, 2);
  round_linesize = round_up(ji->Width*3, 4);
  n = (round_width * round_height) /
      ((ji->CompInfo[0].horz_sampling/ji->CompInfo[1].horz_sampling) *
       (ji->CompInfo[0].vert_sampling/ji->CompInfo[1].vert_sampling));
  size = round_width * round_height + 2 * n;
  ji->Y_Space  = malloc(size);
  ji->Cb_Space = ji->Y_Space + (round_width * round_height);
  ji->Cr_Space = ji->Cb_Space + n;
  ji->RGB_Image  = malloc(round_linesize * ji->Height);

  if (ji->Y_Space == NULL || ji->RGB_Image == NULL)
    return 0;

  return 1;
}

int ParseStartofScan (JPEG_INFO *ji)
{
  int size = 0;
  unsigned char *sos_buf;
  int n, offset = 0;
  int component;

  if (!ReadBits(ji, 16, &size))
    return 0;
  printf("    Start of Scan size is %d bytes\n", size);

  size -= 2;
  sos_buf = malloc(size);

  // run out left bits
  switch (ji->DataStream.leftbits) {
  case 0:
    if (!getnbyte(ji->f, sos_buf, size))
      return 0;
    break;
  case 8:
    sos_buf[0] = (ji->DataStream.data >> 8) & 0xff;
    if (!getnbyte(ji->f, &sos_buf[1], size-1))
      return 0;
    ji->DataStream.leftbits = ji->DataStream.data = 0;
    break;
  default:
    return 0;
  }

  // component count
  component = sos_buf[offset];
  offset++;

  // component descriptor
  for (n = 0; n < component; n++) {
    switch (sos_buf[offset]) {
    case 1:
      printf("      Y: ");
      break;
    case 2:
      printf("     Cb: ");
      break;
    case 3:
      printf("     Cr: ");
      break;
    }
    offset++;

    // DC and AC Huffman table
    ji->CompInfo[n].dc_table_id = (sos_buf[offset]>>4) & 0x0f;
    ji->CompInfo[n].ac_table_id = sos_buf[offset] & 0x0f;
    printf("Huffman table: DC is %d, AC is %d\n",
           ji->CompInfo[n].dc_table_id, ji->CompInfo[n].ac_table_id);
    offset++;
  }

  // spectral selection start and end
  printf("    Spectral selection start is %d, end is %d\n",
         sos_buf[offset], sos_buf[offset+1]);
  offset += 2;

  // successive approximation
  printf("    Successive approximation is (%d, %d)\n\n",
         ((sos_buf[offset]>>4)&0x0f), (sos_buf[offset]&0x0f));

  free(sos_buf);
  return 1;
}

int ParseAppMarker (JPEG_INFO *ji)
{
  int size = 0;
  unsigned char *app_buf;

  if (!ReadBits(ji, 16, &size))
    return 0;
  printf("    Application Marker size is %d bytes\n", size);

  size -= 2;
  app_buf = malloc(size);
  if (app_buf == NULL)
    return 0;

  // skip the content of application marker
  if (!getnbyte(ji->f, app_buf, size))
    return 0;

  free(app_buf);
  return 1;
}

//
#define DCTSIZE     8
#define PASS1_BITS  2
void iDCT_Quan (JPEG_INFO *ji, int cid, int *dct, unsigned int *img)
{
  unsigned char quantab[64];
  unsigned char *quantptr;
  unsigned char *RangeLimit = ji->Range_Limit + (256+128);
  int *dctptr;
  unsigned int *imgptr;
  int *tptr;
  int tmpbuf[64];
  int m[8], n[8];
  int i;

  if (cid == 0)
    quantptr = ji->Quan_Table[0]->quan_data;
  else
    quantptr = ji->Quan_Table[1]->quan_data;

  if (quantptr == NULL) {
    memset(quantab, 1, sizeof(quantab));
  } else {
    for (i = 0; i < 64; i++)
      quantab[natural_order[i]] = quantptr[i];
  }
  quantptr = quantab;

  /*printf("quant table\n");
  for (i = 0; i < 64; i++) {
    if ((i % 8) == 0)
      printf("    ");
    printf(" %d", quantab[i]);
    if ((i % 8) == 7)
      printf("\n");
  } */

  dctptr = dct;
  imgptr = img;
  tptr = tmpbuf;

  /* Pass 1: process columns from input, store into work array. */
  for (i = 0; i < DCTSIZE; i++) {

    m[0] = (dctptr[DCTSIZE*0] * quantptr[DCTSIZE*0]) << PASS1_BITS;
    m[1] = (dctptr[DCTSIZE*1] * quantptr[DCTSIZE*1]) << PASS1_BITS;
    m[2] = (dctptr[DCTSIZE*2] * quantptr[DCTSIZE*2]) << PASS1_BITS;
    m[3] = (dctptr[DCTSIZE*3] * quantptr[DCTSIZE*3]) << PASS1_BITS;
    m[4] = (dctptr[DCTSIZE*4] * quantptr[DCTSIZE*4]) << PASS1_BITS;
    m[5] = (dctptr[DCTSIZE*5] * quantptr[DCTSIZE*5]) << PASS1_BITS;
    m[6] = (dctptr[DCTSIZE*6] * quantptr[DCTSIZE*6]) << PASS1_BITS;
    m[7] = (dctptr[DCTSIZE*7] * quantptr[DCTSIZE*7]) << PASS1_BITS;

    iDCT(m, n);

    tptr[DCTSIZE*0] = n[0];
    tptr[DCTSIZE*1] = n[1];
    tptr[DCTSIZE*2] = n[2];
    tptr[DCTSIZE*3] = n[3];
    tptr[DCTSIZE*4] = n[4];
    tptr[DCTSIZE*5] = n[5];
    tptr[DCTSIZE*6] = n[6];
    tptr[DCTSIZE*7] = n[7];

    dctptr++;			/* advance pointers to next column */
    quantptr++;
    tptr++;
  }
  /*printf("Work space, component %d:\n", cid);
  for (i = 0; i < 64; i++) {
    if ((i % 8) == 0)
      printf("    ");
    printf(" %d", tmpbuf[i]);
    if ((i % 8) == 7)
      printf("\n");
  }*/

  /* Pass 2: process rows from work array, store into output array. */
  tptr = tmpbuf;

  for (i = 0; i < DCTSIZE; i++) {

    iDCT(tptr, n);

    imgptr[0] = RangeLimit[n[0]>>5]; //3+PASS1_BITS=5
    imgptr[1] = RangeLimit[n[1]>>5];
    imgptr[2] = RangeLimit[n[2]>>5];
    imgptr[3] = RangeLimit[n[3]>>5];
    imgptr[4] = RangeLimit[n[4]>>5];
    imgptr[5] = RangeLimit[n[5]>>5];
    imgptr[6] = RangeLimit[n[6]>>5];
    imgptr[7] = RangeLimit[n[7]>>5];

    /* printf("xxxxx: %d %d %d %d %d %d %d %d\n",
           n[0], n[1], n[2], n[3], n[4], n[5], n[6], n[7]); */

    tptr += DCTSIZE;		/* advance pointer to next row */
    imgptr += DCTSIZE;
  }

  /*printf("Sample values, component %d:\n", cid);
  for (i = 0; i < 64; i++) {
    if ((i % 8) == 0)
      printf("    ");
    printf(" %d", img[i]);
    if ((i % 8) == 7)
      printf("\n");
  }*/
}

void Fill_Y_Plane (JPEG_INFO *ji, unsigned int *img, int x, int y)
{
  int round_width, round_height;
  int m, n;
  unsigned char *pY;
  unsigned int *pdata;

  round_width = round_up(ji->Width, 2);
  round_height = round_up(ji->Height, 2);
  pY = &ji->Y_Space[y*round_width+x];
  //printf("Fill Y: x=%d, y=%d\n", x, y);

  for (n = 0; n < 8; n++) {
    if (y+n >= round_height) break;
    pdata = img + (n * 8);
    for (m = 0; m < 8; m++) {
      if (x+m >= round_width) break;
      pY[m] = (unsigned char)pdata[m];
    }
    pY += round_width;
  }
}

void Fill_Cb_Plane (JPEG_INFO *ji, unsigned int *img, int x, int y)
{
  unsigned char *pCb;
  int n, m, w, h;
  unsigned int *pdata;

  w = round_up(ji->Width, 2);
  h = round_up(ji->Height, 2);

  if ((ji->CompInfo[0].horz_sampling / ji->CompInfo[1].horz_sampling) == 2) {
    x >>= 1;
    w >>= 1;
  }
  if ((ji->CompInfo[0].vert_sampling / ji->CompInfo[1].vert_sampling) == 2) {
    y >>= 1;
    h >>= 1;
  }
  //printf("Fill Cb: x=%d, y=%d, w=%d\n", x, y, w);

  pCb = &ji->Cb_Space[y*w+x];
  for (n = 0; n < 8; n++) {
    if (y+n >= h) break;
    pdata = img + (n * 8);
    for (m = 0; m < 8; m++) {
      if (x+m >= w) break;
      pCb[m] = (unsigned char)pdata[m];
    }
    pCb += w;
  }
}

void Fill_Cr_Plane (JPEG_INFO *ji, unsigned int *img, int x, int y)
{
  unsigned char *pCr;
  int n, m, w, h;
  unsigned int *pdata;

  w = round_up(ji->Width, 2);
  h = round_up(ji->Height, 2);

  if ((ji->CompInfo[0].horz_sampling / ji->CompInfo[1].horz_sampling) == 2) {
    x >>= 1;
    w >>= 1;
  }
  if ((ji->CompInfo[0].vert_sampling / ji->CompInfo[1].vert_sampling) == 2) {
    y >>= 1;
    h >>= 1;
  }
  //printf("Fill Cr: x=%d, y=%d, w=%d\n", x, y, w);

  pCr = &ji->Cr_Space[y*w+x];
  for (n = 0; n < 8; n++) {
    if (y+n >= h) break;
    pdata = img + (n * 8);
    for (m = 0; m < 8; m++) {
      if (x+m >= w) break;
      pCr[m] = (unsigned char)pdata[m];
    }
    pCr += w;
  }
}

int Decode_MCU (JPEG_INFO *ji, int mcu_no)
{
  int dct_coff[64];
  unsigned int *img_block;
  unsigned int *img_ptr;
  int h_offset, v_offset;
  int h, v, cid = 0;
  //int n, blk = 0;

  //printf("\nDecode_MCU entered, no=%d\n", mcu_no);
  //printf("---------------------- decode_mcu() %d ----------------------\n", mcu_no);
  //printf("3 blocks in MCU\n\n");

  img_ptr = img_block = malloc(ji->MCU_Blocks*64*sizeof(int));
  if (img_block == NULL)
    return 0;

  while (cid < 3) { // 3 components, Y, Cb, Cr

    v_offset = (mcu_no / ji->Row_MCUs) * ji->MCU_Height;

    for (v = 0; v < ji->CompInfo[cid].vert_sampling; v++) {
      //if (v_offset > ji->Height) // if vertical offset is out of range
      //  break;                   // then skip MCU process ???

      h_offset = (mcu_no % ji->Row_MCUs) * ji->MCU_Width;

      for (h = 0; h < ji->CompInfo[cid].horz_sampling; h++) {
        // if horizontal offset is out of range
        // then continue process MCU block ???

        memset(dct_coff, 0, sizeof(dct_coff));

        if (!Decode_DC(ji, cid, dct_coff))
          return 0;

        if (!Decode_AC(ji, cid, &dct_coff[1]))
          return 0;

        //printf("DCT coff, component %d, dc=%d, ac=%d:\n",
        //       cid, ji->CompInfo[cid].dc_table_id, ji->CompInfo[cid].ac_table_id);
        /*printf("DCT coff, block %d:\n", blk);
        for (n = 0; n < 64; n++) {
          //if ((n % 8) == 0) printf("    ");
          if ((n % 8) == 7)
            printf("%d\n", dct_coff[n]);
          else
            printf("%d ", dct_coff[n]);
        }
        if (blk != 2)
        printf("\n");*/

        iDCT_Quan(ji, cid, dct_coff, img_ptr);

        switch (cid) {
        case 0: // fill Y data
          Fill_Y_Plane(ji, img_ptr, h_offset, v_offset);
          break;
        case 1:
          Fill_Cb_Plane(ji, img_ptr, h_offset, v_offset);
          break;
        case 2:
          Fill_Cr_Plane(ji, img_ptr, h_offset, v_offset);
          break;
        }

        img_ptr += 64;
        h_offset += 8;

        //blk++;
      }

      v_offset += 8;
    }

    cid++;
  }

  free(img_block);

  return 1;
}

void prepare_range_limit_table (JPEG_INFO *ji)
{
  unsigned char * table;
  unsigned char * rangelimit;
  int i, size;

  size = 1408; //5 * 256 + 128;
  ji->Range_Limit = malloc(size);
  table = ji->Range_Limit;

  table += 256;	/* allow negative subscripts of simple table */
  rangelimit = table;

  /* First segment of "simple" table: limit[x] = 0 for x < 0 */
  memset(table-256, 0, 256);
  /* Main part of "simple" table: limit[x] = x */
  for (i = 0; i <= 255; i++)
    table[i] = i;
  table += 128;	/* Point to where post-IDCT table starts */
  /* End of simple table, rest of first half of post-IDCT table */
  for (i = 128; i < 512; i++)
    table[i] = 255;
  /* Second half of post-IDCT table */
  memset(table + 512, 0, 384/*(512-128)*/);
  memcpy(table + 896/*(4*256-128)*/, rangelimit, 128);

  /*
  table -= (256+128);
  for (i = 0; i < size; i++) {
    if ((i % 16) == 0)
      printf("    ");
    printf(" %d", table[i]);
    if ((i % 16) == 15)
      printf("\n");
  }
  printf("size = %d        ---------------- \n", size);
  */
}

void ReleaseAllocateMemory (JPEG_INFO *ji_first)
{
  JPEG_INFO *ji;
  int n;

  ji = ji_first;

  while (ji != NULL) {
    if (ji->CompInfo)
      free(ji->CompInfo);
    if (ji->LastDC)
      free(ji->LastDC);

    for (n = 0; n < ji->components; n++) {
      if (ji->DC_Huff_Table[n])
        free(ji->DC_Huff_Table[n]);
      if (ji->AC_Huff_Table[n])
        free(ji->AC_Huff_Table[n]);
      if (ji->Quan_Table[n])
        free(ji->Quan_Table[n]);
    }

    if (ji->Y_Space)  free(ji->Y_Space);
    if (ji->RGB_Image)  free(ji->RGB_Image);
    if (ji->Range_Limit) free(ji->Range_Limit);

    // erase all
    ji->CompInfo = NULL;
    ji->LastDC = NULL;
    ji->Y_Space = NULL;
    ji->RGB_Image = NULL;
    ji->Range_Limit = NULL;
    for (n = 0; n < ji->components; n++) {
      ji->DC_Huff_Table[n] = NULL;
      ji->AC_Huff_Table[n] = NULL;
      ji->Quan_Table[n] = NULL;
    }

    ji = (JPEG_INFO *)ji->next;
  }
}

int AllocateNexJpegInfo (JPEG_INFO **ji)
{
  JPEG_INFO *ji_new;

  ji_new = malloc(sizeof(JPEG_INFO));
  if (ji_new == NULL) {
    printf("Memory Insufficient!\n");
    return 0;
  }
  memset(ji_new, 0, sizeof(JPEG_INFO));

  ji_new->f = (*ji)->f;
  ji_new->Range_Limit = (*ji)->Range_Limit;
  (*ji)->next = (void *)ji_new;
  *ji = ji_new;

  return 1;
}

int jpeg_decode (char * filename, char **imgbuf, int *pW, int *pH)
{
  JPEG_INFO *ji;
  JPEG_INFO *ji_first, *ji_show;
  unsigned char buf[2];
  int DecodeStart = 0;
  int ShowPicture = 0;
  int Total_MCU = 0;
  int r, n, data;
  unsigned char *image_data = NULL;

  // allocate jpeg information structure
  ji = malloc(sizeof(JPEG_INFO));
  if (ji == NULL) {
    printf("Memory Insufficient!\n");
    return 0;
  }
  memset(ji, 0, sizeof(JPEG_INFO));
  ji_first = ji;

  if ((ji->f = fopen(filename, "rb")) == NULL) {
    printf("The file %s was not opened\n", filename);
    return 0;
  }

  if (!get2byte(ji->f, buf))
    goto file_failed;

  if (buf[0] != 0xff || buf[1] != SOI) {
    printf("This file is not a JPEG file!\n");
    goto file_failed;
  }
  printf("SOI marker\n");

  //
  prepare_range_limit_table(ji);
  create_yuv_table();

  while (1) {
    if (!ReadBits(ji, 8, &data))
      goto file_failed;

    if ((data & 0xff) != 0xff)
      continue;

    if (!ReadBits(ji, 8, &data))
      goto file_failed;

    switch (data & 0xff) {
    case SOI:
      printf("SOI marker\n");
      break;
    case APP0:
      printf("APP0 marker\n");
      r = ParseAppMarker(ji);
      break;
    case APP1:
      printf("APP1 marker\n");
      r = ParseAppMarker(ji);
      break;
    case APP2:
      printf("APP2 marker\n");
      r = ParseAppMarker(ji);
      break;
    case DQT:
      printf("DQT marker\n");
      r = ParseQuantTable(ji);
      break;
    case DHT:
      printf("DHT marker\n");
      r = ParseHuffmanTable(ji);
      break;
    case SOF:
      printf("SOF marker\n");
      r = ParseStartofFrame(ji);
      if (r) {
        Total_MCU = div_round_up(ji->Width, (8*ji->CompInfo[0].horz_sampling)) *
                    div_round_up(ji->Height, (8*ji->CompInfo[0].vert_sampling));
      }
      break;
    case DRI:
      printf("DRI marker\n"); // restart interval
      data = 0;
      r = ReadBits(ji, 16, &data); // currently we skip restart marker RST(0-7).
      if (data == 4) {
        r = ReadBits(ji, 16, &data);
        ji->Restart_Interval = data;
        printf("    Restart interval is %d\n\n", ji->Restart_Interval);
      } else {
        printf("Bad DRI marker!, program stop.\n");
        r = 0; // data invalidate
      }
      break;
    case SOS:
      printf("SOS marker\n");
      r = ParseStartofScan(ji);
      DecodeStart = 1;
      ShowPicture = 0;
      break;
    case EOI:
      printf("EOI marker\n");
      r = AllocateNexJpegInfo(&ji);
      ShowPicture = 1;
      break;
    default:
      if (buf[0] != 0)
        printf("???: 0x%x\n", (data & 0xff));
      break;
    }

    if (!r)
      goto file_failed;

    if (DecodeStart) {
      //printf("Total_MCU=%d\n", Total_MCU);
      for (n = 0; n < Total_MCU; n++) {
        if (ji->Restart_Interval != 0 && n != 0 &&
            (n % ji->Restart_Interval) == 0) { // restart marker
          // ignore current bits in data stream
          ji->DataStream.data >>= (16-ji->DataStream.leftbits);
          ji->DataStream.leftbits = 0;
          // skip restart marker
          if ((ji->DataStream.data & 0xFF) == 0xFF)
            get1byte(ji->f, buf);
          else
            get2byte(ji->f, buf);
          ji->DataStream.data = 0;
          // reset last DC value
          memset(ji->LastDC, 0, ji->components*sizeof(int));
        }

        if (!Decode_MCU(ji, n))
          goto file_failed;
      }

      n = ji->CompInfo[0].horz_sampling / ji->CompInfo[1].horz_sampling;
      r = ji->CompInfo[0].vert_sampling / ji->CompInfo[1].vert_sampling;

      // color space transfer
      if (n == 2 && r == 2)
        yuv411torgb(ji->Y_Space, ji->RGB_Image, ji->Width, ji->Height);
      else if (n == 2 && r == 1)
        yuv422torgb(ji->Y_Space, ji->RGB_Image, ji->Width, ji->Height);
      else if (n == 1 && r == 1)
        yuv444torgb(ji->Y_Space, ji->RGB_Image, ji->Width, ji->Height);

      // clear no used bits
      if (ji->DataStream.leftbits > 0) {
          n = ji->DataStream.leftbits % 8;
          if (!ReadBits(ji, n, &data))
            goto file_failed;
      }

      DecodeStart = 0;
    }
  }

file_failed:

  if (ShowPicture) {
    // pick up the biggest picture to display
    ji_show = ji = ji_first;
    while (ji->next) {
      if (ji->Width < ((JPEG_INFO *)(ji->next))->Width ||
          ji->Height < ((JPEG_INFO *)(ji->next))->Height)
        ji_show = (JPEG_INFO *)ji->next;
      ji = (JPEG_INFO *)ji->next;
    }

    if (image_data != NULL)
      free(image_data);
    image_data = malloc(ji_show->Width*ji_show->Height*3);
    {
      int x, y;
      int line_length = round_up(ji_show->Width*3, 4);
      unsigned char *pSrc = ji_show->RGB_Image;
      unsigned char *pDst = image_data;
      pSrc += ((ji_show->Height-1) * line_length);
      for (y = ji_show->Height-1; y >= 0 ;y--) {
        for (x = 0; x < ji_show->Width; x++) {
          *pDst++ = pSrc[x*3+2]; // Blue
          *pDst++ = pSrc[x*3+1]; // Green
          *pDst++ = pSrc[x*3];   // Red
        }
        pSrc -= line_length;
      }
    }
    *imgbuf = (char *)image_data;
    *pW = ji_show->Width;
    *pH = ji_show->Height;
  }

  fclose(ji->f);
  ReleaseAllocateMemory(ji);

  return 1;
}
