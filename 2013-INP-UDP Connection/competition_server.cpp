/*
// 9817024 Davis
// UDP Server
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

#define SA          struct sockaddr
#define TIMEVAL     struct timeval
#define TIMEOUT     500000
#define TIME_US     1000000
#define WINDOW_SIZE 700
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

int HASH( unsigned int number )
{
    return number / MAXLINE + ( number % MAXLINE != 0 );
}

void receive( int sockfd, SA* pcliaddr, socklen_t clilen )
{
    bool*           hash_table;
    int             n;
    file_type*      recvfile = new file_type( 0 );
    packet_type*    recvpacket = new packet_type( 0, 0, new char[ MAXLINE + 1 ] );
    ack_type        sendack;
    FILE*           fp = fopen( SERVER_PATH, "wb" );
    TIMEVAL         timeout;

    timeout.tv_sec  = 0;
    timeout.tv_usec = TIMEOUT;
    setsockopt( sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof( timeout ) );

    printf( "[SERVER] Receive begin!!\n" );
//set file header
    printf( "[SERVER] Header wait!!\n" );
    while( true )
    {
        if( recvfrom( sockfd, recvfile, sizeof( recvfile ), 0, pcliaddr, &clilen ) > 0 )
        {
            hash_table = new bool[ recvfile->seq ];
            for( int i = 0; i < recvfile->seq; i++ )
                hash_table[ i ] = false;

            sendack.seq[ 0 ]        = HEADER;
            sendack.sendtime        = recvfile->sendtime;

            sendto( sockfd, &sendack, sizeof( sendack ), 0, pcliaddr, clilen );
            packetcounter++;

            printf( "[SERVER] Header receive!!\n" );
            break;
        }
        else
            printf( "[SERVER] Header timeout!!\n" );
    }
//done file header
//start receive
    printf( "[SERVER] Packet wait!!\n" );
    while( true )
    {
wait_next_packet:
        if( ( n = recvfrom( sockfd, recvpacket, sizeof( packet_type ), 0, pcliaddr, &clilen ) ) > 0 )
        {
            if( n != sizeof( packet_type ) )
            {
                sendto( sockfd, &sendack, sizeof( sendack ), 0, pcliaddr, clilen );
                packetcounter++;
                continue;
            }

            fseek( fp, recvpacket->seq, SEEK_SET );
            fwrite( recvpacket->data, 1, recvpacket->psize, fp );
            hash_table[ HASH( recvpacket->seq ) ] = true;

            sendack.seq[ 0 ]        = recvpacket->seq;
            for( int i = 1; i < SEQ_KEEP; i++ )
                sendack.seq[ i ]    = sendack.seq[ i - 1 ];
            sendack.sendtime        = recvpacket->sendtime;

            sendto( sockfd, &sendack, sizeof( sendack ), 0, pcliaddr, clilen );
            packetcounter++;
        }
        //else
            //printf( "[SERVER] Packet timeout!!\n" );

        for( int i = 0; i < recvfile->seq; i++ )
            if ( !hash_table[ i ] )
                goto wait_next_packet;
        break;
    }
//done receive
//start final check( tell client not to send packet if timeout then think client is shutdown )
    printf( "[SERVER] Final packet transfer begin!!\n" );
    while( true )
    {
        for( int i = SEQ_KEEP - 1; i > 0; i-- )
            sendack.seq[ i ]    = sendack.seq[ i - 1 ];
        sendack.seq[ 0 ]        = FINISH;
        sendack.sendtime        = recvpacket->sendtime;

        sendto( sockfd, &sendack, sizeof( sendack ), 0, pcliaddr, clilen );
        packetcounter++;

        if( recvfrom( sockfd, recvpacket, sizeof( packet_type ), 0, pcliaddr, &clilen ) <= 0 )
            break;
    }
    printf( "[SERVER] Final packet transfer done!!\n" );
//done final check
    delete( recvpacket );
    delete( recvfile );
    delete( hash_table );
    fclose( fp );

    printf( "[SERVER] Receive complete!!\n" );
}

int main( int argc, char** argv )
{
    int                 sockfd, servport;
    struct sockaddr_in  servaddr, cliaddr;
    TIMEVAL             sendtime, recvtime, timecost;

    if( argc > 1 )
        servport = atoi( argv[1] );
    else
        servport = SERV_PORT;

    bzero( &servaddr, sizeof( servaddr ) );
    servaddr.sin_family         = AF_INET;
    servaddr.sin_addr.s_addr    = htonl( INADDR_ANY );
    servaddr.sin_port           = htons( servport );

    sockfd = socket( AF_INET, SOCK_DGRAM, 0 );

    bind( sockfd, ( SA* ) &servaddr, sizeof( servaddr ) );

    gettimeofday( &sendtime, NULL );

    receive( sockfd, ( SA* ) &cliaddr, sizeof( cliaddr ) );

    gettimeofday( &recvtime, NULL );

    timecost.tv_sec = recvtime.tv_sec - sendtime.tv_sec;
    timecost.tv_usec = recvtime.tv_usec - sendtime.tv_usec;
    printf( "[SERVER] Transfer time: %.6f s\n", double ( timecost.tv_sec * TIME_US + timecost.tv_usec ) / TIME_US );
    printf( "[SERVER] Transfer packet: %d\n", packetcounter );
    return 0;
}
