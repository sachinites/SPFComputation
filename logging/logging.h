#ifndef __LOGGING__
#define __LOGGING__

#include <string.h>
#include <stdio.h>

#define LOG_BUFFER_SIZE 1024

extern char enable_log;
extern char LOG[LOG_BUFFER_SIZE];


#define TRACE()                                     \
    if(enable_log){                                 \
        printf("%s(): %s\n", __FUNCTION__, LOG);    \
        memset(LOG, 0, LOG_BUFFER_SIZE);            \
}

void
enable_logging();
void
disable_logging();

#endif /* __LOGGING__ */
