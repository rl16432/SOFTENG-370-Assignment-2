/*
 * fileSystem.c
 *
 *  Modified on: 10 May 2023
 *      Author: rluo154
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
#define MAX_DATA_SIZE (BLOCK_SIZE - sizeof(int) * 3)
#define NUM_BLOCKS 1500

struct dir_entry {
	char name[8];
	int first_block;
	int size;
};

struct system_block {
	int free_block;
};

struct block_struct {
	char data[MAX_DATA_SIZE];
	int next_block;
	int next_free_block;
	int num_dir_entries;
};

/* The file system error number. */
int file_errno = 0;

/* Automatically zero-initialized */
int location_pointers[NUM_BLOCKS];

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
	file_block->num_dir_entries = -1;
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

/*
* Adds a new block to the file linked list and returns the new block number
*/
int addFileBlock(int block) {
	struct block_struct* file_block = malloc(sizeof(struct block_struct));
	blockRead(block, (unsigned char*) file_block);

	int free_block = getAndUpdateFreeBlock();
	if (free_block == -1) {
		return -1;
	}
	file_block->next_block = free_block;
	blockWrite(block, (unsigned char*) file_block);

	formatFileBlock(free_block);

	free(file_block);
	return free_block;
}


int getNumDirEntries(int block) {
	struct block_struct* block_str = malloc(sizeof(struct block_struct));
	blockRead(block, (unsigned char*) block_str);
	
	int num_dir_entries = block_str->num_dir_entries;
	free(block_str);
	return num_dir_entries;
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
	// Assign new block to directory if current block is full
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
	
	int new_file_first_block;

	struct block_struct* dir = malloc(sizeof(struct block_struct));
	blockRead(block, (unsigned char*) dir);

	struct dir_entry* dir_entries = (struct dir_entry*) dir->data;
	dir_entries[num_entries].first_block = getAndUpdateFreeBlock();
	new_file_first_block = dir_entries[num_entries].first_block;
	dir_entries[num_entries].size = 0;
	strncpy(dir_entries[num_entries].name, name, MAX_NAME_LENGTH);

	dir->num_dir_entries = num_entries + 1;
	blockWrite(block, (unsigned char*) dir);
	
	formatFileBlock(dir_entries[num_entries].first_block);

	free(dir);

	return new_file_first_block;
}

/*
 * Formats the device for use by this file system.
 * The volume name must be < 64 bytes long.
 * All information previously on the device is lost.
 * Also creates the root directory "/".
 * Returns 0 if no problem or -1 if the call failed.
 */
int format(char *volumeName) {
	// Reset location pointers
	memset(location_pointers, 0, sizeof(location_pointers));

	// Store volume name
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

	// Format root directory (block 2)
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
	blockRead(0, (unsigned char*) result);
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
		path[dir_pos - temp_path] = '\0';

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
		path[dir_pos - temp_path] = '\0';

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
			char *dir_item = malloc(20);
			sprintf(dir_item, "%s:\t%d\n", entries[i].name, entries[i].size);
			strcat(result, dir_item);
			free(dir_item);
		}
	}
	free(entries);
}

