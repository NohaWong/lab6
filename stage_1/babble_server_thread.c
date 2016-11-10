#include <stdlib.h>
#include <sys/types.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "babble_config.h"
#include "babble_types.h"
#include "babble_communication.h"
#include "babble_server.h"

pthread_mutex_t mutex_tasks = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_empty_tasks = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_full_tasks = PTHREAD_COND_INITIALIZER;

task_t task_buffer[BABBLE_TASK_QUEUE_SIZE];
int task_count = 0;
int task_in = 0;
int task_out = 0;

void produce_task(task_t task) {
    pthread_mutex_lock(&mutex_tasks);
    while (task_count == BABBLE_TASK_QUEUE_SIZE) {
        // printf("[prod] task buffer full, waiting\n");
        pthread_cond_wait(&not_full_tasks, &mutex_tasks);
    }

    if (task_buffer[task_in].cmd_str == NULL) {
        task_buffer[task_in].cmd_str = malloc(BABBLE_SIZE * sizeof(char));
    }
    strncpy(task_buffer[task_in].cmd_str, task.cmd_str, BABBLE_SIZE);
    task_buffer[task_in].key = task.key;

    task_in = (task_in + 1) % BABBLE_TASK_QUEUE_SIZE;
    task_count++;
    pthread_cond_signal(&not_empty_tasks);
    pthread_mutex_unlock(&mutex_tasks);
    // printf("[prod] task added: %s\n", task.cmd_str);
}

void consume_task(void (*executor)(task_t task)) {
    pthread_mutex_lock(&mutex_tasks);
    while (task_count == 0) {
        // printf("[cons] task buffer empty, waiting\n");
        pthread_cond_wait(&not_empty_tasks, &mutex_tasks);
    }

    task_t next_task = task_buffer[task_out];
    task_out = (task_out + 1) % BABBLE_TASK_QUEUE_SIZE;
    task_count--;
    // printf("[cons] got task [%d] %s\n", task_out, next_task.cmd_str);
    pthread_cond_signal(&not_full_tasks);
    pthread_mutex_unlock(&mutex_tasks);

    executor(next_task);
}

void *communication_thread(void *args) {
    int newsockfd = *((int *) args);

    char* recv_buff=NULL;
    int recv_size=0;

    command_t *cmd;
    unsigned long client_key=0;
    char client_name[BABBLE_ID_SIZE+1];

    bzero(client_name, BABBLE_ID_SIZE+1);
    if((recv_size = network_recv(newsockfd, (void**)&recv_buff)) < 0){
        fprintf(stderr, "Error -- recv from client\n");
        close(newsockfd);
        return (void *) -1;
    }

    cmd = new_command(0);
    
    if(parse_command(recv_buff, cmd) == -1 || cmd->cid != LOGIN){
        fprintf(stderr, "Error -- in LOGIN message\n");
        close(newsockfd);
        free(cmd);
        return (void *) -1;
    }

    /* before processing the command, we should register the
     * socket associated with the new client; this is to be done only
     * for the LOGIN command */
    cmd->sock = newsockfd;

    if(process_command(cmd) == -1){
        fprintf(stderr, "Error -- in LOGIN\n");
        close(newsockfd);
        free(cmd);
        return (void *) -1;
    }

    /* notify client of registration */
    if(answer_command(cmd) == -1){
        fprintf(stderr, "Error -- in LOGIN ack\n");
        close(newsockfd);
        free(cmd);
        return (void *) -1;
    }

    /* let's store the key locally */
    client_key = cmd->key;

    strncpy(client_name, cmd->msg, BABBLE_ID_SIZE);
    free(recv_buff);
    free(cmd);

    task_t task;
    /* looping on client commands */
    while((recv_size=network_recv(newsockfd, (void**) &recv_buff)) > 0){
        task.cmd_str = recv_buff;
        task.key = client_key;
        produce_task(task);
        free(recv_buff);
    }

    if(client_name[0] != 0){
        cmd = new_command(client_key);
        cmd->cid= UNREGISTER;
        
        if(unregisted_client(cmd)){
            fprintf(stderr,"Warning -- failed to unregister client %s\n",client_name);
        }
        free(cmd);
    }

    return (void *) 0;
}

void *executor_thread(void *args) {
    while (1) {
        consume_task(&exec_single_task);
    }

}
