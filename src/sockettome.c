#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <signal.h>
#include <strings.h>

/*
static char* inadport_decimal(struct sockaddr_in* sad) {
    static char buf[32];
    int a;

    a = ntohl(0xffffffff & sad->sin_addr.s_addr);
    sprintf(buf, "%d.%d.%d.%d:%d",
                 0xff & (a >> 24),
                 0xff & (a >> 16),
                 0xff & (a >> 8),
                 0xff & a,
                 0xffff & (int)ntohs(sad->sin_port));

    return buf;
}
*/

int serve_socket(int port) {
    struct hostent* he = gethostbyname("localhost");
    if (!he) {
        puts("gethostbyname(): can not get entry for localhost.\n");
        exit(1);
    }

    struct sockaddr_in sn;
    bzero((char*)(&sn), sizeof(sn));
    sn.sin_family = AF_INET;
    sn.sin_port = htons((short)port);
    sn.sin_addr.s_addr = htonl(INADDR_ANY);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket()");
        exit(1);
    }

    int option = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    if (bind(sockfd, (struct sockaddr*)(&sn), sizeof(sn)) == -1) {
        perror("bind()");
        exit(1);
    }

    return sockfd;
}

int accept_connection(int serverfd) {
    if(listen(serverfd, 1) == -1) {
        perror("listen()");
        exit(1);
    }

    int clientfd = accept(serverfd, NULL, NULL);
    if(clientfd == -1) {
        perror("accept()");
        exit(1);
    }

    return clientfd;
}

int request_connection(char* hn, int port) {
    struct hostent* he = gethostbyname(hn);
    if (!he) {
        printf("gethostbyname(): can not lookup hostname '%s'.\n", hn);
        exit(1);
    }

    int serverfd;
    for (;;) {
        struct sockaddr_in sn;

        bzero((char*)(&sn), sizeof(sn));
        sn.sin_family = AF_INET;
        sn.sin_port  = htons((short)port);
      	sn.sin_addr = *((struct in_addr *)(he->h_addr_list[0]));

        serverfd = socket(AF_INET, SOCK_STREAM, 0);
        if (serverfd == -1) {
            perror("socket()");
            exit(1);
        }

        if (connect(serverfd, (struct sockaddr*)(&sn), sizeof(sn)) != -1) break;

        sleep(1);
        perror("connect()");
    }

    return serverfd;
}
