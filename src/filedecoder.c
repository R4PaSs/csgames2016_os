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
	// Forward to file hierarchy
	move_to_extent(in, 1, part);
	dir_meta* dirs = parse_file_hierarchy(in, part);
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

dir_meta* parse_file_hierarchy(FILE* in, partition* fsmeta)
{
	unsigned char* ext = malloc(fsmeta->ext_size * sector_size);
	fread(ext, fsmeta->ext_size, sector_size, in);
	if(ext[0] != 0x80) {
		printf("Error: Top entity is not a directory !\n");
		exit(-1);
	}
	if(ext[1] != 0x00) {
		printf("Error: First entity in file hierarchy must be a root directory with 0 as ID.\n");
		exit(-2);
	}
	dir_meta *root = malloc(sizeof(dir_meta));
	root->name = "root";
	root->id = 0;
	root->parent_id = 0;
	root->ext_location = 1;
	directory* rootdir = calloc(sizeof(directory), 1);
	int i;
	for(i = 0; i < (fsmeta->hierarchy_size - 1); i++) {
		move_to_extent(in, i + 2, fsmeta);
		fread(ext, fsmeta->ext_size, sector_size, in);
		if(ext[0] == 0x40) {
			file_meta* fm = read_file_info(ext);
			dump_filemeta(fm);
		} else if(ext[0] == 0x80) {
			dir_meta* dm = read_folder_info(ext);
			dump_dirmeta(dm);
		} else {
			printf("Bad data chunk.\n");
			exit(-3);
		}
	}
}

dir_meta* read_folder_info(unsigned char* ext)
{
	dir_meta* ret = malloc(sizeof(dir_meta));
	ret->id = u32be_to_le(ext + 1);
	ret->parent_id = u32be_to_le(ext + 5);
	ret->name = calloc(50, 1);
	memcpy(ret->name, ext+9, 50);
	ret->ext_location = u64me_to_le(ext + 59);
	return ret;
}

file_meta* read_file_info(unsigned char* ext)
{
	file_meta* ret = malloc(sizeof(file_meta));
	ret->id = u32be_to_le(ext + 1);
	ret->parent_id = u32be_to_le(ext + 5);
	ret->name = calloc(50, 1);
	memcpy(ret->name, ext + 9, 50);
	ret->ext_location = u64me_to_le(ext + 59);
	ret->ext_size = u64me_to_le(ext + 67);
	ret->byte_size = u64me_to_le(ext + 75);
	return ret;
}

void move_to_extent(FILE* in, uint64_t extent_id, partition* meta)
{
	fseek(in, meta->ext_size * sector_size * extent_id, SEEK_SET);
}

directory* find_dir_id(uint32_t id, directory* dir)
{
	int i = 0;
	for(i = 0; i < dir->children_dirs_nb; i++){
		directory* dirdir = dir->children_dirs[i];
		dir_meta* dirmet = dirdir->meta;
		if(dirmet->id == id)
			return dirdir;
		directory* retdir = find_dir_id(id, dirdir);
		if(retdir != NULL)
			return retdir;
	}
	return NULL;
}

void add_child_dir_to_dir(dir_meta* meta, directory* dir)
{
	int sz = ++dir->children_dirs_nb;
	directory* ndir = calloc(sizeof(directory), 1);
	ndir->meta = meta;
	directory** ndirs = malloc(sz * sizeof(void*));
	if(sz > 1) {
		free(dir->children_dirs);
		memcpy(ndirs, dir->children_dirs, (sz - 1) * sizeof(void*));
	}
	ndirs[sz - 1] = ndir;
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

void dump_dirmeta(dir_meta *dir)
{
	printf("Dir meta dump:\n");
	printf("- ID: %d\n", dir->id);
	printf("- Parent ID: %d\n", dir->parent_id);
	printf("- Name: %s\n", dir->name);
	printf("- Extent: %ld\n", dir->ext_location);
}

void dump_filemeta(file_meta *file)
{
	printf("File meta dump:\n");
	printf("- ID: %d\n", file->id);
	printf("- Parent ID: %d\n", file->parent_id);
	printf("- Name: %s\n", file->name);
	printf("- Extent: %ld\n", file->ext_location);
	printf("- Extent Size: %ld\n", file->ext_size);
	printf("- Byte Size: %ld\n", file->byte_size);
}
