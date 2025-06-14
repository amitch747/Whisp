#include "server.h"

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

#define PORT "8888" // localhost port
#define BACKLOG 10 // pending connections for listen queue
#define MAXSESSIONS 10


// get sockaddr, v4/v6 - Taken from Beej
void *get_in_addr(struct sockaddr *sa)
{
    if(sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}




void* clientHandler(void* sock) {
    int cfd = *(int*)sock;
    free(sock);

    printf("Connection on thread %i\n",cfd);
    // Get Option
    int option = 0;
    while(option != 3){
        int optionByte = recv(cfd, &option, 1, 0);
        if (optionByte > 0) {
            printf("Client %d sent option %d\n", cfd, option);
        }
        char *hosting = "Hosting new session!";
        char *joining = "Joining";
        char *quiting = "Quiting Whisp";

        switch(option){
            case 1: {
                send(cfd, hosting, strlen(hosting), 0); 
                break;
            }
            case 2: {
                send(cfd, joining, strlen(joining), 0);  
                break;
            }
            case 3: {
                send(cfd,quiting, strlen(quiting), 0);  
                break;
            }
        }
    }



    // char buffer[1024];
    // int bytes = recv(cfd, buffer, sizeof(buffer)-1, 0);
    // if (bytes > 0) {
    //     buffer[bytes] = '\0';
    //     printf("Thread %d received: %s\n", cfd, buffer);
    //     char *msg = "Thx for the message bozo";
    //     send(cfd, msg, strlen(msg), 0);    
    // }
    
    
    close(cfd);
    return NULL;
}


int main(void)
{
    Session currentSessions[MAXSESSIONS];

    int sockfd, newClient;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage client_addr;
    socklen_t sin_size;
    int gaiRet;
    int yes=1;
    char s[INET6_ADDRSTRLEN]; 


    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // What is this for again?

    // Get local machine address info
    if((gaiRet = getaddrinfo("127.0.0.1", PORT, &hints, &servinfo)) != 0 ) {
        fprintf(stderr, "server: getaddrinfo: %s\n", gai_strerror(gaiRet));
        return 1;
    }
    // Loop through list of address info, try to bind listen socket to port
    for(p = servinfo; p != NULL; p = p->ai_next){
        // Build socket
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            perror("server: socket");
            continue;
        }
        // Allow for quick socket reuse on restart
        if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("server: setsockopt");
            exit(1);
        }
        // Bind socket
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        break;
    }
    freeaddrinfo(servinfo); // No need for this anymore, we have our listen socket

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if(listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    // Run forever
    while(1) {
        sin_size = sizeof client_addr;
        newClient = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);
        if (newClient == -1) {
            perror("server: accept");
            continue;
        }

        inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), s, sizeof(s));

        printf("server: connection secured from %s\n", s);

        // Dedicated memory in case newClient is overwritten
        int *cSock = malloc(sizeof(int));
        *cSock = newClient;

        pthread_t cThread;
        pthread_create(&cThread, NULL, &clientHandler, cSock);
        pthread_detach(cThread);
    }




    return 0;
}

