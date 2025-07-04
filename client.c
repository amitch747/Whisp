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
#include <poll.h>

#include "utils.h"
#include "client.h"

char username[12];
char color[9];

char* getWhispOnion() {
    char* onion = getenv("WHISP_ONION");
    if (!onion) {
        fprintf(stderr, "Error: WHISP_ONION environment variable not set\n");
        fprintf(stderr, "Please set: export WHISP_ONION=your_onion_address.onion\n");
        exit(1);
    }
    return onion;
}

// Function needed to get through tor - SOCKS5 proxy
int socks5Connect(int sockfd, const char * addr, const char * port) 
{
    /*
        The client connects to the server, and sends a version
        identifier/method selection message:

        +----+----------+----------+
        |VER | NMETHODS | METHODS  |
        +----+----------+----------+
        | 1  |    1     | 1 to 255 |
        +----+----------+----------+

        The VER field is set to X'05' for this version of the protocol.  The
        METHODS field contains the number of method identifier octets that
        appear in the METHODS field.
    */
    char authMsg[3] = {0x05, 0x01, 0x00}; // Sock5, 1 method, no auth
    //printf("Sending authMsg\n");
    if (sendAll(sockfd, authMsg, 3) == -1) {
        perror("client: authMsg");
        return -1;
    }

    char authResp[2];
    if (recvAll(sockfd, authResp, 2) != 2){
        return -1;
    }

    /*
        The SOCKS request is formed as follows:

        +----+-----+-------+------+----------+----------+
        |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
        +----+-----+-------+------+----------+----------+
        | 1  |  1  | X'00' |  1   | Variable |    2     |
        +----+-----+-------+------+----------+----------+

        Where:

        o  VER    protocol version: X'05'
        o  CMD
            o  CONNECT X'01'
            o  BIND X'02'
            o  UDP ASSOCIATE X'03'
        o  RSV    RESERVED
        o  ATYP   address type of following address
            o  IP V4 address: X'01'
            o  DOMAINNAME: X'03'
            o  IP V6 address: X'04'
        o  DST.ADDR       desired destination address
        o  DST.PORT desired destination port in network octet
            order

    The SOCKS server will typically evaluate the request based on source
    and destination addresses, and return one or more reply messages, as
    appropriate for the request type.

    In an address field (DST.ADDR, BND.ADDR), the ATYP field specifies
    the type of address contained within the field:
        o  X'03'
            the address field contains a fully-qualified domain name.  The first
            octet of the address field contains the number of octets of name that
            follow, there is no terminating NUL octet.
    */
    char reqMsg[256] = {0x05, 0x01, 0x00, 0x03};
    // Length of address
    int addrLength = strlen(addr);
    // Address
    reqMsg[4] = addrLength;
    memcpy(&reqMsg[5], addr, addrLength);
    // Port, should it be little or big end?
    int portInt = atoi(port);
    uint16_t portNet = htons(portInt);
    memcpy(&reqMsg[5+addrLength], &portNet, 2);
    // for (int i = 0; i< 256; i++){
    //     printf("%c",reqMsg[i]);
    // }
    int reqMsgLength = 5 + addrLength + 2;
    //printf("Sending reqMsg\n");
    if (sendAll(sockfd, reqMsg, reqMsgLength) != reqMsgLength) {
        perror("client: reqMsg");
        return -1;
    }

    /*
        The SOCKS request information is sent by the client as soon as it has
        established a connection to the SOCKS server, and completed the
        authentication negotiations.  The server evaluates the request, and
        returns a reply formed as follows:

        +----+-----+-------+------+----------+----------+
        |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
        +----+-----+-------+------+----------+----------+
        | 1  |  1  | X'00' |  1   | Variable |    2     |
        +----+-----+-------+------+----------+----------+

        Where:
        o  VER    protocol version: X'05'
        o  REP    Reply field:
            o  X'00' succeeded
            o  X'01' general SOCKS server failure
            o  X'02' connection not allowed by ruleset
            o  X'03' Network unreachable
            o  X'04' Host unreachable
            o  X'05' Connection refused
            o  X'06' TTL expired
            o  X'07' Command not supported
            o  X'08' Address type not supported
            o  X'09' to X'FF' unassigned
        o  RSV    RESERVED
        o  ATYP   address type of following address
            o  IP V4 address: X'01'
            o  DOMAINNAME: X'03'
            o  IP V6 address: X'04'
        o  BND.ADDR       server bound address
        o  BND.PORT       server bound port in network octet order

        Fields marked RESERVED (RSV) must be set to X'00'.

        If the chosen method includes encapsulation for purposes of
        authentication, integrity and/or confidentiality, the replies are
        encapsulated in the method-dependent encapsulation
    
    
    */
    char reqResp[256];
    if(recvAll(sockfd, reqResp, 10) < 10) {
        return -1;
    }
    // Ensure we've succeded
    if (reqResp[0] != 0x05 || reqResp[1] != 0x00) {
        return -1;
    }


    return 0;
}



