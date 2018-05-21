#include <chrono>
#include <csignal>
#include <ctime>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>

#include "common.h"

int mbId;
pid_t ppid;
std::map<pid_t, std::string> jobs;

void waitChilds(int) {
    pid_t pid;
    while ((pid = waitpid((pid_t)(-1), 0, WNOHANG)) > 0) {
        jobs.erase(pid);
    }
}

void printTuples(int) {
    std::stringstream stm;
    for (auto job : jobs) {
        stm << job.first << '\t' << job.second << std::endl;
    }
    struct bufferMap buffer;
    buffer.mtype = MSG_MAP;
    std::string mapStr(stm.str());
    if (mapStr.length() >= MAP_MAX_SIZE) {
        mapStr = "Lista de postergados eh grande demais para ser enviada\n";
    }
    strcpy(buffer.mtext, mapStr.c_str());

    if (msgsnd(mbId, (void*)&buffer, sizeof(buffer.mtext), IPC_NOWAIT) == -1) {
        std::cerr << "Nao conseguiu enviar as tuplas na caixa postal"
                  << std::endl;
        std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
    }
}

void epitaph(int) {
    pid_t pid = getpid();
    if ((ppid == pid) && (msgctl(mbId, IPC_RMID, NULL) != 0)) {
        std::cerr << "Nao conseguiu fechar a caixa postal" << std::endl;
        std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
    }
    exit(0);
}

void waitTuple(std::string tuple) {
    auto cTime = std::chrono::system_clock::now();
    std::time_t sTime = std::chrono::system_clock::to_time_t(cTime);
    std::string submitTime = std::ctime(&sTime);

    std::string tokens[4];
    std::string delimiter("\t\t");
    int i = 0;
    size_t pos;
    while ((i < 3) && ((pos = tuple.find(delimiter)) != std::string::npos)) {
        tokens[i++] = tuple.substr(0, pos);
        tuple.erase(0, pos + delimiter.length());
    }
    tokens[i] = tuple;

    unsigned int hour, min, time;
    pos = tokens[0].find(':');
    hour = std::stoi(tokens[0].substr(0, pos));
    min = std::stoi(tokens[0].substr(pos + 1, tokens[0].length()));
    time = (60 * hour + min) * 60;
    // sleep(time);

    unsigned int nJobs = std::stoi(tokens[1]);
    struct bufferJob buffer;
    buffer.mtype = MSG_JOB;
    buffer.job.pid = 0;
    buffer.job.priority = std::stoi(tokens[2]) - 1;
    buffer.job.finished = false;
    strcpy(buffer.job.file, tokens[3].c_str());
    for (int i = 0; i < nJobs; i++) {
#ifdef DEBUG
        std::cout << DEBUG_PRINT << "Copia " << i << " sendo enviada.."
                  << std::endl;
#endif
        if (msgsnd(mbId, (void*)&buffer, sizeof(buffer.job), IPC_NOWAIT) ==
            -1) {
            std::cerr << "Nao conseguiu enviar o job para executar"
                      << std::endl;
            std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
        }
    }
}

int main(int argc, char** argv) {
    mbId = msgget(MAILBOX, IPC_CREAT | MAIL_PERMISSION);
    if (mbId == -1) {
        std::cerr << "Nao conseguiu criar a caixa postal, chave: " << MAILBOX
                  << std::endl;
        std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    std::ofstream stm(VERIFICA_PID_FILE, std::ofstream::out);
    stm << getpid() << std::endl;
    stm.close();

    ppid = getpid();
#ifdef DEBUG
    std::cout << DEBUG_PRINT << "ppid = " << ppid << std::endl;
#endif
    signal(SIGCHLD, waitChilds);
    signal(SIGUSR1, printTuples);
    signal(SIGTERM, epitaph);
    signal(SIGQUIT, epitaph);
    signal(SIGINT, epitaph);

    struct bufferTuple bufferTuple1, bufferTuple2;
    while (true) {
        if (msgrcv(mbId, (void*)&bufferTuple1, sizeof(bufferTuple1.mtext),
                   MSG_TUPLE, 0) == -1) {
            continue;
        }

        pid_t pid = fork();
        if (pid == 0) {
            waitTuple(std::string(bufferTuple1.mtext));
            return 0;
        } else {
            jobs[pid] = std::string(bufferTuple1.mtext);

            bufferTuple2.mtype = MSG_REPLY;
            strcpy(bufferTuple2.mtext, std::to_string(pid).c_str());
            if (msgsnd(mbId, (void*)&bufferTuple2, sizeof(bufferTuple2.mtext),
                       0) == -1) {
                std::cerr << "Nao conseguiu enviar resposta da tupla"
                          << std::endl;
                std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
                exit(EXIT_FAILURE);
            }
        }
    }
}
