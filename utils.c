#include "utils.h"

#include <netinet/in.h> 

// get sockaddr, v4/v6 - Taken from Beej
void *getInAddress(struct sockaddr *sa)
{
    if(sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int sendAll(int sock, void* buf, int len) {
    char* dataBuf = (char*)buf;
    int sentBytes = 0;
    int numBytes;
    
    while(sentBytes < len) {
        numBytes = send(sock, dataBuf + sentBytes, len - sentBytes, 0);
        if (numBytes <= 0) {break;}
        sentBytes += numBytes;
    }
    return (numBytes <= 0 ? -1 : sentBytes);
}

int recvAll(int sock, void* buf, int len) {
    char* dataBuf = (char*)buf;
    int recvBytes = 0;
    int numBytes;
    
    while(recvBytes < len) {
        numBytes = recv(sock, dataBuf + recvBytes, len - recvBytes, 0);
        if (numBytes <= 0) {break;}
        recvBytes += numBytes;
    }
    return (numBytes <= 0 ? -1 : recvBytes);
}