/*
* Write to a file and add new blocks if necessary
*/
int writeToFile(int curr_block_num, void *data, int length, int file_size) {
	int bytes_to_write = length;
	int bytes_in_current_block = file_size % MAX_DATA_SIZE;
	int first_write = 1;
	char* data_to_write = (char *) data;

	while (bytes_to_write > 0) {
		int bytes_to_write_to_block = bytes_to_write;

		if (bytes_to_write_to_block > MAX_DATA_SIZE - bytes_in_current_block) {
			bytes_to_write_to_block = MAX_DATA_SIZE - bytes_in_current_block;
		}
		
		struct block_struct* file_block = malloc(sizeof(struct block_struct));
		blockRead(curr_block_num, (unsigned char*) file_block);

		char* result = (char *) malloc(strlen(file_block->data) + bytes_to_write_to_block + 1);
		char test[120];


		strcpy(result, file_block->data);

		memcpy(test, file_block->data, strlen(file_block->data) + 1);
		if (first_write && strcmp(file_block->data, "") != 0) {
			// If this is the first write to the file, we need to add a null terminator
        	strncpy(result + strlen(result) + 1, data_to_write, bytes_to_write_to_block);
			memcpy(file_block->data, result, strlen(result) + bytes_to_write_to_block + 1);
		}
		else {
			strncat(result, data_to_write, bytes_to_write_to_block);
			memcpy(file_block->data, result, strlen(result) + 1);
		}
		first_write = 0;

		blockWrite(curr_block_num, (unsigned char*) file_block);

		if (bytes_to_write > bytes_to_write_to_block) {
			curr_block_num = addFileBlock(curr_block_num);
			bytes_in_current_block = 0;
		}

		free(result);
		free(file_block);
		
		data_to_write += bytes_to_write_to_block;
		bytes_to_write -= bytes_to_write_to_block;
	}
	return 0;
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
	char* final_file_name = last_slash + 1;

	// Get directory name
	strncpy(directory_name, fileName, last_slash + 1 - fileName);
	directory_name[last_slash + 1 - fileName] = '\0';
	if (strcmp(directory_name, "/") != 0) {
		directory_name[last_slash - fileName] = '\0';
	}

	// Get first block of directory
	int dir_block_num = findDir(directory_name);
	if (dir_block_num == -1) {
		return -1;
	}

	// Find file in directory
	int file_block_num;
	int file_size;
	int next_block = dir_block_num;
	int file_exists = 0;
	int position = 0;

	struct dir_entry* entries = malloc(MAX_DIR_ENTRIES * sizeof(struct dir_entry));

	while (next_block != -1) {
		dir_block_num = next_block;
		int num_dir_entries = getNumDirEntries(next_block);
		next_block = readDirBlock(next_block, entries);
		for (int i = 0; i < num_dir_entries; i++) {
			if (strcmp(entries[i].name, final_file_name) == 0) {
				file_exists = 1;
				file_block_num = entries[i].first_block;
				file_size = entries[i].size;
				next_block = -1;
				position = i;
				break;
			}
		}
	}

	if (!file_exists) {
		file_errno = ENOSUCHFILE;
		return -1;
	}
	
	// Find last block of file and store in next_block
	next_block = file_block_num;
	while (next_block != -1) {
		struct block_struct* file_block = malloc(sizeof(struct block_struct));
		blockRead(next_block, (unsigned char*) file_block);
		if (file_block->next_block == -1) {
			free(file_block);
			break;
		}
		next_block = file_block->next_block;
		free(file_block);
	}

	// Update size in directory entry
	entries[position].size += length;
	struct block_struct* dir_block = malloc(sizeof(struct block_struct));
	blockRead(dir_block_num, (unsigned char*) dir_block);
	memcpy(dir_block->data, entries, sizeof(struct dir_entry) * MAX_DIR_ENTRIES);
	blockWrite(dir_block_num, (unsigned char*) dir_block);

	// Write data to file
	writeToFile(next_block, data, length, file_size);

	free(directory_name);
	free(entries);
	free(dir_block);

	return 0;
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
	char* last_slash = strrchr(fileName, '/');
	char* directory_name = malloc(strlen(fileName) + 1);
	char* final_file_name = last_slash + 1;

	// Get directory name
	strncpy(directory_name, fileName, last_slash + 1 - fileName);
	directory_name[last_slash + 1 - fileName] = '\0';
	if (strcmp(directory_name, "/") != 0) {
		directory_name[last_slash - fileName] = '\0';
	}

	// Get first block of directory
	int dir_block_num = findDir(directory_name);
	if (dir_block_num == -1) {
		return -1;
	}

	// Find file in directory
	int file_block_num;
	int file_size;
	int next_block = dir_block_num;
	int file_exists = 0;

	struct dir_entry* entries = malloc(MAX_DIR_ENTRIES * sizeof(struct dir_entry));

	while (next_block != -1) {
		dir_block_num = next_block;
		int num_dir_entries = getNumDirEntries(next_block);
		next_block = readDirBlock(next_block, entries);
		for (int i = 0; i < num_dir_entries; i++) {
			if (strcmp(entries[i].name, final_file_name) == 0) {
				file_exists = 1;
				file_block_num = entries[i].first_block;
				file_size = entries[i].size;
				next_block = -1;
				break;
			}
		}
	}

	if (!file_exists) {
		file_errno = ENOSUCHFILE;
		return -1;
	}

	// Read data from file starting from location pointer
	int curr_location = location_pointers[file_block_num];
	int end_location = curr_location + length;
	if (end_location > file_size) {
		end_location = file_size;
	}
	int block_count = 1;

	struct block_struct* file_block = malloc(sizeof(struct block_struct));
	blockRead(file_block_num, (unsigned char*) file_block);

	// Go to first block that needs to be read from
	while (curr_location > block_count * MAX_DATA_SIZE) {
		file_block_num = file_block->next_block;
		block_count++;
		blockRead(file_block_num, (unsigned char*) file_block);
	}

	// Read from multiple blocks until the 'data' reaches its length
	while (curr_location < end_location) {
		int bytes_to_read = MAX_DATA_SIZE - curr_location % MAX_DATA_SIZE;
		if (end_location - curr_location < bytes_to_read) {
			bytes_to_read = end_location - curr_location;
		}
		memcpy(data + curr_location - location_pointers[file_block_num], file_block->data + curr_location % MAX_DATA_SIZE, bytes_to_read);

		blockRead(file_block->next_block, (unsigned char*) file_block);

		block_count++;
		curr_location += bytes_to_read;
	}

	// Update location pointer
	location_pointers[file_block_num] = end_location;

	free(directory_name);
	free(entries);
	free(file_block);

	return 0;
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
	char* last_slash = strrchr(fileName, '/');
	char* directory_name = malloc(strlen(fileName) + 1);
	char* final_file_name = last_slash + 1;

	// Get directory name
	strncpy(directory_name, fileName, last_slash + 1 - fileName);
	directory_name[last_slash + 1 - fileName] = '\0';
	if (strcmp(directory_name, "/") != 0) {
		directory_name[last_slash - fileName] = '\0';
	}

	// Get first block of directory
	int dir_block_num = findDir(directory_name);
	if (dir_block_num == -1) {
		return -1;
	}

	// Find file in directory
	int file_block_num;
	int next_block = dir_block_num;
	int file_exists = 0;

	struct dir_entry* entries = malloc(MAX_DIR_ENTRIES * sizeof(struct dir_entry));

	while (next_block != -1) {
		dir_block_num = next_block;
		int num_dir_entries = getNumDirEntries(next_block);
		next_block = readDirBlock(next_block, entries);
		for (int i = 0; i < num_dir_entries; i++) {
			if (strcmp(entries[i].name, final_file_name) == 0) {
				file_exists = 1;
				file_block_num = entries[i].first_block;
				next_block = -1;
				break;
			}
		}
	}

	if (!file_exists) {
		file_errno = ENOSUCHFILE;
		return -1;
	}

	location_pointers[file_block_num] = location;

	free(directory_name);
	free(entries);

	return 0;
}
