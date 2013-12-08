/* JPEG marker codes */ 
#define TEM  0x01 
#define SOF  0xc0 
#define DHT  0xc4 
#define JPGA 0xc8 
#define DAC  0xcc 
#define RST  0xd0 
#define SOI  0xd8 
#define EOI  0xd9 
#define SOS  0xda 
#define DQT  0xdb 
#define DNL  0xdc 
#define DRI  0xdd 
#define DHP  0xde 
#define EXP  0xdf 
#define APP0 0xe0 
#define APP1 0xe1
#define APP2 0xe2
#define JPG  0xf0 
#define COM  0xfe 


/* Global variables */
typedef struct {
  unsigned short data;
  int leftbits;
  int isFFexist;
} STREAM_STATUS;

typedef struct {
  unsigned char horz_sampling;
  unsigned char vert_sampling;
  unsigned char quan_table_id;
  unsigned char dc_table_id;
  unsigned char ac_table_id;
} COMPONENT_INFO;

typedef struct {
  unsigned short codesize[256];
  unsigned short codeword[256];
  unsigned short symbols[256];
  unsigned long maxcode[16];
  unsigned long firstdata[16];
} HUFFMAN_TABLE;

typedef struct {
  unsigned char quan_data[64];
} QUAN_TABLE;

typedef struct {  
  // Huffman table
  HUFFMAN_TABLE *DC_Huff_Table[3];
  HUFFMAN_TABLE *AC_Huff_Table[3];
  
  // Quantization Table
  int           quan_Tables;
  QUAN_TABLE    *Quan_Table[3];
  
  // Components in MCU
  int components;
  COMPONENT_INFO *CompInfo;
  
  // MCU information
  int           MCU_Blocks;
  int           Row_MCUs;
  int           MCU_Width;
  int           MCU_Height;
  
  // Last DC coff value
  int           *LastDC; 
  
  // DRI marker, restart interval
  int           Restart_Interval;
  
  // picture area
  int           Width;
  int           Height;
  
  // image space
  unsigned char *Y_Space;
  unsigned char *Cb_Space;
  unsigned char *Cr_Space;
  unsigned char *RGB_Image;
  
  // image value range limit
  unsigned char *Range_Limit;
  
  FILE          *f;
  void *        next;

  STREAM_STATUS DataStream;

} JPEG_INFO;

