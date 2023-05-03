#include "../fs/operations.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(){

    char const target_path[] = "/f1";
    char const link_path[] = "/l1";
    char const link_path2[] = "/l2";

    assert(tfs_init(NULL) != -1);

    // Create file
    int fd = tfs_open(target_path, TFS_O_CREAT);
    assert(fd != -1);

    // Create hardlink of file
    assert(tfs_link(target_path, link_path) != -1);

    // Create hardlink for harlink
    assert(tfs_link(link_path, link_path2) != -1);

    printf("Successful test.\n");

    return 0;
}