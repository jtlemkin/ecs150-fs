#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 0xFFFF
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

struct fd_entry {
	int file_i;
	size_t offset;
};

struct superblock *superblock = NULL;
struct fat_block *fat = NULL;
struct root_dir *root_dir = NULL;
struct fd_entry fd_table[FS_OPEN_MAX_COUNT];

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

	//printf("First fat entry %d\n", fat[0].entries[0]);

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

void fd_table_create() {
	int i;
	for (i = 0; i < FS_OPEN_MAX_COUNT; ++i) {
		fd_table[i].file_i = -1;
	}
}

int fs_mount(const char *diskname)
{
	FAILABLE(block_disk_open(diskname));
	FAILABLE(superblock_read());
	FAILABLE(fat_read());
	FAILABLE(root_dir_read());

	fd_table_create();

	return 0;
}

void fs_backup() {
	int i = 1;

	for (i = 0; i < superblock->num_fat; i++) {
		block_write(1 + i, fat + i);
	}

	block_write(superblock->num_fat + 1, root_dir);
}

int fs_umount(void)
{
	fs_backup();

	free(fat);
	fat = NULL;

	free(superblock);
	superblock = NULL;

	free(root_dir);
	root_dir = NULL;

	FAILABLE(block_disk_close());

	return 0;
}

uint16_t* fat_entry_at_index(int index) {
	int fat_index = index / FAT_SIZE;
	int entry_index = index % FAT_SIZE;

	return fat[fat_index].entries + entry_index;
}

int num_fat_free() {
	int i, num_free;

	num_free = 0;
	for (i = 0; i < superblock->num_data; i++) {
		if (*fat_entry_at_index(i) == 0) {
			num_free += 1;
		}
	}

	return num_free;
}

int num_files_free() {
	int i, num_free;

	num_free = 0;
	for (i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (root_dir->entries[i].fname[0] == '\0') {
			num_free += 1;
		}
	}

	return num_free;
}

int fs_info(void)
{
	if (!is_disk_opened()) {
		return -1;
	}

	printf("FS Info:\n");
	printf("total_blk_count=%d\n", superblock->num_blocks_disk);
	printf("fat_blk_count=%d\n", superblock->num_fat);
	printf("rdir_blk=%d\n", superblock->num_fat + 1);
	printf("data_blk=%d\n", superblock->num_fat + 2);
	printf("data_blk_count=%d\n", superblock->num_data);
	printf("fat_free_ratio=%d/%d\n", num_fat_free(), superblock->num_data);
	printf("rdir_free_ratio=%d/%d\n", num_files_free(), FS_FILE_MAX_COUNT);

	return 0;
}

int first_free_fat_index() {
	int i = 0;
	for (i = 0; i < superblock->num_data; ++i) {
		if (*fat_entry_at_index(i) == 0) {
			return i;
		}
	}

	return -1;
}


// Returns -1 if filename already in root_dir
int new_file_index(const char* filename) {
	int i;
	int file_index = -1;

	for (i = 0; i < FS_FILE_MAX_COUNT; ++i) {
		if (strcmp((const char*) root_dir->entries[i].fname, filename) == 0) {
			return -1;
		}

		if (file_index == -1 && root_dir->entries[i].fname[0] == '\0') {
			file_index = i;
		}
	}

	return file_index;
}

// Returns -1 if file not found
int first_index_of_filename(const char* filename) {
	int i;

	for (i = 0; i < FS_FILE_MAX_COUNT; ++i) {
		if (strcmp((const char*) root_dir->entries[i].fname, filename) == 0) {
			return i;
		}
	}

	return -1;
}

void create_file(const char *filename, int index) {
	strcpy((char * restrict) root_dir->entries[index].fname, filename);
	root_dir->entries[index].fsize = 0;
	root_dir->entries[index].first_block_i = FAT_EOC;
}

int fs_create(const char *filename)
{
	// TODO: Make sure name not too long

	if (!is_disk_opened()) {
		fprintf(stderr, "Disk not opened\n");
		return -1;
	}

	size_t file_index = new_file_index(filename);
	if (file_index == -1) {
		fprintf(stderr, "Error creating file: file name not unique\n");
		return -1;
	}

	create_file(filename, file_index);

	fs_backup();
	
	return 0;
}

void clear_blocks(struct file_entry *file) {
	int data_index = file->first_block_i;

	//printf("Data index clear blocks %d\n", data_index);

	uint8_t *empty_buffer = (uint8_t*)calloc(BLOCK_SIZE, sizeof(uint8_t));

	while (data_index != FAT_EOC) {
		//printf("Adjusted index %d\n", superblock->num_fat + 1 + data_index);
		block_write(superblock->num_fat + 2 + data_index, empty_buffer);
		int old_index = data_index;
		data_index = *fat_entry_at_index(data_index);
		*fat_entry_at_index(old_index) = 0;
	}

	free(empty_buffer);
}

