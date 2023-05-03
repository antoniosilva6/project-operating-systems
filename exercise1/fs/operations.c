#include "operations.h"
#include "config.h"
#include "state.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <pthread.h>

#include "betterassert.h"

pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;

tfs_params tfs_default_params() {
    tfs_params params = {
        .max_inode_count = 64,
        .max_block_count = 1024,
        .max_open_files_count = 16,
        .block_size = 1024,
    };
    return params;
}

int tfs_init(tfs_params const *params_ptr) {
    tfs_params params;
    if (params_ptr != NULL) {
        params = *params_ptr;
    } else {
        params = tfs_default_params();
    }

    if (state_init(params) != 0) {
        return -1;
    }

    // create root inode
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }

    return 0;
}

int tfs_destroy() {
    if (state_destroy() != 0) {
        return -1;
    }
    return 0;
}

static bool valid_pathname(char const *name) {
    return name != NULL && strlen(name) > 1 && name[0] == '/';
}

/**
 * Looks for a file.
 *
 * Note: as a simplification, only a plain directory space (root directory only)
 * is supported.
 *
 * Input:
 *   - name: absolute path name
 *   - root_inode: the root directory inode
 * Returns the inumber of the file, -1 if unsuccessful.
 */
static int tfs_lookup(char const *name, inode_t const *root_inode) {
    // TODO: assert that root_inode is the root directory
    if (!valid_pathname(name)) {
        return -1;
    }

    // skip the initial '/' character
    name++;

    return find_in_dir(root_inode, name);
}

int tfs_open(char const *pathname, tfs_file_mode_t mode) {
    // Checks if the path name is valid
    if (!valid_pathname(pathname)) {
        return -1;
    }
    char name[MAX_FILE_NAME];
    strcpy(name, pathname);
    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
    ALWAYS_ASSERT(root_dir_inode != NULL,
                  "tfs_open: root dir inode must exist");
    int inum = tfs_lookup(pathname, root_dir_inode); // confirmar se é pathname ou name
    size_t offset;

    if (inum >= 0) {
        // The file already exists
        inode_t *inode = inode_get(inum);
        ALWAYS_ASSERT(inode != NULL,
                      "tfs_open: directory files must have an inode");

        if(inode->i_node_type == T_SYMLINK){
            void *block;
            if((block = data_block_get(inode->i_data_block)) == NULL){
                return -1;
            }
            strcpy(name, block);

            return tfs_open(name, mode);
        }

        // Truncate (if requested)
        if (mode & TFS_O_TRUNC) {
            if (inode->i_size > 0) {
                data_block_free(inode->i_data_block);
                inode->i_size = 0;
            }
        }
        // Determine initial offset
        if (mode & TFS_O_APPEND) {
            offset = inode->i_size;
        } else {
            offset = 0;
        }
    } else if (mode & TFS_O_CREAT) {
        // The file does not exist; the mode specified that it should be created
        // Create inode
        inum = inode_create(T_FILE);
        if (inum == -1) {
            return -1; // no space in inode table
        }

        // Add entry in the root directory
        if (add_dir_entry(root_dir_inode, name + 1, inum) == -1) {
            inode_delete(inum);
            return -1; // no space in directory
        }

        offset = 0;
    } else {
        return -1;
    }

    // Finally, add entry to the open file table and return the corresponding
    // handle

    return add_to_open_file_table(inum, offset);

    // Note: for simplification, if file was created with TFS_O_CREAT and there
    // is an error adding an entry to the open file table, the file is not
    // opened but it remains created
}

int tfs_sym_link(char const *target, char const *link_name) {

    /* Check if the file exists */
    
    int targetFd;
    targetFd = tfs_open(target, TFS_O_APPEND);
    if(targetFd == -1){
        return -1;        
    }
    tfs_close(targetFd);

    /* Create i-node for symlink */
    
    int inum_soft = inode_create(T_SYMLINK);
    if(inum_soft == -1){
        return -1;
    }
    
    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);

    inode_t *inode_soft = inode_get(inum_soft);
    int data_alloc = data_block_alloc();
    if (data_alloc == -1) {
        return -1; // no space
    }

    inode_soft->i_data_block = data_alloc;
    void *block = data_block_get(inode_soft->i_data_block);

    memcpy(block, target, strlen(target));

    int symlink = add_dir_entry(root_dir_inode, link_name + 1, inum_soft);
    if(symlink == -1){
        return -1;
    }
    
    return 0;
}

