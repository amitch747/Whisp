#define PORT "8888" // localhost port
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