//int fs_ls(void);

int fs_delete(const char *filename)
{
	int i;

	if (!is_disk_opened()) {
		fprintf(stderr, "Disk not opened\n");
		return -1;
	}

	int file_index = first_index_of_filename(filename);

	// Check to see if file is open
	for (i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		if (fd_table[i].file_i == file_index) {
			return -1;
		}
	}

	//fs_ls();

	if (file_index == -1) {
		fprintf(stderr, "Unable to find file in file root directory\n");
		return -1;
	}

	//printf("findex %d\n", file_index);

	clear_blocks(root_dir->entries + file_index);

	// Clear file entry
	memset(root_dir->entries + file_index, 0, sizeof(struct file_entry));

	fs_backup();

	return 0;
}

int fs_ls(void)
{
	int i;
	struct file_entry entry;

	if (!is_disk_opened()) {
		fprintf(stderr, "fs not opened\n");
		return -1;
	}

	for (i = 0; i < FS_FILE_MAX_COUNT; i++) {
		entry = root_dir->entries[i];
		if (entry.fname[0] != '\0') {
			printf("%s %d bytes\n", entry.fname, entry.fsize);
		}
	}
	return 0;
}

// Returns -1 if max number of files are open
int first_open_fd_i() {
	int i;
	for (i = 0; i < FS_OPEN_MAX_COUNT; ++i) {
		if (fd_table[i].file_i == -1) {
			return i;
		}
	}
	return -1;
}

int fs_open(const char *filename)
{
	int fd = first_open_fd_i();
	if (fd == -1) {
		fprintf(stderr, "Unable to open file: max num files opened\n");
		return -1;
	}

	int file_i = first_index_of_filename(filename);
	if (file_i == -1) {
		fprintf(stderr, "Unable to open file: file not found\n");
	}

	fd_table[fd].file_i = file_i;
	fd_table[fd].offset = 0;

	return fd;
}

int verify_fd(int fd) {
	if (fd < 0 || fd > 31) {
		fprintf(stderr, "fd out of bounds\n");
		return -1;
	}

	if (fd_table[fd].file_i == -1) {
		fprintf(stderr, "fd not open\n");
		return -1;
	}

	return 0;
}

int fs_close(int fd)
{
	FAILABLE(verify_fd(fd));

	fd_table[fd].file_i = -1;

	return 0;
}

int fs_stat(int fd)
{
	FAILABLE(verify_fd(fd));

	return root_dir->entries[fd_table[fd].file_i].fsize;
}

int fs_lseek(int fd, size_t offset)
{
	FAILABLE(verify_fd(fd));

	fd_table[fd].offset = offset;
	
	return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	FAILABLE(verify_fd(fd));

    uint16_t *data_index_ptr = &(root_dir->entries[fd_table[fd].file_i].first_block_i);

    int startingByte = fd_table[fd].offset;

	//printf("offset %d\n", startingByte);

    int finalByte = startingByte + count - 1;

    uint8_t *bounce_buffer = (uint8_t*)calloc(BLOCK_SIZE, sizeof(uint8_t));

    int blocksIteratedOver = 0;
    int total_bytes_written = 0;
	bool newBlocksCreated = false;
    while (total_bytes_written < count) {
		// Allocate block if we are out of room
		if (*data_index_ptr == FAT_EOC){
            // now we allocate new space, and then update the data index to point to the new space.
			int new_index = first_free_fat_index();

			//printf("FIRST FREE FAT: %d\n", new_index);

			// Check to see if out of space
			if (new_index == -1) {
				fprintf(stderr, "Disk space unavailable\n");
				break;
			}

			newBlocksCreated = true;

			*data_index_ptr = new_index;
			*fat_entry_at_index(new_index) = FAT_EOC;

			//printf("Set fat entry at %d to EOC %d\n", new_index, *fat_entry_at_index(new_index));
        }

        int blockLowerBound = blocksIteratedOver * BLOCK_SIZE;
        int blockUpperBound = ((blocksIteratedOver + 1) * BLOCK_SIZE) - 1;

        // If byte upper bound is greater than starting byte, we know that
        // this block intersects with the bytes that we are trying to read
        if (blockUpperBound >= startingByte) {
            int start_write, end_write;

            if (blockLowerBound < startingByte) {
                // The start of the section to read is in the middle of the block
                start_write = startingByte - blockLowerBound;
            } else {
                start_write = 0;
            }

            if (blockUpperBound > finalByte) {
                // The end of the section to read is in the middle of the block
                end_write = finalByte - blockLowerBound;
            } else {
                end_write = BLOCK_SIZE - 1;
            }

			int block_bytes_written = end_write - start_write + 1;

			// Debug
			//fwrite(buf + total_bytes_written, 1, block_bytes_written, stdout);
			//fflush(stdout);
			//printf("\n");

			// TODO: Change block_read and block_write calls to access correct
			// data blocks

			//printf("Index: %d, %d\n", *data_index_ptr, superblock->num_fat + 2 + *data_index_ptr);

            if (start_write == 0 && end_write == BLOCK_SIZE - 1) {
                // Perfect case
                FAILABLE(block_write(superblock->num_fat + 2 + *data_index_ptr, buf + total_bytes_written));
            } else {
                // We're don't need the whole block so we use a bounce buffer
                FAILABLE(block_read(superblock->num_fat + 2 + *data_index_ptr, bounce_buffer));
				/*fwrite(bounce_buffer + start_write, 1, block_bytes_written, stdout);
				fflush(stdout);
				printf("\n");*/
                memcpy(bounce_buffer + start_write, buf + total_bytes_written, block_bytes_written);
				/*fwrite(bounce_buffer + start_write, 1, block_bytes_written, stdout);
				fflush(stdout);
				printf("\n");*/

                FAILABLE(block_write(superblock->num_fat + 2 + *data_index_ptr, bounce_buffer));

				/*FAILABLE(block_read(superblock->num_fat + 1 + *data_index_ptr, bounce_buffer));
				fwrite(bounce_buffer + start_write, 1, block_bytes_written, stdout);
				fflush(stdout);
				printf("\n");*/
            }

			if (newBlocksCreated) {
				//printf("Writing bytes %d\n", block_bytes_written);
				root_dir->entries[fd_table[fd].file_i].fsize += block_bytes_written;
			}

            total_bytes_written += block_bytes_written;

            /*
            if (total_bytes_written >= count){ // If we have done the correct amount of writing, we terminate.
                break;
            }
            */
            if (blockUpperBound > finalByte) { // If we have done the correct amount of writing, we terminate.
                break;
            }
        }

        data_index_ptr = fat_entry_at_index(*data_index_ptr);
        blocksIteratedOver++;
    }

    free(bounce_buffer);

	fs_backup();

	// Increment offset in fd_table
	fd_table[fd].offset += total_bytes_written;

	return total_bytes_written;
}

