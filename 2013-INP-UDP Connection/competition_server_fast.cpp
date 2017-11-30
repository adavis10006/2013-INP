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
#define WINDOW_SIZE 1000
#define MAXLINE     1468
#define SLEEP_USEC  130
#define SERV_PORT   5566
#define SERVER_PATH "/home/estinet/9817024/received.tar"
#define CLIENT_PATH "/tmp/input.tar"

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

void receive( int sockfd, SA* pcliaddr, socklen_t clilen )
{
    int             timeout_try = 0;
    bool            hash_table;
    unsigned int    fsize = 0;
    packet_type*    recvpacket = new packet_type( 0, 0, 0, new char[ MAXLINE + 1 ] );
    ack_type        sendack;
    FILE*           fp = fopen( SERVER_PATH, "wb" );
    TIMEVAL         timeout;

    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;
    setsockopt( sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof( timeout ) );

    printf( "Receive begin!!\n" );

    while( true )
    {
        if( recvfrom( sockfd, recvpacket, sizeof( packet_type ), 0, pcliaddr, &clilen ) > 0 )
        {
            //printf( "recv seq:%10d size:%10d\n", recvpacket->seq, recvpacket->psize );
            timeout_try = 0;
            fseek( fp, recvpacket->seq, SEEK_SET );
            fwrite( recvpacket->data, 1, recvpacket->psize, fp );

            sendack.seq         = recvpacket->seq;
            sendack.sendtime    = recvpacket->sendtime;
            sendto( sockfd, &sendack, sizeof( sendack ), 0, pcliaddr, clilen );
            usleep( SLEEP_USEC );
        }
        else
        {
            printf( "TIMEOUT!!\n" );
            timeout_try++;
        }

        if( timeout_try > 10 )
            break;
    }

    delete( recvpacket );
    fclose( fp );

    printf( "Receive complete!!\n" );
}

int main( int argc, char** argv )
{
    int                 sockfd, servport;
    struct sockaddr_in  servaddr, cliaddr;

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

    receive( sockfd, ( SA* ) &cliaddr, sizeof( cliaddr ) );

    return 0;
}