int tfs_link(char const *target, char const *link_name) {

    /* Check if the file exists in TFS */

    int targetFd;
    targetFd = tfs_open(target, TFS_O_APPEND);
    if(targetFd == -1){
        return -1;        
    }
    tfs_close(targetFd);

    /* Get the inode corresponding to the target file */
    
    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
    int inum_target = tfs_lookup(target, root_dir_inode);
    inode_t *target_inode = inode_get(inum_target);
    
    /* Create the hard link, while updating the right constants */

    if(target_inode->i_node_type == T_SYMLINK){
        return -1;
    }

    int hard_link = add_dir_entry(root_dir_inode, link_name + 1, inum_target);
    if(hard_link == -1){
        return -1;
    }

    target_inode->hardlinks_counter++;
    
    return 0;
}

int tfs_close(int fhandle) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1; // invalid fd
    }

    remove_from_open_file_table(fhandle);

    return 0;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    //  From the open file table entry, we get the inode
    inode_t *inode = inode_get(file->of_inumber);
    ALWAYS_ASSERT(inode != NULL, "tfs_write: inode of open file deleted");

    // Determine how many bytes to write
    size_t block_size = state_block_size();
    if (to_write + file->of_offset > block_size) {
        to_write = block_size - file->of_offset;
    }

    if (to_write > 0) {
        if (inode->i_size == 0) {
            // If empty file, allocate new block
            int bnum = data_block_alloc();
            if (bnum == -1) {
                return -1; // no space
            }

            inode->i_data_block = bnum;
        }

        void *block = data_block_get(inode->i_data_block);
        ALWAYS_ASSERT(block != NULL, "tfs_write: data block deleted mid-write");

        // Perform the actual write
        
        pthread_rwlock_wrlock(&rwlock);
        memcpy(block + file->of_offset, buffer, to_write);

        // The offset associated with the file handle is incremented accordingly
        file->of_offset += to_write;
        if (file->of_offset > inode->i_size) {
            inode->i_size = file->of_offset;
        }
    }
    pthread_rwlock_unlock(&rwlock);

    return (ssize_t)to_write;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    // From the open file table entry, we get the inode
    inode_t const *inode = inode_get(file->of_inumber);
    ALWAYS_ASSERT(inode != NULL, "tfs_read: inode of open file deleted");

    // Determine how many bytes to read
    size_t to_read = inode->i_size - file->of_offset;
    if (to_read > len) {
        to_read = len;
    }

    pthread_rwlock_wrlock(&rwlock);
    if (to_read > 0) {
        void *block = data_block_get(inode->i_data_block);
        ALWAYS_ASSERT(block != NULL, "tfs_read: data block deleted mid-read");

        // Perform the actual read
        
        memcpy(buffer, block + file->of_offset, to_read);
        // The offset associated with the file handle is incremented accordingly
        file->of_offset += to_read;
    }
    pthread_rwlock_unlock(&rwlock);

    return (ssize_t)to_read;
}

int tfs_unlink(char const *target) {
    /* Check is target exiists in TFS */

    int outputFd;
    outputFd = tfs_open(target, TFS_O_APPEND);
    if(outputFd == -1){
        return -1;        
    }
    tfs_close(outputFd);

    /* Get the inode that corresponds to target file */

    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
    int target_inum = tfs_lookup(target, root_dir_inode);
    inode_t *inode_target = inode_get(target_inum);

    /* Verifications and elimination */

    if(inode_target->hardlinks_counter > 1){
        int clear_hard = clear_dir_entry(root_dir_inode, target + 1);
        if(clear_hard == -1){
            return -1;
        }
        inode_target->hardlinks_counter--;
    }

    else if(inode_target->hardlinks_counter == 1){
        inode_delete(target_inum);
        int clear_hard = clear_dir_entry(root_dir_inode, target + 1);
        if(clear_hard == -1){
            return -1;
        }
    }
    
    return 0;
}

int tfs_copy_from_external_fs(char const *source_path, char const *dest_path) {
    
    size_t BUFFER_SIZE = 1024;

    /* Open input and output files */

    int inputFd;
    inputFd = open(source_path, O_RDONLY);
    
    if(inputFd == -1){
        return -1;
    } 

    tfs_file_mode_t openFlags;
    int outputFd;
    openFlags = TFS_O_CREAT | TFS_O_TRUNC | TFS_O_APPEND;

    outputFd = tfs_open(dest_path, openFlags);
    if(outputFd == -1){
        close(inputFd);
        return -1;
    }

    /* Transfer data until we encounter end of input an error */

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    while((bytes_read = read(inputFd, buffer, BUFFER_SIZE)) > 0){
        if(tfs_write(outputFd, buffer, (size_t)bytes_read) != bytes_read){
            return -1;
        }
        if(bytes_read == -1){
            close(inputFd);
            tfs_close(outputFd);
        }
    }

    /* Close input and output files */

    if(close(inputFd) == -1){
        return -1;
    }
    
    if(tfs_close(outputFd) == -1){
        return -1;
    }

    return 0;
}