ClientState handleMenu(int sockfd)
 {
    struct pollfd pfds[2];
    pfds[0].fd = sockfd;
    pfds[0].events = POLLIN | POLLERR | POLLHUP;
    pfds[1].fd = STDIN_FILENO;
    pfds[1].events = POLLIN;

    int choice = 0;
    char line[MAXBYTES];

    for (;;) {
        printf("1. Host Session\n2. Join Session\n3. Quit\nEnter choice: ");
        fflush(stdout);

        if (poll(pfds, 2, -1) == -1) { 
            perror("poll"); 
            return STATE_QUIT; 
        }

        // Unexpected server data
        if (pfds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            printf("\nLost connection to server\n");
            return STATE_QUIT;
        }
        if (pfds[0].revents & POLLIN) {
            char buf[MAXBYTES];
            int n = recv(sockfd, buf, sizeof buf - 1, 0);  

            if (n == 0) {                    
                printf("\nServer closed the connection\n");
                return STATE_QUIT;
            }
            if (n < 0) {
                perror("recv");
                return STATE_QUIT;
            }

        
            buf[n] = '\0';

            if (strcmp(buf, "SERVER_SHUTDOWN") == 0) {
                printf("\nServer is shutting down\n");
                return STATE_QUIT;
            }
        }

        // Keyboard input
        if (pfds[1].revents & POLLIN) {
            if (fgets(line, sizeof(line), stdin) != NULL) {
                choice = atoi(line);
                memset(line, 0, sizeof(line));
                switch(choice) {
                    case 1: 
                        // HOST
                        send(sockfd, &choice, 1, 0);
                        return STATE_HOSTING;
                        break;
                    
                    case 2: 
                        // JOIN
                        send(sockfd, &choice, 1, 0);
                        return STATE_JOINING;
                        break;
                    
                    case 3:
                        // QUIT
                        send(sockfd, &choice, 1, 0);  
                        return STATE_QUIT;
                        break;
                    
                    default:
                        printf("Invalid option\n");
                        return STATE_MENU;
                }
            }
        }
    }
    return STATE_QUIT;
}

int isAsciiMsg(const char* input) {
    for (size_t i = 0; input[i] != '\0'; i++) {
        if (input[i] < 32 || input[i] > 126) {
            return 0;
        }
    }
    return 1;
}

MessageState validateMessage(const char* input) {
    size_t len = strlen(input);
    if (len == 0) return MSG_EMPTY;

    static time_t lastMessage = 0; // Persists in memory 
    time_t now = time(NULL);
    if (now - lastMessage < 2) {
        return MSG_SPAM;
    }
    lastMessage = now;

    if (!isAsciiMsg(input)) return MSG_INVALID;

    return MSG_VALID;
}

void serializeMessage(char* buffer, const char* name, const char* color, const char* msg) {
    snprintf(buffer, 276, "%s|%s|%s", 
        name ? name : DEFAULT_NAME, color ? color : DEFAULT_COLOR, msg);
    //printf("%s\n", buffer);
}

