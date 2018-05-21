#include <chrono>
#include <ctime>
#include <list>
#include <queue>

#include "common.h"

bool executing = false;
int currentPriority = -1;
std::map<unsigned int, std::queue<struct job>> queues;
std::list<struct job> finishedJobs;

void jobQuantum(int) {}

void jobFinished(int) {
    pid_t pid;
    if ((pid = wait(0)) == -1) {
        std::cerr << "Nao conseguiu dar wait no job" << std::endl;
        std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
        return;
    }

    struct job job = queues[currentPriority].front();
    auto cTime = std::chrono::system_clock::now();
    std::time_t eTime = std::chrono::system_clock::to_time_t(cTime);
    strcpy(job.endTime, std::ctime(&eTime));
    finishedJobs.emplace_back(job);
    queues[currentPriority].pop();
    if (queues[currentPriority].empty()) {
        queues.erase(currentPriority);
    }
    executing = false;
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

    struct bufferJob buffer;
    struct job job;
    while (true) {
        while (msgrcv(mbId, (void*)&buffer, sizeof(buffer.job), MSG_JOB,
                      IPC_NOWAIT) > 0) {
            auto cTime = std::chrono::system_clock::now();
            std::time_t iTime = std::chrono::system_clock::to_time_t(cTime);
            strcpy(buffer.job.initTime, std::ctime(&iTime));
            queues[buffer.job.priority].push(buffer.job);
        }
        if (!queues.empty() && !executing) {
            job = queues.begin()->second.front();
            currentPriority = job.priority;
            if (job.pid == 0) {
                job.descending = true;
                job.oldQueue = false;
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
                kill(job.pid, SIGCONT);
            }
            executing = true;
        }
    }
}
