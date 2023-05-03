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

int ACTIVE_SESSIONS = 0;

/* Function used to get the path and box name of the client */
char *cut(char string[], char *mode) {
  int counter = 0;
  char *pipe = "path";
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

/* Create a box in the server */

int create_box(char *boxName) {
  char boxToCreate[strlen(boxName + 1)];
  strcpy(boxToCreate, "/");
  strcat(boxToCreate, boxName); // Add a '/' to be added in the path

  // Creating box
  int boxCreated = tfs_open(boxToCreate, TFS_O_CREAT);
  if (boxCreated == -1) {
    return -1;
  }

  return boxCreated;
}

/* Remove a box in the server */

int remove_box(char *clientPath, char *boxName) {
  char addBar[strlen(boxName + 1)];
  strcpy(addBar, "/");
  strcat(addBar, boxName); // Add a '/' to be added in the path

  char boxToRemove[strlen(addBar) + strlen(clientPath)];
  strcpy(boxToRemove, clientPath);
  strcat(boxToRemove, addBar);

  int remove = tfs_unlink(boxToRemove);

  return remove;
}

int main(int argc, char **argv) {

  if (argc == 1) {
    printf("Specify server's pipe name and max sessions\n");
    return -1;
  }

  if (argc == 2) {
    printf("Specify server's max sessions\n");
    return -1;
  }

  char *pipename = argv[1];
  int MAX_SESSIONS = atoi(argv[2]);

  tfs_init(NULL);

  // Remove pipe if it does exist
  if (unlink(pipename) != 0 && errno != ENOENT) {
    fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", pipename,
            strerror(errno));
    return -1;
  }

  // Create pipe
  if (mkfifo(pipename, 0640) != 0) {
    fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
    return -1;
  }

  while (true) {
    // Open pipe for writing
    // This waits for someone to open it for reading
    int server = open(pipename, O_RDONLY);
    if (server == -1) {
      fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
      return -1;
    }

    char buffer[BUFFER_SIZE];

  read_line:
    // Read the message received in the server
    ssize_t ret = read(server, buffer, BUFFER_SIZE - 1);
    buffer[ret] = 0;

    char op_code = buffer[0];

    switch (op_code) {
    case '1':
      /* Verifying if the client can be created */
      ACTIVE_SESSIONS++;
      if (ACTIVE_SESSIONS >= MAX_SESSIONS) {
        fprintf(stdout, "[ERR]: server is full\n");
        return -1;
      }

      /* Answering to the request of regist a publisher */
      char safeBuffer1[BUFFER_SIZE];
      strcpy(safeBuffer1, buffer);
      char *clientPath1 = cut(safeBuffer1, "path");
      // char *boxName1 = cut(buffer, "box");

      // opening publisher's pipe
      int publisher = open(clientPath1, O_WRONLY);
      if (publisher == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        return -1;
      }

      // send answer to the subscriber
      send_msg(publisher, "0");

      break;

    case '2':
      /* Verifying if the client can be created */
      ACTIVE_SESSIONS++;
      if (ACTIVE_SESSIONS >= MAX_SESSIONS) {
        fprintf(stdout, "[ERR]: server is full");
        return -1;
      }

      /* Answering to the request of regist a subscriber */
      char safeBuffer2[BUFFER_SIZE];
      strcpy(safeBuffer2, buffer);
      char *clientPath2 = cut(safeBuffer2, "path");
      // char *boxName2 = cut(buffer, "box");

      // opening publisher's pipe
      int subscriber = open(clientPath2, O_WRONLY);
      if (subscriber == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        return -1;
      }

      // send answer to the subscriber
      send_msg(subscriber, "0");

      break;

    case '3':
      /* Answering to the request of create a box */
      char safeBuffer3[BUFFER_SIZE];
      strcpy(safeBuffer3, buffer);
      char *clientPath3 = cut(safeBuffer3, "path");
      char *boxName3 = cut(buffer, "box");

      int box = create_box(boxName3);

      // opening Manager's pipe
      int manager3 = open(clientPath3, O_WRONLY);
      if (manager3 == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        return -1;
      }

      // Send answer to the manager
      char answerToRequest3[BUFFER_SIZE];
      sprintf(answerToRequest3, "%d|%d|%s", OP_CODE_CREATE_BOX_VERIFY, box,
              "\0");
      send_msg(manager3, answerToRequest3);

      break;

    case '5':
      /* Answering to the request of create a box */
      char safeBuffer5[BUFFER_SIZE];
      strcpy(safeBuffer5, buffer);
      char *clientPath5 = cut(safeBuffer5, "path");
      char *boxName5 = cut(buffer, "box");

      int remove = remove_box(clientPath5, boxName5);

      // opening Manager's pipe
      int manager5 = open(clientPath5, O_WRONLY);
      if (manager5 == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        return -1;
      }

      // Send answer to the manager
      char answerToRequest5[BUFFER_SIZE];
      sprintf(answerToRequest5, "%d|%d|%s", OP_CODE_REMOVE_BOX_VERIFY, remove,
              "\0");
      send_msg(manager5, answerToRequest5);

      break;

    case '7':

      break;

    default:
      goto read_line;
    }
  }
}
