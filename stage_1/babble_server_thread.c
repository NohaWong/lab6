#include <stdlib.h>
#include <sys/types.h>
#include <pthread.h>
#include <string.h>
#include "babble_config.h"
#include "babble_types.h"
#include "babble_server.h"

#define BUFFER_SIZE 3

pthread_mutex_t mutex_tasks = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_empty_tasks = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_full_tasks = PTHREAD_COND_INITIALIZER;

task_t task_buffer[BUFFER_SIZE];
int task_count = 0;
int task_in = 0;
int task_out = 0;

void produce_task(task_t task) {
    pthread_mutex_lock(&mutex_tasks);
    while (task_count == BUFFER_SIZE) {
        printf("[prod] task buffer full, waiting\n");
        pthread_cond_wait(&not_full_tasks, &mutex_tasks);
    }

    if (task_buffer[task_in].cmd_str == NULL) {
        task_buffer[task_in].cmd_str = malloc(BABBLE_SIZE * sizeof(char));
    }
    strncpy(task_buffer[task_in].cmd_str, task.cmd_str, BABBLE_SIZE);
    task_buffer[task_in].key = task.key;

    task_in = (task_in + 1) % BUFFER_SIZE;
    task_count++;
    pthread_cond_signal(&not_empty_tasks);
    pthread_mutex_unlock(&mutex_tasks);
    printf("[prod] task added: %s\n", task.cmd_str);
}

void consume_task(void (*executor)(task_t task)) {
    pthread_mutex_lock(&mutex_tasks);
    while (task_count == 0) {
        printf("[cons] task buffer empty, waiting\n");
        pthread_cond_wait(&not_empty_tasks, &mutex_tasks);
    }

    task_t next_task = task_buffer[task_out];
    task_out = (task_out + 1) % BUFFER_SIZE;
    task_count--;
    printf("[cons] got task [%d] %s\n", task_out, next_task.cmd_str);
    pthread_cond_signal(&not_full_tasks);
    pthread_mutex_unlock(&mutex_tasks);

    executor(next_task);
}