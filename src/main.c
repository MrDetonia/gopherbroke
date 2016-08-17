#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define DEFAULT_PORT 70

/* called when a syscall fails */
void error(char *msg) {
    perror(msg);
    exit(1);
}


int main(int argc, char* argv[]) {
    /* socket file descriptors */
    int sockfd, newsockfd;

    /* port to listen on */
    int port = DEFAULT_PORT;

    /* peer address length */
    socklen_t clilen;

    /* buffer to read into */
    char buffer[256];

    /* socket address structure */
    struct sockaddr_in {
        short   sin_family;
        u_short sin_port;
        struct  in_addr sin_addr;
        char    sin_zero[8];
    } serv_addr, cli_addr;

    /* parse options */
    int o, n;
    while ((o = getopt(argc, argv, "p:")) != -1) {
        switch(o) {
            case 'p':
                n = atoi(optarg);
                if (n > 0) port = n;
                else {
                    fprintf(stderr, "%d is not a valid port, using default of %d\n", n, DEFAULT_PORT);
                }
                break;
            case '?':
                if (optopt == 'p') fprintf(stderr, "Option -%c requires an argument\n", optopt);
                else if (isprint(optopt)) fprintf(stderr, "Unknown option '-%c'\n", optopt);
                else fprintf(stderr, "Unkown option character '\\x%x'\n", optopt);
                exit(2);
            default:
                abort();
        }

        for(n = optind; n < argc; n++) fprintf(stderr, "Non option argument %s\n", argv[n]);
    }

    /* open new socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR opening socket");

    /* set socket to be re-useable */
    int option = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    /* initialise the serv_addr struct */
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    /* bind the socket */
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR binding socket");
    }

    /* listen to the socket */
    listen(sockfd, 5);

    /* accept incoming connections */
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0) error("ERROR accepting connection");

    /* read incoming data from socket */
    bzero(buffer, 256);
    n = read(newsockfd, buffer, 255);
    if (n < 0) error("ERROR reading from socket");

    /* print data to stdout */
    printf("Received: %s", buffer);

    /* write data to socket */
    n = write(newsockfd, "data received", 13);
    if (n < 0) error("ERROR writing to socket");

    /* close sockets */
    n = close(sockfd);
    if (n < 0) error("ERROR closing socket");
    n = close(newsockfd);
    if (n < 0) error("ERROR closing socket");

    /* exit successfully */
    exit(0);
}
