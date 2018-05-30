#include "common.h"

int main(int argc, char** argv) {
    UNUSED_VAR argc;
    UNUSED_VAR argv;
#ifdef DEBUG
    std::cout << DEBUG_PRINT(blue) << "HELLO WORLD STARTED: " << getpid()
              << std::endl;
#endif

    for (int i = 0; i < 30; i++) {
        sleep(1);
#ifdef DEBUG
        std::cout << DEBUG_PRINT(blue) << "HELLO WORLD EXECUTING: " << getpid()
                  << std::endl;
#endif
    }
    return 0;
}
