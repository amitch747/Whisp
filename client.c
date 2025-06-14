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
    printf("Sending authMsg\n");
    if (send(sockfd, authMsg, 3, 0) != 3) {
        perror("client: authMsg");
        return -1;
    }

    char authResp[2];
    if (recv(sockfd, authResp, 2, 0) != 2){
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
    printf("Sending reqMsg\n");
    if (send(sockfd, reqMsg, reqMsgLength, 0) != reqMsgLength) {
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
    if(recv(sockfd, reqResp, 10, 0)< 10) {
        return -1;
    }
    // Ensure we've succeded
    if (reqResp[0] != 0x05 || reqResp[1] != 0x00) {
        return -1;
    }


    return 0;
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

        inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),s, sizeof s);
        printf("client: attemping connection to %s\n", s);

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("client: connect to tor");
            close(sockfd);
            continue;
        }
        
        // Now do SOCKS5 handshake
        if (socks5Connect(sockfd, WHISP_ONION, PORT) == -1) {
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

