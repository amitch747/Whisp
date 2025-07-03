#ifndef UTILS_H
#define UTILS_H

#include <sys/socket.h>

#define DELIMITER '|'
#define DEFAULT_NAME "????"
#define DEFAULT_COLOR "\033[37m"

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m" 
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"

#define PORT "8888" 

typedef struct {
    char username[12];
    char color [9];
    char message[256];
} ChatMessage;

void *getInAddress(struct sockaddr *sa);
int sendAll(int sock, void* buf, int len);
int recvAll(int sock, void* buf, int len);

#endif