#include <chrono>
#include <csignal>
#include <ctime>
#include <ctime>
#include <sstream>
#include <string>

#include "common.h"

int mbId;
pid_t ppid, epid;
std::map<pid_t, std::string> jobs;

// trata o sinal SIGCHLD para dar wait
// nos filhos finalizados evitando
// processos zumbie
void waitChilds(int);
// envia tuplas do processos que estao postegados
void sendTuple(msg_type type);
// trata o sinal SIGUSR1 que solicita a listagem das tuplas
// dos processos postergados
void listTuples(int);
// trata o sinal SIGTERM, SIGQUIT, SIGINT
// para finalizar graciosamente fechando
// a caixa postal aberta
void epitaph(int);
// trata o sinal SIGUSR2 que eh a solicitacao de
// shutdown
void vShutdown(int);
// funcao usada para controlar o tempo de
// postergacao do execucao solicitada
void waitTuple(std::string tuple);

int main(int argc, char** argv) {
    UNUSED_VAR argc;
    UNUSED_VAR argv;

    mbId = msgget(MAILBOX, IPC_CREAT | MAIL_PERMISSION);
    if (mbId == -1) {
        std::cerr << "Nao conseguiu criar a caixa postal, chave: " << MAILBOX
                  << std::endl;
        std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    if ((epid = fork()) == 0) {
        if (execl(ESCALONA_EXEC, ESCALONA_EXEC, NULL) == -1) {
            std::cerr << "Nao conseguiu abrir o escalona_jobs " << std::endl;
            std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    // salva seu pid em um arquivo para que outros processos possam
    // o consultar e o mandar sinais
    std::ofstream stm(VERIFICA_PID_FILE, std::ofstream::out);
    stm << getpid() << std::endl;
    stm.close();

    ppid = getpid();
#ifdef DEBUG
    std::cout << DEBUG_PRINT(green) << "verifica ppid = " << ppid << std::endl;
#endif
    // Definição dos tratamento de sinais
    signal(SIGCHLD, waitChilds);
    signal(SIGUSR1, listTuples);
    signal(SIGUSR2, vShutdown);
    signal(SIGTERM, epitaph);
    signal(SIGQUIT, epitaph);
    signal(SIGINT, epitaph);

    struct bufferTuple bufferTuple1, bufferTuple2;
    while (true) {
        // Mensagem do solicita_execucao
        if (msgrcv(mbId, (void*)&bufferTuple1, sizeof(bufferTuple1.mtext),
                   MSG_TUPLE, 0) == -1) {
            continue;
        }

        // cria um processo que ira cuidar do tempo de postegarmento
        pid_t pid = fork();
        if (pid == 0) {
            waitTuple(std::string(bufferTuple1.mtext));
            return 0;
        } else {
            // estrutura com as tuplas postergadas
            jobs[pid] = std::string(bufferTuple1.mtext);

            // resposta com o pid do processo
            // que controla o tempo de postergacao
            // para o solicita_execucao
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

void waitChilds(int) {
    pid_t pid;
    while ((pid = waitpid((pid_t)(-1), 0, WNOHANG)) > 0) {
        if (pid != epid) jobs.erase(pid);
    }
}

void sendTuple(msg_type type) {
    // monta mensagem
    std::stringstream stm;
    for (auto job : jobs) {
        stm << job.first << '\t' << job.second << std::endl;
    }
    // cria o buffer e verifica se tem o tamanho é menor
    // que o tamanho maximo definido
    struct bufferMap buffer;
    buffer.mtype = type;
    std::string mapStr(stm.str());
    if (mapStr.length() >= MAP_MAX_SIZE) {
        mapStr = "Lista de postergados eh grande demais para ser enviada\n";
    }
    strcpy(buffer.mtext, mapStr.c_str());

    // envia a mensagem para o processo lista_prostergados
    if (msgsnd(mbId, (void*)&buffer, sizeof(buffer.mtext), IPC_NOWAIT) == -1) {
        std::cerr << "Nao conseguiu enviar as tuplas na caixa postal"
                  << std::endl;
        std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
    }
}

void listTuples(int) { sendTuple(MSG_MAP); }

void epitaph(int) {
    pid_t pid = getpid();
    if ((ppid == pid) && (msgctl(mbId, IPC_RMID, NULL) != 0)) {
        std::cerr << "Nao conseguiu fechar a caixa postal" << std::endl;
        std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
    }
    exit(0);
}

void vShutdown(int) {
    sendTuple(MSG_V_KILL);

    for (auto job : jobs) {
        kill(job.first, SIGTERM);
    }
}

void waitTuple(std::string tuple) {
    auto cTime = std::chrono::system_clock::now();
    std::time_t sTime = std::chrono::system_clock::to_time_t(cTime);
    std::string submitTime = std::string(std::ctime(&sTime));

    // separa os conteudos da tupla
    // pelos seu delimitador
    std::string tokens[4];
    std::string delimiter("\t\t");
    int i = 0;
    size_t pos;
    while ((i < 3) && ((pos = tuple.find(delimiter)) != std::string::npos)) {
        tokens[i++] = tuple.substr(0, pos);
        tuple.erase(0, pos + delimiter.length());
    }
    tokens[i] = tuple;

    // tempo que sera postergado
    unsigned int hour, min, time;
    pos = tokens[0].find(':');
    hour = std::stoi(tokens[0].substr(0, pos));
    min = std::stoi(tokens[0].substr(pos + 1, tokens[0].length()));
    time = (60 * hour + min) * 60;
    sleep(time);

    unsigned int nJobs = std::stoi(tokens[1]);
    struct bufferJob buffer;
    buffer.mtype = MSG_JOB;
    buffer.job.pid = 0;
    buffer.job.priority = std::stoi(tokens[2]) - 1;
    buffer.job.finished = false;
    submitTime = submitTime.substr(0, submitTime.size() - 1);
    strcpy(buffer.job.submitTime, submitTime.c_str());
    strcpy(buffer.job.file, tokens[3].c_str());
    for (unsigned int i = 0; i < nJobs; i++) {
#ifdef DEBUG
        std::cout << DEBUG_PRINT(green) << "Send copy " << i << ".."
                  << std::endl;
#endif
        // envia a tarefa pronta para executar para o escalonador de fato
        if (msgsnd(mbId, (void*)&buffer, sizeof(buffer.job), IPC_NOWAIT) ==
            -1) {
            std::cerr << "Nao conseguiu enviar o job para executar"
                      << std::endl;
            std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
        }
    }
}
