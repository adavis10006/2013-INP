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
MAXLINE = 1500 - 24 - 8 = 1468
                 packet   ack
still can add
1. slide window cause by ( rtt time / 120 ) maybe no difference
2. write file once ------------ ( hard to implement ) all I/O cost to short and if can do 3 than this is not problem
3. reduce retranfer time ( not transfer lose in ack )
*/

#define SA          struct sockaddr
#define TIMEVAL     struct timeval
#define TIMEOUT     500000
#define TIME_US     1000000
#define WINDOW_SIZE 3600
#define MAXLINE     1468
#define SLEEP_USEC  150
#define SEQ_KEEP    5
#define SERV_PORT   5566
#define SERVER_PATH "received.tar"
#define CLIENT_PATH "/tmp/input.tar"

int packetcounter = 0;

enum seq_type
{
    NONE    = -3,
    HEADER  = -2,
    FINISH  = -1,
    NORMAL
};

struct packet_type
{
    unsigned int    seq;                //sequence
    unsigned int    psize;              //packet carry data size
    char            data[ MAXLINE ];    //packet carry data
    TIMEVAL         sendtime;           //packet create time

    packet_type( unsigned int seq, unsigned int psize, char data_in[ MAXLINE ] ) :
    seq( seq ), psize( psize )
    {
        memcpy( data, data_in, psize );
    }
}__attribute__((packed));;

struct file_type
{
    unsigned int    seq;                //file slice size
    TIMEVAL         sendtime;           //packet create time

    file_type( unsigned int seq ) : seq( seq ) {}
}__attribute__((packed));;

struct ack_type
{
    unsigned int    seq[ SEQ_KEEP ];    //sequence
    TIMEVAL         sendtime;           //packet create time

    ack_type()
    {
        for( int i = 0; i < SEQ_KEEP; i++ )
            seq[ i ] = NONE;
    }
}__attribute__((packed));;

unsigned int HASH( unsigned int number )
{
    return number / MAXLINE + ( number % MAXLINE != 0 );
}

// to get transfer file size
unsigned int get_fsize( FILE* fp )
{
    unsigned int fsize;

    fseek( fp, 0, SEEK_END );
    fsize = ftell( fp );
    rewind( fp );
    printf("[CLIENT] Filesize: %d\n", fsize );
    return fsize;
}

long time_sub( TIMEVAL A, TIMEVAL B )
{
    return ( A.tv_sec - B.tv_sec ) * TIME_US + ( A.tv_usec - B.tv_usec );
}

void timeout_interval( TIMEVAL& timeout, TIMEVAL sendtime, unsigned int& window_size )
{
    static bool         initial = false;
    static unsigned int estimatedRTT = 0, devRTT = 0;
    unsigned int        sampleRTT, timeoutInterval;
    TIMEVAL             recvtime;

    gettimeofday( &recvtime, NULL );
    sampleRTT       = time_sub( recvtime, sendtime );
    estimatedRTT    = 0.875 * estimatedRTT + 0.125 * sampleRTT;
    devRTT          = 0.75 * devRTT + 0.25 * abs( sampleRTT - estimatedRTT );
    timeoutInterval = estimatedRTT + 4 * devRTT;

    if( !initial )
    {
        if( timeoutInterval >= TIMEOUT )
            timeoutInterval = TIMEOUT;
        initial = true;
    }
    timeout.tv_sec  = timeoutInterval / TIME_US;
    timeout.tv_usec = timeoutInterval % TIME_US;
    window_size     = timeoutInterval / SLEEP_USEC;
}

void clear_sendlist( std::vector< packet_type* >& sendlist, struct ack_type& recvack )
{
    for( int i = 0; i < SEQ_KEEP; i++ )
        if( recvack.seq[ i ] >= 0 )
            for( std::vector< packet_type* >::iterator iter = sendlist.begin(); iter != sendlist.end(); iter++ )
                if( recvack.seq[ i ] == ( *iter )->seq )
                {
                    delete( *iter );
                    sendlist.erase( iter );
                    break;
                }
}

