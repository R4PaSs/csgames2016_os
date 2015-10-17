#include <dirent.h>

// File metadata
typedef struct filemeta {
	// Must be 0x40 for a file
	char file_magic;
	// Parent directory id, unsigned 32 bits
	uint32_t parent_dir;
	// File name in UTF-8
	char filename[50];
	// Location of the file in extents
	uint32_t location;
	// File size in extents
	uint32_t ext_size;
	// File size in bytes
	uint32_t byte_size;
} filemeta;

// A data block for a file
typedef struct filedata {
	// Location in extents of the file data
	uint32_t location;
} filedata;

// A File partition
typedef struct filepart {
	// Size of the size of the volume
	char volume_volume_sz;
	// Size of the volume in extents (maximum 8 bytes)
	uint64_t size_of_the_volume;
	// Size of an extent
	int ext_size;
	// Version of the volume
	char version[7];
	// Name of the volume
	char name[40];
	// Size of the file_hierarchy
	uint64_t file_hierarchy_sz;
	// Type of encoding for the data
	char data_enc;
	// Length of the mask (if necessary)
	char masklen;
	// Mask data (length is known with masklen)
	char* mask;
} filepart;

char* parse_out(int argc, char** argv);
int parse_ext(int argc, char** argv);
void make_filesystem(char* root_path, char* out_path, filepart fp);
void build_header(FILE* out, filepart fp);
void encode_file(FILE* f, FILE* out);
void encode_dir(DIR* d, FILE* out);
int in_byte_size(char* parent_path, char* inpath);
int in_file_byte_size(char* parent_path, char* in);
int in_dir_byte_size(char* parent_path, char* in_path);
