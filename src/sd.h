#ifndef _SIMPLE_DICOM_H
#define _SIMPLE_DICOM_H

#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <unistd.h>
#include "dict.h"


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

#if 0
#define VR_AE_BIT 1UL<<0
#define VR_AS_BIT 1UL<<1
#define VR_AT_BIT 1UL<<2
#define VR_CS_BIT 1UL<<3
#define VR_DA_BIT 1UL<<4
#define VR_DS_BIT 1UL<<5
#define VR_DT_BIT 1UL<<6
#define VR_FD_BIT 1UL<<7
#define VR_FL_BIT 1UL<<8
#define VR_IS_BIT 1UL<<9
#define VR_LO_BIT 1UL<<10
#define VR_LT_BIT 1UL<<11
#define VR_OB_BIT 1UL<<12
#define VR_OD_BIT 1UL<<13
#define VR_OF_BIT 1UL<<14
#define VR_OW_BIT 1UL<<15
#define VR_PN_BIT 1UL<<16
#define VR_SH_BIT 1UL<<17
#define VR_SL_BIT 1UL<<18
#define VR_SQ_BIT 1UL<<19
#define VR_SS_BIT 1UL<<20
#define VR_ST_BIT 1UL<<21
#define VR_TM_BIT 1UL<<22
#define VR_UI_BIT 1UL<<23
#define VR_UL_BIT 1UL<<24
#define VR_UN_BIT 1UL<<25
#define VR_US_BIT 1UL<<26
#define VR_UT_BIT 1UL<<27
#endif


//typedef uint16_t u16;
//typedef uint32_t u32;

/*
union tag {
	uint32_t tag;
	struct { uint16_t g, e; };
};
*/


const uint32_t ITEM_START    = 0xe000fffe;
const uint32_t ITEM_STOP  = 0xe00dfffe;
const uint32_t SEQ_STOP   = 0xe0ddfffe;
const uint32_t SIZE_UNDEFINED = 0xffffffff;

#define _pop4(x)  *((uint32_t*)(x))
#define _pop2(x)  *((uint16_t*)(x))

char * parse_data_set(char *data, const size_t size, const int level);

#endif
