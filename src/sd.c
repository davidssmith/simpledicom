/*
 * TODO: see if using int VR instead of char[2] is faster
 *
 */

#include "sd.h"

static bool hide_private = false;
static int nthreads = 1;
static bool explicit_syntax = true;
//static bool in_metadata = true;



/*

static int
compdict(void * const l, void * const r)
{
	struct Tag *t1 = (struct Tag *) l;
	struct Tag *t2 = (struct Tag *) r;
	if (t1->tag > t2->tag)
		return 1;
	else if (t1->tag < t2->tag)
		return -1;
	else
		return 0;
}

void
sort_dict ()
{
	qsort((void*)dict, DICTSIZE, sizeof(struct Tag), compdict);
}


char *
dict_lookup_keyword (const uint32_t tag) 
{
	struct Tag key = { .tag = tag }, *res;
 	res = bsearch(&key, (void*)dict, DICTSIZE, sizeof(struct Tag), compdict);
	return res->keyword;
}
*/

int 
dict_lookup (const uint32_t tag)
{
	for (int min = 0, max = DICTSIZE; max - min > 1; )
	{
		int i = (min + max) / 2;
		if (tag < dict[i].tag)
			max = i;
		else if (tag > dict[i].tag)
			min = i;	
		else
			return i;
	}
	return -1;  // either private or unknown
}

char * 
dict_keyword (const uint32_t tag)
{
	const int i = dict_lookup(tag);
	return i >= 0 ? dict[i].keyword : NULL;
}

char * 
dict_VR (const uint32_t tag)
{
	const int i = dict_lookup(tag);
	return i >= 0 ? dict[i].VR : "OB";
}


uint16_t 
pop_u16 (char **x)   // TODO: see if macro faster
{
	uint16_t n = *(uint16_t*)*x;
	*x += 2;
	return n;
}

uint32_t 
pop_u32 (char **x)
{
	uint32_t n = *(uint32_t*)*x;
	*x += 4;
	return n;
}


void 
_errchk (const int ret, const int errval) {
	if (ret == errval)
		perror("sd: ");
}


void 
print_vr_magics ()
{
	char all_vrs[] = "AEASSHLODADTTMCSSTLTUTPNISDSUISQSSUSSLULATFLFDOBOWOFUN";

	for (size_t i = 0; i < sizeof all_vrs - 1; i += 2) {
		int16_t n = *((int16_t*)(all_vrs + i));
		char a = all_vrs[i];
		char b = all_vrs[i+1];
		printf("#define VR_%c%c 0x%x\n", a, b, n);
	}
}

bool 
is_big (char *VR)
{
	// VR is too big to print
	uint16_t n = *((uint16_t*)VR);  // cast char[2] to int16
	return (n == VR_SQ || VR[0] == 'O' || n == VR_UN || n == VR_UT);
}

bool is_vector (const char *const VR) { return VR[0] == 'O'; }
bool is_sequence (char * const VR) { return *((uint16_t*)VR) == VR_SQ; }


uint16_t grp (const uint32_t tag) { return tag >> 16; }
uint16_t ele (const uint32_t tag) { return tag & 0xffff; }
bool is_private (const uint32_t tag) { return grp(tag) % 2 == 1; }

void
print_data_element (char* const data, const uint32_t tag,
		char *const VR, const uint32_t length, const int level)
{
	char *keyword = dict_keyword(tag);

	if (hide_private && (keyword == NULL || is_private(tag))) {// skip private tags
		printf("SKIP\n");
		return;
	}

	if (level > 0)
		printf("%*s", 4*level, " ");
	printf("(%04x,%04x) %c%c %-*s %6d [", grp(tag), ele(tag), VR[0], VR[1],
		36-4*level, keyword != NULL ? keyword : "Private", length);
	int16_t v = *((int16_t*)VR);  // cast char[2] to int16 for matching
	int m = length > 6 ? 6 : length;
	if (v == VR_SQ)
		goto done;
	else if (v == VR_FD) // moved up due to frequency
		printf("%g", *(double*)data);
	else if (v == VR_US)
		printf("%d", *(uint16_t*)data);
	else if (v == VR_UL)
		printf("%d", *(uint32_t*)data);
	else if (v == VR_OF)
		for (int i = 0; i < m; ++i)
			printf("%g%*s", *((float*)data+i), i<m-1, "\\");
	else if (v == VR_FL)
		printf("%f", *(float*)data); 
	else if (v == VR_OD)
		for (int i = 0; i < m; ++i)
			printf("%g%*s", *((double*)data+i), i<m-1, "\\");
	else if (v == VR_SL)
		printf("%d", *(int32_t*)data);
	else if (v == VR_SS)
		printf("%d", *(int16_t*)data);
	else if (v == VR_OB)
		for (int i = 0; i < m; ++i)
			printf("%02x%*s", *((uint8_t*)data+i), i<m-1, "\\");
	else if (v == VR_OW)
		for (int i = 0; i < m; ++i)
			printf("%04x%*s", *((uint16_t*)data+i), i<m-1, "\\");
	else if (v == VR_AT)
		printf("%08x", *(uint32_t*)data); 
	else if (v == VR_UN)
		for (int i = 0; i < m; ++i)
			printf("%02x%*s", *(data+i), i<m-1, "\\");
	else
		printf("%.*s", length, data);
	if (is_vector(VR) && length > m)
		printf("...");
done:
	printf("]\n");
}


