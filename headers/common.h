#ifndef COMMON_H
#define COMMON_H

#include <map>

#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <csignal>

#define MAILBOX 1234
#define MAIL_PERMISSION 0755
#define TUPLE_MAX_SIZE 512
#define MAP_MAX_SIZE 512

#define VERIFICA_PID_FILE "/tmp/verifica_jobs.pid"
#define TUPLE_TITLE "jobs\thora:min\tcopias\t\tprioridade\tarq executavel"

#define BOLD "\e[1m"
#define OFF "\e[0m"
#define COLOR(id) "\033[1;3" << id << "m"
#define ERROR_PRINT COLOR(color::red) << "Erro: " << OFF
enum color {
    red = 1,
    green = 2,
    yellow = 3,
    blue = 4,
    magenta = 5,
    cyan = 6,
};

enum msg_type { MSG_TUPLE = 1, MSG_MAP, MSG_REPLY };
struct bufferTuple {
    long mtype;               /*  message type, must be > 0 */
    char mtext[TUPLE_MAX_SIZE]; /*  message data */
};

struct bufferMap {
    long mtype;               /*  message type, must be > 0 */
    char mtext[MAP_MAX_SIZE]; /*  message data */
};

#endif  // COMMON_H
