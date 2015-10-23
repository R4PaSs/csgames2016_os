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
			make_filesystem(argv[1], out, ext_sz);
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
void make_filesystem(char* root_path, char* out_path, int extent_size)
{
	int byte_size = in_byte_size(".", root_path);
	FILE* out = fopen(out_path, "w");
	dirmeta* top = build_hierarchy(root_path);
	write_fs_to(top, out_path, extent_size);
	struct stat s;
}

void write_fs_to(dirmeta* d, char* path, int extent_size)
{
	filepart* fs = calloc(1, sizeof(filepart));
	fs->ext_size = extent_size;
	memmove(fs->name, "test.coal", strlen("test.coal"));
	fs->file_hierarchy_sz = hierarchy_size(d);
	fs->data_enc = 0x00;
	int data_sz = data_size(d, extent_size);
	fs->size_of_the_volume = data_sz + fs->file_hierarchy_sz + 1;
	FILE* fout = fopen(path, "w");
	write_fs_info(fs, fout);
	fclose(fout);
}

void write_fs_info(filepart* fs, FILE* out)
{
	int ext_bytesz = fs->ext_size * 512;
	char* extent = calloc(1, ext_bytesz);
	char* buffer = u64le_to_me(fs->size_of_the_volume);
	memcpy(extent, buffer, 8);
	extent[8] = fs->ext_size;
	memcpy(extent + 9, fs->name, 40);
	free(buffer);
	buffer = u64le_to_be(fs->file_hierarchy_sz);
	memcpy(extent + 49, buffer, 8);
	free(buffer);
	buffer[57] = fs->data_enc;
	if(fs->data_enc == 0x40) {
		buffer[58] = fs->masklen;
		memcpy(extent + 59, fs->mask, fs->masklen);
	}
	fwrite(extent, 1, ext_bytesz, out);
}

char* u64le_to_me(uint64_t data)
{
	char* buffer = malloc(8);
	uint32_t p1 = (data & 0xFFFFFFFF00000000) >> 32;
	memcpy(buffer, &p1, 4);
	p1 = data & 0xFFFFFFFF;
	memcpy(buffer + 4, &p1, 4);
	return buffer;
}

char* u64le_to_be(uint64_t data)
{
	int i = 0;
	uint8_t d;
	char* buf = malloc(8);
	for(i = 0; i < 8; i++) {
		d = (data & (0xFFl << (8 * i))) >> (8 * i);
		buf[7 - i] = d;
	}
	return buf;
}

// Computes the size of the data in extents
int data_size(dirmeta* d, int extent_size)
{
	int extents = 0;
	metalist* m = d->children;
	int ext_byte_size = extent_size * 512;
	int fl_data_chunk_sz = ext_byte_size - 1 - 4 - 4;
	int dir_data_chunk_sz = ext_byte_size - 4 - 6 - 6 - 4;
	int children = 0;
	while(m != NULL) {
		if(m->magic == 0x40) {
			int flsz = in_byte_size(".", ((filemeta*)m->metadata)->src_path);
			extents += flsz / fl_data_chunk_sz;
			if((flsz % fl_data_chunk_sz) != 0) extents += 1;
		} else if (m->magic == 0x80) {
			extents += data_size((dirmeta*)m->metadata, extent_size);
		}
		children += 1;
		m = m->next;
	}
	children *= 4;
	extents += children / dir_data_chunk_sz;
	if((children % dir_data_chunk_sz)!= 0) extents += 1;
	return extents;
}

// Computes the size of the file hierarchy metadata in extents
int hierarchy_size(dirmeta* d)
{
	int extents = 1;
	metalist* m = d->children;
	while(m != NULL) {
		if(m->magic == 0x40) {
			extents += 1;
		}else if(m->magic = 0x80) {
			extents += hierarchy_size((dirmeta*)m->metadata);
		}
		m = m->next;
	}
	return extents;
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
	return top;
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
		} else {
			free(s);
		}
		dent = readdir(dir);
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
