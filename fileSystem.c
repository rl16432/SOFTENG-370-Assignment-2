/*
 * fileSystem.c
 *
 *  Modified on: 
 *      Author: Your UPI
 * 
 * Complete this file.
 */

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "device.h"
#include "fileSystem.h"

#define MAX_DIR_ENTRIES 3
struct dir_entry {
	char name[8];
	int first_block;
	int size;
	int num_dir_entries;
};

struct system_block {
	int free_block;
	int num_root_dir_entries;
};

// typedef struct dir {
// 	struct dir_entry entries[4];
// 	int next_block;
// 	int next_free_block;
// };

struct block_struct {
	char data[BLOCK_SIZE - sizeof(int) * 3];
	int next_block;
	int next_free_block;
};

/* The file system error number. */
int file_errno = 0;

void testMa() {
	printf("%d %d\n", sizeof(struct block_struct), sizeof(struct dir_entry));
	struct block_struct root_dir;
	root_dir.next_block = -1;
	root_dir.next_free_block = 3;
	struct dir_entry testma[MAX_DIR_ENTRIES] = {
		{"fileA", 1, 6, 0},
		{"fileB", 2, 6, 0},
		{"dir1", 3, 0, 1},
	};
	memcpy(root_dir.data, testma, sizeof(testma));
	if (blockWrite(2, (unsigned char*) &root_dir) == -1) {
		file_errno = EOTHER;
	}
	
}

void testRead() {
	struct block_struct* root_dir = malloc(sizeof(struct block_struct));
	if (blockRead(2, (unsigned char*) root_dir) == -1) {
		file_errno = EOTHER;
	}
	printf("%d\n", root_dir->next_free_block);	printf("%d\n", root_dir->next_block);
	struct dir_entry* test = (struct dir_entry*) root_dir->data;
	printf("%s\n", test[0].name);
	printf("%s\n", test[1].name);
	printf("%s\n", test[2].name);

}

int readDir(int block, struct dir_entry* entries) {
	int next_block = block;
	int last_block = block;
	int num_blocks = 0;
	struct dir_entry* current_pos = entries;

	while (next_block != -1) {
		num_blocks++;
		struct block_struct* dir = malloc(sizeof(struct block_struct));
		
		if (blockRead(next_block, (unsigned char*) dir) == -1) {
			file_errno = EOTHER;
			return -1;
		}

		// Append entries to entries array
		memcpy(current_pos, dir->data, sizeof(struct dir_entry) * MAX_DIR_ENTRIES);
		current_pos = current_pos + MAX_DIR_ENTRIES;
		
		last_block = next_block;
		next_block = dir->next_block;

		free(dir);
	}
	return last_block;
}

int createDir() {
	int free_block = getFreeBlock();
	struct block_struct* block_struct = getBlock(free_block);
	block_struct->next_block = -1;
	block_struct->next_free_block = -1;
	if (blockWrite(free_block, (unsigned char*) block_struct) == -1) {
		file_errno = EOTHER;
		return -1;
	}
	free(block_struct);
	return free_block;
}

int getNumRootDirEntries() {
	struct system_block* system_block = malloc(sizeof(struct system_block));
	if (blockRead(1, (unsigned char*) system_block) == -1) {
		file_errno = EOTHER;
		return -1;
	}
	return system_block->num_root_dir_entries;
}

int getFreeBlock() {
	struct system_block* system_block = malloc(sizeof(struct system_block));
	int free_block;
	if (blockRead(1, (unsigned char*) system_block) == -1) {
		file_errno = EOTHER;
		return -1;
	}
	free_block = system_block->free_block;
	free(system_block);

	return free_block;
}

struct block_struct* getBlock(int block) {
	struct block_struct* block_struct = malloc(sizeof(struct block_struct));
	// block_struct->data = malloc(BLOCK_SIZE);
	
	if (blockRead(block, (unsigned char*) block_struct) == -1) {
		file_errno = EOTHER;
		return NULL;
	}
	return block_struct;
}

/* 
 * Update the free block in the system area.
 * Return next free block if no problem, -1 if there is no more free blocks, -2 if the call failed.
 */
int getAndUpdateFreeBlock() {
	int free_block = getFreeBlock();
	int next_free_block;

	// Get next free block
	struct block_struct* block_struct = getBlock(free_block);

	// Update free block in system area
	struct system_block* system_block = malloc(sizeof(struct system_block));
	if (blockRead(1, (unsigned char*) system_block) == -1) {
		file_errno = EOTHER;
		return -2;
	}
	system_block->free_block = block_struct->next_free_block;
	next_free_block = block_struct->next_free_block;

	// Write to system area
	if (blockWrite(1, (unsigned char*) system_block) == -1) {
		file_errno = EOTHER;
		return -2;
	};

	free(block_struct);
	free(system_block);

	return next_free_block;
}

/*
 * Formats the device for use by this file system.
 * The volume name must be < 64 bytes long.
 * All information previously on the device is lost.
 * Also creates the root directory "/".
 * Returns 0 if no problem or -1 if the call failed.
 */
int format(char *volumeName) {
	if (blockWrite(0, (unsigned char*) volumeName) == -1) {
		file_errno = EBADVOLNAME;
		return -1;
	}

	// Store free block number of root directory entries in block 1
	struct system_block system_block;
	system_block.free_block = 3;
	system_block.num_root_dir_entries = 0;
	if (blockWrite(1, (unsigned char*) &system_block) == -1) {
		file_errno = EOTHER;
		return -1;
	};

	// Format all free blocks
	for (int i = 3; i < numBlocks(); i++) {
		struct block_struct dir;
		dir.next_block = -1;
		dir.next_free_block = i + 1;

		if (dir.next_free_block == numBlocks()) {
			dir.next_free_block = -1;
		}

		if (blockWrite(i, (unsigned char*) &dir) == -1) {
			file_errno = EOTHER;
			return -1;
		}
	}

	// format root directory (block 2)
	struct block_struct root_dir;
	root_dir.next_block = -1;
	root_dir.next_free_block = 3;
	struct dir_entry root_entries[MAX_DIR_ENTRIES] = {};
	memcpy(root_dir.data, root_entries, sizeof(root_entries));
	if (blockWrite(2, (unsigned char*) &root_dir) == -1) {
		file_errno = EOTHER;
		return -1;
	}
	
	return 0;
}

