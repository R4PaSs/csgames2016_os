#include <dirent.h>

// File metadata
typedef struct filemeta {
	// Parent directory id, unsigned 32 bits
	uint32_t parent_dir;
	// File name in UTF-8
	char filename[50];
	// Location of the file in extents
	uint64_t data_location;
	// File size in extents
	uint64_t ext_size;
	// File size in bytes
	uint64_t byte_size;
	// Path to source file
	char* src_path;
} filemeta;

// A File partition
typedef struct filepart {
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
	uint64_t data_location;
	// Children data
	metalist* children;
	// Path to source directory
	char* src_path;
} dirmeta;

char* parse_out(int argc, char** argv);
int parse_ext(int argc, char** argv);
void make_filesystem(char* root_path, char* out_path, int ext_sz);
int in_byte_size(char* parent_path, char* inpath);
int in_file_byte_size(char* parent_path, char* in);
int in_dir_byte_size(char* parent_path, char* in_path);
char* join(char** strings, int len, char* sep);
dirmeta* build_hierarchy(char* top_path);
void add_file_to(char* path, dirmeta* d);
int add_folder_to(char* path, dirmeta* d, int next);
uint32_t file_byte_to_extent_size(long byte_size);
void append_meta_to_list(metalist* lst, metalist* node);
void write_fs_to(dirmeta* d, char* out_path, int ext_sz);
int hierarchy_size(dirmeta* d);
int data_size(dirmeta* d, int extent_size);
void write_fs_info(filepart* fs, FILE* out);
void write_hierarchy_to(dirmeta* d, FILE* out, char* extent, int ext_size);
void write_filemeta_to(filemeta* f, FILE* out, char* extent, int ext_size);
void u64le_to_be(uint64_t data, char* b);
void u32le_to_be(uint32_t data, char* b);
void u64le_to_me(uint64_t data, char* b);
