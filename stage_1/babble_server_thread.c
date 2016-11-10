#include <sys/types.h>
#include <pthread.h>
#include "babble_server.h"

#define BUFFER_SIZE 128

pthread_mutex_t mutex_tasks = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_empty_tasks = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_full_tasks = PTHREAD_COND_INITIALIZER;

char *task_buffer[BUFFER_SIZE];
int task_count = 0;
int task_in = 0;
int task_out = 0;

void produce_task(char *task) {
	pthread_mutex_lock(&mutex_tasks);
	while (task_count == BUFFER_SIZE) {
		pthread_cond_wait(&not_full_tasks, &mutex_tasks);
	}

	task_buffer[task_in] = task;
	task_in = (task_in + 1) % BUFFER_SIZE;
	task_count++;
	pthread_cond_signal(&not_empty_tasks);
	pthread_mutex_unlock(&mutex_tasks);
}

void consume_task(void (*executor)(char *task)) {
	for (;;) {
		pthread_mutex_lock(&mutex_tasks);
		while (task_count == 0) {
			pthread_cond_wait(&not_empty_tasks, &mutex_tasks);
		}

		char *next_task = task_buffer[task_out];
		task_out = (task_out + 1) % BUFFER_SIZE;
		task_count--;
		pthread_cond_signal(&not_full_tasks);
		pthread_mutex_unlock(&mutex_tasks);

		executor(next_task);
	}

}