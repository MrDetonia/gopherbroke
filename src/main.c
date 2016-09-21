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
#include <sys/capability.h>
#include <netinet/in.h>

/* typical boolean definitions */
typedef int bool;
#define TRUE 1
#define FALSE 0

/* program defaults */
#define DEFAULT_PORT 70
#define DEFAULT_ROOT "/var/gopher"

/* string to write to socket on error */
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
    if(stat(dir, &sb) == 0 && S_ISDIR(sb.st_mode)) return TRUE;
    return FALSE;
}

/* checks if a file exists */
bool fileexists(const char* file) {
    struct stat sb;
    if(stat(file, &sb) == 0 && S_ISREG(sb.st_mode)) return TRUE;
    return FALSE;
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
        char* buf = NULL;

        /* line length */
        size_t len = 0;

        /* bytes read */
        ssize_t read;

        while((read = getline(&buf, &len, f)) != -1) {
            /* allocate space for adjusted line */
            char* line = malloc(read + 2);
            memset(line, 0, read + 2);

            /* replace \n with \r\n */
            strncpy(line, buf, read - 1);
            strcat(line, "\r\n");

            /* write line to socket */
            write2(sockfd, line, strlen(line));

            /* free up line buffer */
            free(line);
        }

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
        char* buf = malloc(strlen(in) - 1);
        strncpy(buf, in, strlen(in) - 1);

        /* determine if given path is a file or directory */
        if(direxists(buf)) {
            printf("INFO serving directory %s\n", buf);

            /* path is directory - print .gopher */
            char* path = malloc(strlen(buf) + 8);

            /* append '/.gopher' to path */
            strcpy(path, buf);
            strcat(path, "/.gopher");

            /* print file */
            printfile(sockfd, path);

            /* free memory */
            free(path);
        }
        else if(fileexists(buf)) {
            printf("INFO serving file %s\n", buf);

            /* print contents of file */
            printfile(sockfd, buf);
        }
        else {
            /* invalid input, return error */
            write2(sockfd, error_string, strlen(error_string));
        }

        /* free memory */
        free(buf);
    }

    /* we always finish with a period on a line */
    write2(sockfd, "\r\n.\r\n", 5);
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
    /* socket file descriptors */
    int sockfd, newsockfd;

    /* port to listen on */
    int port = DEFAULT_PORT;

    /* root directory of server */
    char* rootdir = malloc(strlen(DEFAULT_ROOT));
    strcpy(rootdir, DEFAULT_ROOT);

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
                    rootdir = (char*) realloc(rootdir, strlen(optarg));
                    if (rootdir == NULL) error("ERROR in memory allocation");
                    strcpy(rootdir, optarg);
                }
                else {
                    fprintf(stderr, "WARNING %s does not seem to be a directory, using default of %s\n", optarg, DEFAULT_ROOT);
                }
                break;
            case 'p':
                /* set port to listen on */
                n = atoi(optarg);
                if (n > 0) port = n;
                else {
                    fprintf(stderr, "WARNING %d is not a valid port, using default of %d\n", n, DEFAULT_PORT);
                }
                break;
            case '?':
                /* error handling */
                if (optopt == 'p') fprintf(stderr, "ERROR option -%c requires an argument\n", optopt);
                else if (isprint(optopt)) fprintf(stderr, "ERROR unknown option '-%c'\n", optopt);
                else fprintf(stderr, "ERROR unknown option character '\\x%x'\n", optopt);
                exit(2);
            default:
                /* this shouldn't happen */
                abort();
        }
    }

    for(n = optind; n < argc; n++) fprintf(stderr, "ERROR non option argument %s\n", argv[n]);

    /* gain required capabilities */
    cap_t caps = cap_init();
    cap_value_t cap_list[3];

    /* chroot */
    cap_list[0] = CAP_SYS_CHROOT;
    /* bind low ports */
    cap_list[1] = CAP_NET_BIND_SERVICE;

    if(cap_set_flag(caps, CAP_PERMITTED, 2, cap_list, CAP_SET) == -1) error("ERROR making capabilities permitted");
    if(cap_set_flag(caps, CAP_EFFECTIVE, 2, cap_list, CAP_SET) == -1) error("ERROR making capabilities effective");
    if(cap_set_proc(caps) == -1) error("ERROR setting capabilities");
    cap_free(caps);

    /* move to specified server directory */
    chdir(rootdir);

    /* chroot into server directory */
    printf("INFO chrooting into %s\n", rootdir);
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
