#include <stdio.h>
#include <stdlib.h>
#include "jpginfo.h"

int get1byte (FILE *f, unsigned char *data)
{
  int c;
  c = getc(f);
  if (c == EOF) {
    //printf("\nend of file\n");
    return 0;
  }
  *data = (unsigned char)c;
  return 1;
}

int get2byte (FILE *f, unsigned char *data)
{
  int n, r;
  for (n = 0; n < 2; n++) {
    r = get1byte(f, &data[n]);
    if (r == 0) break;
  }
  return n;
}

int getnbyte (FILE *f, unsigned char *data, int size)
{
  fread(data, size, 1, f);
  if (ferror(f)) {
    printf("fread was failed!!\n");
    return 0;
  }
  return 1;
}

int ReadBits (JPEG_INFO *ji, int count, int *value)
{
  unsigned char buf[2];
  unsigned long tmp = 0;
  int left = 0;
  
  //printf("ReadBits entered, leftbits=%d, need=%d .....\n", ji->DataStream.leftbits, count);
  
  if (count == 0) {
    *value = 0;
    return 1;
  }
  
  if (ji->DataStream.leftbits < count) {
    tmp = (unsigned long)ji->DataStream.data;
    
    if (!get2byte(ji->f, buf)) return 0;
    
    /* ----- SKIP FF 00 ------------------------ */
    if (ji->DataStream.isFFexist && buf[0] == 0) {
      buf[0] = buf[1];
      get1byte(ji->f, &buf[1]);
    }
    ji->DataStream.isFFexist = 0;
    
    if (buf[0] == 0xff) {
      if (buf[1] == 0)
        get1byte(ji->f, &buf[1]);
    }
    if (buf[1] == 0xff)
      ji->DataStream.isFFexist = 1;
    
    //printf("..... 0x%02x, 0x%02x.....\n", buf[0], buf[1]);
    
    ji->DataStream.data = (buf[0] << 8) | buf[1];
    /* ----------------------------------------- */
    
    if (ji->DataStream.leftbits > 0) {
      left = (tmp << count) >> 16;
      count -= ji->DataStream.leftbits;
    }
    
    ji->DataStream.leftbits = 16;
    tmp = 0;
  }
  
  tmp = (unsigned long)ji->DataStream.data;
  *value = left | ((tmp << count) >> 16);
  //printf("******** read bits=0x%x, count=%d\n", *value, count);
  
  ji->DataStream.leftbits -= count;
  ji->DataStream.data <<= count;
  
  return 1;  
}

