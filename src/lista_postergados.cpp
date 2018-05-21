#include <cstring>
#include <fstream>

#include "common.h"

int main(int argc, char** argv) {
    std::ifstream stm(VERIFICA_PID_FILE, std::ifstream::in);
    pid_t pid;
    stm >> pid;
    kill(pid, SIGUSR1);

    int mbId = msgget(MAILBOX, MAIL_PERMISSION);
    if (mbId == -1) {
        std::cerr << "Nao conseguiu abrir a caixa postal, chave: " << MAILBOX
                  << std::endl;
        std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    struct bufferMap buffer;
    if (msgrcv(mbId, (void*)&buffer, sizeof(buffer.mtext), MSG_MAP, 0) == -1) {
        std::cerr << "Nao conseguiu receber a lista de postergados"
                  << std::endl;
        std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << BOLD << COLOR(cyan) << TUPLE_TITLE << OFF << std::endl;
    std::cout << buffer.mtext;

    return 0;
}
