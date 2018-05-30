#include <cerrno>
#include <cstring>
#include <sstream>

#include "common.h"

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "Sintaxe Obrigatoria: " << std::endl;
        std::cerr << "$ ./solicita_execucao <hora:min> <copias> <prioridade> "
                     "<nome arq exec>"
                  << std::endl;
        exit(EXIT_FAILURE);
    }
    if (atoi(argv[2]) < 1) {
        std::cerr << "Deve pedir ao menos uma copia" << std::endl;
        exit(EXIT_FAILURE);
    }
    if (atoi(argv[3]) < 1 || atoi(argv[3]) > 3) {
        std::cerr << "A prioridade da execucao deve ser de 1 a 3, "
                  << "sendo que 1>2>3" << std::endl;
        exit(EXIT_FAILURE);
    }

    int mbId = msgget(MAILBOX, MAIL_PERMISSION);
    if (mbId == -1) {
        std::cerr << "Nao conseguiu abrir a caixa postal, chave: " << MAILBOX
                  << std::endl;
        std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    std::stringstream tuple;
    tuple << argv[1] << "\t\t" << argv[2] << "\t\t" << argv[3] << "\t\t"
          << argv[4];
    struct bufferTuple buffer1;
    buffer1.mtype = MSG_TUPLE;
    strcpy(buffer1.mtext, tuple.str().c_str());

    if (msgsnd(mbId, (void*)&buffer1, sizeof(buffer1.mtext), 0) == -1) {
        std::cerr << "Nao conseguiu enviar na caixa postal" << std::endl;
        std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    struct bufferTuple buffer2;
    if (msgrcv(mbId, (void*)&buffer2, sizeof(buffer2.mtext), MSG_REPLY, 0) ==
        -1) {
        std::cerr << "Nao conseguiu receber na caixa postal" << std::endl;
        std::cerr << ERROR_PRINT << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << BOLD << COLOR(cyan) << TUPLE_TITLE << OFF << std::endl;
    std::cout << buffer2.mtext << '\t' << tuple.str() << std::endl;

    return 0;
}
