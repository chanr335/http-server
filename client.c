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
#define BUFFER_SIZE 4096 

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
    if(argc != 2){
        fprintf(stderr, "usage: client hostname\n");
        exit(1);
    }

    int serverfd = findListener(argv[1]);
    if(serverfd == -1){
        printf("finding listener descriptor failed: %d", serverfd);
    }

    const char *request = "GET /helloworld.html HTTP/1.0\r\n\r\n";
    ssize_t numbytes = send(serverfd, request, strlen(request), 0);
    if (numbytes == -1) {
        perror("send");
        close(serverfd);
        exit(1);
    }

    printf("client: sent request:\n%s\n", request);
    printf("client: waiting for response...\n\n");

    // loop to receive the full response (headers + file)
    char buf[BUFFER_SIZE];
    while ((numbytes = recv(serverfd, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[numbytes] = '\0';   // null terminate buffer
        printf("%s", buf);     
    }

    if (numbytes == -1) {
        perror("recv");
    } else {
        printf("\nclient: connection closed by server.\n");
    }

    close(serverfd);
    return 0;
}
