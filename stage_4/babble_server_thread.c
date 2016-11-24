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
pthread_cond_t need_exec = PTHREAD_COND_INITIALIZER;
pthread_cond_t need_hybrid = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_full_tasks = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex_connection = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_exec_ids = PTHREAD_MUTEX_INITIALIZER;

task_t task_buffer[BABBLE_TASK_QUEUE_SIZE];
int task_count = 0;
int task_in = 0;
int task_out = 0;

int exec_ids[BABBLE_EXECUTOR_THREADS + BABBLE_COMMUNICATION_THREADS];
int nb_consumers = 0;
int no_com_thr = 1;

// maintain list of active consumers
// to distribute client commands accordingly
static void add_exec_id(int exec_id) {
    int i;
    pthread_mutex_lock(&mutex_exec_ids);
    for (i = 0; i < nb_consumers; i++) {
        if (exec_ids[i] == exec_id) {
            break;
        }
    }

    if (i == nb_consumers) {
        exec_ids[nb_consumers] = exec_id;
        nb_consumers++;
    }
    pthread_mutex_unlock(&mutex_exec_ids);
}

static void remove_exec_id(int exec_id) {
    int i;
    pthread_mutex_lock(&mutex_exec_ids);
    for (i = 0; i < nb_consumers; i++) {
        if (exec_ids[i] == exec_id) {
            nb_consumers--;
            exec_ids[i] = exec_ids[nb_consumers];
            break;
        }
    }
    pthread_mutex_unlock(&mutex_exec_ids);
}

void produce_task(task_t task) {
    pthread_mutex_lock(&mutex_tasks);
    while (task_count == BABBLE_TASK_QUEUE_SIZE) {
        pthread_cond_wait(&not_full_tasks, &mutex_tasks);
    }

    if (task_buffer[task_in].cmd_str == NULL) {
        task_buffer[task_in].cmd_str = malloc(BABBLE_SIZE * sizeof(char));
    }
    strncpy(task_buffer[task_in].cmd_str, task.cmd_str, BABBLE_SIZE);
    task_buffer[task_in].key = task.key;

    task_in = (task_in + 1) % BABBLE_TASK_QUEUE_SIZE;
    task_count++;

    pthread_cond_broadcast(&need_exec);
    pthread_cond_broadcast(&need_hybrid);
    pthread_mutex_unlock(&mutex_tasks);
}

void consume_task(void (*executor)(task_t task), int exec_id, int is_hybrid) {
    pthread_mutex_lock(&mutex_tasks);
    task_t next_task;

    while (task_count == 0) {
        if (is_hybrid) {
            pthread_cond_wait(&need_hybrid, &mutex_tasks);
        } else {
            pthread_cond_wait(&need_exec, &mutex_tasks);
        }

        // no communication thread avail now
        // i might be needed to become communication thread
        if (no_com_thr && is_hybrid) {
            pthread_mutex_unlock(&mutex_tasks);
            return;
        }
    }

    int hash = task_buffer[task_out].key % nb_consumers;
    // assign commands from the same client to single consumer
    // to ensure command ordering
    if (exec_ids[hash] != exec_id) {
        // not the task for me
        pthread_mutex_unlock(&mutex_tasks);
        return;
    }

    next_task = task_buffer[task_out];
    task_out = (task_out + 1) % BABBLE_TASK_QUEUE_SIZE;
    task_count--;
    
    task_t cloned_task;
    cloned_task.cmd_str = malloc(BABBLE_SIZE * sizeof(char));
    strncpy(cloned_task.cmd_str, next_task.cmd_str, BABBLE_SIZE);
    cloned_task.key = next_task.key;

    pthread_cond_signal(&not_full_tasks);
    pthread_mutex_unlock(&mutex_tasks);
    executor(cloned_task);
    free(cloned_task.cmd_str);
}

void *executor_thread(void *args) {
    int exec_id = *((int *) args);
    add_exec_id(exec_id);
    while (1) {
        consume_task(&exec_single_task, exec_id, 0);
    }
    free(args);
}

void *hybrid_thread(void *args) {
    hybrid_thr_args_t *hargs = (hybrid_thr_args_t *) args;
    while (1) {
        // if i cant get the mutex_connection, i'll be consumer
        if (!pthread_mutex_trylock(&mutex_connection)) {
            no_com_thr = 0;
            int newsockfd = server_connection_accept(hargs->sockfd);
            // i'll maintain communication to this client
            // need to elect another thread to handle incoming connection
            // so wake all hybrid consumers up

            // acquire lock for mutex_tasks
            // so that running consumers can finish and go to sleep before creating signal to wake them up
            pthread_mutex_lock(&mutex_tasks);
            no_com_thr = 1;
            pthread_cond_broadcast(&need_hybrid);
            pthread_mutex_unlock(&mutex_tasks);
            pthread_mutex_unlock(&mutex_connection);

            if(newsockfd == -1){
                continue;
            }

            char* recv_buff=NULL;
            int recv_size=0;

            command_t *cmd;
            unsigned long client_key=0;
            char client_name[BABBLE_ID_SIZE+1];

            bzero(client_name, BABBLE_ID_SIZE+1);
            if((recv_size = network_recv(newsockfd, (void**)&recv_buff)) < 0){
                fprintf(stderr, "Error -- recv from client\n");
                close(newsockfd);
                continue;
            }

            cmd = new_command(0);
            
            if(parse_command(recv_buff, cmd) == -1 || cmd->cid != LOGIN){
                fprintf(stderr, "Error -- in LOGIN message\n");
                close(newsockfd);
                free(cmd);
                continue;
            }

            /* before processing the command, we should register the
             * socket associated with the new client; this is to be done only
             * for the LOGIN command */
            cmd->sock = newsockfd;

            int login_result = process_command(cmd);
            if(login_result == -1){
                fprintf(stderr, "Error -- in LOGIN\n");
                close(newsockfd);
                free(cmd);
                continue;
            }

            /* notify client of registration */
            if(answer_command(cmd) == -1){
                fprintf(stderr, "Error -- in LOGIN ack\n");
                close(newsockfd);
                free(cmd);
                continue;
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
        } else {
            add_exec_id(hargs->exec_id);
            consume_task(&exec_single_task, hargs->exec_id, 1);
            remove_exec_id(hargs->exec_id);
        }
    }
    free(hargs);
}