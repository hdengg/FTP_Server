#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <ctype.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "dir.h"
#include "usage.h"

struct timeval timeout;
fd_set fds;
int sockfd;
int datasockfd;
int new_datasock;
int maxfd;
struct sockaddr_storage their_addr;
struct sockaddr_storage connect_addr;
struct addrinfo * servinfo;
struct addrinfo hints;
struct addrinfo * p;
socklen_t addr_size;
socklen_t data_size;
int new_fd;
int logged_in;
int is_connection_open;
char curr_dir[1024] = {'\0'};
char intial_dir[1024];

// send data
int send_data(char * data) {
    return send(new_fd, data, strlen(data), 0);
}

int return_index(char * str) {
    return strcspn(str, "\r\n");
}

// error response codes
int sendMessage(int code) {
    char * message;
    switch (code) {
        case 125:
            message = "125 Data connection already open; transfer starting.\r\n";
            break;
        case 200:
            message = "200 Command okay.\r\n";
            break;
        case 220:
            message = "220 Service ready for new user.\r\n";
            break;
        case 221:
            message = "221 Service closing control connection.\r\n";
            break;
        case 226:
            message = "226 Closing data connection.\r\n";
            break;
        case 230:
            message = "230 User logged in, proceed.\r\n";
            break;
        case 250:
            message = "250 Requested file action okay, completed.\r\n";
            break;
        case 425:
            message = "425 Can't open data connection.\r\n";
            break;
        case 426:
            message = "426 Connection closed; transfer aborted.\r\n";
            break;
        case 421:
            message = "421 Timeout. Closing data connection.\r\n";
            break;
        case 500:
            message = "500 Syntax error, command unrecognized and the requested action did not take place.\r\n";
            break;
        case 501:
            message = "501 Syntax error in parameters or arguments.\r\n";
            break;
        case 503:
            message = "503 Bad sequence of commands.\r\n";
            break;
        case 504:
            message = "504 Command not implemented for that parameter.\r\n";
            break;
        case 530:
            message = "530 Not logged in.\r\n";
            break;
        case 550:
            message = "550 Requested action not taken. File or directory unavailable. (e.g. file not found, no access).\r\n";
            break;
        default:
            message = "500 Syntax error, command unrecognized and the requested action did not take place.\r\n";
            break;
    }
    return send_data(message);
}

// user
void user_cmd(char * args) {
    if (logged_in == 1) {
        sendMessage(503);
    } else {
        char * username = strsep(&args, " ");
        // is username null
        if (username != NULL) {
            // check for correct amount of arguments 
            int i = 1;
            while (strsep(&args, " ") != NULL) {
                i ++;
            }
            // if there are correct number of arguments
            if (i == 1) {
                username[return_index(username)] = '\0';
                if (strcmp(username, "cs317") == 0) {
                    logged_in = 1;
                    sendMessage(230);
                } else {
                    // not logged in
                    sendMessage(530);
                }
            } else {
                // wrong args
                sendMessage(501);
            }
        }
        else {
            // wrong args
            sendMessage(501);
        }
    }
}

// type
void type_cmd(char * args) {
    if (logged_in == 0) {
        sendMessage(530);
    } else {
        char *type = strsep(&args, " ");
        if (type != NULL) {
            // check for one argument
            int i = 1;
            while (strsep(&args, " ") != NULL) {
                i++;
            }
            if (i == 1) {
                type[return_index(type)] = '\0';
                if (strcmp(type, "I") == 0 || strcmp(type, "A") == 0) {
                    // supported command
                    sendMessage(200);
                } else {
                    // unsupported parameter
                    sendMessage(504);
                }
            } else {
                // incorrect args
                sendMessage(501);
            }
        } else {
            // incorrect args
            sendMessage(501);
        }
    }
}

// mode
void mode_cmd(char * args) {
    if (logged_in == 0) {
        sendMessage(530);
    } else {
        char * mode = strsep(&args, " ");
        // if not null, proceed
        if (mode != NULL) {
            // can only have one argument
            int i = 1;
            while (strsep(&args, " ") != NULL) {
                i++;
            }
            if (i == 1) {
                mode[return_index(mode)] = '\0';
                if (strcmp(mode, "S") == 0) {
                    sendMessage(200);
                } else {
                    // unsupported parameter
                    sendMessage(504);
                }
            } else {
                // incorrect args
                sendMessage(501);
            }
        } else {
            // incorrect args
            sendMessage(501);
        }
    }
}

