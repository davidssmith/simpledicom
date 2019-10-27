#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <sysexits.h>

int ndirs = 0;
int nfiles = 0;


int 
is_dir (const char *path)
{
	struct stat s;
	stat(path, &s);
	return S_ISDIR(s.st_mode);
}

int 
dirwalk (char *path)
{
	struct dirent *ent;
	DIR *dir = opendir(path);
	
	if (dir != NULL) {
		while((ent = readdir(dir)) != NULL) {
			if (strncmp(ent->d_name, ".", 1) == 0 || strncmp(ent->d_name, "..", 2) == 0)
				continue;
			char *entpath;
			if (asprintf(&entpath, "%s/%s", path, ent->d_name) == -1) {
				return EX_OSERR;
			}
			if (is_dir(entpath)) {
				ndirs += 1;
				//printf("DIR: %s\n", ent->d_name);
				dirwalk(entpath);
			} else {
				nfiles += 1;
				//printf("FILE: %s\n", ent->d_name);
			}
		}
		closedir(dir);
	} else {
		fprintf(stderr, "\nFailed to walk directory \"%s\"\n", path);
		return EX_IOERR;
	}
	return EX_OK;
}



int 
main (int argc, char *argv[])
{
	if (argc < 2) {
		printf("Usage: dirwalk <path>\n");
		return EX_USAGE;
	}
	int res = dirwalk(argv[1]);
	printf("ndirs: %d\nnfiles: %d\n", ndirs, nfiles);
	return res;
}
