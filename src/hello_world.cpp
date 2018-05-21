#include "common.h"

int main(int argc, char** argv) {
    while (true) {
#ifdef DEBUG
        std::cout << DEBUG_PRINT << "HELLO WORLD: " << getpid() << std::endl;
#endif
        sleep(1);
    }
    return 0;
}
