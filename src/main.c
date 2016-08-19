#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>

#define DEFAULT_PORT 70
#define DEFAULT_ROOT "/var/gopher"

/* called when a syscall fails */
void error(char *msg) {
    perror(msg);
    exit(1);
}

/* handle a connection */
void handle_conn(int sockfd) {
    /* buffer to read into */
    char buffer[256];

    /* read incoming data from socket */
    memset(buffer, 0, 256);
    if (read(sockfd, buffer, 255) != 0) error("ERROR reading from socket");

    /* print data to stdout */
    printf("INFO received: %s", buffer);

    /* write data to socket */
    if (write(sockfd, "data received", 13) != 0) error("ERROR writing to socket");

    /* close this socket */
    close(sockfd);
}


int main(int argc, char* argv[]) {
    /* socket file descriptors */
    int sockfd, newsockfd;

    /* port to listen on */
    int port = DEFAULT_PORT;

    /* root directory of server */
    char* rootdir = DEFAULT_ROOT;

    /* peer address length */
    socklen_t clilen;

    /* socket address structure */
    struct sockaddr_in {
        short           sin_family;
        unsigned short  sin_port;
        struct          in_addr sin_addr;
        char            sin_zero[8];
    } serv_addr, cli_addr;

    /* parse options */
    int o, n;
    while ((o = getopt(argc, argv, "d:p:")) != -1) {
        switch(o) {
            case 'd': {
                struct stat sb;
                if(stat(optarg, &sb) == 0 && S_ISDIR(sb.st_mode)) {
                    rootdir = optarg;
                }
                else {
                    fprintf(stderr, "WARNING %s does not seem to be a directory, using default of %s\n", optarg, DEFAULT_ROOT);
                }
                break;
            }
            case 'p':
                n = atoi(optarg);
                if (n > 0) port = n;
                else {
                    fprintf(stderr, "WARNING %d is not a valid port, using default of %d\n", n, DEFAULT_PORT);
                }
                break;
            case '?':
                if (optopt == 'p') fprintf(stderr, "ERROR option -%c requires an argument\n", optopt);
                else if (isprint(optopt)) fprintf(stderr, "ERROR unknown option '-%c'\n", optopt);
                else fprintf(stderr, "ERROR unknown option character '\\x%x'\n", optopt);
                exit(2);
            default:
                abort();
        }
    }

    for(n = optind; n < argc; n++) fprintf(stderr, "ERROR non option argument %s\n", argv[n]);

    /* move to specified server directory */
    chdir(rootdir);

    /* attempt to chroot into server directory */
    if(geteuid() == 0) {
        printf("INFO chrooting into %s\n", rootdir);
        if (chroot(rootdir) != 0) error("ERROR chrooting into server directory");
    }
    else {
        fprintf(stderr, "WARNING not root user, can't chroot into %s\n", rootdir);
    }

    /* open new socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR opening socket");

    /* set socket to be re-useable */
    int option = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    /* initialise the serv_addr struct */
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    /* bind the socket */
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR binding socket");
    }

    /* listen to the socket */
    listen(sockfd, 5);

    /* initialise clilen */
    clilen = sizeof(cli_addr);

    /* server loop */
    int pid;
    while(1) {
        /* accept incoming connections */
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) error("ERROR accepting connection");

        /* fork process to handle multiple connections */
        pid = fork();
        if (pid < 0) error("ERROR forking process");

        if (pid == 0) {
            /* we are the child */
            close(sockfd);
            handle_conn(newsockfd);
            exit(0);
        }
        else {
            /* we are the parent */
            close(newsockfd);
            wait(0);
        }
    }

    /* exit successfully - never reached */
    exit(0);
}
