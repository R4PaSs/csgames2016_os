#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "filecoder.h"
#include <dirent.h>
#include <string.h>

int main(int argc, char** argv)
{
	if(argc < 2){
		printf("Usage: ./filecoder file [-o outputfile] [-e extent_size]\n");
		exit(1);
	}
	char* out = parse_out(argc, argv);
	int ext_sz = parse_ext(argc, argv);
	struct stat s;
	if(!stat(argv[1], &s)) {
		if(S_ISDIR(s.st_mode) || S_ISREG(s.st_mode)) {
			//make_filesystem(argv[1], out);
		} else {
			printf("Bad argument %s, expected path to a file or a directory\n", argv[1]);
		}
	} else {
		printf("Error: syscall to stat failed, are you sure %s exists ?", argv[1]);
	}
}

char* parse_out(int argc, char** argv)
{
	int i;
	for(i = 1; i < (argc - 1); i++) {
		if(!strcmp(argv[i], "-o")){
			return argv[i + 1];
		}
	}
	return "out";
}

int parse_ext(int argc, char** argv)
{
	int i = 0;
	for(i = 1; i < (argc - 1); i++) {
		if(!strcmp(argv[i], "-e")) {
			return atoi(argv[i + 1]);
		}
	}
	return 8;
}

void make_filesystem(char* root_path, char* out_path, filepart fp)
{
	int byte_size = in_byte_size(".", root_path);
	FILE* out = fopen(out_path, "w");
	build_header(out, fp);
	struct stat s;
}

void build_header(FILE* out, filepart fp)
{
}

int in_byte_size(char* parent_path, char*inpath)
{
	struct stat s;
	if(!stat(inpath, &s)) {
		if(S_ISDIR(s.st_mode)) {
			return in_dir_byte_size(parent_path, inpath);
		} else if (S_ISREG(s.st_mode)) {
			return in_file_byte_size(parent_path, inpath);
		} else {
			return 0;
		}
	}
}

int in_file_byte_size(char* parent_path, char* in_path)
{
	int f = open(in_path, O_RDONLY);
	struct stat s;
	if(!fstat(f, &s)) {
		return s.st_size;
	}
	return 0;
}

int in_dir_byte_size(char* parent_path, char* in_path)
{
	int size = 0;
	DIR* d = opendir(in_path);
	if(d == NULL){
		return -1;
	}
	struct dirent* dent;
	dent = readdir(d);
	struct stat s;
	while(dent != NULL) {
		char* file = dent->d_name;
		int path_sz = 0;
		path_sz += strlen(parent_path);
		path_sz++;
		path_sz += strlen(in_path);
		path_sz++;
		path_sz += strlen(file);
		char* path = calloc(path_sz, 1);
		strcat(path, parent_path);
		strcat(path, "/");
		strcat(path, in_path);
		strcat(path, "/");
		strcat(path, file);
		if(strcmp(file, ".") && strcmp(file, "..") && !stat(file, &s)){
			if(S_ISDIR(s.st_mode) || S_ISREG(s.st_mode)){
				size += in_byte_size(parent_path, path);
			}
		}
		dent = readdir(d);
		free(path);
	}
	closedir(d);
	return size;
}

void encode_dir(DIR* d, FILE* out)
{

}

// Encode a file `f` to `out`
void encode_file(FILE* f, FILE* out)
{

}
