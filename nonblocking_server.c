/**
 * @file nonblocking_server.c
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
#include <sys/ioctl.h>
#include <sys/socket.h>

#define MIN(a, b) (((a)<(b))?(a):(b))
#define MAX(a, b) (((a)>(b))?(a):(b))

#define SERVER_SOCKET_PORT 1998
#define BUFFER_MAX_SIZE 16*1024 // 16ko

volatile sig_atomic_t isRunning = true;
static pthread_t thread_id = -1;
static int server_socket = -1, client_socket = -1;
static char reply_buffer[BUFFER_MAX_SIZE];

static int server_create(void) {
    int rc = 0, on = 1;
    struct sockaddr_in server_addr;

    // Create an endpoint for communication
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Unable to open the server socket: %s\n", strerror(errno));
        rc = -1;
        goto __error;
    }

    // Allow address to be reused instantly
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    // Allow to keep alive the connection
    setsockopt(server_socket, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));

    // Set socket to be nonblocking
    /*
    if (ioctl(*server_socket, FIONBIO, (char *) &on) < 0) {
        printf("Unable to set the server socket to be nonblocking: %s\n", strerror(errno));
        rc = -1;
        goto __error;
    }
    */

    // Assign IP, PORT
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_SOCKET_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the newly created socket
    if (bind(server_socket, (const struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        printf("Unable to bind the server socket: %s\n", strerror(errno));
        rc = -1;
        goto __error;
    }

    printf("Server thread is running on port %d\n", SERVER_SOCKET_PORT);

    __error:
    return rc;
}

static int write_to_client(void) {
    int rc = 0;

    // Build a response
    char time_str[10];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(time_str, sizeof(time_str) - 1, "%H:%M:%S", t);
    sprintf(reply_buffer, "{\"payload\":\"%s\"}\n", time_str);

    unsigned int msg_len = strlen(reply_buffer);
    if (write(client_socket, reply_buffer, msg_len) != msg_len) {
        if (errno == EPIPE) {
            printf("Catch error EPIPE\n");
            rc = 1;
            goto __error;
        } else {
            printf("Error write #%d: %s\n", errno, strerror(errno));
            rc = -1;
            goto __error;
        }
    }

    printf("Message send to the client: %s", reply_buffer);

    __error:
    return rc;
}

static void *server_thread(void __attribute__((unused)) *p) {
    fd_set set;
    int max_fd_set;
    int rc;
    struct timeval timeout;

    // Allow the thread to be canceled
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    // Disable SIGPIPE
    signal(SIGPIPE, SIG_IGN);

    if (server_create() < 0) {
        goto __error;
    }

    // Listen for connections on a socket
    if (listen(server_socket, 1) < 0) { // only one client
        printf("Unable to listen the server socket: %s\n", strerror(errno));
        goto __error;
    }

    while (isRunning) {
        printf("Waiting for a new client\n");
        // Accept a connection on a socket
        if ((client_socket = accept(server_socket, NULL, NULL)) < 0) {
            printf("Unable to accept a client: %s\n", strerror(errno));
            goto __error;
        }
        printf("New client incoming connection: %d\n", client_socket);

        while (isRunning) {
            // Initialize fd_set
            FD_ZERO(&set);
            FD_SET(server_socket, &set);
            FD_SET(client_socket, &set);
            max_fd_set = MAX(server_socket, client_socket) + 1;

            // Initialize timeout
            timeout.tv_sec = 30; // 30 sec
            timeout.tv_usec = 0;

            rc = select(max_fd_set, &set, NULL, NULL, &timeout);
            // printf("activity is %d\n", activity);

            // Error
            if (rc < 0) {
                printf("Select failed: %s\n", strerror(errno));
                goto __error;
            }

            // Timeout
            if (rc == 0) {
                // Send a response to the client
                rc = write_to_client();
                // Error
                if (rc < 0) goto __error;
                // Client is disconnected => break;
                if (rc > 0) break;

                // Go back to the `select`
                continue;
            }

            // There is an event
            char buffer[BUFFER_MAX_SIZE];

            // Get the client message
            ssize_t msg_len = read(client_socket, buffer, sizeof(buffer) - 1);

            if (msg_len < 0) {
                printf("Unable to read a message from the client socket: %s\n", strerror(errno));
                goto __error;
            }

            // Client is disconnected
            if (msg_len == 0) break;

            // Check the minimal length
            if (msg_len < 2) {
                printf("Client socket read error: incomplete chain\n");
                continue;
            }

            // To be sure to have a finished chain
            buffer[msg_len] = 0;
            printf("Message received from the client: %s\n", buffer);

            // Send a response to the client
            rc = write_to_client();
            // Error
            if (rc < 0) goto __error;
            // Client is disconnected => break;
            if (rc > 0) break;
        }

        // Client is disconnected
        printf("Client is now disconnected\n");
        if (close(client_socket) < 0) {
            printf("Unable to close the client socket: %s\n", strerror(errno));
            goto __error;
        }
        client_socket = -1;
    }

    __error:
    if (client_socket != -1) {
        if (close(client_socket) < 0) {
            printf("Unable to close the client socket: %s\n", strerror(errno));
        }
        client_socket = -1;
    }

    if (server_socket != -1) {
        if (close(server_socket) < 0) {
            printf("Unable to close the server socket: %s\n", strerror(errno));
        }
        server_socket = -1;
    }

    printf("End of the server thread\n");
    kill(getppid(), SIGTERM);
    // pthread_exit(0);
}

static int server_start(void) {
    int rc = 0;
    if ((rc = pthread_create(&thread_id, NULL, server_thread, NULL)) != 0) {
        rc = -1;
        printf("Unable to create the server thread: %s\n", strerror(errno));
        goto __error;
    }

    __error:
    return rc;
}

static void server_stop(void) {
    printf("server_stop()\n");
    isRunning = false;
    if (thread_id != -1) {
        pthread_cancel(thread_id);
        if (client_socket != -1) {
            if (shutdown(client_socket, SHUT_RDWR) < 0) {
                printf("Unable to close the client socket: %s\n", strerror(errno));
            }
        }
        if (server_socket != -1) {
            if (shutdown(server_socket, SHUT_RDWR) < 0) {
                printf("Unable to close the server socket: %s\n", strerror(errno));
            }

        }
        pthread_join(thread_id, NULL);
    }
    printf("The server thread is stopped\n");
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
            printf("Catch SIG %s\n", strsignal(sig));
            break;
    }
    exit(EXIT_FAILURE);
}

static void sig_intercept(void) {
    struct sigaction sig_action;
    memset(&sig_action, 0, sizeof(sig_action));
    sig_action.sa_handler = sig_handler;

    if (sigaction(SIGSEGV, &sig_action, NULL) < 0) {
        printf("Error: could not define sigaction SIGSEGV: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGTERM, &sig_action, NULL) < 0) {
        printf("Error: could not define sigaction SIGTERM: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGFPE, &sig_action, NULL) < 0) {
        printf("Error: could not define sigaction SIGFPE: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

int main() {
    // Callback before exit
    atexit(server_stop);

    // Define signals interception
    sig_intercept();

    if (server_start() < 0) {
        exit(EXIT_FAILURE);
    }

    while (1) {
        usleep(500000);
    }

    exit(EXIT_FAILURE);
}