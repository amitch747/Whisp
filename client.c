#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <pthread.h>


#define WHISP_ONION "bz5yr2xruyrp3hcfkcefaatool4r7jqpynpxvl5nvliqqkkac7d3x5yd.onion" // Hardcode for now. Not ideal.
#define PORT "8888" // localhost port
#define MAXBYTES 200


// get sockaddr, v4/v6 - Taken from Beej
void *get_in_addr(struct sockaddr *sa)
{
    if(sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


int main(void) 
{
    // Connect to server, get back a message
    int sockfd, numbytes;
    char buf[MAXBYTES];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(WHISP_ONION, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),s, sizeof s);
        printf("client: attemping connection to %s\n", s);

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("client: connect");
            close(sockfd);
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),s, sizeof s);
    printf("Client connected to %s\n", s);

    freeaddrinfo(servinfo); // drop the linked list, we have a connection

    char *msg = "Hello server";
    send(sockfd, msg, strlen(msg), 0);  


    if ((numbytes = recv(sockfd, buf, MAXBYTES-1, 0)) == -1) {
        perror("recv");
        exit(1);
    }
    buf[numbytes] = '\0';

    printf("client received '%s'\n", buf);

    close(sockfd);

    return 0;
    /*
        Display options:
        1. Host Session
        2. Join Session
        3. Quit Application
    */

    // HOST

    // JOIN

    // QUIT




}