void unpackMessage(const char* buffer, ChatMessage* msgData) {
    char temp[276];
    strncpy(temp, buffer, sizeof(temp)-1);
    temp[sizeof(temp)-1] = '\0';  // Manually null char just incase

    char* username = strtok(temp, "|");
    char* color = strtok(NULL, "|");
    char* message = strtok(NULL, ""); 

    if (username && strlen(username) > 0) {
        strncpy(msgData->username, username, sizeof(msgData->username));
        msgData->username[sizeof(msgData->username)-1] = '\0';

    } else {
        strcpy(msgData->username, DEFAULT_NAME);
    }
    
    // Set color
    if (color && strlen(color) > 0) {
        strncpy(msgData->color, color, sizeof(msgData->color));
    } else {
        strcpy(msgData->color, DEFAULT_COLOR);
    }
    
    // Set message
    if (message) {
        strncpy(msgData->message, message, sizeof(msgData->message));
        msgData->message[sizeof(msgData->message)-1] = '\0'; // I may be doing this too much, rework needed
    } else {
        msgData->message[0] = '\0';
    }
}

void chatLoop(int sockfd) {
    printf("Type EXIT to return to menu\n");
    // Setup poll. Client side ONLY tracks the socket connceted to tor and the fd for the keyboard
    struct pollfd pfds[2];
    pfds[0].fd = sockfd; // Tor socket
    pfds[0].events = POLLIN;
    pfds[1].fd = STDIN_FILENO; // Keyboard input
    pfds[1].events = POLLIN;


    char networkBuf[276];
    // Chat loop
    for(;;) {
        //printf("Chatloop\n");
        int pollCount = poll(pfds, 2, -1); // Block until keyboard or network change

        if (pollCount == -1) {
            perror("poll");
            exit(1);
        }
        if (pfds[0].revents & POLLHUP) {
            // Hangup, shut it down
            printf("Hangup signal from server! Returning to menu\n");
            break;
        }
        if (pfds[0].revents & POLLIN) {
            // Message (maybe)
            memset(networkBuf, 0, sizeof(networkBuf)); 
            
            int numbytes = recv(sockfd, networkBuf, sizeof(networkBuf)-1, 0);

            
            if (numbytes <= 0) {
                printf("Server error. Disconnecting\n");
                break;
            }
        
            networkBuf[numbytes] = '\0'; 
            ChatMessage msgData;
            unpackMessage(networkBuf, &msgData);
            printf("%s%s" RESET ": %s\n", msgData.color, msgData.username, msgData.message);
        }
        if (pfds[1].revents & POLLIN) {
            // Client keyboard
            char input[256];
            if (fgets(input, sizeof(input), stdin) != NULL) {

                // Check length
                if (strchr(input, '\n') == NULL) {
                    int c;
                    while ((c = getchar()) != '\n' && c != EOF);
                    printf("Message too long. Try again\n");
                    continue;
                }

                // Replace the newline character
                input[strcspn(input, "\n")] = '\0';
                MessageState messageState = validateMessage(input);

                switch(messageState) {
                    case MSG_EMPTY:
                        printf("Empty message. Try again\n");
                        break;
                    case MSG_SPAM:
                        printf("Please don't spam\n");
                        break;
                    case MSG_INVALID:
                        printf("Invalid ASCII characters\n");
                        break;
                    case MSG_VALID: {
                        // Pack username and color, then send
                        if (strcmp(input, "EXIT") == 0) {
                            sendAll(sockfd, "EXIT", 5);  // Send plain EXIT
                            return;
                        }
                        char msgBuffer[276];
                        serializeMessage(msgBuffer, username, color, input);
                        sendAll(sockfd, msgBuffer, strlen(msgBuffer) + 1);
                    }

                }
            }
        }
    }
}




ClientState handleHost(int sockfd) {
    // Get sessionID from server thread, it already knows you selected host
    int32_t newSession_net;
    int numbytes = recvAll(sockfd, &newSession_net, sizeof(int32_t));
    if (numbytes != sizeof(int32_t)) {
        perror("Failed to receive session ID");
        return STATE_MENU;
    }
    int32_t sessionId = ntohl(newSession_net);  // Convert from network order
    printf("Your session ID: %d\n", sessionId);

    chatLoop(sockfd);
    return STATE_MENU;
}



