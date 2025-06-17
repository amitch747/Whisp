#include <stdint.h>
#include <time.h>
#include <poll.h>
#include <pthread.h>

#define MAXCONNECTIONS 20
#define MAXSESSIONS 10
#define MAXCLIENTS 4
#define PORT "8888" 
#define BACKLOG 20 // pending connections for listen queue

extern pthread_mutex_t sessions_mutex;
extern pthread_mutex_t broadcast_mutex;


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

struct clientData {
    int cfd;
    Session* session;
};