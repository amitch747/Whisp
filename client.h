#ifndef CLIENT_H
#define CLIENT_H

#define MAXBYTES 256

typedef enum {
    STATE_MENU,
    STATE_HOSTING,
    STATE_JOINING,
    STATE_QUIT
} ClientState;

typedef enum {
    MSG_VALID,
    MSG_EMPTY,
    MSG_INVALID,
    MSG_SPAM
} MessageState;

#endif