// stru
void stru_cmd(char * args) {
    if (logged_in == 0) {
        sendMessage(530);
    } else {
        char *stru = strsep(&args, " ");
        if (stru != NULL) {
            // only accept one argument
            int i = 1;
            while (strsep(&args, " ") != NULL) {
                i++;
            }
            if (i == 1) {
                stru[return_index(stru)] = '\0';
                if (strcmp(stru, "F") == 0) {
                    // accept file struct
                    sendMessage(200);
                } else {
                    // incorrect params
                    sendMessage(504);
                }
            } else {
                // incorrect args
                sendMessage(501);
            }
        } else {
            // incorrect args
            sendMessage(501);
        }
    }
}

// cwd
void cwd_cmd(char * args) {
    if (logged_in == 0) {
        sendMessage(530);
    } else {
        char *cwd = strsep(&args, " ");
        if (cwd != NULL) {
            // one arg
            int i = 1;
            while (strsep(&args, " ") != NULL) {
                i++;
            }
            if (i == 1) {
                cwd[return_index(cwd)] = '\0';
                // cannot accept ./ or ../
                if ((strncmp(cwd, "./", 2) == 0) || (strncmp(cwd, "../", 3) == 0)) {
                    sendMessage(550);
                } else if (strstr(cwd, "../")) {
                    sendMessage(550);
                } else {
                    if (chdir(cwd) == 0) {
                        getcwd(curr_dir, 1024);
                        // file action is okay
                        sendMessage(250);
                        printf("Current directory has changed to %s\n", curr_dir);
                    } else {
                        // file not found
                        sendMessage(550);
                    }
                }
            } else {
                // incorrect args
                sendMessage(501);
            }
        } else {
            // incorrect args
            sendMessage(501);
        }
    }
}


// cdup
void cdup_cmd(char * args) {
    if (logged_in == 0) {
        sendMessage(530);
    } else {
        char *cdup = strsep(&args, " ");
        if (cdup == NULL) {
            // cannot be in the root directory
            if (strcmp(intial_dir, curr_dir) == 0) {
                sendMessage(550);
            } else {
                chdir("..");
                getcwd(curr_dir, 1024);
                printf("Current directory has changed to %s\n", curr_dir);
                sendMessage(200);
            }
        } else {
            sendMessage(501);
        }
    }
}

// opens a socket
int open_data_sock() {
    // Check for existing data connection
    if (!is_connection_open) {
        return -1;
    }

    data_size = sizeof connect_addr;

    // set timeout to 20 seconds
    timeout.tv_sec = 20;
    timeout.tv_usec = 0;

    // timeout to establish connection
    if (select(maxfd + 1, &fds, NULL, NULL, &timeout) <= 0) {
        sendMessage(421);
        close(new_datasock);
        close(datasockfd);
        is_connection_open = 0;
        FD_ZERO(&fds);
        return -20;
    }

    // accept socket connection
    new_datasock = accept(datasockfd, (struct sockaddr *) &connect_addr, &data_size);
    // set timer back to 0
    FD_ZERO(&fds);

    return 0;
}

// retr
void retr_cmd(char * args) {
    if (logged_in == 0) {
        sendMessage(530);
    } else {
        char * retr = strsep(&args, " ");
        if (retr != NULL) {
            // one arg
            int i = 1;
            while (strsep(&args, " ") != NULL) {
                i ++;
            }
            if (i == 1) {
                // need pasv to be open
                if (is_connection_open) {
                    // get the file name
                    retr[return_index(retr)] = '\0';
                    FILE * file_name;
                    // open the file
                    if ((file_name = fopen(retr, "r")) == NULL) {
                        close(datasockfd);
                        is_connection_open = 0;
                        sendMessage(550);
                        return;
                    }
                    // determine file size
                    fseek(file_name, 0, SEEK_END);
                    long filesize = ftell(file_name);
                    rewind(file_name);
                    // write into buffer
                    char file_buf[filesize];
                    bzero(file_buf, filesize);
                    fread(file_buf, filesize, 1, file_name);
                    // open a data socket connection 
                    int connect = open_data_sock();
                    if (connect < 0) {
                        // wait till timeout
                        if (connect == -20) {
                            is_connection_open = 0;
                            fclose(file_name);
                            return;
                        }
                        sendMessage(425);
                        close(datasockfd);
                        is_connection_open = 0;
                        fclose(file_name);
                        return;
                    }
                    sendMessage(125);
                    // begin sending the file
                    if (send(new_datasock, file_buf, filesize, 0) < 0) {
                        // close socket if the file could not be sent
                        close(new_datasock);
                        close(datasockfd);
                        is_connection_open = 0;
                        fclose(file_name);
                        return;
                    }
                    sendMessage(226);
                    // close sockets after success
                    close(new_datasock);
                    close(datasockfd);
                    is_connection_open = 0;
                    fclose(file_name);
                    printf("File %s was successfully sent.\n", retr);
                } else {
                    // closing connection
                    sendMessage(425);
                    return;
                }
            } else {
                // incorrect args
                sendMessage(501);
            }
        } else {
            sendMessage(501);
        }
    }
}

