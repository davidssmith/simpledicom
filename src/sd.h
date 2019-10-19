#ifndef _SIMPLE_DICOM_H
#define _SIMPLE_DICOM_H

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sysexits.h>
#include <unistd.h>


// MAGIC NUMBERS for testing VR string equality
// Text VRs are UI, SH, LO, DA, DT, TM, CS, ST, LT, UT, AE, PN, AS,IS,DS
// Binary VRs: SS SL US UL FL FD OB OW OF AT
// Other: SQ UN
#define VR_AE 0x4541
#define VR_AS 0x5341
#define VR_AT 0x5441
#define VR_CS 0x5343
#define VR_DA 0x4144
#define VR_DS 0x5344
#define VR_DT 0x5444
#define VR_FD 0x4446
#define VR_FL 0x4c46
#define VR_IS 0x5349
#define VR_LO 0x4f4c
#define VR_LT 0x544c
#define VR_OB 0x424f
#define VR_OD 0x444f
#define VR_OF 0x464f
#define VR_OW 0x574f
#define VR_PN 0x4e50
#define VR_SH 0x4853
#define VR_SL 0x4c53
#define VR_SQ 0x5153
#define VR_SS 0x5353
#define VR_ST 0x5453
#define VR_TM 0x4d54
#define VR_UI 0x4955
#define VR_UL 0x4c55
#define VR_UN 0x4e55
#define VR_US 0x5355
#define VR_UT 0x5455

const uint32_t ITEM_START    = 0xe000fffeUL;
const uint32_t ITEM_STOP  = 0xe00dfffeUL;
const uint32_t SEQ_STOP   = 0xe0ddfffeUL;

//#define _pop4(x)  *((uint32_t*)(x));(x)+=4;
//#define _pop2(x)  *((uint16_t*)(x));(x)+=2;

union VR {
	uint16_t w;
	char c[2];
};


union tag {
	uint32_t t;
	struct { 
		uint16_t group;
		uint16_t element;
	} ge;
};

union quad {
	uint32_t q;
	uint16_t w[2];
	char c[4];
};


uint16_t pop2 (char **x); 
uint32_t pop4 (char **x);
void _errchk (const int ret, const int errval);
void print_vr_magics ();
int is_big_vr (char *VR);
int is_sequence (const char *VR);
int is_binary_vr (char *VR);
int is_vector_vr (char *VR);
void print_de_value (char* data, char *VR, uint32_t length);
size_t seek_undefined (char *data, uint16_t stop_code);
//char * print_seq (char *data, uint32_t length);
char * next_tag_explicit (char *data);
char * parse_data_set(char *data, size_t size, int level);


#endif
