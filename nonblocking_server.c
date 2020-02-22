/**
 * @file server.c
 * Compile command : gcc nonblocking_server.c -o server -lpthread
 * @author Alexandre.Tortevois
 */

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define MIN(a, b) (((a)<(b))?(a):(b))
#define MAX(a, b) (((a)>(b))?(a):(b))

#define SERVER_SOCKET_PORT 1998
#define BUFFER_MAX_SIZE 16*1024 // 16ko

static bool isRunning = true;
static pthread_t thread_id = -1;
static char reply_buffer[BUFFER_MAX_SIZE];

static int server_create(int *server_socket) {
    int rc = 0, on = 1;
    struct sockaddr_in server_addr;

    // Create an endpoint for communication
    if ((*server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Unable to open `server` socket");
        rc = -1;
        goto __error;
    }

    // Allow address to be reused instantly
    setsockopt(*server_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    // Assign IP, PORT
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_SOCKET_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the newly created socket
    if (bind(*server_socket, (const struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("Unable to bind `server` socket");
        rc = -1;
        goto __error;
    }

    printf("Thread `server` is running on port %d\n", SERVER_SOCKET_PORT);

    __error:
    return rc;
}

static int write_to_socket(int socket) {
    int rc = 0;
    // Build a response
    char time_str[10];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(time_str, sizeof(time_str) - 1, "%H:%M:%S", t); // %d %m %Y -- %D %T

    // strcpy(reply_buffer, "{}");
    sprintf(reply_buffer, "It's %s\n", time_str);

    int msg_len = strlen(reply_buffer);

    if (write(socket, reply_buffer, strlen(reply_buffer)) != msg_len) {
        printf("Error write #%d: %s\n", errno, strerror(errno));
        if (errno == EPIPE) {
            printf("Catch error EPIPE\n");
            rc = -1;
            goto __error;
        }
        // TODO
        //exit(EXIT_FAILURE);
    }

    printf("Message send to the client: %s\n", reply_buffer);
    __error:
    return rc;
}

static void *server_thread(void __attribute__((unused)) *p) {
    int server_socket = -1, client_socket = -1;
    fd_set set;
    int max_fd_set;
    int activity;
    struct timeval timeout;

    // Disable SIGPIPE
    signal(SIGPIPE, SIG_IGN);

    if (server_create(&server_socket) < 0) {
        goto __error;
    }

    while (isRunning) {

        printf("Waiting for a new client\n");

        // Listen for connections on a socket
        if (listen(server_socket, 1) < 0) { // only one client
            perror("Error listen");
            goto __error;
        }

        // Accept a connection on a socket : wait a new client
        if ((client_socket = accept(server_socket, NULL, NULL)) < 0) {
            perror("Unable to accept a client");
            goto __error;
        }

        printf("New client incoming connection: %d\n", client_socket);

        FD_ZERO(&set);
        FD_SET(server_socket, &set);
        FD_SET(client_socket, &set);
        max_fd_set = MAX(server_socket, client_socket) + 1;

        while (isRunning) {
            // Initialize timeout
            timeout.tv_sec = 20; // 10 sec
            timeout.tv_usec = 0;

            activity = select(max_fd_set, &set, NULL, NULL, &timeout);

            // Error
            if (activity < 0) {
                perror("select() failed");
                exit(EXIT_FAILURE);
            }

            // Timeout
            if (activity == 0) {
                // If client is disconnected => break; else continue
                if (write_to_socket(client_socket) < 0) break;

                continue;
            }

            // There is an event
            char buffer[BUFFER_MAX_SIZE];

            // Get the client message
            ssize_t msg_len = read(client_socket, buffer, sizeof(buffer) - 1);

            if (msg_len < 0) {
                perror("Unable to read a message from the client socket");
                goto __error;
            }

            // Client is disconnected
            if (msg_len == 0) break; // TODO with SIGPIPE ?

            // Check the minimal length
            if (msg_len < 2) {
                printf("Client socket read error: incomplete chain\n");
                continue;
            }

            // To be sure to have a finished chain
            buffer[msg_len] = 0;
            printf("Message received from the client: %s\n", buffer);

            // If client is disconnected => break;
            if (write_to_socket(client_socket) < 0) break;
        }

        // Client is disconnected
        printf("Client is now disconnected\n");
        close(client_socket);
    }

    __error:
    if (server_socket != -1) {
        close(server_socket);
    }
}

static int server_start(void) {
    int rc = 0;
    if ((rc = pthread_create(&thread_id, NULL, server_thread, NULL)) != 0) {
        rc = -1;
        perror("Unable to create the Thread `server`: %s");
        goto __error;
    }

    __error:
    return rc;
}

static void server_stop(void) {
    isRunning = false;
    if (thread_id != -1) {
        // pthread_join(thread_id, NULL);
        printf("Thread `server` stop");
    }
}

static void sig_handler(int sig) {
    switch (sig) {
        case SIGSEGV:
            printf("Catch SIGSEGV\n");
            break;

        case SIGTERM:
            printf("Catch SIGTERM\n");
            break;

        case SIGFPE : // float error
            printf("Catch SIGFPE\n");
            break;

        default:
            break;
    }
    exit(EXIT_FAILURE);
}

int main() {
    // Callback before exit
    atexit(server_stop);

    // Define the callback for signals interception
    struct sigaction sig_action;
    memset(&sig_action, 0, sizeof(sig_action));
    sig_action.sa_handler = sig_handler;

    if (sigaction(SIGSEGV, &sig_action, NULL) < 0) {
        perror("Error: could not define sigaction SIGSEGV");
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGTERM, &sig_action, NULL) < 0) {
        perror("Error: could not define sigaction SIGTERM");
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGFPE, &sig_action, NULL) < 0) {
        perror("Error: could not define sigaction SIGFPE");
        exit(EXIT_FAILURE);
    }

    if (server_start() < 0) {
        exit(EXIT_FAILURE);
    }

    //    const char *command = "node /root/websocket_server/wss.js &";
    //    if (system(command) < 0) {
    //        perror("Unable to launch the command");
    //        exit(EXIT_FAILURE);
    //    }

    while (1) {
        usleep(500000);
    }

    return EXIT_FAILURE;
}