#ifndef CONFIG_H
#define CONFIG_H

// FS root inode number
#define ROOT_DIR_INUM (0)

#define MAX_FILE_NAME (40)

#define DELAY (5000)

#define MB_PATH "../mbroker/"
#define SUB_PATH "../subscriber/"
#define PUB_PATH "../publisher/"
#define MAN_PATH "../manager/"

#define BUFFER_SIZE (256)

enum {
  OP_CODE_REGISTER_PUBLISHER = 1,
  OP_CODE_REGISTER_SUBSCRIBER = 2,
  OP_CODE_CREATE_BOX_REQUEST = 3,
  OP_CODE_CREATE_BOX_VERIFY = 4,
  OP_CODE_REMOVE_BOX_REQUEST = 5,
  OP_CODE_REMOVE_BOX_VERIFY = 6,
  OP_CODE_LIST_BOX_REQUEST = 7,
  OP_CODE_LIST_BOX_VERIFY = 8,
  OP_CODE_SEND_PUBLISHER = 9,
  OP_CODE_RECEIVE_SUBSCRIBER = 10
};

#endif // CONFIG_H
