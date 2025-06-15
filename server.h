#include <stdint.h>
#include <time.h>

#define MAXCLIENTS 4

typedef struct {
    int32_t sessionId;
    int sfdArray[MAXCLIENTS];
    int clientCount;
    time_t createdTime;
    int active;
} Session;


struct ThreadArgs {
    int* client_fd;
    Session* sessions;
};