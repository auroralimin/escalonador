#include "common.h"

int main(int argc, char** argv) {
    pid_t vPid;
    std::ifstream vStm(VERIFICA_PID_FILE, std::ifstream::in);
    vStm >> vPid;
    vStm.close();

    pid_t ePid;
    std::ifstream eStm(ESCALONA_PID_FILE, std::ifstream::in);
    eStm >> ePid;
    eStm.close();

    int mbId = msgget(MAILBOX, MAIL_PERMISSION);
    if (mbId == -1) {
        std::cerr << "Nao conseguiu abrir a caixa postal, chave: " << MAILBOX
                  << std::endl;
        std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    kill(vPid, SIGUSR2);
    kill(ePid, SIGUSR2);

    struct bufferMap vBuffer;
    if (msgrcv(mbId, (void*)&vBuffer, sizeof(vBuffer.mtext), MSG_V_KILL, 0) ==
        -1) {
        std::cerr << "Nao conseguiu receber a lista de postergados"
                  << std::endl;
        std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << BOLD << COLOR(blue) << "Os seguintes processos não serão"
              << " executados:" << OFF << std::endl;
    std::cout << BOLD << COLOR(cyan) << TUPLE_TITLE << OFF << std::endl;
    std::cout << vBuffer.mtext;
    std::cout << BOLD << COLOR(blue) << "-------------------------------------"
              << "------------------------------------------------------------"
              << OFF << std::endl;

    std::cout << std::endl;

    struct bufferJob eBuffer;
    std::cout << BOLD << COLOR(blue) << "Processos finalizados" << OFF
              << std::endl;
    std::cout << BOLD << COLOR(cyan) << JOB_TITLE << OFF << std::endl;
    while (msgrcv(mbId, (void*)&eBuffer, sizeof(eBuffer.job), MSG_E2_KILL, 0) >
               0 &&
           eBuffer.job.pid != -1) {
        std::cout << eBuffer.job.pid << "\t" << eBuffer.job.file << "\t"
                  << eBuffer.job.submitTime << "\t" << eBuffer.job.initTime
                  << "\t" << eBuffer.job.endTime << std::endl;
    }
    std::cout << BOLD << COLOR(blue) << "-------------------------------------"
              << "------------------------------------------------------------"
              << OFF << std::endl;

    std::cout << BOLD << COLOR(blue) << "Processos que estavam na fila de"
              << " execução e serão interrompidos:\n(OBS: pid 0 significa que"
              << " o processo foi para a fila de execução mas nunca começou a"
              << " executar)" << OFF << std::endl;
    std::cout << BOLD << COLOR(cyan) << JOB_TITLE << OFF << std::endl;
    while (msgrcv(mbId, (void*)&eBuffer, sizeof(eBuffer.job), MSG_E1_KILL, 0) >
               0 &&
           eBuffer.job.pid != -1) {
        std::cout << eBuffer.job.pid << "\t" << eBuffer.job.file << "\t"
                  << eBuffer.job.submitTime << "\t" << eBuffer.job.initTime
                  << "\t"
                  << "--" << std::endl;
    }
    std::cout << BOLD << COLOR(blue) << "-------------------------------------"
              << "------------------------------------------------------------"
              << OFF << std::endl;
    kill(vPid, SIGTERM);
    return 0;
}