char * 
parse_sequence (char *cursor, const uint32_t size, const int level)
{
	// Data Elements with a group of 0000, 0002 and 0006 shall not be present
	// within Sequence Items.
	const char * const start = cursor;
	do {
		uint32_t tag = pop_u32(&cursor);
		if (size == SIZE_UNDEFINED && tag == SEQ_STOP)
			return cursor + 4;
		if (tag == ITEM_START) {
			uint32_t length = pop_u32(&cursor);
			cursor = parse_data_set(cursor, length, level + 1);
		} else {
			fprintf(stderr, "nonsensical tag in Sequence: 0x%08x\n", tag);
			return NULL;
		}
	} while (cursor < start + size); /* defined length case */
	return cursor;
}


char *
parse_data_set (char *cursor, const size_t size, const int level)
{
	for (const char *const start = cursor; cursor < start + size; )
	{
		uint32_t tag = pop_u32(&cursor);

		if (size == SIZE_UNDEFINED && tag == ITEM_STOP)
			return cursor + 4;

		tag = grp(tag) | ele(tag) << 16; // swap for little endian

		//if (in_metadata && grp(tag) > 0x2) {
		//	in_metadata = false;
		//}

		uint32_t length;
		char *VR;
		if (explicit_syntax || grp(tag) > METADATA_GROUP) {
			VR = cursor;
			cursor += 2;
			if (is_big(VR)) {
				cursor += 2;
				length = pop_u32(&cursor);
			} else
				length = pop_u16(&cursor);
		} else {
			length = pop_u32(&cursor);
			VR = dict_VR(tag);
		}

		if (tag == 0x00020010) {
			if (strncmp(cursor, "1.2.840.10008.1.2", length) == 0)
				explicit_syntax = false;
			else
				explicit_syntax = true;
		}

		print_data_element(cursor, tag, VR, length, level);

		if (is_sequence(VR) && !is_private(tag))
			cursor = parse_sequence(cursor, length, level);
		else
			cursor += length;

	}

	return cursor;
}


int
parse_dicom_file (char *filename)
{
	int fd = open(filename, O_RDONLY);
	_errchk(fd, -1);

	struct stat st;
	_errchk(fstat(fd, &st), -1);
	size_t filesize = st.st_size;

	char *data = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
	assert(data != MAP_FAILED);

	//int zero_header_length = 132;
	int level = 0;  // top level
	char *ret = parse_data_set(data + 132, filesize - 132, level); // skip zero region

    assert(munmap(data, filesize) == 0);
	close(fd);
	if (ret == NULL)
		return EX_DATAERR;

	return EX_OK;
}


void
print_usage()
{
	fprintf(stderr, "Stream DICOM parser and manipulator\n");
	fprintf(stderr, "sd [-hkp] <dicomfile>\n");
	fprintf(stderr, "\t-h\tthis help\n");
	fprintf(stderr, "\t-k\tprint keywords\n");
	fprintf(stderr, "\t-p\tprint private tags\n");
}

int 
main (int argc, char *argv[])
{
	if (argc < 2) {
		print_usage();
		return EX_USAGE;
	}
	extern char *optarg;
    extern int optind, opterr, optopt;
	opterr = 0;
	int c;
	while ((c = getopt (argc, argv, "hipt:")) != -1) {
		switch (c) {
			case 'p':
				hide_private = true;
				break;
			case 't':
				nthreads = atoi(optarg);
				break;
			case 'i':
				explicit_syntax = false;
				break;
			case 'h':
			default:
				print_usage();
		}
	}
	char *path = argv[optind];
	struct stat path_stat;
	stat(path, &path_stat);

	//sort_dict(); // if dict not already sorted

	if S_ISREG(path_stat.st_mode) {
		int ret = parse_dicom_file(argv[optind]);
		return ret;
	} else {
		fprintf(stderr, "Directory parsing not implemented yet.\n");
		return EX_USAGE;
	}
}

