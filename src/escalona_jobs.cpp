#include <chrono>
#include <ctime>
#include <list>
#include <queue>

#include "common.h"

bool executing = false;
int currentPriority = -1;
int mbId;
pid_t currentPid = 0;
std::list<struct job> queues[N_QUEUES];
std::list<struct job> finishedJobs;

void jobQuantum(int) {
#ifdef DEBUG
    std::cout << DEBUG_PRINT(magenta) << "Quantum! " << currentPid << std::endl;
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
    queues[job.priority].push_back(job);
    queues[currentPriority].front().finished = true;

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
        std::cout << DEBUG_PRINT(magenta) << "Finished " << pid << std::endl;
#endif
        int index;
        struct job job;
        bool found = false;
        for (index = 0; (index < N_QUEUES) && (!found); index++) {
            for (auto& qJob : queues[index]) {
                if ((qJob.pid == pid) && (!qJob.finished)) {
                    qJob.finished = true;
                    job = qJob;
                    found = true;
                    break;
                }
            }
        }
        if (!found) continue;

        auto cTime = std::chrono::system_clock::now();
        std::time_t eTime = std::chrono::system_clock::to_time_t(cTime);
        std::string endTime = std::string(std::ctime(&eTime));
        endTime = endTime.substr(0, endTime.size() - 1);
        strcpy(job.endTime, endTime.c_str());
        finishedJobs.emplace_back(job);
        executing = false;
    }
}

void eShutdown(int) {
    struct bufferJob buffer;

    buffer.mtype = MSG_E2_KILL;
    for (auto job : finishedJobs) {
        buffer.job = job;
        if (msgsnd(mbId, (void*)&buffer, sizeof(buffer.job), 0) ==
            -1) {
            std::cerr << "Nao conseguiu enviar o job para shutdown"
                      << std::endl;
            std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
        }
#ifdef DEBUG
        std::cout << DEBUG_PRINT(magenta) << "Sending finished job: " << job.pid
                  << std::endl;
#endif
    }

    buffer.job.pid = -1;
    if (msgsnd(mbId, (void*)&buffer, sizeof(buffer.job), 0) == -1) {
        std::cerr << "Nao conseguiu enviar o job para finalizar shutdown"
                  << std::endl;
        std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
    }

    buffer.mtype = MSG_E1_KILL;
    for (int i = 0; i < N_QUEUES; i++) {
        while (!queues[i].empty()) {
            if (queues[i].front().finished) {
                queues[i].pop_front();
                continue;
            }

            buffer.job = queues[i].front();
            if (msgsnd(mbId, (void*)&buffer, sizeof(buffer.job), 0) ==
                -1) {
                std::cerr << "Nao conseguiu enviar o job para shutdown"
                          << std::endl;
                std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
            }
#ifdef DEBUG
            std::cout << DEBUG_PRINT(magenta)
                      << "Sending unfinished job: " << buffer.job.pid
                      << std::endl;
#endif
            if (buffer.job.pid) {
                kill(buffer.job.pid, SIGTERM);
            }
            queues[i].pop_front();
        }
    }

    buffer.job.pid = -1;
    if (msgsnd(mbId, (void*)&buffer, sizeof(buffer.job), 0) == -1) {
        std::cerr << "Nao conseguiu enviar o job para finalizar shutdown"
                  << std::endl;
        std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
    }

    exit(0);
}

void cleanQueues() {
    for (int i = 0; i < N_QUEUES; i++) {
        while ((!queues[i].empty()) && (queues[i].front().finished)) {
            queues[i].pop_front();
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
    mbId = msgget(MAILBOX, MAIL_PERMISSION);
    if (mbId == -1) {
        std::cerr << "Nao conseguiu abrir a caixa postal, chave: " << MAILBOX
                  << std::endl;
        std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    std::ofstream stm(ESCALONA_PID_FILE, std::ofstream::out);
    stm << getpid() << std::endl;
    stm.close();

    signal(SIGALRM, jobQuantum);
    signal(SIGCHLD, jobFinished);
    signal(SIGUSR2, eShutdown);

#ifdef DEBUG
    std::cout << DEBUG_PRINT(magenta) << "escalona ppid = " << getpid() << std::endl;
#endif
    struct bufferJob buffer;
    while (true) {
        while (msgrcv(mbId, (void*)&buffer, sizeof(buffer.job), MSG_JOB,
                      IPC_NOWAIT) > 0) {
#ifdef DEBUG
            std::cout << DEBUG_PRINT(magenta) << "Received msg: " << buffer.job.file
                      << std::endl;
#endif
            auto cTime = std::chrono::system_clock::now();
            std::time_t iTime = std::chrono::system_clock::to_time_t(cTime);
            std::string initTime = std::string(std::ctime(&iTime));
            initTime = initTime.substr(0, initTime.size() - 1);
            strcpy(buffer.job.initTime, initTime.c_str());
            queues[buffer.job.priority].push_back(buffer.job);
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
                std::cout << DEBUG_PRINT(magenta) << "Executed for the 1sr time"
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
                std::cout << DEBUG_PRINT(magenta) << "Continued process: " << job.pid
                          << std::endl;
#endif
                kill(job.pid, SIGCONT);
            }

            currentPriority = job.priority;
            currentPid = job.pid;
            alarm(QUANTUM);
#ifdef DEBUG
            std::cout << DEBUG_PRINT(magenta) << "Executed proccess: " << job.pid
                      << std::endl;
            std::cout << DEBUG_PRINT(magenta) << "Priority = " << job.priority + 1
                      << std::endl;
#endif
        }
    }
}
