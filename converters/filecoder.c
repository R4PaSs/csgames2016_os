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
#include <unistd.h>

int main(int argc, char** argv)
{
	if(argc < 2){
		printf("Usage: ./filecoder file [-o outputfile] [-e extent_size] [-s sector_size] [-c none|xor|rol key?]\n");
		exit(1);
	}
	char* out = parse_out(argc, argv);
	extent_size = parse_ext(argc, argv);
	sector_size = parse_sec(argc, argv);
	parse_enc(argc, argv);
	struct stat s;
	int ln = strlen(argv[1]);
	char* path = calloc(1, ln + 1);
	memcpy(path, argv[1], ln);
	if(!stat(argv[1], &s)) {
		if(S_ISDIR(s.st_mode) || S_ISREG(s.st_mode)) {
			make_filesystem(path, out);
		} else {
			printf("Bad argument %s, expected path to a file or a directory\n", path);
			free(path);
		}
	} else {
		printf("Error: syscall to stat failed, are you sure %s exists ?", path);
		free(path);
	}
}

// Parse the encryption method of the data
void parse_enc(int argc, char** argv)
{
	int i;
	for(i = 0; i < (argc - 1); i++) {
		if(!strcmp(argv[i], "xor")) {
			encryption = 0x40;
			int keyln = strlen(argv[i + 1]);
			if(keyln > 255) {
				keyln = 255;
			}
			xor_key = calloc(1, keyln + 1);
			memcpy(xor_key, argv[i + 1], keyln);
			return;
		} else if(!strcmp(argv[i], "rol")) {
			encryption = 0x80;
			rol_len = atoi(argv[i + 1]) % 8;
			return;
		}
	}
	encryption = 0x00;
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

// Checks if option '-s' is used, defaults to 512
int parse_sec(int argc, char** argv)
{
	int i = 0;
	for(i = 1; i < (argc - 1); i++) {
		if(!strcmp(argv[i], "-s")) {
			return atoi(argv[i + 1]);
		}
	}
	return 512;
}

// Encrypts an extent using either ROL or XOR
void encrypt_extent(char* extent)
{
	if(encryption == 0x40)
		xor_extent(extent);
	if(encryption == 0x80)
		rol_extent(extent);
}

// Does a ROL on every byte of the extent
void rol_extent(char* extent)
{
	int i;
	int ext_sz = extent_size * sector_size;
	for(i = 0; i < ext_sz; i++) {
		extent[i] = (extent[i] << rol_len) | (extent[i] >> (8 - rol_len));
	}
}

// XOR each byte of an extent with a char of the key
void xor_extent(char* extent)
{
	int keylen = strlen(xor_key);
	int i;
	int ext_sz = extent_size * sector_size;
	for(i = 0; i < ext_sz; i++) {
		extent[i] = extent[i] ^ xor_key[i % keylen];
	}
}

// Makes the filesystem from the files or directories a
// `root_path` and outputs the binary data to `out_path`
void make_filesystem(char* root_path, char* out_path)
{
	int byte_size = in_byte_size(".", root_path);
	dirmeta* top = build_hierarchy(root_path);
	write_fs_to(top, out_path);
}

// Assign data start extents to each entity in hierarchy
uint32_t assign_start_ext_to_meta(dirmeta* d, uint32_t from)
{
	//printf("Assigning start extent %d to directory %s\n", from, d->src_path);
	d->data_location = from;
	uint32_t nxt = from + 1;
	metalist* m = d->children;
	while(m != NULL) {
		if(m->magic == 0x80) {
			nxt = assign_start_ext_to_meta((dirmeta*)m->metadata, nxt);
		} else if(m->magic == 0x40) {
			((filemeta*) m->metadata)->data_location = nxt;
			//printf("Assigning start extent %d to file %s\n", nxt, ((filemeta*)m->metadata)->src_path);
			nxt += 1;
		} else {
			printf("Fatal error: item not a directory or file in file hierarchy.\n");
			exit(-1);
		}
		m = m->next;
	}
	return nxt;
}

// Write file system to `path`
void write_fs_to(dirmeta* d, char* path)
{
	filepart* fs = calloc(1, sizeof(filepart));
	fs->ext_size = extent_size;
	memmove(fs->name, "test.coal", strlen("test.coal"));
	fs->file_hierarchy_sz = hierarchy_size(d);
	fs->data_enc = 0x00;
	int data_sz = data_size(d);
	fs->size_of_the_volume = data_sz + fs->file_hierarchy_sz + 1;
	FILE* fout = fopen(path, "w");
	write_fs_info(fs, fout);
	char* ext = malloc(extent_size * sector_size);
	assign_start_ext_to_meta(d, fs->file_hierarchy_sz);
	write_hierarchy_to(d, fout, ext);
	writequeue* w = build_writequeue(d);
	commit_data_to_disk(w, fout, ext);
	free(ext);
	free(fs);
	fclose(fout);
}

// Write first extent to `out`
void write_fs_info(filepart* fs, FILE* out)
{
	int ext_bytesz = fs->ext_size * sector_size;
	char* extent = calloc(1, ext_bytesz);
	u64le_to_me(fs->size_of_the_volume, extent);
	extent[8] = fs->ext_size;
	memcpy(extent + 9, fs->name, 40);
	u64le_to_be(fs->file_hierarchy_sz, extent + 49);
	extent[57] = encryption;
	if(encryption == 0x40) {
		int keyln = strlen(xor_key);
		extent[58] = keyln;
		memcpy(extent + 59, xor_key, keyln);
	} else if(encryption == 0x80) {
		extent[57] |= rol_len;
	}
	fwrite(extent, 1, ext_bytesz, out);
	free(extent);
}

// Write metadata to `out`, `extent` is used as a temporary buffer to store data
void write_hierarchy_to(dirmeta* d, FILE* out, char* extent)
{
	memset(extent, 0, extent_size * sector_size);
	extent[0] = 0x80;
	u32le_to_be(d->dir_id, extent + 1);
	u32le_to_be(d->parent_dir, extent + 5);
	memcpy(extent + 9, d->dir_name, 50);
	u64le_to_be(d->data_location, extent + 59);
	encrypt_extent(extent);
	fwrite(extent, 1, extent_size * sector_size, out);
	metalist* m = d->children;
	while(m != NULL) {
		if(m->magic == 0x80) {
			write_hierarchy_to((dirmeta*)m->metadata, out, extent);
		} else if(m->magic == 0x40) {
			write_filemeta_to((filemeta*)m->metadata, out, extent);
		} else {
			printf("Error while writing directory meta, found invalid child\n");
			exit(-1);
		}
		m = m->next;
	}
}

// Write a file's metadata to `out`, `extent` is used as a temporary buffer to store data
void write_filemeta_to(filemeta* f, FILE* out, char* extent)
{
	memset(extent, 0, extent_size * sector_size);
	extent[0] = 0x40;
	u32le_to_be(f->file_id, extent + 1);
	u32le_to_be(f->parent_dir, extent + 5);
	memcpy(extent + 9, f->filename, 50);
	u64le_to_me(f->data_location, extent + 59);
	u64le_to_me(f->ext_size, extent + 67);
	u64le_to_me(f->byte_size, extent + 75);
	encrypt_extent(extent);
	fwrite(extent, 1, extent_size * sector_size, out);
}

// Builds a write queue for the data
writequeue* build_writequeue(dirmeta* d)
{
	writequeue* curr = make_dirnode(d);
	metalist* m = d->children;
	while(m != NULL) {
		if(m->magic == 0x80) {
			add_dir_to_queue((dirmeta*)m->metadata, curr);
		} else if(m->magic == 0x40) {
			append_to_writequeue(curr, make_filenode((filemeta*)m->metadata));
		}
		m = m->next;
	}
	return curr;
}

// Adds a directory write node to `w`
//
// Recursively parses the info from the metadata contained in `d`
void add_dir_to_queue(dirmeta* d, writequeue* w)
{
	writequeue* curr = make_dirnode(d);
	append_to_writequeue(w, curr);
	metalist* m = d->children;
	while(m != NULL) {
		if(m->magic == 0x80) {
			add_dir_to_queue((dirmeta*)m->metadata, curr);
		} else if(m->magic == 0x40) {
			append_to_writequeue(w, make_filenode((filemeta*)m->metadata));
		}
		m = m->next;
	}
}

// Makes a directory write node from the data `d`
writequeue* make_dirnode(dirmeta* d)
{
	writequeue* curr = calloc(1, sizeof(writequeue));
	writedef* wd = calloc(1, sizeof(writedef));
	dirwrite* dw = calloc(1, sizeof(dirwrite));
	dw->dir = d;
	dw->children = d->children;
	wd->magic = 0x80;
	wd->meta = dw;
	wd->next_extent = d->data_location;
	curr->definition = wd;
	return curr;
}

// Makes a file write node from the data `f`
writequeue* make_filenode(filemeta* f)
{
	writequeue* curr = calloc(1, sizeof(writequeue));
	writedef* wd = calloc(1, sizeof(writedef));
	wd->magic = 0x40;
	filewrite* fw = calloc(1, sizeof(filewrite));
	FILE* fr = fopen(f->src_path, "r");
	fw->src = fr;
	fw->remaining = in_byte_size(".", f->src_path);
	fw->metadata = f;
	wd->meta = fw;
	wd->next_extent = f->data_location;
	curr->definition = wd;
	return curr;
}

// Appends `node` to `lst`
void append_to_writequeue(writequeue* lst, writequeue* node)
{
	while(lst->next != NULL) lst = lst->next;
	lst->next = node;
}

// Prints the content of a writequeue; for debugging purposes only
void debug_writequeue(writequeue* w)
{
	printf("In queue: \n");
	while(w != NULL) {
		char* s;
		char* path;
		if(w->definition->magic == 0x80) {
			s = "directory";
			path = ((dirwrite*)w->definition->meta)->dir->src_path;
		} else {
			s = "file";
			path = ((filewrite*)w->definition->meta)->metadata->src_path;
		}
		printf("%s %s: next extent = %d\n", s, path, w->definition->next_extent);
		w = w->next;
	}
}

// Writes the contents of queue `wq` to disk
void commit_data_to_disk(writequeue* wq, FILE* out, char* extent)
{
	//printf("Start commiting data to disk\n");
	//debug_writequeue(wq);
	while(wq != NULL) {
		if(wq->definition->magic == 0x80) {
			wq = write_dir_chunk(wq, out, extent);
		} else if(wq->definition->magic == 0x40) {
			wq = write_file_chunk(wq, out, extent);
		}
	}	
}

// Writes a chunk of data for the directory in `wq`
writequeue* write_dir_chunk(writequeue* wq, FILE* out, char* extent)
{
	//printf("Writing directory chunk to disk\n");
	//debug_writequeue(wq);
	int extpos = 0;
	int extentbtsz = extent_size * sector_size;
	memset(extent, 0, extentbtsz);
	dirwrite* dw = wq->definition->meta;
	dirmeta* d = dw->dir;
	metalist* s = dw->children;
	if(d->data_location == wq->definition->next_extent) {
		extent[extpos] = 0x40;
		u32le_to_be(count_children(d), extent);
		// TODO date, 14 bytes in two fields
		extpos += 19;
	} else {
		extent[extpos] = 0x80;
		extpos += 1;
	}
	metalist* m = dw->children;
	int i;
	int entries = (extentbtsz - extpos - 4) / 5;
	for(i = 0; i < entries; i++) {
		extent[extpos] = m->magic;
		if(m->magic = 0x80) {
			u32le_to_be(((dirmeta*)m->metadata)->dir_id, extent + extpos + 1);
		} else if(m->magic == 0x40) {
			u32le_to_be(((filemeta*)m->metadata)->file_id, extent + extpos + 1);
		}
		extpos += 5;
		metalist* m2 = m->next;
		free(m);
		m = m2;
		if(m == NULL) break;
	}
	if(m != NULL) {
		writequeue* w = wq;
		while(w->next != NULL) w = w->next;
		int nxt = w->definition->next_extent + 1;
		wq = w->next;
		w->definition->next_extent = nxt;
		if(wq != NULL) {
			append_to_writequeue(w, wq);
		} else {
			wq = w;
		}
	} else {
		//printf("Freeing directory path %s at address %p\n", d->src_path, d->src_path);
		writequeue* w = wq;
		wq = wq->next;
		free(d->src_path);
		free(d);
		free(dw);
		free(w->definition);
		free(w);
	}
	encrypt_extent(extent);
	fwrite(extent, 1, extentbtsz, out);
	return wq;
}

// Writes a chunk of data for the file in `wq`
writequeue* write_file_chunk(writequeue* wq, FILE* out, char* extent)
{
	//printf("Writing file chunk to disk\n");
	//debug_writequeue(wq);
	int extentbtsz = extent_size * sector_size;
	int maxdata = extentbtsz - 9;
	memset(extent, 0, extentbtsz);
	writedef* wd = wq->definition;
	filewrite* fw = wd->meta;
	filemeta* fm = fw->metadata;
	if(fm->data_location != wd->next_extent) {
		extent[0] = 0x20;
	} else {
		extent[0] = 0x00;
	}
	if(fw->remaining > maxdata) {
		u32le_to_me(maxdata, extent + 1);
	} else {
		u32le_to_me(fw->remaining, extent + 1);
	}
	int rd = fread(extent + 5, 1, maxdata, fw->src);
	fw->remaining -= rd;
	if(fw->remaining > 0) {
		if(wq->next == NULL) {
			wq->definition->next_extent += 1;
			u32le_to_be(wq->definition->next_extent, extent + extentbtsz - 5);
			encrypt_extent(extent);
			fwrite(extent, 1, extentbtsz, out);
			return wq;
		}
		//printf("Remaining space for file %s: %d\n", fm->src_path, fw->remaining);
		writequeue* w = wq;
		wq = wq->next;
		w->next = NULL;
		writequeue* lst = wq;
		while(lst->next != NULL) lst = lst->next;
		lst->next = w;
		int nxt = lst->definition->next_extent + 1;
		w->definition->next_extent = nxt;
		u32le_to_be(nxt, extent + extentbtsz - 5);
	} else {
		//printf("Freeing file path %s at address %p\n", fm->src_path, fm->src_path);
		writequeue* w = wq;
		wq = w->next;
		fclose(fw->src);
		free(fm->src_path);
		free(fm);
		free(fw);
		free(w->definition);
		free(w);
	}
	encrypt_extent(extent);
	fwrite(extent, 1, extentbtsz, out);
	return wq;
}

// Count the number of children of directory `d`
int count_children(dirmeta* d)
{
	metalist* m = d->children;
	int children = 0;
	while(m != NULL) {
		children++;
		m = m->next;
	}
	return children;
}

// Write the 16 bits of `data` to `buffer` in big-endian form
void u16le_to_be(uint16_t data, char* buffer) {
	int i = 0;
	uint8_t d;
	for(i = 0; i < 2; i++) {
		d = (data & (0xFFl << (2 * i))) >> (2 * i);
		buffer[1 - i] = d;
	}
}

// Write the 32 bits of `data` to `buffer` in big-endian form
void u32le_to_be(uint32_t data, char* buffer) {
	int i = 0;
	uint8_t d;
	for(i = 0; i < 4; i++) {
		d = (data & (0xFFl << (4 * i))) >> (4 * i);
		buffer[3 - i] = d;
	}
}

// Write the 32 bits of `data` to `buffer` in middle-endian form
void u32le_to_me(uint32_t data, char* buffer)
{
	uint16_t p1 = (data & 0xFFFF0000) >> 16;
	memcpy(buffer, &p1, 2);
	p1 = data & 0xFFFF;
	memcpy(buffer + 2, &p1, 2);
}

// Write the 64 bits of `data` to `buffer` in middle-endian form
void u64le_to_me(uint64_t data, char* buffer)
{
	uint32_t p1 = (data & 0xFFFFFFFF00000000) >> 32;
	memcpy(buffer, &p1, 4);
	p1 = data & 0xFFFFFFFF;
	memcpy(buffer + 4, &p1, 4);
}

// Write the 64 bits of `data` to `buf` in big-endian form
void u64le_to_be(uint64_t data, char* buf)
{
	int i = 0;
	uint8_t d;
	for(i = 0; i < 8; i++) {
		d = (data & (0xFFl << (8 * i))) >> (8 * i);
		buf[7 - i] = d;
	}
}

// Computes the size of the data in extents
int data_size(dirmeta* d)
{
	int extents = 0;
	metalist* m = d->children;
	int ext_byte_size = extent_size * sector_size;
	int fl_data_chunk_sz = ext_byte_size - 1 - 4 - 4;
	int dir_data_chunk_sz = ext_byte_size - 4 - 6 - 6 - 4;
	int children = 0;
	while(m != NULL) {
		if(m->magic == 0x40) {
			filemeta* f = (filemeta*)m->metadata;
			int flsz = in_byte_size(".", f->src_path);
			int flextents = flsz / fl_data_chunk_sz;
			if((flsz % fl_data_chunk_sz) != 0) flextents += 1;
			f->ext_size = flextents;
			extents += flextents;
			f->byte_size = flsz;
		} else if (m->magic == 0x80) {
			extents += data_size((dirmeta*)m->metadata);
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
			add_file_to(top_path, top, next_id);
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
	memcpy(curr->dir_name, bn, nlen);
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
				next = add_file_to(s, curr, next);
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
int add_file_to(char* path, dirmeta* d, int next) {
	filemeta* fm = calloc(1, sizeof(filemeta));
	fm->parent_dir = d->dir_id;
	fm->byte_size = in_byte_size("", path);
	fm->src_path = path;
	fm->file_id = next;
	char* bn = basename(path);
	int bnln = strlen(bn);
	if(bnln > 49) bnln = 49;
	memcpy(fm->filename, bn, bnln);
	metalist* m = calloc(1, sizeof(metalist));
	m->magic = 0x40;
	m->metadata = fm;
	if(d->children != NULL) {
		append_meta_to_list(d->children, m);
	}else{
		d->children = m;
	}
	return next + 1;
}

// Add metadata to list
void append_meta_to_list(metalist* lst, metalist* node) {
	metalist* lnk = lst;
	while(lnk->next != NULL) {
		lnk = lnk->next;
	}
	lnk->next = node;
}

// Computes the size in bytes of `parent_path/inpath`
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

// Gets the byte size of a file
int in_file_byte_size(char* parent_path, char* in_path)
{
	char* ss[] = {parent_path, in_path};
	char* path = join(ss, 2, "/");
	int f = open(path, O_RDONLY);
	if(f == -1)
		return 0;
	struct stat s;
	if(!fstat(f, &s)) {
		return s.st_size;
	}
	close(f);
	free(path);
	return 0;
}

// Computes the size of a directory's elements
int in_dir_byte_size(char* parent_path, char* in_path)
{
	int size = 0;
	char* strs1[] = {parent_path, in_path};
	char* dirpath = join(strs1, 2, "/");
	DIR* d = opendir(dirpath);
	if(d == NULL){
		return -1;
	}
	struct dirent* dent;
	dent = readdir(d);
	struct stat s;
	while(dent != NULL) {
		char* file = dent->d_name;
		char* strs[] = {in_path, file};
		char* path = join(strs, 2, "/");
		if(strcmp(file, ".") && strcmp(file, "..") && !stat(path, &s)){
			if(S_ISDIR(s.st_mode) || S_ISREG(s.st_mode)){
				size += in_byte_size(parent_path, path);
			}
		}
		dent = readdir(d);
		free(path);
	}
	closedir(d);
	free(dirpath);
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
	//printf("Allocating path at address %p\n", endstr);
	return endstr;
}
