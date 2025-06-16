#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <pthread.h>

#include <sodium.h>

#include <poll.h>


#define PORT "8888" 
#define BACKLOG 10 // pending connections for listen queue
#define MAXSESSIONS 10
static Session* g_sessions = NULL; // Used for server failure



static void sigintHandler()
{
    const char *bye = "SERVER_GOING_AWAY";
    for (int s = 0; s < MAXSESSIONS; ++s) {
        if (!g_sessions[s].active) continue;

        for (int c = 0; c < MAXCLIENTS; ++c) {
            int fd = g_sessions[s].sessionPFDS[c].fd;
            if (fd == -1) continue;

            
            send(fd, bye, strlen(bye) + 1, MSG_NOSIGNAL); // MSG_NOSIGNAL ignores send errors. Don't care.
            shutdown(fd, SHUT_RDWR); // FIN 
        }
    }
    _exit(0); // Special kernel exit
}

// get sockaddr, v4/v6 - Taken from Beej
void *getInAddress(struct sockaddr *sa)
{
    if(sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


int findSessionIndex(int cfd, Session* session) {
    int cfdIndex = -1;
    for (int i = 0; i < MAXCLIENTS; i++) {
        if (session->sessionPFDS[i].fd == cfd) {
            cfdIndex = i;
            break;
        }
    }
    if (cfdIndex == -1) {
        printf("Error: Client %d not found in session\n", cfd);
        return cfdIndex;
    }
    return cfdIndex;
}


int32_t createSession(int cfd, Session* sessionList) 
{
    // generate and add to list. return -1 if unable
    for (int i = 0; i < MAXSESSIONS; i++) {
        if (!sessionList[i].active) {
            sessionList[i].active = 1;
            sessionList[i].sessionPFDS[0].fd = cfd; // Assign host file descriptor
            sessionList[i].sessionPFDS[0].events = POLLIN;
            sessionList[i].clientCount = 1;
            sessionList[i].createdTime = time(NULL);
            sessionList[i].sessionId = randombytes_uniform(999999) + 100000; // Uses somekind of OS entropy?
        
            for (int c = 1; c < MAXCLIENTS; c++) {
                sessionList[i].sessionPFDS[c].fd = -1; // -1 for empty slot
            }
            return sessionList[i].sessionId;
        }
    }
    return -1;
}


void chatRelay(int cfd, Session* session) 
{
    // Assume that the session's sessionPFDS array is already set
    char networkBuf[256];

    struct pollfd mySocket;
    mySocket.fd = cfd;
    mySocket.events = POLLIN;

    for (;;) {
       // int clientCount = session->clientCount;
       // struct pollfd sessionPFDS[MAXCLIENTS];

        printf("Relay loop for thread %d\n",cfd);
        //int pollCount = poll(sessionPFDS, clientCount, -1); // Block until data in a socket
        int pollCount = poll(&mySocket, 1, -1);

        if (pollCount == -1) {
            perror("poll");
            exit(1);
        }

        // Handle personal cfd, also check for input from others, if recived send to cfd client
        if (mySocket.revents & POLLHUP) {
            printf("Client %d hung up\n", cfd);
            break;
        }

        if (mySocket.revents & POLLIN) {
            printf("Relaying message for client %d\n",cfd);

            memset(networkBuf, 0, sizeof(networkBuf)); 
            int numbytes = recv(cfd, &networkBuf, sizeof(networkBuf), 0);
            if (numbytes <= 0) {
                printf("Client %d disconnected\n", cfd);
                break;
            }

            printf("Client %d message: %s\n",cfd, networkBuf);

            if (strcmp(networkBuf, "EXIT") == 0) break;
            else {
                // Relay message to other clients
                for (int i = 0; i < MAXCLIENTS; i++) {
                    if ((session->sessionPFDS[i].fd != cfd) && session->sessionPFDS[i].fd != -1) {
                        send(session->sessionPFDS[i].fd, networkBuf, numbytes, 0);
                    }
                }
            }
            
        }
    }


    return;
}


int validateSession(int32_t sessionID, Session* sessionList) {
    for (int i = 0; i < MAXCLIENTS; i++) {
        if (sessionList[i].active && sessionList[i].sessionId == sessionID){
            if (sessionList[i].clientCount == MAXCLIENTS) {
                return 2;
            }
            else {
                return 1;
            }
        } 
    }

    return 0;
}

Session* addToSession(int cfd, Session* sessionList, int32_t sessionID) 
{
    for (int i = 0; i < MAXSESSIONS; i++) {
        if (sessionList[i].active && sessionList[i].sessionId == sessionID){
            sessionList[i].clientCount += 1;
            sessionList[i].sessionPFDS[sessionList[i].clientCount -1].fd = cfd;
            sessionList[i].sessionPFDS[sessionList[i].clientCount -1].events = POLLIN;
            return &sessionList[i];
        } 
    }
    return NULL;
}

void leaveSession(int cfd, Session* session) {
    int cfdIndex = findSessionIndex(cfd, session);

    // Remove user from session
     session->clientCount -= 1;
     session->sessionPFDS[cfdIndex].fd = -1;

    // Start at leaving index, slide the rest down
    for (int j = cfdIndex; j < MAXCLIENTS - 1; ++j) {
        session->sessionPFDS[j] = session->sessionPFDS[j + 1];
    }

    session->sessionPFDS[MAXCLIENTS - 1].fd = -1; 


    // Wipe session if out of users (I NEED MUTEX LOCKS THIS COULD BE NASTY)
    if (session->clientCount == 0) {
        session->active = 0;
        session->sessionId = -1;
    }
}


void* clientHandler(void* args) 
{
    struct ThreadArgs* thread_args = (struct ThreadArgs*)args;
    int cfd = *(int*)thread_args->client_fd;
    Session* sessionList = thread_args->sessions;
    free(thread_args->client_fd);
    free(args);

    printf("Connection on thread %i\n",cfd);
    // Get Option
    int option = 0;
    while(option != 3) {
        printf("Menu loop thread %i\n",cfd);
        int optionByte = recv(cfd, &option, 1, 0);
        if (optionByte > 0) {
            printf("Client %d sent option %d\n", cfd, option);
        }
        if (optionByte <= 0) {
            printf("Client %d disconnected in menu\n", cfd);
            break;  
        }

        switch(option){
            case 1: {
                // HOST
                int32_t newSession = createSession(cfd, sessionList);
                int32_t newSession_net = htonl(newSession);  // Convert to network order
                send(cfd, &newSession_net, sizeof(int32_t), 0); // Send host client sessionID or -1 if failed
                // Host is in session, and on their end they are in the chat loop
                // Find assigned session
                Session* hostSession;
                for (int i = 0; i < MAXSESSIONS; i++) {
                    if (sessionList[i].sessionId == newSession) {
                        hostSession = &sessionList[i];
                        break;
                    }
                }
                chatRelay(cfd, hostSession);
                // Remove this guy from the session, also check if session is now empty
                leaveSession(cfd, hostSession);

                break;
            }
            case 2: { 
                // JOIN
                int32_t joinSession_net;
                recv(cfd, &joinSession_net, sizeof(int32_t), 0);
                int32_t sessionId = ntohl(joinSession_net);  // Convert from network order
                // Confirm valid sessionID 
                int valid = validateSession(sessionId, sessionList);
                send(cfd, &valid, sizeof(int), 0);
                // Add to session
                Session* clientSession = addToSession(cfd, sessionList, sessionId);
                // Chat loop
                chatRelay(cfd, clientSession);
                // Remove this guy from the session, also check if session is now empty
                leaveSession(cfd, clientSession);
                break;
            }
            case 3: { 
                // QUIT
                break;
            }
        }
    }
    close(cfd);
    return NULL;
}




int main(void)
{
    if (sodium_init() < 0) {
        fprintf(stderr, "Sodium not working!!!\n");
        exit(1);
    }

    Session currentSessions[MAXSESSIONS];
    for (int i = 0; i < MAXSESSIONS; i++) {
        currentSessions[i].active = 0; // Inactive
        currentSessions[i].clientCount = 0;
    }

    g_sessions = currentSessions;
    struct sigaction sa;
    memset(&sa, 0, sizeof sa); // Lots of other stuff to ignore
    sa.sa_handler = sigintHandler; // This is a union. Look into more kinda interesting

    
    sigemptyset(&sa.sa_mask); // Clear mask
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);  // Ctrl-C
    sigaction(SIGTERM, &sa, NULL); // kill


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
        printf("Server: waiting for new connections\n");
        sin_size = sizeof client_addr;
        newClient = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);
        if (newClient == -1) {
            perror("server: accept");
            continue;
        }

        inet_ntop(client_addr.ss_family, getInAddress((struct sockaddr *)&client_addr), s, sizeof(s));
        printf("Server: connection secured from %s\n", s);

        // Dedicated memory in case newClient is overwritten
        int *cSock = malloc(sizeof(int));
        *cSock = newClient;

        struct ThreadArgs* args = malloc(sizeof(struct ThreadArgs));
        args->client_fd = cSock;
        args->sessions = currentSessions;


        pthread_t cThread;
        pthread_create(&cThread, NULL, &clientHandler, args);
        pthread_detach(cThread);
    }
    return 0;
}

