
#include "walk.h"


int
is_dir (const char *path)
{
	struct stat s;
	stat(path, &s);
	return S_ISDIR(s.st_mode);
}


int
print_path (char *path)
{
	if (is_dir(path))
		printf("%s/\n", path);
	else
		printf("%s\n", path);
	return 0;
}

int
dirwalk (char *path, int(*func)(char *path))
{
	char entpath[512];
	struct dirent *ent;
	DIR *dir = opendir(path);

	if (dir != NULL) {
		while((ent = readdir(dir)) != NULL) {
			if (strncmp(ent->d_name, ".", 1) == 0 || strncmp(ent->d_name, "..", 2) == 0)
				continue;
			if (snprintf(entpath, 512, "%s/%s", path, ent->d_name) == -1) {
				return EX_OSERR;
			}
			if (is_dir(entpath)) {
				(*func)(entpath);
				dirwalk(entpath, func);
			} else {
				(*func)(entpath);
			}
		}
		closedir(dir);
	} else {
		fprintf(stderr, "%s ", path);
		perror("failed");
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
	int res = dirwalk(argv[1], print_path);
	return res;
}
