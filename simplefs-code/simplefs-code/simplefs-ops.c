#include "simplefs-ops.h"
extern struct filehandle_t file_handle_array[MAX_OPEN_FILES]; // Array for storing opened files

int simplefs_create(char *filename){
     // Allocate an inode for the new file
    int inodenum = simplefs_allocInode();
    if (inodenum == -1) {
        printf("Error: No free inodes available.\n");
        return -1;
    }
    
    struct inode_t inode;
    simplefs_readInode(inodenum, &inode);

    // Initialize the inode with the filename and default values
    strcpy(inode.name, filename);
    inode.status = INODE_IN_USE;
    inode.file_size = 0;
    for (int i = 0; i < MAX_FILE_SIZE; i++) {
        inode.direct_blocks[i] = -1;
    }

    // Write the updated inode back to disk
    simplefs_writeInode(inodenum, &inode);
    return inodenum;
}


void simplefs_delete(char *filename){
    // Search for the file by name
    struct inode_t inode;
    for (int i = 0; i < NUM_INODES; i++) {
        simplefs_readInode(i, &inode);
        if (inode.status == INODE_IN_USE && strcmp(inode.name, filename) == 0) {
            // Free the associated data blocks
            for (int j = 0; j < MAX_FILE_SIZE; j++) {
                if (inode.direct_blocks[j] != -1) {
                    simplefs_freeDataBlock(inode.direct_blocks[j]);
                }
            }
            // Free the inode
            simplefs_freeInode(i);
            printf("File '%s' deleted successfully.\n", filename);
            return;
        }
    }
    printf("Error: File not found.\n");
}

int simplefs_open(char *filename){
    // Search for the file by name
    struct inode_t inode;
    for (int i = 0; i < NUM_INODES; i++) {
        simplefs_readInode(i, &inode);
        if (inode.status == INODE_IN_USE && strcmp(inode.name, filename) == 0) {
            // Assign an entry in file_handle_array
            for (int j = 0; j < MAX_OPEN_FILES; j++) {
                if (file_handle_array[j].inode_number == -1) {
                    file_handle_array[j].inode_number = i;
                    file_handle_array[j].offset = 0;
                    return j; // Return the file handle
                }
            }
            printf("Error: No free file handles available.\n");
            return -1;
        }
    }
    printf("Error: File not found.\n");
    return -1;
}

void simplefs_close(int file_handle){
    if (file_handle >= 0 && file_handle < MAX_OPEN_FILES) {
        file_handle_array[file_handle].inode_number = -1;
        file_handle_array[file_handle].offset = 0;
    }

}

int simplefs_read(int file_handle, char *buf, int nbytes){
    if (file_handle < 0 || file_handle >= MAX_OPEN_FILES || file_handle_array[file_handle].inode_number == -1) {
        printf("Error: Invalid file handle.\n");
        return -1;
    }
    
    struct inode_t inode;
    int inodenum = file_handle_array[file_handle].inode_number;
    simplefs_readInode(inodenum, &inode);

    int bytes_read = 0;
    int block_index = file_handle_array[file_handle].offset / BLOCKSIZE;
    int block_offset = file_handle_array[file_handle].offset % BLOCKSIZE;

    while (bytes_read < nbytes && block_index < MAX_FILE_SIZE && inode.direct_blocks[block_index] != -1) {
        char tempBuf[BLOCKSIZE];
        simplefs_readDataBlock(inode.direct_blocks[block_index], tempBuf);

        int bytes_to_read = BLOCKSIZE - block_offset;
        if (bytes_to_read > nbytes - bytes_read) {
            bytes_to_read = nbytes - bytes_read;
        }
        memcpy(buf + bytes_read, tempBuf + block_offset, bytes_to_read);
        bytes_read += bytes_to_read;
        block_offset = 0;
        block_index++;
    }
    file_handle_array[file_handle].offset += bytes_read;
    return bytes_read;
}


int simplefs_write(int file_handle, char *buf, int nbytes){
    if (file_handle < 0 || file_handle >= MAX_OPEN_FILES || file_handle_array[file_handle].inode_number == -1) {
        printf("Error: Invalid file handle.\n");
        return -1;
    }
    
    struct inode_t inode;
    int inodenum = file_handle_array[file_handle].inode_number;
    simplefs_readInode(inodenum, &inode);

    int bytes_written = 0;
    int block_index = file_handle_array[file_handle].offset / BLOCKSIZE;
    int block_offset = file_handle_array[file_handle].offset % BLOCKSIZE;

    while (bytes_written < nbytes && block_index < MAX_FILE_SIZE) {
        if (inode.direct_blocks[block_index] == -1) {
            inode.direct_blocks[block_index] = simplefs_allocDataBlock();
        }
        
        char tempBuf[BLOCKSIZE];
        simplefs_readDataBlock(inode.direct_blocks[block_index], tempBuf);

        int bytes_to_write = BLOCKSIZE - block_offset;
        if (bytes_to_write > nbytes - bytes_written) {
            bytes_to_write = nbytes - bytes_written;
        }
        memcpy(tempBuf + block_offset, buf + bytes_written, bytes_to_write);
        simplefs_writeDataBlock(inode.direct_blocks[block_index], tempBuf);
        bytes_written += bytes_to_write;
        block_offset = 0;
        block_index++;
    }
    file_handle_array[file_handle].offset += bytes_written;
    inode.file_size += bytes_written;
    simplefs_writeInode(inodenum, &inode);
    return bytes_written;
}


int simplefs_seek(int file_handle, int nseek){
    if (file_handle < 0 || file_handle >= MAX_OPEN_FILES || file_handle_array[file_handle].inode_number == -1) {
        printf("Error: Invalid file handle.\n");
        return -1;
    }
    file_handle_array[file_handle].offset += nseek;
    return file_handle_array[file_handle].offset;
}

void simplefs_formatDisk() {
    diskFD = open("simplefs_disk.dat", O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (diskFD < 0) {
        perror("Error opening disk file");
        exit(1);
    }
    
    char zero[BLOCKSIZE] = {0};
    for (int i = 0; i < TOTAL_BLOCKS; i++) {
        if (write(diskFD, zero, BLOCKSIZE) != BLOCKSIZE) {
            perror("Error formatting disk");
            close(diskFD);
            exit(1);
        }
    }
    
    // Reset the file descriptor position
    lseek(diskFD, 0, SEEK_SET);
    printf("Disk formatted successfully.\n");
}
