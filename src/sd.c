/*
 * TODO: add implicit 
 * TODO: track nested level for indentation or indication
 * TODO: see if using int VR instead of char[2] is faster
 *
 */

#include "sd.h"



uint16_t
pop2 (char **x)   // TODO: see if macro faster
{
	uint16_t n = *(uint16_t*)*x;
	*x += 2;
	return n;
}

uint32_t
pop4 (char **x)
{
	uint32_t n = *(uint32_t*)*x;
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

char *
format_string (char *VR)
{
	int16_t n = *((int16_t*)VR);  // cast char[2] to int16
	switch (n) {
		case VR_FL:
		case VR_OF:
		case VR_FD:
		case VR_OD:
			return "%f";
			break;
		case VR_SL:
		case VR_SS:
			return "%d";
			break;
		case VR_UL:
		case VR_US:
			return "%u";
			break;
		case VR_AT:
		case VR_OB:
		case VR_OW:
		case VR_UN:
			return "%x";
			break;
		default:
			return "%s";
	}
}

size_t
seek_undef (char *data, uint16_t stop_code)
{
	char *data_head = data;
	uint32_t s, t;
	int depth = 0;
	while (1) {
		t = pop4(&data);
		//printf("%08x> %08x (+%x +%x -%x)\n", data, t, ITEM_START, ITEM_STOP, SEQ_STOP);
		if (t == SEQ_STOP && depth == 0){
			printf("000000000000\n");
			break;
		} else if (t == ITEM_START) {
			depth++;
			printf("++ descending to %d\n", depth);
			//data = print_sequence(data);
		} else if (t == ITEM_STOP || t == SEQ_STOP) {
			depth--;
			printf("-- ascending to %d\n", depth);
		}
		//data -= 2; // backtrack two bytes
	}
	data += 4; // skip final 0x00000000
	return data - data_head;
}


char *
print_seq (char *data, int32_t length)
{
	data += 2;
	uint32_t length = pop4(&data);
	if (is_undefined(length)) {
		printf("undefined\n");
		length = seek_undef(data, SEQ_STOP);
	}
	data += length + 4;
	printf("SEQUENCE length %d ", length);
	return data;
}

char *
print_data_elem (char *data)
{
	/* read and parse next tag in explicit VR encoding 
	 * TODO: handle implicit VR 
	 * */


	// read g,e,vr
	uint16_t group = pop2(&data);
	uint16_t element = pop2(&data);
	char *VR = data;
	data += 2;

	// read length
	uint32_t length;

	// TODO: use this instead if faster...
	//length = pop2(&data);
	//if (length == 0) length = pop4(&data);

	if (is_big_vr(VR)) {
		data += 2;
		length = pop4(&data);
	} else
		length = pop2(&data);

	printf("(%04x,%04x) %c%c [", group, element, VR[0], VR[1]);
	if (is_undefined(length))
		printf(" <SEQ> ]  # undef\n");
	else {
		print_value(data, VR, length);
		printf("]  # %d\n", length);
	}

	if (is_sequence(VR)) 
		length = print_seq(data, length);

	return data + length;
}

void
print_value (char* data, char *VR, uint32_t length)
{
	if (is_sequence(VR)) {
		printf("<SEQ length:%d>", length);
	} else if (is_binary_vr(VR)) {
		if (is_vector_vr(VR)) {
			int n = length > 6 ? 6 : length;
			for (int i = 0; i < n; ++i)
				printf(format_string(VR), data[i]);
		} else
			printf(format_string(VR), data);
	} else {  /* must be string */
		printf("%.*s", length, data);
	}
}


char *
print_data_set(char *data, size_t size, int level)
{
	char *cur = data;  
	while (cur < data + size) {
		printf("%08x/%08x: ", cur, data+size);
		cur = print_data_elem(cur);
	}
}

int 
main (int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Usage: sd <dicomfile>\n");
		return 1;
	}
	int fd = open(argv[1], O_RDONLY);

	struct stat st;
	_errchk(fstat(fd, &st), -1);

	char *data = malloc(st.st_size);
	_errchk(read(fd, data, st.st_size), -1);
	close(fd);

	// add 132 to skip padding and magic
	int level = 0;
	print_data_set(data + 132, st.st_size, level);

	//print_vr_magics();

	free(data);
}


