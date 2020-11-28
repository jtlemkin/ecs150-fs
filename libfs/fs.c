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

// The failable macro propogates a subfunctions failure to the calling function
#define failable(result)						\
do {											\
	if (result == -1) { 						\
		perror(#result); 						\
		return -1;								\
	}											\
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

struct __attribute__((__packed__)) fat {
	uint16_t entries[2048];
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
struct fat *fat = NULL;
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

	failable(block_read(0, superblock));
	if (!is_valid_superblock(superblock)) {
		printf("Error reading superblock with signature: ");
		print_signature(superblock);
		return -1;
	}

	return 0;
}

int fat_read() {
	int i;

	fat = (struct fat*)malloc(superblock->num_fat * sizeof(struct fat));
	if (!fat) {
		perror("fs_mount fat array: ");
		return -1;
	}
	for (i = 0; i < superblock->num_fat; ++i) {
		failable(block_read(1 + i, fat + i));
	}

	return 0;
}

int root_dir_read() {
	root_dir = (struct root_dir*)malloc(sizeof(struct root_dir));
	if (!root_dir) {
		perror("fs_mount root_dir: ");
		return -1;
	}
	failable(block_read(superblock->num_fat + 1, root_dir));

	return 0;
}

int fs_mount(const char *diskname)
{
	failable(block_disk_open(diskname));
	failable(superblock_read());
	failable(fat_read());
	failable(root_dir_read());

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

	failable(block_disk_close());

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

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
	return -1;
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
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

