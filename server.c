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

#include <regex.h>
#include <fcntl.h>
#include <sys/stat.h>

#define PORT "3000"
#define BUFFER_SIZE 4096 

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

void make_http_response(const char *file_name, const char *file_ext, char **response, size_t *response_len){

    // if file DNE, return 404
    int file_fd = open(file_name, O_RDONLY);
    if (file_fd == -1){
        const char *not_found = "HTTP/1.0 404 Not Found\n";
        printf("HTTP/1.0 404 Not Found\r\n"
               "Content-Type: text/plain\r\n"
               "\r\n"
               "404 Not Found");
        *response_len = strlen(not_found);
        strcpy(*response, not_found); 
        return;
    }

    // build the HTTP header
    char *mime_type = "text/html";
    char *header = (char *)malloc(BUFFER_SIZE * sizeof(char));
    snprintf(header, BUFFER_SIZE,
           "HTTP/1.0 200 OK\r\n"
           "Content-Type: %s\r\n"
           "\r\n",
           mime_type);

    // get file size for content length
    struct stat file_stat;
    fstat(file_fd, &file_stat);
    off_t file_size = file_stat.st_size;

    // copy header + file size to response buffer
    *response_len = strlen(header) + file_size;
    *response = malloc(*response_len + 1);

    memcpy(*response, header, strlen(header));

    // copy file to response buffer
    ssize_t bytes_read, offset = strlen(header);
    while ((bytes_read = read(file_fd, *response + offset, BUFFER_SIZE)) > 0){
        offset+= bytes_read;
    }

    free(header);
    close(file_fd);
}

void handle_client(int clientfd){
    char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));
    ssize_t bytes_received;
    
    if((bytes_received = recv(clientfd, buffer, BUFFER_SIZE, 0)) == -1){ //store into buffer
        perror("recv");
        return;
    }

    regex_t regex;
    regcomp(&regex, "^GET /([^ ]*) HTTP/1", REG_EXTENDED);
    regmatch_t matches[2];

    if (regexec(&regex, buffer, 2, matches, 0) == 0){
        buffer[matches[1].rm_eo] = '\0';
        const char *url_encoded_file_name = buffer + matches[1].rm_so;
        char *file_name = url_encoded_file_name;

        printf("filename: %s\n", file_name);

        char file_extension[] = "html";

        char *response = NULL; // initialize as NULL because ill malloc in make_http_response
        size_t response_len;
        make_http_response(file_name, file_extension, &response, &response_len);

        send(clientfd, response, response_len, 0);

        free(response);
    }
    regfree(&regex);
    close(clientfd);
    free(buffer);
}

int main(int argc, char* argv[]){
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    int clientfd;
    char s[INET6_ADDRSTRLEN];

    char hostname[20];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        printf("Hostname: %s\n", hostname);
    }

    int serverfd = getListener();
    if (serverfd == -1) {
        printf("listener descriptor failed: %d\n", serverfd);
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
        clientfd = accept(serverfd, (struct sockaddr *)&their_addr, &sin_size);
        if (clientfd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof s);
        printf("server: got connection from %s\n", s);
        
        if(!fork()){
            close(serverfd); 
            handle_client(clientfd);
            close(clientfd);
            exit(0);
        }
        close(clientfd);
    }
    return 0;
}
