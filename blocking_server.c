/**
 * @file server.c
 * Compile command : gcc server.c -o server -lpthread
 * @author Alexandre.Tortevois
 */

#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define SERVER_SOCKET_PORT 1998
#define BUFFER_MAX_SIZE 16*1024 // 16ko

static bool isRunning = true;
static pthread_t thread_id = -1;
static char reply_buffer[BUFFER_MAX_SIZE];

static void *server_thread(void __attribute__((unused)) *p) {
    int server_socket = -1;
    struct sockaddr_in server_addr;
    int client_socket = -1;
    int isEnable = 1;

    // Create an endpoint for communication
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Unable to open `server` socket");
        goto __error;
    }

    // Allow address to be reused instantly
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &isEnable, sizeof(isEnable));

    // Assign IP, PORT
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_SOCKET_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the newly created socket
    if (bind(server_socket, (const struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("Unable to bind `server` socket");
        goto __error;
    }

    printf("Thread `server` is running on port %d\n", SERVER_SOCKET_PORT);

    while (isRunning) {
        // Listen for connections on a socket
        if (listen(server_socket, 1) < 0) { // only one client
            perror("Error listen");
            goto __error;
        }

        // Accept a connection on a socket : wait a new client
        if ((client_socket = accept(server_socket, NULL, NULL)) < 0) { // TODO with select() ?
            perror("Unable to accept a client");
            goto __error;
        }

        while (isRunning) {
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

            // Build a response
            char text[100];
            time_t now = time(NULL);
            struct tm *t = localtime(&now);
            strftime(text, sizeof(text) - 1, "%d %m %Y %H:%M:%S", t); // %D %T

            sprintf(reply_buffer, "It's %s\n", text);

            msg_len = strlen(reply_buffer);
            if (write(client_socket, reply_buffer, msg_len) != msg_len) {
                printf("Error write\n");
                continue;
            } else {
                reply_buffer[msg_len - 1] = 0;
                printf("Message send to the client: %s\n", reply_buffer);
            }
        }

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