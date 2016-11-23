#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>

#include "babble_server.h"
#include "babble_types.h"
#include "babble_utils.h"
#include "babble_communication.h"

pthread_mutex_t mutex_socket_creation = PTHREAD_MUTEX_INITIALIZER;

static void display_help(char *exec)
{
    printf("Usage: %s -p port_number\n", exec);
}

int main(int argc, char *argv[])
{
    int sockfd;
    int portno=BABBLE_PORT;
    
    int i;
    int opt;
    int nb_args=1;

    pthread_t *exec_thread_pool = malloc(BABBLE_EXECUTOR_THREADS * sizeof(pthread_t));
    for (i = 0; i < BABBLE_EXECUTOR_THREADS; i++) {
        int *exec_id = malloc(sizeof(int));
        *exec_id = i;
        pthread_create(&exec_thread_pool[i], NULL, executor_thread, (void *) exec_id);
    }

    while ((opt = getopt (argc, argv, "+p:")) != -1){
        switch (opt){
        case 'p':
            portno = atoi(optarg);
            nb_args+=2;
            break;
        case 'h':
        case '?':
        default:
            display_help(argv[0]);
            return -1;
        }
    }
    
    if(nb_args != argc){
        display_help(argv[0]);
        return -1;
    }

    server_data_init();

    if((sockfd = server_connection_init(portno)) == -1){
        return -1;
    }

    printf("Babble server bound to port %d\n", portno);    

    pthread_t *com_thread_pool = malloc(BABBLE_COMMUNICATION_THREADS * sizeof(pthread_t));
    for (i = 0; i < BABBLE_COMMUNICATION_THREADS; i++) {
        pthread_create(&com_thread_pool[i], NULL, communication_thread, (void *) &sockfd);
    }

    void **retval = NULL;
    for (i = 0; i < BABBLE_COMMUNICATION_THREADS; i++) {
        pthread_join(com_thread_pool[i], retval);
    }

    close(sockfd);
    return 0;
}
