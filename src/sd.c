/*
 * TODO: add implicit 
 * TODO: track nested level for indentation or indication
 * TODO: see if using int VR instead of char[2] is faster
 *
 */

#include "sd.h"
#include "dict.h"

static bool hide_private = false;
static int nthreads = 1;


char * dict_lookup (const uint32_t tag)
{
	int min = 0, max = DICTSIZE;
	do {
		int i = (min + max) / 2;
		if (tag < dict[i].val)
			max = i;
		else if (tag > dict[i].val)
			min = i;	
		else 
			return dict[i].keyword;
	} while (max - min > 1);
	return NULL;  // either private or unknown
}


uint16_t pop_u16 (char **x)   // TODO: see if macro faster
{
	uint16_t n = *(uint16_t*)*x;
	*x += 2;
	return n;
}

uint32_t pop_u32 (char **x)
{
	uint32_t n = *(uint32_t*)*x;
	*x += 4;
	return n;
}


void _errchk (const int ret, const int errval) {
	if (ret == errval)
		perror("sd: ");
}


void print_vr_magics ()
{
	char all_vrs[] = "AEASSHLODADTTMCSSTLTUTPNISDSUISQSSUSSLULATFLFDOBOWOFUN";

	for (int i = 0; i < sizeof all_vrs - 1; i += 2) {
		int16_t n = *((int16_t*)(all_vrs + i));
		char a = all_vrs[i];
		char b = all_vrs[i+1];
		printf("#define VR_%c%c 0x%x\n", a, b, n);
	}
}

bool is_big (char *VR)
{
	// VR is too big to print
	int16_t n = *((int16_t*)VR);  // cast char[2] to int16
	return (n == VR_SQ || VR[0] == 'O' || n == VR_UN || n == VR_UT);
}

bool is_vector (const char *VR) { return VR[0] == 'O'; }
bool is_sequence (const char *VR) { return *((int16_t*)VR) == VR_SQ; }

uint16_t grp (const uint32_t tag) { return tag >> 16; }
uint16_t ele (const uint32_t tag) { return tag & 0xffff; }

void
print_data_element (const char* const data, const uint32_t tag,
		const char *VR, const uint32_t length, const int level)
{
	char *keyword = dict_lookup(tag);

	if (hide_private && keyword == NULL) // skip private tags
		return;

	if (level > 0)
		printf("%*s", 4*level, " ");
	printf("(%04x,%04x) %c%c %-*s %6d [", grp(tag), ele(tag), VR[0], VR[1],
		36-4*level, keyword != NULL ? keyword : "Private", length);
	int16_t nVR = *((int16_t*)VR);  // cast char[2] to int16 for matching
	int m = length > 6 ? 6 : length;
	switch (nVR) {
		case VR_SQ: 
			break;
		case VR_UI:
			printf("%.*s", length, data);
			break;
		case VR_FD:
			printf("%g", *(double*)data);
			break;
		case VR_OF:
			for (int i = 0; i < m; ++i)
				printf("%g%*s", *((float*)data+i), i<m-1, ",");
			break;
		case VR_FL:
			printf("%f", *(float*)data);
			break;
		case VR_OD:
			for (int i = 0; i < m; ++i)
				printf("%g%*s", *((double*)data+i), i<m-1, ",");
			break;
		case VR_SL:
			printf("%d", *(int32_t*)data);
			break;
		case VR_SS:
			printf("%d", *(int16_t*)data);
			break;
		case VR_US:
			printf("%d", *(uint16_t*)data);
			break;
		case VR_UL:
			printf("%d", *(uint32_t*)data);
			break;
		case VR_OB:
			for (int i = 0; i < m; ++i)
				printf("%02x%*s", *((uint8_t*)data+i), i<m-1, ",");
			break;
		case VR_OW:
			for (int i = 0; i < m; ++i)
				printf("%04x%*s", *((uint16_t*)data+i), i<m-1, ",");
			break;
		case VR_AT:
			printf("%08x", *(uint32_t*)data);
			break;
		case VR_UN:
			for (int i = 0; i < m; ++i)
				printf("%02x%*s", *(data+i), i<m-1, ",");
			break;
		default:
			printf("%.*s", length, data);
	}
	if (is_vector(VR) && length > m)
		printf("...");
	printf("]\n");
}


char * parse_sequence (char *data, const uint32_t size, const int level)
{
	// Data Elements with a group of 0000, 0002 and 0006 shall not be present
	// within Sequence Items.
	const char *start = data;
	do {
		uint32_t tag = pop_u32(&data);
		if (size == SIZE_UNDEFINED && tag == SEQ_STOP)
			return data + 4;
		if (tag == ITEM_START) {
			uint32_t length = pop_u32(&data);
			data = parse_data_set(data, length, level + 1);
		} else {
			fprintf(stderr, "nonsensical tag in Sequence: 0x%08x\n", tag);
			return NULL;
		}
	} while (data < start + size); /* defined length case */
	return data;
}


char *
parse_data_set (char *data, const size_t size, const int level)
{
	// TODO: handle implicit VR
	uint32_t length;
	char *start = data;

	do {
		uint32_t tag = pop_u32(&data);

		if (size == SIZE_UNDEFINED && tag == ITEM_STOP)
			return data + 4;

		// swap about bytes for little endian
		tag = (tag >> 16) | (tag & 0xffff) << 16;
		char *VR = data;
		data += 2;

		if (is_big(VR)) {
			data += 2;
			length = pop_u32(&data);
		} else
			length = pop_u16(&data);

		print_data_element(data, tag, VR, length, level);

		if (is_sequence(VR))
			data = parse_sequence(data, length, level);
		else
			data += length;

	} while (data < start + size);

	return data;
}


int
parse_dicom_file (char *filename)
{
	int fd = open(filename, O_RDONLY);
	_errchk(fd, -1);

	struct stat st;
	_errchk(fstat(fd, &st), -1);
	size_t filesize = st.st_size;

	char *data = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE|MAP_POPULATE, fd, 0);
	assert(data != MAP_FAILED);

	int zero_header_length = 132;
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
	fprintf(stderr, "\t-h\tthis help\n");
	fprintf(stderr, "\t-k\tprint keywords\n");
	fprintf(stderr, "\t-p\tprint private tags\n");
}

int 
main (int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Usage: sd <dicomfile>\n");
		return EX_USAGE;
	}
	opterr = 0;
	int c;
	while ((c = getopt (argc, argv, "hpt:v")) != -1) {
		switch (c) {
			case 'p':
				hide_private = true;
				break;
			case 't':
				nthreads = atoi(optarg);
				break;
			case 'h':
			default:
				print_usage();
		}
	}
	char *path = argv[optind];
	struct stat path_stat;
	stat(path, &path_stat);
	int ret;
	if S_ISREG(path_stat.st_mode) {
		int ret = parse_dicom_file(argv[optind]);
		return ret;
	} else {
		fprintf(stderr, "Directory parsing not implemented yet.\n");
		return EX_USAGE;
	}
}

