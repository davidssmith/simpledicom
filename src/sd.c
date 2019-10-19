/*
 * TODO: add implicit 
 * TODO: track nested level for indentation or indication
 * TODO: see if using int VR instead of char[2] is faster
 *
 */

#include "sd.h"
#include "dict.h"

#define dprint  if(DEBUG)printf

//#define BINARY_DICT_SEARCH 1

static int DEBUG = 0;
static int SKIP_PRIVATE = 0;


char *
dict_lookup (const uint16_t group, const uint16_t element)
{
	uint32_t tag = (group << 16) | element;
	//printf("tag=%x\n", tag);
	int min = 0, max = DICTSIZE;
	while (1) {
		int i = (min + max) / 2;
		//printf("min:%d max:%d i:%d\n", min, max, i);
		if (tag < dict[i].val)
			max = i;
		else if (tag > dict[i].val)
			min = i;	
		else 
			return dict[i].keyword;
		if (min + 1 == max)
			return NULL;
	}
	/* linear search
	for (int i = 0; i < DICTSIZE; ++i) {
		if (dict[i].val != tag)
			continue;
		return dict[i].keyword;
	}
	*/
	return NULL;  // either private or unknown
}


inline uint16_t
pop_u16 (char **x)   // TODO: see if macro faster
{
	uint16_t n = *(uint16_t*)*x;
	//dprint("D: pop_u16 %x\n", n);
	*x += 2;
	return n;
}

inline uint32_t
pop_u32 (char **x)
{
	uint32_t n = *(uint32_t*)*x;
	//dprint("D: pop_u32 %x\n", n);
	*x += 4;
	return n;
}


void 
_errchk (const int ret, const int errval)
{
	if (ret == errval)
		perror("simple_dicom: ");
}


void
print_vr_magics ()
{
	char all_vrs[] = "AEASSHLODADTTMCSSTLTUTPNISDSUISQSSUSSLULATFLFDOBOWOFUN";

	for (int i = 0; i < sizeof all_vrs - 1; i += 2) {
		int16_t n = *((int16_t*)(all_vrs + i));
		char a = all_vrs[i];
		char b = all_vrs[i+1];
		printf("#define VR_%c%c 0x%x\n", a, b, n);
	}
}

int
is_big_vr (char *VR)
{
	// VR is too big to print
	int16_t n = *((int16_t*)VR);  // cast char[2] to int16
	switch (n) {
		case VR_OB:
		case VR_OW:
		case VR_OF:
		case VR_UN:
		case VR_UT:
		case VR_SQ:
			return 1;
		default:
			return 0;
	}
}

int
is_vector_vr (char *VR)
{
	// VR can be more than one value
	int16_t n = *((int16_t*)VR);  // cast char[2] to int16
	switch (n) {
		case VR_OD:
		case VR_OF:
		case VR_OW:
		case VR_OB:
			return 1;
		default:
			return 0;
	}
}


int is_sequence (const char *VR) { return *((int16_t*)VR) == VR_SQ; }


int is_undefined (const int32_t length) { return length == 0xffffffff; }


int 
is_binary_vr (char *VR)
{
	/* Is VR something other than a string of chars */
	int16_t n = *((int16_t*)VR);  // cast char[2] to int16
	// TODO: BLoom filter or such for membership testing?
	switch (n) {
		case VR_SS:
		case VR_SL:
		case VR_US:
		case VR_UL:
		case VR_UN:
		case VR_FL:
		case VR_FD:
		case VR_OB:
		case VR_OW:
		case VR_OF:
		case VR_AT:
			return 1;
		default:
			return 0;
	}
}


int
is_float_vr (char *VR)
{
	int16_t n = *((int16_t*)VR);  // cast char[2] to int16
	switch (n) {
		case VR_FL:
		case VR_FD:
		case VR_OF:
		case VR_OD:
			return 1;
		default:
			return 0;
	}
}



void
print_data_element (char* data, uint16_t group, uint16_t element, char *VR, uint32_t length, int level)
{
	char *keyword = dict_lookup(group, element);
	if (SKIP_PRIVATE && keyword == NULL) // skip private tags
		return;
	//dprint("D: DE print length %08x at level %d\n", length, level);
	printf("%*s", 4*level, " ");
	printf("(%04x,%04x) %c%c %-40s [", group, element, VR[0], VR[1], 
			keyword != NULL ? keyword : "Private");
	int16_t nVR = *((int16_t*)VR);  // cast char[2] to int16 for matching
	int veclen = length > 6 ? 6 : length;
	switch (nVR) {
		// TODO: handle OF,OD properly
		case VR_OF:
			for (int i = 0; i < veclen; ++i)
				printf("%f ", *(float*)data);
			break;
		case VR_FL:
			printf("%f", *(float*)data);
			break;
		case VR_OD:
			for (int i = 0; i < veclen; ++i)
				printf("%f ", *(double*)data);
			break;
		case VR_FD:
			printf("%f", *(double*)data);
			break;
		case VR_SL:
			printf("%d", *(int32_t*)data);
			break;
		case VR_SS:
			printf("%d", *(int16_t*)data);
			break;
		//case VR_UI:
		//	printf("%s", (char*)data);
		//	break;
		case VR_US:
			printf("%d", *(uint16_t*)data);
			break;
		case VR_UL:
			printf("%d", *(uint32_t*)data);
			break;
		case VR_OB:
			for (int i = 0; i < veclen; ++i)
				printf("%x ", *(uint8_t*)data);
			break;
		case VR_OW:
			for (int i = 0; i < veclen; ++i)
				printf("%x ", *(uint16_t*)data);
			break;
		case VR_AT:
			printf("%x", *(uint32_t*)data);
			break;
		case VR_UN:
			for (int i = 0; i < veclen; ++i)
				printf("%x", *(uint8_t*)data);
			break;
		default:
			printf("%.*s", length, data);
	}
	if (is_vector_vr(VR) && length > veclen)
		printf("...");
	printf("]  # %d\n", length);
}


