/*
// 9817024 Davis
// UDP Client
*/

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

/*
still can add
1. slide window cause by ( rtt time / 120 * 2 ) maybe no difference
2. write file once
*/


#define SA          struct sockaddr
#define TIMEVAL     struct timeval
#define WINDOW_SIZE 1000
#define MAXLINE     1468
#define SLEEP_USEC  130
#define SERV_PORT   5566
#define SERVER_PATH "/home/estinet/9817024/received.tar"
#define CLIENT_PATH "/tmp/input.tar"

int packetcounter = 0;

struct packet_type
{
    unsigned int    seq;                    //sequence
    unsigned int    fsize;                  //fragment
    unsigned int    psize;                  //packet carry data size
    char            data[ MAXLINE + 1 ];    //packet carry data
    TIMEVAL         sendtime;               //packet create time

    packet_type( unsigned int seq, unsigned int fsize, unsigned int psize, char data_in[ MAXLINE + 1 ] ) :
    seq( seq ), fsize( fsize ), psize( psize )
    {
        memcpy( data, data_in, psize );
    }
};

struct ack_type
{
    unsigned int    seq;        //sequence
    TIMEVAL         sendtime;   //packet create time
};

// to get transfer file size
unsigned int get_fsize( FILE* fp )
{
    unsigned int fsize;

    fseek( fp, 0, SEEK_END );
    fsize = ftell( fp );
    rewind( fp );
    printf("filesize: %d\n", fsize );
    return fsize;
}

long time_sub( TIMEVAL A, TIMEVAL B )
{
    return ( A.tv_sec - B.tv_sec ) * 1000000 + ( A.tv_usec - B.tv_usec );
}

void timeout_interval( TIMEVAL& timeout, TIMEVAL sendtime )
{
    static unsigned int estimatedRTT = 0, devRTT = 0;
    unsigned int        sampleRTT, timeoutInterval;
    TIMEVAL             recvtime;

    gettimeofday( &recvtime, NULL );
    sampleRTT       = time_sub( recvtime, sendtime );
    estimatedRTT    = 0.875 * estimatedRTT + 0.125 * sampleRTT;
    devRTT          = 0.75 * devRTT + 0.25 * abs( sampleRTT - estimatedRTT );
    timeoutInterval = estimatedRTT + 4 * devRTT;

    timeout.tv_sec  = timeoutInterval / 1000000;
    timeout.tv_usec = timeoutInterval % 1000000;
}

void clear_sendlist( std::vector< packet_type* >& sendlist, unsigned int seq )
{
    for( int i = 0; i < sendlist.size(); i++ )
    {
        if( sendlist[ i ]->seq == seq )
        {
            delete( sendlist[ i ] );
            sendlist.erase( sendlist.begin() + i );
        }
    }
}

void transfer( int sockfd, SA* pservaddr, socklen_t servlen )
{
    std::vector< packet_type* > sendlist;
    ack_type                    recvack;
    FILE*                       fp = fopen( CLIENT_PATH, "rb" );
    unsigned int                fsize = get_fsize( fp ), tsize = 0;
    TIMEVAL                     timeout;

    printf( "Transfer begin!!\n" );

    printf( "file slice: %d\n", fsize / MAXLINE + ( fsize % MAXLINE != 0 ) );

    while( sendlist.size() != 0 || fsize != tsize )
    {
        while( sendlist.size() != WINDOW_SIZE && fsize != tsize )
        {
            char            buffer[ MAXLINE + 1 ];
            unsigned int    psize;

            psize = fread( buffer, 1, MAXLINE, fp );
            sendlist.push_back( new packet_type( tsize, fsize, psize, buffer ) );
            tsize += psize;
        }

        for( int i = 0; i < sendlist.size(); i++ )
        {
            gettimeofday( &sendlist[ i ]->sendtime, NULL );
            //printf( "[%d]send seq:%10d size:%10d\n", i, sendlist[ i ]->seq, sendlist[ i ]->psize );
            sendto( sockfd, sendlist[ i ], sizeof( *sendlist[ i ] ), 0, pservaddr, servlen );
            packetcounter++;
            usleep( SLEEP_USEC );
        }

        while( recvfrom( sockfd, &recvack, sizeof( recvack ), 0, NULL, NULL ) > 0 )
        {
            timeout_interval( timeout, recvack.sendtime );
            setsockopt( sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof( timeout ) );
            if( timeout.tv_sec >= 1 )
                printf( "ERRO\n" );
            clear_sendlist( sendlist, recvack.seq );
        }
    }

    printf( "estimatedRTT: %.6f s\n", double ( timeout.tv_sec * 1000000 + timeout.tv_usec ) / 1000000 );
    printf( "prefer window: %d\n", ( timeout.tv_sec * 1000000 + timeout.tv_usec ) / 120 );

    fclose( fp );
    close( sockfd );

    printf( "Transfer complete!!\n" );
}

int main( int argc, char** argv )
{
    int                 sockfd, servport, fsize, timeout;
    struct sockaddr_in  servaddr;
    TIMEVAL             sendtime, recvtime, timecost;

    if( argc > 2 )
        servport = atoi( argv[ 2 ] );
    else
        servport = SERV_PORT;

    bzero( &servaddr, sizeof( servaddr ) );
    servaddr.sin_family = AF_INET;
    servaddr.sin_port   = htons( servport );
    inet_pton( AF_INET, argv[ 1 ], &servaddr.sin_addr );

    sockfd = socket( AF_INET, SOCK_DGRAM, 0 );

    gettimeofday( &sendtime, NULL );

    transfer( sockfd, ( SA* ) &servaddr, sizeof( servaddr ) );

    gettimeofday( &recvtime, NULL );

    timecost.tv_sec = recvtime.tv_sec - sendtime.tv_sec;
    timecost.tv_usec = recvtime.tv_usec - sendtime.tv_usec;
    printf( "transfer time: %.3f s\n", double ( timecost.tv_sec * 1000000 + timecost.tv_usec ) / 1000000 );
    printf( "transfer packet: %d\n", packetcounter );
    return 0;
}
