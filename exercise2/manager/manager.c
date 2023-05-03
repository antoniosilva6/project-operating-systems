#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../fs/config.h"
#include "../fs/operations.h"
#include "../fs/state.h"
#include "../utils/logging.h"

void send_msg(int tx, char *str) {
  size_t len = strlen(str);
  size_t written = 0;

  while (written < len) {
    ssize_t ret = write(tx, str + written, len - written);
    if (ret < 0) {
      fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }

    written += (size_t)ret;
  }
}

/* Function used to know if the operation of a box was sucessfull */
char *cut(char string[], char *mode) {
  int counter = 0;
  char *pipe = "2";
  char *pch;
  pch = strtok(string, "|");
  counter++;
  while (pch != NULL) { // separates and splits the buffer string every '|'
    if (counter == 2 && (strcmp(pipe, mode) == 0)) {
      return pch;
    } else if (counter == 3) {
      return pch;
    }
    counter++;
    pch = strtok(NULL, "|");
  }
  return pch;
}

int main(int argc, char **argv) {

  // if(argc == 4){

  // }

  if (argc == 5) {
    char name[strlen(MB_PATH) + strlen(argv[1])];
    strcpy(name, MB_PATH);
    strcat(name, argv[1]);

    char *man_pipename = argv[2];

    // opening tfs
    int server = open(name, O_WRONLY);
    if (server == -1) {
      fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
      return -1;
    }

    // remove pipe if it does exist
    if (unlink(man_pipename) != 0 && errno != ENOENT) {
      fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", man_pipename,
              strerror(errno));
      return -1;
    }

    // create pipe
    if (mkfifo(man_pipename, 0640) != 0) {
      fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
      return -1;
    }

    char buffer[BUFFER_SIZE];
    char str1[strlen(argv[3])];
    strcpy(str1, argv[3]);
    char str2[6] = "create";

    char client_path[strlen(MAN_PATH) + strlen(man_pipename)];
    strcpy(client_path, MAN_PATH);
    strcat(client_path, man_pipename);

    if (strcmp(str1, str2) == 0) {
      sprintf(buffer, "%d|%s|%s", OP_CODE_CREATE_BOX_REQUEST, client_path,
              argv[4]);
    }

    else {
      sprintf(buffer, "%d|%s|%s", OP_CODE_REMOVE_BOX_REQUEST, client_path,
              argv[4]);
    }

    send_msg(server, buffer);

    // open pipe for writing
    // this waits for someone to open it for reading
    int manager = open(man_pipename, O_RDONLY);
    if (manager == -1) {
      fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
      return -1;
    }

    while (true) {
      char answer[BUFFER_SIZE];
      ssize_t ret = read(manager, answer, BUFFER_SIZE - 1);
      answer[ret] = 0;

      // Print on stdout if it was sucessfull or not
      char *returnCode = cut(answer, "2");
      char *compare = "0";
      if (strcmp(returnCode, compare) == 0) {
        fprintf(stdout, "OK\n");
      } else {
        close(manager);
      }
    }
  }
}