int fs_read(int fd, void *buf, size_t count)
{
	FAILABLE(verify_fd(fd));

   	uint16_t data_index = root_dir->entries[fd_table[fd].file_i].first_block_i;

    uint32_t startingByte = fd_table[fd].offset;
    uint32_t finalByte = startingByte + count - 1;

    printf("Final byte: %" PRIu32 "\n",finalByte);


	if (finalByte > root_dir->entries[fd_table[fd].file_i].fsize - 1) {
        finalByte = root_dir->entries[fd_table[fd].file_i].fsize - 1;
        printf("Final byte, capped: %" PRIu32 "\n",finalByte);
    }

	uint8_t *bounce_buffer = (uint8_t*)calloc(BLOCK_SIZE, sizeof(uint8_t));

    uint32_t blocksIteratedOver = 0;
    uint32_t total_bytes_read = 0;
	while (data_index != FAT_EOC) {
        uint32_t blockLowerBound = blocksIteratedOver * BLOCK_SIZE;
        uint32_t blockUpperBound = ((blocksIteratedOver + 1) * BLOCK_SIZE) - 1;

		// If byte upper bound is greater than starting byte, we know that
		// this block intersects with the bytes that we are trying to read
		if (blockUpperBound >= startingByte) {
            int start_read, end_read;

			if (blockLowerBound < startingByte) {
				// The start of the section to read is in the middle of the block
				start_read = startingByte - blockLowerBound;
			} else {
				start_read = 0;
			}

			if (blockUpperBound > finalByte) {
				// The end of the section to read is in the middle of the block
				end_read = finalByte - blockLowerBound;
			} else {
				end_read = BLOCK_SIZE - 1;
			}

			if (start_read == 0 && end_read == BLOCK_SIZE - 1) {
				// Perfect case
				FAILABLE(block_read(superblock->num_fat + 2 + data_index, buf + total_bytes_read));
			} else {
				// We're don't need the whole block so we use a bounce buffer
				FAILABLE(block_read(superblock->num_fat + 2 + data_index, bounce_buffer));
				memcpy(buf + total_bytes_read, bounce_buffer + start_read, end_read - start_read + 1);
				/*printf("Starting print content\n");
				fwrite(buf + total_bytes_read, 1, 19, stdout);
				fflush(stdout);
				printf("\n");
				printf("End print content\n");*/
			}

			total_bytes_read += end_read - start_read + 1;

			if (blockUpperBound > finalByte) {
				break;
			}
		}

		data_index = *fat_entry_at_index(data_index);
		blocksIteratedOver++;
	}

	free(bounce_buffer);

	// Increment offset in fd_table
	fd_table[fd].offset += total_bytes_read;

	return total_bytes_read;
}

