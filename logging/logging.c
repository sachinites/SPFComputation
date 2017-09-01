#include "logging.h"

char enable_log = 0;
char LOG[LOG_BUFFER_SIZE];


void
enable_logging(){
    enable_log = 1;
}
void
disable_logging(){
    enable_log = 0;
}