char * 
parse_sequence (char *data, uint32_t size, int level)
{
	// TODO: add assert
	// Data Elements with a group of 0000, 0002 and 0006 shall not be present
	// within Sequence Items.
	//dprint("D: SEQ PARSE of size %08x at level %d\n", size, level);
	const char *start = data;
	while (1) {
		uint32_t tag = pop_u32(&data);
		//dprint("D: SEQ read tag %08x\n", tag);
		if (is_undefined(size) && tag == SEQ_STOP) 
		{
			//dprint("D: SEQ STOP in undefined-length sequence, returning ...\n");
			data += 4;
			return data;
		}
		else if (tag == ITEM_START) 
		{
			uint32_t length = pop_u32(&data);
			//dprint("D: SEQ ITEM_START length %08x\n", length);
			data = parse_data_set(data, length, level + 1);
		} 
		else 
		{
			//dprint("E: SEQ Nonsensical tag %u\n", tag);
			fprintf(stderr, "Nonsensical tag in Sequence: 0x%08x\n", tag);
			return NULL;
		}
		//dprint("D: SEQ read %08lx bytes so far ...\n", data - start);
		if (data >= start + size) {  /* defined length case */
			//dprint("D: SEQ read to end of defined length sequence, returning ...\n");
			return data;
		}
	}
}


char *
parse_data_set(char *data, size_t size, int level)
{
	// TODO: handle implicit VR
	uint32_t length;
	char *start = data;
	//dprint("D: DS parse of size %08zx at level %d\n", size, level);

	while (1)
	{
		//printf("%08x/%08x: ", cur, data+size);
		uint32_t tag = pop_u32(&data);
		//dprint("D: DS read tag %08x\n", tag);
		if (is_undefined(size) && tag == ITEM_STOP) {
			//dprint("D: DS ITEM_STOP tag in undefined-length data set, returning ...\n");
			data += 4;
			return data;
		}


		//union tag t; 
		//t.t = tag;
		uint16_t element = (tag & 0xffff0000) >> 16;
		uint16_t group = tag & 0x0000ffff;
		//dprint("D: DS read group %04x element %04x\n", group, element);
		char *VR = data;
		data += 2;
		//dprint("D: DS read VR '%c%c'\n", VR[0], VR[1]);

		// TODO: use this instead if faster...
		//length = pop_u16(&data);
		//if (length == 0) length = pop_u32(&data);

		if (is_big_vr(VR)) { 
			data += 2;
			length = pop_u32(&data);
			//dprint("D: DS read length %08x\n", length);
		} else {
			length = pop_u16(&data);
			//dprint("D: DS read length %08x\n", length);
		}

		if (is_sequence(VR))  /* handle sequence encoding */
		{
			printf("%*s(%04x,%04x) SQ %-40s\n", 4*level, " ", group, element, dict_lookup(group, element));
			data = parse_sequence(data, length, level);
		}
		else /* regular tags ... just print */
		{
			print_data_element(data, group, element, VR, length, level);
			data += length;
		}

		//dprint("D: DS read %lx bytes so far...\n", data - start);
		if (data >= start + size) {
			//dprint("D: DS END of %08x length data set, returning ...\n", length);
			return data;
		}

	}
}


int 
main (int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Usage: sd <dicomfile>\n");
		return EX_USAGE;
	}
	int fd = open(argv[1], O_RDONLY);

	struct stat st;
	_errchk(fstat(fd, &st), -1);
	size_t filesize = st.st_size;

	char *data = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE|MAP_POPULATE, fd, 0);
	assert(data != MAP_FAILED);

	int zero_header_length = 132;
	int level = 0;  // top level
	char *ret = parse_data_set(data + 132, filesize - 132, level); // skip zero region

	//print_vr_magics();

    assert(munmap(data, filesize) == 0);
	//assert(rc == 0);

	close(fd);
	if (ret == NULL)
		return EX_DATAERR;

	return EX_OK;
}


