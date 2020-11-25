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

struct __attribute__((__packed__)) root_dir {
	uint8_t fname[16];
	uint32_t fsize;
	uint16_t first_block_i;
	uint8_t padding[10];
};

struct superblock superblock;
struct fat* fat = NULL;
struct root_dir root_dir;

bool is_valid_superblock(struct superblock *superblock) {
	int i;

	// ecs150fs is the hexadecimal representation of the string "ECS150FS"
	uint8_t ecs150fs[8] = {'E', 'C', 'S', '1', '5', '0', 'F', 'S'};

	for (i = 0; i < 8; i++) {
		if (superblock->signature[i] != ecs150fs[i]) {
			return false;
		}
	}

	return true;
}

void print_signature(struct superblock * superblock) {
	int i = 0;

	for (i = 0; i < 8; i++) {
		printf("%c", superblock->signature[i]);
	}

	printf("\n");
}

bool is_disk_opened() {
	return fat != NULL;
}

int fs_mount(const char *diskname)
{
	int i;

	if (block_disk_open(diskname) == -1) {
		return -1;
	}

	// Read in superblock
	block_read(0, &superblock);
	if (!is_valid_superblock(&superblock)) {
		printf("Error reading superblock with signature: ");
		//print_signature(&superblock);
		return -1;
	}

	// Read in fat
	fat = (struct fat*)malloc(sizeof(struct fat));
	if (!fat) {
		perror("fs_mount: ");
		return -1;
	}

	for (i = 0; i < superblock.num_fat; ++i) {
		block_read(1 + i, fat + i);
	}

	// Read in root_dir
	block_read(superblock.num_fat + 2, &root_dir);

	return 0;
}

int fs_umount(void)
{
	// TODO: save edits

	free(fat);
	fat = NULL;

	memset(&superblock, 0, sizeof(struct superblock));
	memset(&root_dir, 0, sizeof(struct root_dir));

	if (block_disk_close() == -1) {
		return -1;
	}

	return 0;
}

int fs_info(void)
{
	if (!is_disk_opened()) {
		return -1;
	}

	printf("Signature: ");
	//print_signature(&superblock);
	printf("Num blocks on disk: %" PRIu16 "\n", superblock.num_blocks_disk);
	printf("Num data blocks: %" PRIu16 "\n", superblock.num_data);
	printf("Num fat %" PRIu8 "\n", superblock.num_fat);

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

