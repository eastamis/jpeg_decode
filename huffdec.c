#include <stdio.h>
#include <stdlib.h>
#include "jpginfo.h"

/* main.c */
extern const int natural_order[64];

/* readbits.c */
extern int ReadBits(JPEG_INFO *ji, int count, int *value);


int Huffman_Decoder (JPEG_INFO *ji, int isDC, int cpno, int *value)
{
  unsigned short *huffcode;
  unsigned short *huffval;
  unsigned long *huffmax;
  unsigned long *huff1st;
  unsigned long mincode;
  int code, code_len, index;
  int bits;
  
  if (isDC == 1) {
    index = ji->CompInfo[cpno].dc_table_id;
    huffcode = ji->DC_Huff_Table[index]->codeword;
    huffval  = ji->DC_Huff_Table[index]->symbols;
    huffmax  = ji->DC_Huff_Table[index]->maxcode;
    huff1st  = ji->DC_Huff_Table[index]->firstdata;
  } else {
    index = ji->CompInfo[cpno].ac_table_id;
    huffcode = ji->AC_Huff_Table[index]->codeword;
    huffval  = ji->AC_Huff_Table[index]->symbols;
    huffmax  = ji->AC_Huff_Table[index]->maxcode;
    huff1st  = ji->AC_Huff_Table[index]->firstdata;
  }
  
  code = 0;
  code_len = 0;
  while (code_len < 16) {
    if (!ReadBits(ji, 1, &bits)) return 0;
    code <<= 1;
    code |= bits;
    code_len++;
    
    if (code <= (int)huffmax[code_len-1]) {
      mincode = huffcode[huff1st[code_len-1]];
      index = huff1st[code_len-1] + code - mincode;
      *value = huffval[index];
      //printf("Huffman decode: value=0x%x [0x%x]----------\n", *value, code);
      return 1;
    }
  }
  
  printf("Huffman_Decoder failed!!!\n");
  // Error if get here
  return 0;
}

int Extend (int additional, int magnitude)
{
  int vt, value;
  
  vt = 1 << (magnitude - 1);
  if (additional < vt)
    value = additional + (-1 << magnitude) + 1;
  else
    value = additional;
      
  return value;  
}

int Decode_DC (JPEG_INFO *ji, int cpno, int *dc)
{
  int code, bits, difference;
  
  if (!Huffman_Decoder(ji, 1, cpno, &code))
    return 0;
  
  if (!ReadBits(ji, code, &bits))
    return 0;
  
  difference = Extend(bits, code);
  *dc = difference + ji->LastDC[cpno];
  ji->LastDC[cpno] = *dc;
  
  //printf("component %d, DC value = %d\n", cpno, ji->LastDC[cpno]);
  
  return 1;
}

int Decode_AC (JPEG_INFO *ji, int cpno, int *ac_coff)
{
  int n, code, bits;
  unsigned char run, level;
  
  for (n = 1; n <= 63; n++) {
    if (!Huffman_Decoder(ji, 0, cpno, &code))
      return 0;
    run = (code >> 4) & 0x0f;
    level = code & 0x0f;
    //printf("AC: run=%d, level=%d - %dth\n", run, level, n);
    
    if (level != 0) {
      if (!ReadBits(ji, level, &bits))
        return 0;
      n += run;
      ac_coff[natural_order[n]-1] = Extend(bits, level);
      //printf("AC: coff[%d]=%d.\n", n, ac_coff[natural_order[n]-1]);
    } else {
      if (run == 15)
        n += 15;
      else if (run == 0)
        n = 64;  
    }
  }
  
  return 1;
}

