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


int 
is_sequence (const char *VR) 
{ 
	return *((int16_t*)VR) == VR_SQ; 
}


int 
is_undefined (const int32_t length) 
{ 
	return length == 0xffffffff; 
}


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


void
print_data_element (char* data, uint16_t group, uint16_t element, char *VR, uint32_t length, int level)
{
	printf("DEBUG: print element of length %08x at level %d\n", length, level);
//	printf("%*c", ' ', level > 0 ? level : 1);
	printf("(%04x,%04x) %c%c [", group, element, VR[0], VR[1]);
	if (is_binary_vr(VR)) {
		if (is_vector_vr(VR)) {
			int n = length > 6 ? 6 : length;
			for (int i = 0; i < n; ++i)
				printf(format_string(VR), data[i]);
		} else
			printf(format_string(VR), data);
	} else {  /* must be string */
		printf("%.*s", length, data);
	}
	printf("]  # %d\n", length);
}


char * 
parse_sequence (char *data, uint32_t length, int level)
{
	// Data Elements with a group of 0000, 0002 and 0006 shall not be present
	// within Sequence Items.
	printf("DEBUG: parse sequence of size %u at level %d\n", length, level);
	const char *start = data;
	while (1) {
		uint32_t tag = pop4(&data);
		printf("DEBUG: read tag %08x\n", tag);
		if (is_undefined(length) && tag == SEQ_STOP) {
			printf("DEBUG: found SEQ_STOP tag in undefined-length sequence, returning ...\n");
			data += 4;
			return data;
		}
		if (tag == ITEM_START) {
			uint32_t length = pop4(&data);
			printf("DEBUG: item length %08x\n", length);
			parse_data_set(data, length, level + 1);
		} else {
			fprintf(stderr, "ERROR: Nonsensical tag %u\n", tag);
			return NULL;
		}
		printf("DEBUG: read %08x bytes so far ...\n", data - start);
		if (data - start >= length) {  /* defined length case */
			printf("DEBUG: read to end of defined length sequence, returning ...\n");
			return data;
		}
	}
	//printf(" <SEQ> ]  # undef\n");
	//printf("<SEQ length:%d>", length);
}


char *
parse_data_set(char *data, size_t size, int level)
{
	// TODO: handle implicit VR
	// TODO: handle undefined length, looking for ITEM_STOP tag
	uint32_t length;
	char *start = data;
	printf("DEBUG: parsing data set of size %lu at level %d\n", size, level);

	while (1)
	{
		//printf("%08x/%08x: ", cur, data+size);
		uint32_t tag = pop4(&data);
		printf("DEBUG: read tag %08x\n", tag);
		if (is_undefined(length) && tag == ITEM_STOP) {
			printf("DEBUG: found ITEM_STOP tag in undefined-length data set, returning ...\n");
			return data;
		}
		uint16_t group = (tag & 0xffff0000) >> 16;
		uint16_t element = (tag & 0x0000ffff);
		printf("DEBUG: read group %04x element %04x\n", group, element);
		char *VR = data;
		data += 2;
		printf("DEBUG: read VR %c%c\n", VR[0], VR[1]);

		// TODO: use this instead if faster...
		//length = pop2(&data);
		//if (length == 0) length = pop4(&data);

		if (is_big_vr(VR)) { /* read length */
			data += 2;
			length = pop4(&data);
			printf("DEBUG: read length %u\n", length);
		} else {
			length = pop2(&data);
			printf("DEBUG: read length %u\n", length);
		}

		if (is_sequence(VR))  /* handle sequence encoding */
		{
			// TODO: decide what to return, and which pointer to advance
			data = parse_sequence(data, length, level);
		}
		else /* regular tags ... just print */
		{
			print_data_element(data, group, element, VR, length, level);
			data += length;
		}

		printf("DEBUG: read %u bytes so far...\n", data - start);
		if (data - start >= size) {
			printf("DEBUG: reached end of %08x length data set, returning ...\n", length);
			return data;
		}

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

	int zero_header_length = 132;
	int level = 0;  // top level
	parse_data_set(data + zero_header_length, st.st_size, level);

	//print_vr_magics();

	free(data);
}


