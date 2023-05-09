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
#define MAX_NAME_LENGTH 8
struct dir_entry {
	char name[8];
	int first_block;
	int size;
};

struct system_block {
	int free_block;
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
	int num_dir_entries;
};

/* The file system error number. */
int file_errno = 0;

void testMa() {
	printf("%d %d\n", sizeof(struct block_struct), sizeof(struct dir_entry));
	struct block_struct root_dir;
	root_dir.next_block = -1;
	root_dir.next_free_block = 3;
	struct dir_entry testma[MAX_DIR_ENTRIES] = {
		{"fileA", 1, 6},
		{"fileB", 2, 6},
		{"dir1", 3, 0},
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

	free(root_dir);
}

/*
* Return next block in the directory and copy entries in current block to entries array
*/
int readDirBlock(int block, struct dir_entry* entries) {
	struct block_struct* dir_block = malloc(sizeof(struct block_struct));
	int next_block;

	if (blockRead(block, (unsigned char*) dir_block) == -1) {
		file_errno = EOTHER;
		return -2;
	}
	// Append entries to entries array
	memcpy(entries, dir_block->data, sizeof(struct dir_entry) * MAX_DIR_ENTRIES);
	next_block = dir_block->next_block;

	free(dir_block);
	return next_block;
}

/*
* Sets up a block as a directory block
*/
int formatDirBlock(int block) {
	struct block_struct* dir_block = malloc(sizeof(struct block_struct));
	blockRead(block, (unsigned char*) dir_block);
	struct dir_entry dir_entries[MAX_DIR_ENTRIES]= {};

	memcpy(dir_block->data, dir_entries, sizeof(dir_entries));
	dir_block->next_block = -1;
	dir_block->num_dir_entries = 0;
	blockWrite(block, (unsigned char*) dir_block);

	free(dir_block);
	return 0;
}

/*
* Sets up a block as a file block
*/
int formatFileBlock(int block) {
	struct block_struct* file_block = malloc(sizeof(struct block_struct));
	blockRead(block, (unsigned char*) file_block);
	file_block->data[0] = '\0';
	file_block->next_block = -1;
	file_block->num_dir_entries = 0;
	blockWrite(block, (unsigned char*) file_block);

	free(file_block);
	return 0;
}


int getFreeBlock() {
	struct system_block* system_blk = malloc(BLOCK_SIZE);
	int free_block;

	blockRead(1, (unsigned char*) system_blk);
	
	free_block = system_blk->free_block;
	free(system_blk);

	return free_block;
}


int getNumDirEntries(int block) {
	struct block_struct* block_str = malloc(sizeof(struct block_struct));
	blockRead(block, (unsigned char*) block_str);
	
	int num_dir_entries = block_str->num_dir_entries;
	free(block_str);
	return num_dir_entries;
}

/* 
 * Gets the current free block and update the free block in the system area.
 * Return next free block if no problem, -1 if there is no more free blocks, -2 if the call failed.
 */
int getAndUpdateFreeBlock() {
	int free_block = getFreeBlock();

	if (free_block == -1) {
		file_errno = ENOROOM;
		return -1;
	}
	// Get next free block
	struct block_struct* block_str = malloc(sizeof(struct block_struct));
	blockRead(free_block, (unsigned char*) block_str);

	// Update free block in system area
	struct system_block* system_blk = malloc(BLOCK_SIZE);
	blockRead(1, (unsigned char*) system_blk);
	system_blk->free_block = block_str->next_free_block;

	// Write to system area
	blockWrite(1, (unsigned char*) system_blk);

	free(block_str);
	free(system_blk);

	return free_block;
}

int createDir(int block, int num_entries, char* name) {
	if (num_entries == MAX_DIR_ENTRIES) {
		struct block_struct* dir = malloc(sizeof(struct block_struct));
		blockRead(block, (unsigned char*) dir);

		dir->next_block = getAndUpdateFreeBlock();
		blockWrite(block, (unsigned char*) dir);

		formatDirBlock(dir->next_block);

		// Set variables to take in new block in next section
		block = dir->next_block;
		num_entries = 0;

		free(dir);
	}
	
	int new_dir_first_block;

	struct block_struct* dir = malloc(sizeof(struct block_struct));
	blockRead(block, (unsigned char*) dir);

	struct dir_entry* dir_entries = (struct dir_entry*) dir->data;
	dir_entries[num_entries].first_block = getAndUpdateFreeBlock();
	new_dir_first_block = dir_entries[num_entries].first_block;
	dir_entries[num_entries].size = 0;
	strncpy(dir_entries[num_entries].name, name, MAX_NAME_LENGTH);

	dir->num_dir_entries = num_entries + 1;
	blockWrite(block, (unsigned char*) dir);
	
	formatDirBlock(dir_entries[num_entries].first_block);

	free(dir);

	return new_dir_first_block;
}

int createFile(int block, int num_entries, char* name) {
	if (num_entries == MAX_DIR_ENTRIES) {
		struct block_struct* dir = malloc(sizeof(struct block_struct));
		blockRead(block, (unsigned char*) dir);

		dir->next_block = getAndUpdateFreeBlock();
		blockWrite(block, (unsigned char*) dir);

		formatDirBlock(dir->next_block);

		// Set variables to take in new block in next section
		block = dir->next_block;
		num_entries = 0;

		free(dir);
	}
	
	int new_dir_first_block;

	struct block_struct* dir = malloc(sizeof(struct block_struct));
	blockRead(block, (unsigned char*) dir);

	struct dir_entry* dir_entries = (struct dir_entry*) dir->data;
	dir_entries[num_entries].first_block = getAndUpdateFreeBlock();
	new_dir_first_block = dir_entries[num_entries].first_block;
	dir_entries[num_entries].size = 0;
	strncpy(dir_entries[num_entries].name, name, MAX_NAME_LENGTH);

	dir->num_dir_entries = num_entries + 1;
	blockWrite(block, (unsigned char*) dir);
	
	formatDirBlock(dir_entries[num_entries].first_block);

	free(dir);

	return new_dir_first_block;
}

/*
 * Formats the device for use by this file system.
 * The volume name must be < 64 bytes long.
 * All information previously on the device is lost.
 * Also creates the root directory "/".
 * Returns 0 if no problem or -1 if the call failed.
 */
int format(char *volumeName) {
	blockWrite(0, (unsigned char*) volumeName);
	
	// Store free block number of root directory entries in block 1
	struct system_block system_blk;
	system_blk.free_block = 3;
	blockWrite(1, (unsigned char*) &system_blk);

	// Format all free blocks
	for (int i = 3; i < numBlocks(); i++) {
		struct block_struct dir;
		dir.next_block = -1;
		dir.next_free_block = i + 1;
		// -2 for neither file nor directory
		dir.num_dir_entries = -2;

		if (dir.next_free_block == numBlocks()) {
			dir.next_free_block = -1;
		}

		blockWrite(i, (unsigned char*) &dir);
	}

	// format root directory (block 2)
	struct block_struct root_dir;
	root_dir.next_block = -1;
	root_dir.next_free_block = 3;
	root_dir.num_dir_entries = 0;

	struct dir_entry root_entries[MAX_DIR_ENTRIES] = {};
	memcpy(root_dir.data, root_entries, sizeof(root_entries));

	blockWrite(2, (unsigned char*) &root_dir);
	
	return 0;
}

/*
 * Returns the volume's name in the result.
 * Returns 0 if no problem or -1 if the call failed.
 */
int volumeName(char *result) {
	// struct block_struct* test = malloc(500);
	blockRead(0, (unsigned char*) result);
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
	char* temp_path = pathName + 1;
	char* dir_pos = strchr(temp_path, '/');

	int curr_dir_block_num = 2;
	int next_block;
	int previous_block;
	int dir_exists;
	int num_dir_entries;

	struct dir_entry* entries = malloc(MAX_DIR_ENTRIES * sizeof(struct dir_entry));

	// Iterate through directories in path
	while (dir_pos != NULL) {
		// Get each directory name
		char* path = malloc(8);
		strncpy(path, temp_path, dir_pos - temp_path);

		// next_block: next block belonging to same directory
		next_block = curr_dir_block_num;
		previous_block = curr_dir_block_num;
		dir_exists = 0;

		while (next_block != -1) {
			// Number of directory entries in CURRENT block (0 to 3)
			num_dir_entries = getNumDirEntries(next_block);
			previous_block = next_block;
			next_block = readDirBlock(next_block, entries);
			for (int i = 0; i < num_dir_entries; i++) {
				if (strcmp(entries[i].name, path) == 0) {
					dir_exists = 1;
					curr_dir_block_num = entries[i].first_block;
					next_block = -1;
					break;
				}
			}
		}

		// If directory doesn't exist, create it
		if (!dir_exists) {
			curr_dir_block_num = createDir(previous_block, num_dir_entries, path);
		}
		temp_path = dir_pos + 1;
	
		free(path);
		dir_pos = strchr(temp_path, '/');
	}

	// Create file
	next_block = curr_dir_block_num;
	previous_block = curr_dir_block_num;
	int file_exists = 0;

	while (next_block != -1) {
		// Number of directory entries in CURRENT block (0 to 3)
		num_dir_entries = getNumDirEntries(next_block);
		previous_block = next_block;
		next_block = readDirBlock(next_block, entries);
		for (int i = 0; i < num_dir_entries; i++) {
			if (strcmp(entries[i].name, temp_path) == 0) {
				file_exists = 1;
				curr_dir_block_num = entries[i].first_block;
				next_block = -1;
				break;
			}
		}
	}

	// If file doesn't exist, create it
	if (!file_exists) {
		curr_dir_block_num = createFile(previous_block, num_dir_entries, temp_path);
	}
	else {
		printf("File already exists\n");
	}

	free(entries);
	return 0;
}

/*
* Returns the block number of the first block of the directory
*/
int findDir(char* directoryName) {
	if (strcmp(directoryName, "/") == 0) {
		return 2;
	}

	char* temp_path = directoryName + 1;
	char* dir_pos = strchr(temp_path, '/');

	int curr_dir_block_num = 2;
	int next_block;
	int dir_exists;
	int num_dir_entries;

	struct dir_entry* entries = malloc(MAX_DIR_ENTRIES * sizeof(struct dir_entry));

	// Iterate through directories in path
	while (dir_pos != NULL) {
		// Get each directory name
		char* path = malloc(8);
		strncpy(path, temp_path, dir_pos - temp_path);

		// next_block: next block belonging to same directory
		next_block = curr_dir_block_num;
		dir_exists = 0;

		while (next_block != -1) {
			// Number of directory entries in CURRENT block (0 to 3)
			num_dir_entries = getNumDirEntries(next_block);
			next_block = readDirBlock(next_block, entries);
			for (int i = 0; i < num_dir_entries; i++) {
				if (strcmp(entries[i].name, path) == 0) {
					dir_exists = 1;
					curr_dir_block_num = entries[i].first_block;
					next_block = -1;
					break;
				}
			}
		}

		// If directory doesn't exist, return error
		if (!dir_exists) {
			file_errno = ENOSUCHFILE;
			return -1;
		}
		temp_path = dir_pos + 1;
	
		free(path);
		dir_pos = strchr(temp_path, '/');
	}

	next_block = curr_dir_block_num;
	dir_exists = 0;

	while (next_block != -1) {
		// Number of directory entries in CURRENT block (0 to 3)
		num_dir_entries = getNumDirEntries(next_block);
		next_block = readDirBlock(next_block, entries);
		for (int i = 0; i < num_dir_entries; i++) {
			if (strcmp(entries[i].name, temp_path) == 0) {
				dir_exists = 1;
				curr_dir_block_num = entries[i].first_block;
				next_block = -1;
				break;
			}
		}
	}
	if (!dir_exists) {
		file_errno = ENOSUCHFILE;
		return -1;
	}

	free(entries);

	return curr_dir_block_num;
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
	int dir_block_num = findDir(directoryName);
	sprintf(result, "%s:\n", directoryName);

	if (dir_block_num == -1) {
		return;
	}
	int next_block = dir_block_num;
	struct dir_entry* entries = malloc(MAX_DIR_ENTRIES * sizeof(struct dir_entry));

	while (next_block != -1) {
		// Number of directory entries in CURRENT block (0 to 3)
		int num_dir_entries = getNumDirEntries(next_block);
		next_block = readDirBlock(next_block, entries);
		for (int i = 0; i < num_dir_entries; i++) {
			sprintf(result, "%s%s\t%d\n", result, entries[i].name, entries[i].size);
		}
	}
	free(entries);
}

/*
 * Writes data onto the end of the file.
 * Copies "length" bytes from data and appends them to the file.
 * The filename is a full pathname.
 * The file must have been created before this call is made.
 * Returns 0 if no problem or -1 if the call failed.
 */
int a2write(char *fileName, void *data, int length) {
	char* last_slash = strrchr(fileName, '/');
	char* directory_name = malloc(strlen(fileName) + 1);
	
	strncpy(directory_name, fileName, last_slash + 1 - fileName);
	directory_name[last_slash + 1 - fileName] = '\0';
	if (strcmp(directory_name, "/") != 0) {
		directory_name[last_slash - fileName] = '\0';
	}

	printf("directory_name: %s\n", directory_name);

	// int dir_block_num = findDir(fileName);
	free(directory_name);
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
