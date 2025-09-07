#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#define PORT "3000"

void sigchld_handler(int s){
    (void)s; // quiet unused variable warning

    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa)
{
    if(sa->sa_family == AF_INET){
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int getListener(void){
    int rv;
    int sockfd;
    int yes = 1;
    struct addrinfo hints, *servinfo, *p;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0){
        perror("getaddrinfo");
        return -1;
    }

    for(p=servinfo; p != NULL; p = p->ai_next){
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            perror("server: socket");
            continue;
        }

        if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
            perror("setsockopt");
            exit(1);
        }

        if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1){
            close(sockfd);
            perror("server: bind");
            continue;
        }
        break;
    }

    if(p == NULL){
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }


    freeaddrinfo(servinfo);

    if (listen(sockfd, 10) == -1){
        perror("listen");
        exit(1);
    }

    return sockfd;
}

int main(int argc, char* argv[]){
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    int newfd, numbytes;
    char s[INET6_ADDRSTRLEN];

    char hostname[20];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        printf("Hostname: %s\n", hostname);
    }

    int listener = getListener();
    if (listener == -1) {
        printf("listener descriptor failed: %d", listener);
    }

    struct sigaction sa;
    sa.sa_handler = sigchld_handler;   // reap all daad processes when receive SIGCHLD (a child terminated)
    sigemptyset(&sa.sa_mask);          // "don't block anything extra during this handler, let those other signals thru"
    sa.sa_flags = SA_RESTART;          // restart a system call if interupted by this signal
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while (1) {
        sin_size = sizeof their_addr;
        newfd = accept(listener, (struct sockaddr *)&their_addr, &sin_size);
        if (newfd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof s);
        printf("server: got connection from %s\n", s);
        
        if(!fork()){
            close(listener); 
            if(send(newfd, "test", 4, 0) == -1)
                perror("send");
            close(newfd);
            exit(0);
        }
        close(newfd);
    }
    return 0;
}
