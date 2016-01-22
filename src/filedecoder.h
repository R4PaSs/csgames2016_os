static int sector_size = 512;

typedef struct partition {
	uint64_t size;
	char ext_size;
	char volume_name[40];
	uint64_t hierarchy_size;
	char encryption_type;
	char mask_sz;
	char* mask_data;
} partition;

typedef struct dir_meta {
	uint32_t id;
	uint32_t parent_id;
	char* name;
	uint64_t ext_location;
} dir_meta;

typedef struct file_meta {
	uint32_t id;
	uint32_t parent_id;
	char* name;
	uint64_t ext_location;
	uint64_t ext_size;
	uint64_t byte_size;
} file_meta;

typedef struct directory {
	dir_meta* meta;
	int children_dirs_nb;
	dir_meta** children_dirs;
	int children_files_nb;
	file_meta** children_files;
} directory;

void parse_filesystem(char *inpath, char* outpath);
partition* parse_part_info(FILE *in);
void add_child_dir_to_dir(dir_meta*, directory*);
void add_child_file_to_dir(file_meta*, directory*);
//Debug function, prints the contents of `part`
void dump_partition(partition *part);
