#include <dirent.h>

// File metadata
typedef struct filemeta {
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
	// Path to source file
	char* src_path;
} filemeta;

// A data block for a file
typedef struct filedata {
	// First byte determines if the file data is a continuation block or not
	uint8_t continuation_header;
	// Size of the block in bytes
	uint32_t byte_size;
	// Data of the file, size up to
	char* data;
	// Next block's location (in extents)
	uint32_t continuation_location;
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

typedef struct metalist metalist;

typedef struct metalist {
	// Magic number, 0x80 is a dirmeta, 0x40 is a filemeta
	uint8_t magic;
	// Pointer to metadata, if magic is 0x80 then cast to dirmeta, else cast to filemeta
	void* metadata;
	// Pointer to the next entity
	metalist* next;
} metalist;

// Directory metadata
typedef struct dirmeta {
	// Identifier of directory in memory
	uint32_t dir_id;
	// Parent directory's id
	uint32_t parent_dir;
	// Name of the directory, must be null-terminated
	char dir_name[50];
	// Location of the data in extents
	uint32_t data_location;
	// Children data
	metalist* children;
	// Path to source directory
	char* src_path;
} dirmeta;

char* parse_out(int argc, char** argv);
int parse_ext(int argc, char** argv);
void make_filesystem(char* root_path, char* out_path);
int in_byte_size(char* parent_path, char* inpath);
int in_file_byte_size(char* parent_path, char* in);
int in_dir_byte_size(char* parent_path, char* in_path);
char* join(char** strings, int len, char* sep);
dirmeta* build_hierarchy(char* top_path);
void add_file_to(char* path, dirmeta* d);
int add_folder_to(char* path, dirmeta* d, int next);
uint32_t file_byte_to_extent_size(long byte_size);
void append_meta_to_list(metalist* lst, metalist* node);
filedata* encode_file(filepart* fp, char* f);
void write_fs_to(dirmeta* d, char* out_path);
