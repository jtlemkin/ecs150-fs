#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 65535
#define FAT_SIZE 2048

// The FAILABLE macro propogates a subfunctions failure to the calling function
#define FAILABLE(result)						\
do {											\
	if (result == -1) { 						\
		perror(#result); 						\
		return -1;								\
	} 											\
} while(0)

struct __attribute__((__packed__)) superblock {
	uint8_t signature[8];
	uint16_t num_blocks_disk;
	uint16_t root_i;
	uint16_t data_i;
	uint16_t num_data;
	uint8_t num_fat;
	uint8_t padding[4079];
};

struct __attribute__((__packed__)) fat_block {
	uint16_t entries[FAT_SIZE];
};

struct __attribute__((__packed__)) file_entry {
	uint8_t fname[FS_FILENAME_LEN];
	uint32_t fsize;
	uint16_t first_block_i;
	uint8_t padding[10];
};

struct __attribute__((__packed__)) root_dir {
	struct file_entry entries[FS_FILE_MAX_COUNT];
};

struct superblock *superblock = NULL;
struct fat_block *fat = NULL;
struct root_dir *root_dir = NULL;

void print_signature(struct superblock *superblock) {
	int i;

	for (i = 0; i < 8; i++) {
		printf("%c", superblock->signature[i]);
	}

	printf("\n");
}

bool is_valid_superblock(struct superblock *superblock) {
	int i;

	// ecs150fs is the hexadecimal representation of the string "ECS150FS"
	uint8_t target[8] = {'E', 'C', 'S', '1', '5', '0', 'F', 'S'};

	for (i = 0; i < 8; i++) {
		if ((superblock->signature)[i] != target[i]) {
			return false;
		}
	}

	return true;
}

bool is_disk_opened() {
	return superblock != NULL;
}

int superblock_read() {
	superblock = (struct superblock*)malloc(sizeof(struct superblock));
	if (!superblock) {
		perror("fs_mount superblock: ");
		return -1;
	}

	FAILABLE(block_read(0, superblock));
	if (!is_valid_superblock(superblock)) {
		printf("Error reading superblock with signature: ");
		print_signature(superblock);
		return -1;
	}

	return 0;
}

int fat_read() {
	int i;

	fat = (struct fat_block*)malloc(superblock->num_fat * sizeof(struct fat_block));
	if (!fat) {
		perror("fs_mount fat array: ");
		return -1;
	}
	for (i = 0; i < superblock->num_fat; ++i) {
		FAILABLE(block_read(1 + i, fat + i));
	}

	return 0;
}

int root_dir_read() {
	root_dir = (struct root_dir*)malloc(sizeof(struct root_dir));
	if (!root_dir) {
		perror("fs_mount root_dir: ");
		return -1;
	}
	FAILABLE(block_read(superblock->num_fat + 1, root_dir));

	return 0;
}

int fs_mount(const char *diskname)
{
	FAILABLE(block_disk_open(diskname));
	FAILABLE(superblock_read());
	FAILABLE(fat_read());
	FAILABLE(root_dir_read());

	return 0;
}

int fs_umount(void)
{
	// TODO: save edits
	free(fat);
	fat = NULL;

	free(superblock);
	superblock = NULL;

	free(root_dir);
	root_dir = NULL;

	FAILABLE(block_disk_close());

	return 0;
}

int fs_info(void)
{
	if (!is_disk_opened()) {
		return -1;
	}

	printf("Signature: ");
	print_signature(superblock);
	printf("Num blocks on disk: %" PRIu16 "\n", superblock->num_blocks_disk);
	printf("Num data blocks: %" PRIu16 "\n", superblock->num_data);
	printf("Num fat %" PRIu8 "\n", superblock->num_fat);

	return 0;
}

int first_free_index() {
	int i, j;

	for (i = 0; i < superblock->num_fat; ++i) {
		for (j = 0; j < FAT_SIZE; ++j) {
			if (fat[i].entries[j] == 0) {
				return i * FAT_SIZE + j;
			}
		}
	}

	return -1;
}

// Checks to see if root directory has a file with the same name already
// Additionally it also finds the first free index for a file
bool is_filename_unique(const char* filename, size_t *first_free_index) {
	int i;
	bool first_found = false;

	for (i = 0; i < FS_FILE_MAX_COUNT; ++i) {
		if (!first_found && root_dir->entries[i].fname[0] == '\0') {
			*first_free_index = i;
			first_found = true;
		}

		if (strcmp(root_dir->entries[i].fname, filename) == 0) {
			return false;
		}
	}

	return true;
}

// Returns -1 if file not found
int first_index_of_filename(const char* filename) {
	int i;

	for (i = 0; i < FS_FILE_MAX_COUNT; ++i) {
		if (strcmp(root_dir->entries[i].fname, filename) == 0) {
			return i;
		}
	}

	return -1;
}

void create_file(const char *filename, int index) {
	strcpy(root_dir->entries[index].fname, filename);
	root_dir->entries[index].fsize = 0;
	root_dir->entries[index].first_block_i = FAT_EOC;
}

int fs_create(const char *filename)
{
	int file_index;

	if (!is_filename_unique(filename, &file_index)) {
		return -1;
	}

	int fat_i = first_free_index();
	if (fat_i == -1) {
		return -1;
	}

	create_file(filename, file_index);
	
	return -1;
}

uint16_t* fat_entry_at_index(int index) {
	int fat_index = index / FAT_SIZE;
	int entry_index = index % FAT_SIZE;

	return fat[fat_index] + entry_index;
}

void clear_blocks(struct file_entry *file) {
	int data_index = file->first_block_i;
	uint8_t *empty_buffer = (uint8_t*)calloc(BLOCK_SIZE, sizeof(uint8_t));

	while (data_index != FAT_EOC) {
		block_write(data_index, empty_buffer);
		data_index = fat->entries[data_index];
		fat->entries[data_index] = 0;
	}

	free(empty_buffer);
}

int fs_delete(const char *filename)
{
	// TODO Phase 3 check to see if file is open
	int file_index = first_index_of_filename(filename);

	clear_blocks(&(root_dir->entries[file_index]));

	memset(root_dir->entries + file_index, 0, sizeof(struct file_entry));

	block_write(superblock->num_fat + 1, root_dir);

	return -1;
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
	return -1;
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
	return -1;
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
	return -1;
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
	return -1;
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
	return -1;
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
	return -1;
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
	return -1;
}

