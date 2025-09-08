#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <regex.h>

#define PORT "3000"
#define BUFFER_SIZE 50

void *get_in_addr(struct sockaddr *sa){
    if (sa ->sa_family == AF_INET){
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int findListener(char *hostname){
    int serverfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(hostname, PORT, &hints, &servinfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next){
        if((serverfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            perror("client: socket");
            continue;
        }

        inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
        printf("client: attempting conenction to %s\n", s);

        if(connect(serverfd, p->ai_addr, p->ai_addrlen) == -1){
            perror("client: connect");
            close(serverfd);
            continue;
        }

        break;
    }

    if(p == NULL){
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    printf("client: connected to %s\n", s);

    freeaddrinfo(servinfo);

    return serverfd;
}

int main(int argc, char *argv[]){
    int numbytes;
    char buf[] = "GET /rfc1945f.html HTTP/1";

    if(argc != 2){
        fprintf(stderr, "usage: client hostname\n");
        exit(1);
    }

    int serverfd = findListener(argv[1]);
    if(serverfd == -1){
        printf("finding listener descriptor failed: %d", serverfd);
    }

    if ((numbytes = send(serverfd, buf, BUFFER_SIZE-1, 0)) == -1){
        perror("send");
        exit(1);
    }

    buf[numbytes] = '\0';
    // printf("client: received '%s'\n", buf);
    close(serverfd);
    return 0;
}