void transfer( int sockfd, SA* pservaddr, socklen_t servlen )
{
    TIMEVAL                     sendtime, recvtime, timecost;
    std::vector< packet_type* > sendlist;
    file_type*                  recvfile = new file_type( 0 );
    ack_type                    recvack;
    FILE*                       fp = fopen( CLIENT_PATH, "rb" );
    unsigned int                fsize = get_fsize( fp ), tsize = 0, window_size = 0;
    TIMEVAL                     timeout;

    timeout.tv_sec  = 0;
    timeout.tv_usec = TIMEOUT;
    setsockopt( sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof( timeout ) );

    printf( "[CLIENT] Transfer begin!!\n" );

    printf( "[CLIENT] File slice: %d\n", HASH( fsize ) );
//set  file header
    printf( "[CLIENT] Header trasfer begin!!\n" );
    while( true )
    {
        gettimeofday( &recvfile->sendtime, NULL );
        recvfile->seq = HASH( fsize );

        sendto( sockfd, recvfile, sizeof( *recvfile ), 0, pservaddr, servlen );
        packetcounter++;

        if( recvfrom( sockfd, &recvack, sizeof( recvack ), 0, NULL, NULL ) > 0 )
        {
            timeout_interval( timeout, recvack.sendtime, window_size );
            setsockopt( sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof( timeout ) );

            clear_sendlist( sendlist, recvack );
            printf( "[CLIENT] Header trasfer done!!\n" );
            break;
        }
        else
            printf( "[CLIENT] Header timeout!!\n" );
    }

//done file header
//start transfer
    printf( "[CLIENT] Packet transfer begin!!\n" );
    while( sendlist.size() != 0 || fsize != tsize )
    {
        while( sendlist.size() != window_size && fsize != tsize )
        {
            char            buffer[ MAXLINE + 1 ];
            unsigned int    psize;
            psize = fread( buffer, 1, MAXLINE, fp );
            sendlist.push_back( new packet_type( tsize, psize, buffer ) );
            tsize += psize;
        }

        for( int i = 0; i < sendlist.size(); i++ )
        {
            gettimeofday( &sendlist[ i ]->sendtime, NULL );

            sendto( sockfd, sendlist[ i ], sizeof( *sendlist[ i ] ), 0, pservaddr, servlen );
            usleep( SLEEP_USEC );
            packetcounter++;
        }


        gettimeofday( &sendtime, NULL );

        while( recvfrom( sockfd, &recvack, sizeof( recvack ), 0, NULL, NULL ) > 0 )
        {
            timeout_interval( timeout, recvack.sendtime, window_size );
            setsockopt( sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof( timeout ) );

            clear_sendlist( sendlist, recvack );
            if( recvack.seq[ 0 ] == FINISH )
            {
                if( sendlist.size() == 0 && fsize == tsize )
                    printf( "[CLIENT] Reveive shoutdown and transfer complete!!\n" );
                else
                    printf( "[CLIENT] Reveive shoutdown and still have packet to transfer!!\n" );

                goto shutdown_window;
            }
        }

        gettimeofday( &recvtime, NULL );

        timecost.tv_sec = recvtime.tv_sec - sendtime.tv_sec;
        timecost.tv_usec = recvtime.tv_usec - sendtime.tv_usec;
        printf( "[CLIENT] EstimatedRTT: %.6f s\n", double ( timeout.tv_sec * TIME_US + timeout.tv_usec ) / TIME_US );
        printf( "[CLIENT] Transfer time: %.6f s\n", double ( timecost.tv_sec * TIME_US + timecost.tv_usec ) / TIME_US );

    }
shutdown_window:
    for( std::vector< packet_type* >::iterator iter = sendlist.begin(); iter != sendlist.end(); iter++ )
        delete( *iter );
    while( sendlist.size() != 0 )
        sendlist.pop_back();
    printf( "[CLIENT] Clear window and ready to shutdown!!\n" );
    printf( "[CLIENT] Packet transfer done!!\n" );
//done transfer
    fclose( fp );
    close( sockfd );

    printf( "[CLIENT] Transfer complete!!\n" );
    printf( "[CLIENT] EstimatedRTT: %.6f s\n", double ( timeout.tv_sec * TIME_US + timeout.tv_usec ) / TIME_US );
    printf( "[CLIENT] Prefer window: %d\n", int ( timeout.tv_sec * TIME_US + timeout.tv_usec ) / SLEEP_USEC );

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

    struct hostent* hen = gethostbyname( argv[ 1 ] );
    bzero( &servaddr, sizeof( servaddr ) );
    servaddr.sin_family = AF_INET;
    servaddr.sin_port   = htons( servport );
    //inet_pton( AF_INET, argv[ 1 ], &servaddr.sin_addr );
    memcpy( &servaddr.sin_addr.s_addr, hen->h_addr_list[ 0 ], hen->h_length );

    sockfd = socket( AF_INET, SOCK_DGRAM, 0 );

    gettimeofday( &sendtime, NULL );

    transfer( sockfd, ( SA* ) &servaddr, sizeof( servaddr ) );

    gettimeofday( &recvtime, NULL );

    timecost.tv_sec = recvtime.tv_sec - sendtime.tv_sec;
    timecost.tv_usec = recvtime.tv_usec - sendtime.tv_usec;
    printf( "[CLIENT] Transfer time: %.6f s\n", double ( timecost.tv_sec * TIME_US + timecost.tv_usec ) / TIME_US );
    printf( "[CLIENT] Transfer packet: %d\n", packetcounter );
    return 0;
}
