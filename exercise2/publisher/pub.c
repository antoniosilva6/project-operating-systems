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

/* Function that allows communication between the client
and the mbroker pipe by sending a string */
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

int main(int argc, char **argv) {

  if (argc == 1) {
    printf(
        "Specify server's register pipe,\npublisher's pipename and box name\n");
    return -1;
  }

  if (argc == 2) {
    printf("Specify publisher's pipename and box name\n");
    return -1;
  }

  if (argc == 3) {
    printf("Specify server's box name\n");
    return -1;
  }

  // Get the server path
  char name[strlen(MB_PATH) + strlen(argv[1])];
  strcpy(name, MB_PATH);
  strcat(name, argv[1]);

  // opening tfs
  int server = open(name, O_WRONLY);
  if (server == -1) {
    fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
    return -1;
  }

  char *pub_pipename = argv[2];

  // remove pipe if it does exist
  if (unlink(pub_pipename) != 0 && errno != ENOENT) {
    fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", pub_pipename,
            strerror(errno));
    return -1;
  }

  // create pipe
  if (mkfifo(pub_pipename, 0640) != 0) {
    fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
    return -1;
  }

  // Create the publisher path
  char pub_name[strlen(PUB_PATH) + strlen(argv[2])];
  strcpy(pub_name, PUB_PATH);
  strcat(pub_name, argv[2]);

  char buffer[BUFFER_SIZE];
  char *box_name = argv[3];

  // Publisher registration request to server
  sprintf(buffer, "%d|%s|%s", OP_CODE_REGISTER_PUBLISHER, pub_name, box_name);

  send_msg(server, buffer);

  // open pipe for writing
  // this waits for someone to open it for reading
  int publisher = open(pub_pipename, O_RDONLY);
  if (publisher == -1) {
    fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
    return -1;
  }

  while (true) {
    char answer[BUFFER_SIZE];
    ssize_t ret = read(publisher, answer, BUFFER_SIZE - 1);
    answer[ret] = 0;

    char *compare = "0";
    if (strcmp(answer, compare) == 0) {
      fprintf(stdout, "[LOG]: publisher registration sucessfull\n");
    } else {
      close(publisher);
    }
  }
}
