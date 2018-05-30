#include <chrono>
#include <ctime>
#include <list>
#include <queue>

#include "common.h"

bool executing = false;
// controle de qual das filas de prioridade
// esta em execucao no memento
int currentPriority = -1;
int mbId;
pid_t currentPid = 0;
std::list<struct job> queues[N_QUEUES];
std::list<struct job> finishedJobs;

// limpa as tarefas finalizadas das
// filas do escalonador
void cleanQueues();
// retorna a proxima tarefa a ser executada,
// a primeiro da fila de maior prioridade
int firstQueueNotEmpty();
// inicia ou retoma a execucao de uma tarefa
void runJob();
// trata o sinal SIGALRM, para gerencia
// do quantum ou seja o tempo maximo seguido que uma
// tarefa fica executando no processador
// e inicia uma troca de contexto
void jobQuantum(int);
// trata o sinal SIGCHLD, para gerencia de tarefas
// que finalizaram
void jobFinished(int);
// trata o sinal SIGUSR2, que eh uma solicitacao
// de shutdown do sistema
// envia um relatorio das tarefas que forem executadas
// via caixa de mensagem
void eShutdown(int);

int main(int argc, char** argv) {
    UNUSED_VAR argc;
    UNUSED_VAR argv;
    mbId = msgget(MAILBOX, MAIL_PERMISSION);
    if (mbId == -1) {
        std::cerr << "Nao conseguiu abrir a caixa postal, chave: " << MAILBOX
                  << std::endl;
        std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    // salva seu pid em um arquivo para que outros processos possam
    // o consultar e o mandar sinais
    std::ofstream stm(ESCALONA_PID_FILE, std::ofstream::out);
    stm << getpid() << std::endl;
    stm.close();

    signal(SIGALRM, jobQuantum);
    signal(SIGCHLD, jobFinished);
    signal(SIGUSR2, eShutdown);

#ifdef DEBUG
    std::cout << DEBUG_PRINT(magenta) << "escalona ppid = " << getpid()
              << std::endl;
#endif
    struct bufferJob buffer;
    while (true) {
        // recebe uma nova tarefa para ser executada
        if (msgrcv(mbId, (void*)&buffer, sizeof(buffer.job), MSG_JOB, 0) ==
            -1) {
            if (errno != EINTR) {
                std::cerr << "Problema de comunicacao entre caixas postais"
                          << std::endl;
                std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
                exit(EXIT_FAILURE);
            }
        } else {
#ifdef DEBUG
            std::cout << DEBUG_PRINT(magenta)
                      << "Received msg: " << buffer.job.file << std::endl;
#endif
            auto cTime = std::chrono::system_clock::now();
            std::time_t iTime = std::chrono::system_clock::to_time_t(cTime);
            std::string initTime = std::string(std::ctime(&iTime));
            initTime = initTime.substr(0, initTime.size() - 1);
            strcpy(buffer.job.initTime, initTime.c_str());
            queues[buffer.job.priority].push_back(buffer.job);
            // caso nao tenha tarefas usado o processador
            // chama o run para que essa possa usar
            if (!executing) runJob();
        }
    }
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

void runJob() {
    int index = firstQueueNotEmpty();
    if (index == -1) return;

    executing = true;
    struct job& job = queues[index].front();
    // caso seja a primeira vez que essa tarefa entra em execucao
    // se nao so retoma a execucao da tarefa
    if (job.pid == 0) {
#ifdef DEBUG
        std::cout << DEBUG_PRINT(magenta) << "Executed for the 1sr time"
                  << std::endl;
#endif
        job.descending = job.priority == 0 ? true : false;
        job.oldQueue = false;
        job.finished = false;
        pid_t pid = fork();
        if (pid == 0) {
            if (execl(job.file, job.file, NULL) == -1) {
                std::cerr << "Nao conseguiu executar " << job.file << std::endl;
                std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
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

        // controle da mudaca de prioridade da tarefa
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
    runJob();
}

void jobFinished(int) {
    pid_t pid;
    bool isRunning = true;
    int status;
    int alarmRemain = alarm(0);

    while ((pid = waitpid(-1, &status, WUNTRACED | WNOHANG)) > 0) {
        if (WIFSTOPPED(status) || WIFCONTINUED(status)) {
            continue;
        }
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
        if (pid == currentPid) {
            executing = false;
            isRunning = false;
        }
    }
    // se a tarefa finalizado era a que estava
    // em execucao, chama a rotina para colocar outra tarefa.
    // se nao volta o alarm do quanto da tarefa que esta executando
    if (!isRunning) {
        runJob();
    } else {
        alarm(alarmRemain);
    }
}

void eShutdown(int) {
    struct bufferJob buffer;

    // tarefas finalizadas
    buffer.mtype = MSG_E2_KILL;
    for (auto job : finishedJobs) {
        buffer.job = job;
        if (msgsnd(mbId, (void*)&buffer, sizeof(buffer.job), 0) == -1) {
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

    // tarefas que nao foram finalizadas
    buffer.mtype = MSG_E1_KILL;
    for (int i = 0; i < N_QUEUES; i++) {
        while (!queues[i].empty()) {
            if (queues[i].front().finished) {
                queues[i].pop_front();
                continue;
            }

            buffer.job = queues[i].front();
            if (msgsnd(mbId, (void*)&buffer, sizeof(buffer.job), 0) == -1) {
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
