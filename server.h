#include <stdint.h>
#include <time.h>
#include <poll.h>
#define MAXCLIENTS 4

typedef struct {
    int32_t sessionId;
    //int sfdArray[MAXCLIENTS];
    struct pollfd sessionPFDS[MAXCLIENTS];
    int clientCount;
    time_t createdTime;
    int active;
} Session;


struct ThreadArgs {
    int* client_fd;
    Session* sessions;
};