/*
 * Returns the volume's name in the result.
 * Returns 0 if no problem or -1 if the call failed.
 */
int volumeName(char *result) {
	// struct block_struct* test = malloc(500);
	if (blockRead(0, (unsigned char*) result) == -1) {
		return -1;
	}
	// blockRead(4, (unsigned char*) test);
	// printf("%d\n", test->next_free_block);

	return 0;
}

/*
 * Makes a file with a fully qualified pathname starting with "/".
 * It automatically creates all intervening directories.
 * Pathnames can consist of any printable ASCII characters (0x20 - 0x7e)
 * including the space character.
 * The occurrence of "/" is interpreted as starting a new directory
 * (or the file name).
 * Each section of the pathname must be between 1 and 7 bytes long (not
 * counting the "/"s).
 * The pathname cannot finish with a "/" so the only way to create a directory
 * is to create a file in that directory. This is not true for the root
 * directory "/" as this needs to be created when format is called.
 * The total length of a pathname is limited only by the size of the device.
 * Returns 0 if no problem or -1 if the call failed.
 */
int create(char *pathName) {
	char* temp_path = pathName;
	char* dir_pos = strchr(temp_path, '/');
	int curr_dir_block_num = 2;
	int num_dir_entries = 0;
	
	while (dir_pos != NULL) {
		// struct block_struct* curr_dir = malloc(500);
		struct dir_entry* entries = malloc(MAX_DIR_ENTRIES * sizeof(struct dir_entry));

		// Get each directory name
		char* path = malloc(8);
		strncpy(path, temp_path, dir_pos - temp_path);

		if (curr_dir_block_num == 2) {
			num_dir_entries = getNumRootDirEntries();
		}

		// Check if directory exists
		int last_block = readDir(curr_dir_block_num, entries);
		int dir_exists = 0;
		for (int i = 0; i < num_dir_entries; i++) {
			printf("%s\n", entries[i].name);
			if (strcmp(entries[i].name, path) == 0) {
				dir_exists = 1;
				curr_dir_block_num = entries[i].first_block;
				num_dir_entries = entries[i].num_dir_entries;
				break;
			}
		}

		// If directory doesn't exist, create it
		if (!dir_exists) {
			// Get free block
			
			int free_block = getFreeBlock();
			if (free_block == -1) {
				file_errno = ENOSPC;
				return -1;
			}

			// Update free block
			if (updateFreeBlock() == -1) {
				return -1;
			}

			// Update parent directory
			struct dir_entry new_dir_entry;
			strncpy(new_dir_entry.name, path, 8);
			new_dir_entry.first_block = free_block;
			new_dir_entry.num_dir_entries = 0;
			entries[num_dir_entries] = new_dir_entry;
			num_dir_entries++;

			// Update parent directory block
			struct block_struct* block_struct = getBlock(curr_dir_block_num);
			memcpy(block_struct->data, entries, MAX_DIR_ENTRIES * sizeof(struct dir_entry));
			// block_struct->data = (void*) entries;
			// if (blockWrite(2, (unsigned char*) &root_dir) == -1) {
			// 	file_errno = EOTHER;
			// 	return -1;
			// }
		}

		struct block_struct* block_struct = getBlock(curr_dir_block_num);
		memcpy(block_struct->data, entries, MAX_DIR_ENTRIES * sizeof(struct dir_entry));
		// block_struct->data = (void*) entries;
		// if (blockWrite(2, (unsigned char*) &root_dir) == -1) {
		// 	file_errno = EOTHER;
		// 	return -1;
		// }
		printf("%s\n", path);
		temp_path = dir_pos + 1;
		
		free(entries);
		dir_pos = strchr(temp_path, '/');
	}
	
	return -1;
}
/*
 * Returns a list of all files in the named directory.
 * The "result" string is filled in with the output.
 * The result looks like this

/dir1:
file1	42
file2	0

 * The fully qualified pathname of the directory followed by a colon and
 * then each file name followed by a tab "\t" and then its file size.
 * Each file on a separate line.
 * The directoryName is a full pathname.
 */
void list(char *result, char *directoryName) {
}

/*
 * Writes data onto the end of the file.
 * Copies "length" bytes from data and appends them to the file.
 * The filename is a full pathname.
 * The file must have been created before this call is made.
 * Returns 0 if no problem or -1 if the call failed.
 */
int a2write(char *fileName, void *data, int length) {
	return -1;
}

/*
 * Reads data from the start of the file.
 * Maintains a file position so that subsequent reads continue
 * from where the last read finished.
 * The filename is a full pathname.
 * The file must have been created before this call is made.
 * Returns 0 if no problem or -1 if the call failed.
 */
int a2read(char *fileName, void *data, int length) {
	return -1;
}

/*
 * Repositions the file pointer for the file at the specified location.
 * All positive integers are byte offsets from the start of the file.
 * 0 is the beginning of the file.
 * If the location is after the end of the file, move the location
 * pointer to the end of the file.
 * The filename is a full pathname.
 * The file must have been created before this call is made.
 * Returns 0 if no problem or -1 if the call failed.
 */
int seek(char *fileName, int location) {
	return -1;
}
