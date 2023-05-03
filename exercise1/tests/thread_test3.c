#include "../fs/operations.h"
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define N_THREADS (8)                     

char buffer[] = "goto";

void test3(){
    char const *path = "/f3";

    int f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);
    
    for (int i = 0; i <= N_THREADS; i++){
        assert(tfs_write(f, buffer, sizeof(buffer)) == sizeof(buffer));
    }

    assert(tfs_close(f) != -1);
    
    int p = tfs_open(path, TFS_O_CREAT);
    assert(p != -1);

    for (int i = 0; i <= N_THREADS; i++){
        assert(tfs_read(p, buffer, sizeof(buffer)) == sizeof(buffer));
    }
    assert(tfs_close(p) != -1);
  
}

int main() {
    pthread_t tid[N_THREADS];
    int i;
    assert(tfs_init(NULL) != -1);

    for(i=0; i < N_THREADS; i++) 
        assert(pthread_create(&tid[i], NULL, (void *) test3, NULL) != -1);
        
        
    for(i=0; i < N_THREADS; i++) 
        assert(pthread_join(tid[i], NULL) != -1);
                
    assert(tfs_destroy() != -1);
    printf("Successful test.\n");
    return 0;
}
