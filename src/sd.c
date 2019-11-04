/*
 * TODO: 
 * - implement big endian
 * - clean up byte orders
 * - fix crash on trying to open no files
 */

#include "sd.h"


enum byte_order {
	LittleEndian,
	BigEndian
};

enum SYNTAX {
	Implicit,
	Explicit
};


static bool hide_private = false;
static int nthreads = 1;
static bool implicit_syntax = true; // ImplicitVRLittleEndian is default
static int max_level = 100;
static int max_dir_depth = 100;
static bool skim_leaves = false; // if true, read just one file in each leaf dir
static bool use_color = false;
static char *keyword_to_print = NULL;
static bool done = false;
static int64_t file_size = 0;
static char *file_start = NULL;
static char *file_path = NULL;
//static bool bigendian = false;
//static bool in_metadata = true;


void
die_if_func (const int condition, const char *msg, const int line)
{
	if (condition) {
		fprintf(stderr, "[%s: line %d] %s\n", file_path, line, msg);
		raise(SIGINT);
	}
}

#define die_if(cond,msg) die_if_func(cond,msg,__LINE__)

#define validate_pointer(p) \
	die_if(p < file_start || p > file_start + file_size,\
			"invalid file pointer");


int
is_dir (const char *path)
{
	struct stat s;
	stat(path, &s);
	return S_ISDIR(s.st_mode);
}

int
dirwalk (char *path, int (*func)(char *path), const int depth)
{
	char *entpath; // TODO: put on stack
	struct dirent *ent;
	DIR *dir = opendir(path);
	bool skimmed = false;

	die_if(dir == NULL, "failed to open dir");
	while((ent = readdir(dir)) != NULL) {
		if (strncmp(ent->d_name, ".", 1) == 0 || strncmp(ent->d_name, "..", 2) == 0)
			continue;
		if (asprintf(&entpath, "%s/%s", path, ent->d_name) == -1) {
			return EX_OSERR;
		}
		if (is_dir(entpath) && depth < max_dir_depth) {
			//printf("=== CD %s\n", entpath);
			dirwalk(entpath, func, depth + 1);
		} else if (!skimmed) {
			//printf("=== PARSE %s\n", entpath);
			//fprintf(stderr, "%d\r", nread++);
			(*func)(entpath);
			if (skim_leaves && depth == max_dir_depth)
				skimmed = true;
		}
	}
	closedir(dir);
	return EX_OK;
}


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

uint16_t
vr_from_string (const char* s) // convert from 2-char str
{
	uint16_t n = s[1] ;
	return (n << 8) | s[0];
}

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

