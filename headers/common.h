#ifndef COMMON_H
#define COMMON_H

#include <iostream>
#include <map>

#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <csignal>
#include <cstring>
#include <fstream>

#include <unistd.h>

#define QUANTUM 1
#define DEBUG true
#define N_QUEUES 3

#define MAILBOX 1234
#define MAIL_PERMISSION 0755
#define TUPLE_MAX_SIZE 512
#define MAP_MAX_SIZE 4096
#define JOB_MAX_SIZE 255

#define VERIFICA_PID_FILE "/tmp/verifica_jobs.pid"
#define ESCALONA_PID_FILE "/tmp/escalona_jobs.pid"
#define TUPLE_TITLE "jobs\thora:min\tcopias\t\tprioridade\tarq executavel"
#define JOB_TITLE "pid\texec\t\tt_submit\t\t\tt_init\t\t\t\tt_term"

#define BOLD "\e[1m"
#define OFF "\e[0m"
#define COLOR(id) "\033[1;3" << id << "m"
#define ERROR_PRINT COLOR(red) << "Erro: " << OFF
#define DEBUG_PRINT COLOR(magenta) << "[DEBUG] " << OFF
enum color {
    red = 1,
    green = 2,
    yellow = 3,
    blue = 4,
    magenta = 5,
    cyan = 6,
};

enum msg_type {
    MSG_TUPLE = 1,
    MSG_MAP,
    MSG_JOB,
    MSG_REPLY,
    MSG_V_KILL,
    MSG_E1_KILL,
    MSG_E2_KILL
};

struct job {
    pid_t pid;
    unsigned int priority;
    bool oldQueue;
    bool descending;
    bool finished;
    char file[JOB_MAX_SIZE];
    char submitTime[JOB_MAX_SIZE];
    char initTime[JOB_MAX_SIZE];
    char endTime[JOB_MAX_SIZE];
};

struct bufferTuple {
    long mtype;                 /*  message type, must be > 0 */
    char mtext[TUPLE_MAX_SIZE]; /*  message data */
};

struct bufferMap {
    long mtype;               /*  message type, must be > 0 */
    char mtext[MAP_MAX_SIZE]; /*  message data */
};

struct bufferJob {
    long mtype;     /*  message type, must be > 0 */
    struct job job; /*  message data */
};

#endif  // COMMON_H
