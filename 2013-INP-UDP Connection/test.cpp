#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <iostream>
#include <vector>

int main()
{
    struct timeval             sendtime, recvtime, timecost;
    FILE*           fp = fopen( "testfile", "wb" );
    char* buffer;
       buffer = new char[ 1476 + 1 ];

    gettimeofday( &sendtime, NULL );



    fseek( fp, 10000000, SEEK_SET );

    fwrite( buffer, 1, 1476, fp );

    gettimeofday( &recvtime, NULL );
    delete( buffer );

    timecost.tv_sec = recvtime.tv_sec - sendtime.tv_sec;
    timecost.tv_usec = recvtime.tv_usec - sendtime.tv_usec;
    printf( "io time: %d s\n", timecost.tv_usec  );
    printf( "%d\n", 15*88803 );
    return 0;
}