uint16_t
dict_VR (const uint32_t tag)
{
	const int i = dict_lookup(tag);
	if (i < 0)
		return VR_OB;
	else {
		uint16_t VR = vr_from_string(dict[i].VR);
		return VR;
	}
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


#ifdef VR_MAGIC
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
#endif

char
char2 (uint16_t n)
{
	return (char)(n >> 8);
}

char
char1 (uint16_t n)
{
	return (char)(n & 0xff);
}

bool
is_big (const uint16_t VR)
{
	// VR is too big to print
	return VR == VR_SQ || char1(VR) == 'O' || VR == VR_UN || VR == VR_UT;
}

bool
is_vector (const uint16_t VR)
{
	return char1(VR) == 'O';
}

bool
is_sequence (const uint16_t VR) {
	return VR == VR_SQ;
}


uint16_t grp (const uint32_t tag) {
	return tag >> 16;
}

uint16_t ele (const uint32_t tag) {
	return tag & 0xffff;
}

bool is_private (const uint32_t tag) {
	return grp(tag) % 2 == 1;
}

bool
is_dicom (char *data) {
	return data[128] == 'D' && data[129] == 'I' &&
		data[130] == 'C' && data[131] == 'M';
}


void
print_data_element (char* const data, const uint32_t tag,
		const uint16_t VR, const uint32_t length, const int level)
{
	char *keyword = dict_keyword(tag);
	if (level >= max_level ||
		(hide_private && (keyword == NULL || is_private(tag))))
		return;

	if (keyword_to_print != NULL) {
		//printf("[%s]\n", keyword_to_print);
	   if (!strncmp(keyword_to_print, keyword, strlen(keyword))) {
			done = true;
	   } else {
		   return;
	   }
	} else {
		if (level > 0)
			printf("%*s", 4*level, " ");
		if (use_color && VR == VR_SQ)
			printf("%s(%04x,%04x) %s%c%c%s %-*s %s%6d%s [", COLOR1, grp(tag), ele(tag), 
					COLOR2, char1(VR), char2(VR), COLOR_GRAY, 36-4*level,
					keyword != NULL ? keyword : "Private", COLOR3, length, COLOR_RESET);
		else if (use_color)
			printf("%s(%04x,%04x) %s%c%c%s %-*s %s%6d%s [", COLOR1, grp(tag), ele(tag), 
					COLOR2, char1(VR), char2(VR), COLOR_WHITE, 36-4*level,
					keyword != NULL ? keyword : "Private", COLOR3, length, COLOR_RESET);
		else
			printf("(%04x,%04x) %c%c %-*s %6d [", grp(tag), ele(tag), char1(VR), char2(VR),
				36-4*level, keyword != NULL ? keyword : "Private", length);
	}
	uint32_t m = length > 6 ? 6 : length;
	if (use_color)
		printf("%s", COLOR4);
	if (VR == VR_SQ)
		goto done;
	else if (VR == VR_FD) // moved up due to frequency
		printf("%g", *(double*)data);
	else if (VR == VR_US)
		printf("%d", *(uint16_t*)data);
	else if (VR == VR_UL)
		printf("%d", *(uint32_t*)data);
	else if (VR == VR_OF)
		for (uint32_t i = 0; i < m; ++i)
			printf("%g%*s", *((float*)data+i), i<m-1, "\\");
	else if (VR == VR_FL)
		printf("%f", *(float*)data);
	else if (VR == VR_OD)
		for (uint32_t i = 0; i < m; ++i)
			printf("%g%*s", *((double*)data+i), i<m-1, "\\");
	else if (VR == VR_SL)
		printf("%d", *(int32_t*)data);
	else if (VR == VR_SS)
		printf("%d", *(int16_t*)data);
	else if (VR == VR_OB)
		for (uint32_t i = 0; i < m; ++i)
			printf("%02x%*s", *((uint8_t*)data+i), i<m-1, "\\");
	else if (VR == VR_OW)
		for (uint32_t i = 0; i < m; ++i)
			printf("%04x%*s", *((uint16_t*)data+i), i<m-1, "\\");
	else if (VR == VR_AT)
		printf("%08x", *(uint32_t*)data);
	else if (VR == VR_UN)
		for (uint32_t i = 0; i < m; ++i)
			printf("%02x%*s", *(data+i), i<m-1, "\\");
	else
		printf("%.*s", length, data);
	if (is_vector(VR) && length > m)
		printf("...");
done:
	if (use_color)
		printf("%s", COLOR_RESET);
	keyword_to_print == NULL ?  printf("]\n") : printf("\n");
}


char *
parse_sequence (char *cursor, const uint32_t size, const int level)
{
	// Data Elements with a group of 0000, 0002 and 0006 shall not be present
	// within Sequence Items.
	const char * const start = cursor;
	do {
		validate_pointer(cursor);
		uint32_t tag = pop_u32(&cursor);
		if (size == SIZE_UNDEFINED && tag == SEQ_STOP)
			return cursor + 4;
		if (tag == ITEM_START) {
			uint32_t length = pop_u32(&cursor);
			cursor = parse_data_set(cursor, length, level + 1);
		} else {
			fprintf(stderr, "[%s: offset %ld] found tag %08x instead of ItemDelim %08x\n",
					file_path, cursor-file_start, tag, ITEM_START);
			done = true;
			return NULL;
		}
		if (done)
			break;
	} while (cursor < start + size); /* defined length case */
	return cursor;
}


char *
parse_data_set (char *cursor, const size_t size, const int level)
{
	uint32_t tag, length;
	uint16_t VR; // TODO: switch VR to u16 everywhere
	//printf("dataset: size=%lu, level=%d\n", size, level);
	for (const char *const start = cursor; cursor < start + size; )
	{
		validate_pointer(cursor);
		tag = pop_u32(&cursor);

		if (size == SIZE_UNDEFINED && tag == ITEM_STOP)
			return cursor + 4;

		tag = grp(tag) | ele(tag) << 16; // swap for little endian

		if (grp(tag) <= METADATA_GROUP || !implicit_syntax) {
			VR = pop_u16(&cursor);
			if (is_big(VR)) {
				cursor += 2;
				length = pop_u32(&cursor);
			} else
				length = pop_u16(&cursor);
			if (tag == 0x00020010 && strncmp(cursor, "1.2.840.10008.1.2", length) != 0)
					implicit_syntax = false;
		} else {
			length = pop_u32(&cursor);
			VR = dict_VR(tag);
		}

		print_data_element(cursor, tag, VR, length, level);

		if (is_sequence(VR) && !is_private(tag))
			cursor = parse_sequence(cursor, length, level);
		else
			cursor += length;

		if (done)
			break;
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
	file_size = st.st_size;

	char *data = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
	die_if(data == MAP_FAILED, "mmap failed");
	madvise(data, file_size, MADV_SEQUENTIAL | MADV_WILLNEED);
	file_start = data;

	char *ret = NULL;
	if (is_dicom(data)) {
		int level = 0;  // top level
		int preamble = 132;
		done = false;
		implicit_syntax = true; // reset defaults
		ret = parse_data_set(data + preamble, file_size - preamble, level);
	}
    die_if(munmap(data, file_size) != 0, "munmap failed");
	close(fd);
	if (ret == NULL)
		return EX_DATAERR;

	return EX_OK;
}


void
print_usage()
{
	fprintf(stderr, "Stream DICOM parser and manipulator\n");
	fprintf(stderr, "sd [-hp] [-k keyword] [-l n] <dicomfile>\n");
	fprintf(stderr, "\t-d depth\tmax depth to search\n");
	fprintf(stderr, "\t-h\t\tthis help\n");
	fprintf(stderr, "\t-k keyword\tprint single keyword\n");
	fprintf(stderr, "\t-l n\t\tmax nesting level to print\n");
	fprintf(stderr, "\t-p\t\tomit private tags\n");
	fprintf(stderr, "\t-s\t\tskim leaves\n");
}

int
main (int argc, char *const argv[])
{
	extern char *optarg;
    extern int optind, opterr, optopt;
	opterr = 0;
	int c;
	while ((c = getopt (argc, argv, "cd:hk:l:pst:")) != -1) {
		switch (c) {
			case 'c':
				use_color = true;
				break;
			case 'd':
				max_dir_depth = atoi(optarg);
				break;
			case 'k':
				keyword_to_print = malloc(128);
				die_if(keyword_to_print == NULL, "malloc for keyword_to_print failed");
				strncpy(keyword_to_print, optarg, 128);
				keyword_to_print[127] = '\0'; /* to ensure null termination */
				//keyword_to_print = optarg;
				break;
			case 'l':
				max_level = atoi(optarg);
				break;
			case 'p':
				hide_private = true;
				break;
			case 's':
				skim_leaves = true;
				break;
			case 't':
				nthreads = atoi(optarg);
				break;
			case 'h':
			default:
				print_usage();
				return EX_USAGE;
		}
	}
	if (!optind) {
		print_usage();
		return EX_USAGE;
	}
	file_path = argv[optind];
	struct stat path_stat;
	stat(file_path, &path_stat);

	//sort_dict(); // if dict not already sorted

	if S_ISREG(path_stat.st_mode) { /* single file, just parse and quit */
		return parse_dicom_file(file_path);
	} else {
		dirwalk(file_path, parse_dicom_file, 0); /* directory, so walk and parse */
		return EX_OK;
	}
}

