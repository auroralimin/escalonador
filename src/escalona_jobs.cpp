#include <chrono>
#include <ctime>
#include <list>
#include <queue>

#include "common.h"

bool executing = false;
int currentPriority = -1;
pid_t currentPid = 0;
std::queue<struct job> queues[N_QUEUES];
std::list<struct job> finishedJobs;

void jobQuantum(int) {
#ifdef DEBUG
    std::cout << DEBUG_PRINT << "Quantum! " << currentPid << std::endl;
#endif
    kill(currentPid, SIGTSTP);

    struct job job = queues[currentPriority].front();
    if (!job.oldQueue) {
        job.oldQueue = true;
    } else {
        job.oldQueue = false;
        int multiplier = job.descending ? -1 : +1;
        if ((job.priority == 0) || (job.priority == N_QUEUES - 1)) {
            job.descending = !job.descending;
            multiplier = multiplier * (-1);
        }
        job.priority += multiplier;
    }
    queues[job.priority].push(job);
    queues[currentPriority].pop();

    executing = false;
}

void jobFinished(int) {
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WUNTRACED | WNOHANG)) > 0) {
        if (WIFSTOPPED(status) || WIFCONTINUED(status)) {
            continue;
        }
        alarm(0);
#ifdef DEBUG
        std::cout << DEBUG_PRINT << "Finished " << pid << std::endl;
#endif
        struct job job = queues[currentPriority].front();
        auto cTime = std::chrono::system_clock::now();
        std::time_t eTime = std::chrono::system_clock::to_time_t(cTime);
        strcpy(job.endTime, std::ctime(&eTime));
        finishedJobs.emplace_back(job);
        queues[currentPriority].front().finished = true;
        executing = false;
    }
}

void cleanQueues() {
    for (int i = 0; i < N_QUEUES; i++) {
        while ((!queues[i].empty()) && (queues[i].front().finished)) {
            queues[i].pop();
        }
    }
}

int firstQueueNotEmpty() {
    cleanQueues();
    for (int i = 0; i < N_QUEUES; i++) {
        if (!queues[i].empty()) {
            return i;
        }
    }
    return -1;
}

int main(int argc, char** argv) {
    int mbId = msgget(MAILBOX, MAIL_PERMISSION);
    if (mbId == -1) {
        std::cerr << "Nao conseguiu abrir a caixa postal, chave: " << MAILBOX
                  << std::endl;
        std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    signal(SIGALRM, jobQuantum);
    signal(SIGCHLD, jobFinished);

#ifdef DEBUG
    std::cout << DEBUG_PRINT << "ppid = " << getpid() << std::endl;
#endif
    struct bufferJob buffer;
    while (true) {
        while (msgrcv(mbId, (void*)&buffer, sizeof(buffer.job), MSG_JOB,
                      IPC_NOWAIT) > 0) {
#ifdef DEBUG
            std::cout << DEBUG_PRINT << "Received msg: " << buffer.job.file
                      << std::endl;
#endif
            auto cTime = std::chrono::system_clock::now();
            std::time_t iTime = std::chrono::system_clock::to_time_t(cTime);
            strcpy(buffer.job.initTime, std::ctime(&iTime));
            queues[buffer.job.priority].push(buffer.job);
        }
        if (errno != ENOMSG) {
            std::cerr << "Problema de comunicacao entre caixas postais"
                      << std::endl;
            std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
            exit(EXIT_FAILURE);
        }

        int index = firstQueueNotEmpty();
        if ((index != -1) && (!executing)) {
            executing = true;
            struct job& job = queues[index].front();
            if (job.pid == 0) {
#ifdef DEBUG
                std::cout << DEBUG_PRINT << "Executed for the 1sr time"
                          << std::endl;
#endif
                job.descending = true;
                job.oldQueue = false;
                job.finished = false;
                pid_t pid = fork();
                if (pid == 0) {
                    if (execl(job.file, job.file, NULL) == -1) {
                        std::cerr << "Nao conseguiu executar " << job.file
                                  << std::endl;
                        std::cerr << ERROR_PRINT << strerror(errno)
                                  << std::endl;
                    }
                } else {
                    job.pid = pid;
                }
            } else {
#ifdef DEBUG
                std::cout << DEBUG_PRINT << "Continued process: " << job.pid
                          << std::endl;
#endif
                kill(job.pid, SIGCONT);
            }

            currentPriority = job.priority;
            currentPid = job.pid;
            alarm(QUANTUM);
#ifdef DEBUG
            std::cout << DEBUG_PRINT << "Executed proccess: " << job.pid
                      << std::endl;
            std::cout << DEBUG_PRINT << "Priority = " << job.priority + 1
                      << std::endl;
#endif
        }
    }
}
