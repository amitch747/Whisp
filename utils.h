#ifndef UTILS_H
#define UTILS_H

#include <sys/socket.h>

#define DELIMITER '|'
#define DEFAULT_NAME "????"
#define DEFAULT_COLOR "#FFFFFF"

typedef struct {
    char username[12];
    char color [8];
    char message[256];
} ChatMessage;

void *getInAddress(struct sockaddr *sa);
int sendAll(int sock, void* buf, int len);
int recvAll(int sock, void* buf, int len);

#endif