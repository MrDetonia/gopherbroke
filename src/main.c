#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <bsd/string.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/capability.h>
#include <netinet/in.h>

/* program defaults */
const int default_port = 70;
const char* default_root = "/var/gopher";
const char* error_string = "3Invalid input\tfake\t(NULL) 0";

/* called when a syscall fails */
void error(char* msg) {
    perror(msg);
    exit(1);
}

/* wrapper for write() with error handling */
void write2(int sockfd, const char* data, int len) {
    if (write(sockfd, data, len) < 0) error("ERROR writing to socket");
}

/* checks if a directory exists */
bool direxists(const char* dir) {
    struct stat sb;
    if(stat(dir, &sb) == 0 && S_ISDIR(sb.st_mode)) return true;
    return false;
}

/* checks if a file exists */
bool fileexists(const char* file) {
    struct stat sb;
    if(stat(file, &sb) == 0 && S_ISREG(sb.st_mode)) return true;
    return false;
}

/* write a file to socket */
void printfile(int sockfd, const char* path) {
    /* open file */
    FILE* f = fopen(path, "rb");

    if(f == 0) {
        /* error reading file */
        write2(sockfd, error_string, strlen(error_string));
    }
    else {
        /* line buffer */
        char* buf = calloc(256, sizeof(char));

        /* line length */
        size_t len = 0;

        /* bytes read */
        ssize_t read = 0;

        while((read = getline(&buf, &len, f)) != -1) {
            /* allocate space for adjusted line */
            char* line = calloc(read + 2, sizeof(char));

            /* replace \n with \r\n */
            strlcpy(line, buf, read);
            strlcat(line, "\r\n\0", read + 2);

            /* write line to socket */
            write2(sockfd, line, strlen(line));

            /* free line buffer */
            free(line);
        }

        /* free buffer */
        free(buf);

        /* close file */
        fclose(f);
    }
}

/* respond to query */
void respond(int sockfd, const char* in) {
    if (strncmp("\n", in, 1) == 0 || strncmp("\r\n", in, 2) == 0) {
        /* empty line received, print root listing */
        printfile(sockfd, "/.gopher");
    }
    else {
        /* clean input */
        char* buf = calloc(strlen(in), sizeof(char));
        strlcpy(buf, in, strlen(in));

        /* determine if given path is a file or directory */
        if(direxists(buf)) {
            printf("INFO serving directory %s\n", buf);

            /* path is directory - print .gopher */
            char* path = calloc(strlen(buf) + 9, sizeof(char));

            /* append '/.gopher' to path */
            strlcpy(path, buf, strlen(buf));
            strlcat(path, "/.gopher", strlen(buf) + 9);

            printfile(sockfd, path);

            free(path);
        }
        else if(fileexists(buf)) {
            printf("INFO serving file %s\n", buf);
            printfile(sockfd, buf);
        }
        else {
            /* invalid input, return error */
            fprintf(stderr, "WARNING invalid input\n");
            write2(sockfd, error_string, strlen(error_string));
        }

        free(buf);
    }

    /* we always finish with a period on a line */
    write2(sockfd, ".\r\n", 3);
}

/* handle a connection */
void handle_conn(int sockfd) {
    /* send/receive buffers */
    char in[256];

    /* read incoming data from socket */
    memset(in, 0, 256);
    if (read(sockfd, in, 255) < 0) error("ERROR reading from socket");

    /* print data to stdout */
    printf("INFO received: %s", in);

    /* decide what to send back */
    respond(sockfd, in);

    /* close this socket */
    close(sockfd);
}


int main(int argc, char* argv[]) {

    /* set required capabilities */
    cap_t caps = cap_init();
    cap_value_t cap_list[2];

    /* chroot */
    cap_list[0] = CAP_SYS_CHROOT;
    /* bind low ports */
    cap_list[1] = CAP_NET_BIND_SERVICE;

    if(cap_set_flag(caps, CAP_PERMITTED, 2, cap_list, CAP_SET) == -1) error("ERROR making capabilities permitted");
    if(cap_set_flag(caps, CAP_EFFECTIVE, 2, cap_list, CAP_SET) == -1) error("ERROR making capabilities effective");
    if(cap_set_proc(caps) == -1) error("ERROR setting capabilities");
    cap_free(caps);

    /* socket file descriptors */
    int sockfd, newsockfd;

    /* port to listen on */
    int port = default_port;

    /* root directory of server */
    char* rootdir = calloc(strlen(default_root) + 1, sizeof(char));
    strlcpy(rootdir, default_root, strlen(default_root) + 1);

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
            case 'd':
                if(direxists(optarg)) {
                    /* set directory to serve from */
                    rootdir = (char*) realloc(rootdir, strlen(optarg) + 1);
                    if (rootdir == NULL) error("ERROR in memory allocation");
                    strlcpy(rootdir, optarg, strlen(optarg) + 1);
                }
                else {
                    fprintf(stderr, "WARNING %s does not seem to be a directory, using default of %s\n", optarg, default_root);
                }
                break;
            case 'p':
                /* set port to listen on */
                n = atoi(optarg);
                if (n > 0) port = n;
                else {
                    fprintf(stderr, "WARNING %d is not a valid port, using default of %d\n", n, default_port);
                }
                break;
            case '?':
            default:
                /* error handling */
                if (optopt == 'p') fprintf(stderr, "ERROR option -%c requires an argument\n", optopt);
                else if (isprint(optopt)) fprintf(stderr, "ERROR unknown option '-%c'\n", optopt);
                else fprintf(stderr, "ERROR unknown option character '\\x%x'\n", optopt);
        }
    }
    for(n = optind; n < argc; n++) fprintf(stderr, "ERROR non option argument %s\n", argv[n]);

    /* chroot into server directory */
    printf("INFO chrooting into %s\n", rootdir);
    if (chdir(rootdir) != 0) error("ERROR moving into server directory");
    if (chroot(rootdir) != 0) error("ERROR chrooting into server directory");

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
            int status;
            waitpid(-1, &status, WNOHANG);
        }
    }

    /* exit successfully - never reached */
    exit(0);
}
