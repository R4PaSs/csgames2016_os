#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "filecoder.h"
#include <dirent.h>
#include <string.h>
#include <libgen.h>

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
			//printf("%d\n", in_byte_size(".", argv[1]));
			make_filesystem(argv[1], out);
		} else {
			printf("Bad argument %s, expected path to a file or a directory\n", argv[1]);
		}
	} else {
		printf("Error: syscall to stat failed, are you sure %s exists ?", argv[1]);
	}
}

// Checks if option '-o' is used, defaults to "out"
char* parse_out(int argc, char** argv)
{
	int i;
	for(i = 1; i < (argc - 1); i++) {
		if(!strcmp(argv[i], "-o")){
			return argv[i + 1];
		}
	}
	return "out.coal";
}

// Checks if option '-e' is used, defaults to 8
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

// Makes the filesystem from the files or directories a
// `root_path` and outputs the binary data to `out_path`
void make_filesystem(char* root_path, char* out_path)
{
	int byte_size = in_byte_size(".", root_path);
	FILE* out = fopen(out_path, "w");
	dirmeta* top = build_hierarchy(root_path);
	write_fs_to(top, out_path);
	struct stat s;
}

// Builds an in-memory file-hierarchy with a few metadata
dirmeta* build_hierarchy(char* top_path) {
	dirmeta* top = calloc(1, sizeof(dirmeta));
	top->dir_id = 0;
	struct stat s;
	int next_id = 1;
	if(!stat(top_path, &s)) {
		if(S_ISDIR(s.st_mode)) {
			next_id = add_folder_to(top_path, top, next_id);
		} else if (S_ISREG(s.st_mode)) {
			add_file_to(top_path, top);
		}
	}
}

// Adds folder at `path` to the hierarchy of `d` with id `next`
int add_folder_to(char* path, dirmeta* d, int next) {
	dirmeta* curr = calloc(1, sizeof(dirmeta));
	curr->parent_dir = d->dir_id;
	curr->dir_id = next;
	curr->src_path = path;
	metalist* m = calloc(1, sizeof(metalist));
	m->magic = 0x80;
	m->metadata = curr;
	if(d->children != NULL) {
		append_meta_to_list(d->children, m);
	}else{
		d->children = m;
	}
	char* bn = basename(path);
	int nlen = strlen(bn);
	if(nlen >= 50) nlen = 49;
	memmove(curr->dir_name, bn, nlen);
	next += 1;
	DIR* dir = opendir(path);
	struct dirent* dent = readdir(dir);
	struct stat st;
	while(dent != NULL) {
		char* bn = dent->d_name;
		char* strs[] = {path, bn};
		char* s = join(strs, 2, "/");
		if(strcmp(bn, ".") && strcmp(bn, "..") && !stat(s, &st)) {
			if(S_ISDIR(st.st_mode)) {
				next = add_folder_to(s, curr, next);
			} else if(S_ISREG(st.st_mode)) {
				add_file_to(s, curr);
			}
		}
		dent = readdir(dir);
		free(s);
	}
	closedir(dir);
	return next;
}

// Adds file at `path` to the hierarchy of `d`
void add_file_to(char* path, dirmeta* d) {
	filemeta* fm = calloc(1, sizeof(filemeta));
	fm->parent_dir = d->dir_id;
	fm->byte_size = in_byte_size("", path);
	fm->src_path = path;
	char* bn = basename(path);
	int bnln = strlen(bn);
	metalist* m = calloc(1, sizeof(metalist));
	m->magic = 0x40;
	m->metadata = fm;
	if(d->children != NULL) {
		append_meta_to_list(d->children, m);
	}else{
		d->children = m;
	}
}

void append_meta_to_list(metalist* lst, metalist* node) {
	metalist* lnk = lst;
	while(lnk->next != NULL) {
		lnk = lnk->next;
	}
	lnk->next = node;
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
	char* ss[] = {parent_path, in_path};
	char* path = join(ss, 2, "/");
	int f = open(path, O_RDONLY);
	struct stat s;
	if(!fstat(f, &s)) {
		return s.st_size;
	}
	free(path);
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
		char* strs[] = {parent_path, in_path, file};
		char* path = join(strs, 3, "/");
		if(strcmp(file, ".") && strcmp(file, "..") && !stat(path, &s)){
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

// Joins an array of strings using `sep`
char* join(char** strings, int len, char* sep)
{
	int i;
	if(len == 0) return "";
	int endlen = 0;
	for(i = 0; i < len; i++) {
		endlen += strlen(strings[i]);
	}
	endlen += strlen(sep) * (len - 1);
	char* endstr = calloc(endlen + 1, 1);
	strcat(endstr, strings[0]);
	for(i = 1; i < len; i++) {
		strcat(endstr, sep);
		strcat(endstr, strings[i]);
	}
	return endstr;
}
