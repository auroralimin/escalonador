#include <chrono>
#include <ctime>
#include <list>
#include <queue>

#include "common.h"

bool executing = false;
int currentPriority = -1;
pid_t currentPid = 0;
std::map<unsigned int, std::queue<struct job>> queues;
std::list<struct job> finishedJobs;

void jobQuantum(int) {
#ifdef DEBUG
	std::cout << DEBUG_PRINT << "Quantum! " << currentPid << std::endl;
#endif
	kill(currentPid, SIGTSTP);

    struct job job = queues[currentPriority].front();
	queues[currentPriority].push(job);
	queues[currentPriority].pop();

	executing = false;
}

void jobFinished(int) {
    pid_t pid;
	int status;
    if ((pid = waitpid(-1, &status, WUNTRACED | WNOHANG)) == -1) {
        std::cerr << "Nao conseguiu dar wait no job" << std::endl;
        std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
        return;
    }
	if (pid == 0 || WIFSTOPPED(status) || WIFCONTINUED(status)) {
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
	alarm(0);
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
		int returnValue;
        while ((returnValue = msgrcv(mbId, (void*)&buffer,
				sizeof(buffer.job), MSG_JOB, IPC_NOWAIT)) > 0) {
#ifdef DEBUG
			std::cout << DEBUG_PRINT << "Received msg: "
			   	      << buffer.job.file << std::endl;
#endif
            auto cTime = std::chrono::system_clock::now();
            std::time_t iTime = std::chrono::system_clock::to_time_t(cTime);
            strcpy(buffer.job.initTime, std::ctime(&iTime));
            queues[buffer.job.priority].push(buffer.job);
        }
		if (errno != ENOMSG) {
			std::cerr << "Problema de comunicacao entre caixas postais" << std::endl;
			std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
			exit(EXIT_FAILURE);
		}

        if ((!queues.empty()) && (!executing)) {
            executing = true;
            struct job &job = queues.begin()->second.front();
#ifdef DEBUG
			std::cout << DEBUG_PRINT << "Executed proccess: " << job.pid << std::endl;
#endif
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
#ifdef DEBUG
				std::cout << DEBUG_PRINT << "Continued process: " << job.pid << std::endl;
#endif
                kill(job.pid, SIGCONT);
            }

            currentPriority = job.priority;
			currentPid = job.pid;
			alarm(QUANTUM);
        }
    }
}
