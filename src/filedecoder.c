#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "utils.h"
#include "filedecoder.h"
#include <dirent.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>

int main(int argc, char** argv)
{
	if(argc < 2){
		printf("Usage: ./filedecoder file [-o outputfolder]\n");
		exit(1);
	}
	char* out = parse_out(argc, argv);
	if(strcmp(out, "out.coal") == 0) {
		out = "newdump";
	}
	sector_size = parse_sec(argc, argv);
	struct stat s;
	int ln = strlen(argv[1]);
	char* path = calloc(1, ln + 1);
	memcpy(path, argv[1], ln);
	if(!stat(argv[1], &s)) {
		if(S_ISDIR(s.st_mode) || S_ISREG(s.st_mode)) {
			parse_filesystem(path, out);
		} else {
			printf("Bad argument %s, expected path to a file or a directory\n", path);
			free(path);
		}
	} else {
		printf("Error: syscall to stat failed, are you sure %s exists ?", path);
		free(path);
	}
}

void parse_filesystem(char *inpath, char* outpath)
{
	FILE* in = fopen(inpath, "r");
	partition* part = parse_part_info(in);
	dump_partition(part);
	fclose(in);
	in = fopen(inpath, "r");
	// Forward to file hierarchy
	fseek(in, part->ext_size * sector_size, SEEK_SET);
}

partition* parse_part_info(FILE *in)
{
	partition* ret = malloc(sizeof(partition));
	char *buffer = calloc(sector_size, 1);
	fread(buffer, sector_size, 1, in);
	ret->size = u64me_to_le(buffer);
	ret->ext_size = buffer[8];
	memcpy(ret->volume_name, (buffer + 9), 40);
	ret->hierarchy_size = u64be_to_le(buffer + 49);
	ret->encryption_type = *(buffer + 57);
	if(ret->encryption_type == 0x40) {
		int mask_sz = *(buffer + 58);
		char* mask_data = malloc(mask_sz);
		memcpy(mask_data, buffer + 59, mask_sz);
		ret->mask_sz = mask_sz;
		ret->mask_data = mask_data;
	} else {
		ret->mask_sz = 0;
	}
	return ret;
}

dir_meta* parse_file_hierarchy(FILE* in, partition* fsmeta) {
	char* ext = malloc(fsmeta->ext_size * sector_size);
	fread(ext, fsmeta->ext_size, sector_size, in);
	if(ext[0] != 0x00) {
		printf("Error: First entity in file hierarchy must be a root directory with 0 as ID.\n");
		exit(-2);
	}
}

void add_child_dir_to_dir(dir_meta* meta, directory* dir)
{
	int sz = ++dir->children_dirs_nb;
	dir_meta** ndirs = malloc(sz * sizeof(void*));
	memcpy(ndirs, dir->children_dirs, (sz - 1) * sizeof(void*));
	ndirs[sz - 1] = meta;
	free(dir->children_dirs);
	dir->children_dirs = ndirs;
}

void add_child_file_to_dir(file_meta* meta, directory* dir)
{
	int sz = ++dir->children_files_nb;
	file_meta** ndirs = malloc(sz * sizeof(void*));
	memcpy(ndirs, dir->children_files, (sz - 1) * sizeof(void*));
	ndirs[sz - 1] = meta;
	free(dir->children_dirs);
	dir->children_files = ndirs;
}

void dump_partition(partition* part) {
	printf("Partition %s:\n", part->volume_name);
	printf("- size: %lu\n", part->size);
	printf("- extent size: %hhu\n", part->ext_size);
	printf("- hierarchy size: %lu\n", part->hierarchy_size);
	unsigned char encryption = part->encryption_type & 0xF0;
	char* encryption_name;
	if(encryption == 0x40) {
		encryption_name = "XOR";
	} else if (encryption == 0x80) {
		encryption_name = "ROL";
	} else {
		encryption_name = "None";
	}
	printf("- encryption_type: %s\n", encryption_name);
	if(encryption == 0x40) {
		int mask_len = part->mask_sz;
		int i;
		printf("- encryption_mask: ");
		for(i = 0; i < mask_len; i ++) {
			printf("%2x", (unsigned char)(part->mask_data[i]));
		}
		printf("\n");
	} else if (encryption == 0x80) {
		printf("- encryption mask: %d\n", part->encryption_type & 0x0F);
	}
}
