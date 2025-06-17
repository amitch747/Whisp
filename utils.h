#ifndef UTILS_H
#define UTILS_H

#include <sys/socket.h>

void *getInAddress(struct sockaddr *sa);
int sendAll(int sock, void* buf, int len);
int recvAll(int sock, void* buf, int len);

#endif