ClientState handleJoin(int sockfd) {
    //printf("Handling join\n");
    // Client sends sessionID to server whom is waiting
    char line[MAXBYTES];
    printf("Provide sessionID: ");
    if (fgets(line, sizeof(line), stdin) != NULL) {
        int32_t sessionID = atoi(line);
        int32_t netSessionID = htonl(sessionID);  // Convert to network order
        sendAll(sockfd, &netSessionID, sizeof(int32_t));
        // Get response from server, verify
        printf("Waiting for server confirmation...\n");
    
        int confirm;
        recvAll(sockfd, &confirm, sizeof(int));
        if (confirm == 1) {
            // Joined session
            // Chat Loop
            chatLoop(sockfd);
        }
        else if (confirm == 2) {
            // Session full
            printf("Session is full try again later\n");
        }
        else {
            printf("Invalid sessionID\n");
        }
    } 
    return STATE_MENU;
}



int main(void) 
{
    ClientState state = STATE_MENU;
  
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // We're connecting to tor first
    if ((rv = getaddrinfo("127.0.0.1", "9050", &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        inet_ntop(p->ai_family, getInAddress((struct sockaddr *)p->ai_addr),s, sizeof s);
        //printf("client: attemping connection to %s\n", s);

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("client: connect to tor");
            close(sockfd);
            continue;
        }
        
        // Now do SOCKS5 handshake
        if (socks5Connect(sockfd, getWhispOnion(), PORT) == -1) {
            perror("client: socks5 handshake failed");
            close(sockfd);
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }
    inet_ntop(p->ai_family, getInAddress((struct sockaddr *)p->ai_addr),s, sizeof s);


    freeaddrinfo(servinfo); // drop the linked list, we have a connection


    printf("\n/////////// WELCOME TO WHISP ///////////\n\n");
    int valid = 0;
    while (!valid) {
        printf("Set username (ENTER to skip): ");
        fflush(stdout);
        
        if (fgets(username, sizeof(username), stdin) != NULL) {
            if (strchr(username, '\n') == NULL) {
                // Too long
                printf("Username too long. Keep it under 12 characters\n");
                int c;
                while ((c = getchar()) != '\n' && c != EOF);
            } else {
                // Good size
                username[strcspn(username, "\n")] = '\0';
                valid = 1;  // Input is valid
                
                if (strlen(username) == 0) {
                    strcpy(username, DEFAULT_NAME);
                }
            }
        } else {
            // Just incase
            strcpy(username, DEFAULT_NAME);
            valid = 1;
        }
    }
    printf("Username set: %s\n", username);
    
    printf("Choose message color:\n");
    printf(RED "1. Red\n" RESET);
    printf(GREEN "2. Green\n" RESET);
    printf(YELLOW "3. Yellow\n" RESET);
    printf(BLUE "4. Blue\n" RESET);
    printf(MAGENTA "5. Magenta\n" RESET);
    printf(CYAN "6. Cyan\n" RESET);
    printf("Enter choice (1-6, ENTER to skip): ");
    
    char colorChoice[3];
    if (fgets(colorChoice, sizeof(colorChoice), stdin) != NULL) {
        int choice = atoi(colorChoice);
        switch(choice) {
            case 1: strcpy(color, RED); break;
            case 2: strcpy(color, GREEN); break;
            case 3: strcpy(color, YELLOW); break;
            case 4: strcpy(color, BLUE); break;
            case 5: strcpy(color, MAGENTA); break;
            case 6: strcpy(color, CYAN); break;
            default: strcpy(color, WHITE); break;
        }
    } else {
        strcpy(color, WHITE);
    }

    while(state != STATE_QUIT) {
        switch(state) {
            case STATE_MENU: 
                state = handleMenu(sockfd);
                break;
            case STATE_HOSTING:
                state = handleHost(sockfd);
                break;
            case STATE_JOINING:
                state = handleJoin(sockfd);
                break;
            default:
                state = handleMenu(sockfd);
                break;
        }
    }
    close(sockfd);
    return 0;
}