// nlst
void nlst_cmd(char * args) {
    if (logged_in == 0) {
        sendMessage(530);
    } else {
        char * nlst  = strsep(&args, " ");
        // Check for no args
        if (nlst == NULL) {
            // Check if PASV was called
            if (is_connection_open == 0) {
                sendMessage(425);
                return;
            }
            int connect = open_data_sock();
            if (connect < 0) {
                if (connect == -20) {
                    is_connection_open = 0;
                    return;
                }
                // could not establish the connection because timeout
                sendMessage(425);
                close(datasockfd);
                is_connection_open = 0;
                return;
            }
            sendMessage(125);
            if (listFiles(new_datasock, (char *)&curr_dir) < 0) {
                // could not list all the files
                sendMessage(550);
                // close all data sockets
                close(new_datasock);
                close(datasockfd);
                is_connection_open = 0;
                return;
            }
            // success in listing the files
            sendMessage(226);
            is_connection_open = 0;
            close(new_datasock);
            close(datasockfd);
        } else {
            sendMessage(501);
        }
    }
}

// pasv
void pasv_command(char * args) {
    if (logged_in == 0) {
        sendMessage(530);
    } else {
        char * pasv  = strsep(&args, " ");
        // Check for no args
        if (pasv == NULL) {
            // close any existing connections
            if (is_connection_open) {
                close(datasockfd);
            }
            
            if ((datasockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
                sendMessage(425);
                return;
            }

            struct sockaddr_in connect_addr;
            memset(&connect_addr, 0, sizeof(connect_addr));
            connect_addr.sin_family = AF_INET;
            connect_addr.sin_addr.s_addr = INADDR_ANY;
            connect_addr.sin_port = 0;

            if (bind(datasockfd, (struct sockaddr *)&connect_addr, sizeof(connect_addr)) < 0) {
                sendMessage(425);
                return;
            }

            if (listen(datasockfd, 10) < 0) {
                sendMessage(425);
                return;
            }
            
            struct sockaddr_in socket_address;
            socklen_t length = sizeof(socket_address);

            // Get socket from new_fd, the original connection
            getsockname(new_fd, (struct sockaddr *)&socket_address, &length);
            // get ip
            unsigned int ip = socket_address.sin_addr.s_addr;

            // Get socket from new data socket
            getsockname(datasockfd, (struct sockaddr *)&socket_address, &length);
            // get the port that was assigned to it
            unsigned int port = ntohs(socket_address.sin_port);

            // randomly split the ports
            unsigned int port1 = port / 256;
            unsigned int port2 = port % 256;

            // separate the ip addresses
            unsigned int ip1 = ip & 0xff;
            unsigned int ip2 = (ip >> 8) & 0xff;
            unsigned int ip3 = (ip >> 16) & 0xff;
            unsigned int ip4 = (ip >> 24) & 0xff;

            char buf[2048];
            sprintf(buf, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n", ip1, ip2, ip3, ip4, port1, port2);
            send_data((char *)&buf);
            is_connection_open = 1;
            // keep track of timeouts
            FD_SET(datasockfd, &fds);
            maxfd = datasockfd;
        } else {
            sendMessage(501);
        }
    }
}

//Listening to socket
int listen_to_server() {

    // check if there is server
    if (p == NULL) {
        fprintf(stderr, "Failed to bind to socket.\n");
        return 1;
    }

    int result = listen(sockfd, 10);
    if (result != 0) {
        return 1;
    }

    logged_in = 0;
    is_connection_open = 0;
    getcwd(curr_dir, 1024);
    strcpy(intial_dir, curr_dir);

    char buf[2048];

    while (1) {

        FD_ZERO(&fds);
        logged_in = 0;
        is_connection_open = 0;
        chdir(intial_dir);
        strcpy(curr_dir, intial_dir);

        printf("Server is listening for connections.\n");

        memset(&buf, 0, sizeof(buf));
        addr_size = sizeof their_addr;

        // accept socket connection here
        new_fd = accept(sockfd, (struct sockaddr *) &their_addr, &addr_size);

        if (new_fd == -1) {
            continue;
        }

        sendMessage(220);
        printf("Server: New connection established.\n");

        while (1) {

            memset(&buf, 0, sizeof(buf));
            result = recv(new_fd, &buf, sizeof(buf), 0);
            buf[result] = '\0';

            if (result <= -1) {
                printf("Server: Connection ended by the user.\n");
                break;
            }

            if (buf[0] != 0) {
                char * args = strdup(buf);
                char * ptr = args;
                char * command = strsep(&args, " ");

                int i;
                for (i =0; i < strlen(command); i++ ) {
                    command[i] = toupper(command[i]);
                }

                // user
                if (strncmp("USER", command, 4) == 0) {
                    user_cmd(args);
                    continue;
                }
                    // quit
                else if (strncmp("QUIT", command, 4) == 0) {
                    sendMessage(221);
                    logged_in = 0;
                    close(new_fd);
                    if (is_connection_open) {
                        close(new_datasock);
                        close(datasockfd);
                    }
                    break;
                }
                    // cwd
                else if (strncmp("CWD", command, 3) == 0) {
                    cwd_cmd(args);
                    continue;
                }
                    // cdup
                else if (strncmp("CDUP", command, 4) == 0) {
                    cdup_cmd(args);
                    continue;
                }
                    // type
                else if (strncmp("TYPE", command, 4) == 0) {
                    type_cmd(args);
                    continue;
                }
                    // mode
                else if (strncmp("MODE", command, 4) == 0) {
                    mode_cmd(args);
                    continue;
                }
                    // stru
                else if (strncmp("STRU", command, 4) == 0) {
                    stru_cmd(args);
                    continue;
                }

                    // retr
                else if (strncmp("RETR", command, 4) == 0) {
                    retr_cmd(args);
                    continue;
                }

                    // nlst
                else if (strncmp("NLST", command, 4) == 0) {
                    nlst_cmd(args);
                    continue;
                }
                    // pasv
                else if (strncmp("PASV", command, 4) == 0) {
                    pasv_command(args);
                    continue;
                }

                else {
                    // default is 500
                    sendMessage(500);
                    continue;
                }
            }

            close(new_fd);
            memset(&buf, 0, sizeof(buf));
        }
        // close any residual sockets
        close(new_datasock);
        close(datasockfd);
        close(new_fd);
        is_connection_open = 0;
    }
}

int main(int argc, char **argv) {

    // This is some sample code feel free to delete it

    // Check the command line arguments
    if (argc != 2) {
        usage(argv[0]);
        return -1;
    }

    // timeout to 20s
    timeout.tv_sec = 20;
    timeout.tv_usec = 0;

    // clear countdown
    FD_ZERO(&fds);

    // Set up a "helper" struct hints to help with socket binding
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int yes = 1;

    // Fill out our servinfo struct using our helper struct
    int res;
    res = getaddrinfo(NULL, argv[1], &hints, &servinfo);
    if (res != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(res));
        return 1;
    }

    // Create a socket using our servinfo struct
    // Loop through results and bind to first port we can
    for (p = servinfo; p != NULL; p = p->ai_next) {

        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        // Set up socket to allow reuse of port- yes is buf where result stored, sizeof(int)) is sizeof yes
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes ,sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        // Bind our socket to the specified port
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // free struct

    // Begin listening on the socket
    res = listen_to_server();

    // free struct
    freeaddrinfo(p);

    return res;

    // This is how to call the function in dir.c to get a listing of a directory.
    // It requires a file descriptor, so in your code you would pass in the file descriptor
    // returned for the ftp server's data connection

    printf("Printed %d directory entries\n", listFiles(1, "."));
    return 0;
}