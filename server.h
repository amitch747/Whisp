#include <time.h>


typedef struct {
    int sessionId;
    int sfdArray[4];
    int clientCount;
    time_t createdTime;
} Session;