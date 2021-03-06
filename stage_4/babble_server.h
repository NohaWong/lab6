#ifndef __BABBLE_SERVER_H__
#define __BABBLE_SERVER_H__

#include <stdio.h>

#include "babble_types.h"

/* server starting date */
extern time_t server_start;

/* Init functions*/
void server_data_init(void);
int server_connection_init(int port);
int server_connection_accept(int sock);
void init_cond_vars(void);

/* new object */
command_t* new_command(unsigned long key);

int parse_command(char* str, command_t *cmd);
int process_command(command_t *cmd);
int answer_command(command_t *cmd);

/* Operations */
int run_login_command(command_t *cmd);
int run_publish_command(command_t *cmd);
int run_follow_command(command_t *cmd);
int run_timeline_command(command_t *cmd);
int run_fcount_command(command_t *cmd);
int run_rdv_command(command_t *cmd);

int unregisted_client(command_t *cmd);

/* Display functions */
void display_command(command_t *cmd, FILE* stream);

/* Error management */
int notify_parse_error(command_t *cmd, char *input);

/* High level comm function */
int write_to_client(unsigned long key, int size, void* buf);

// task for communication threads
void *hybrid_thread(void *args);
void *executor_thread(void *args);

void exec_single_task(task_t task);

void produce_task(task_t task);
void consume_task(void (*executor)(task_t task), int exec_id, int is_hybrid);
#endif
