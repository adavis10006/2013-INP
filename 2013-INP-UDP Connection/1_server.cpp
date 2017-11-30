/*
// 9817024 Davis
// UDP Server

 protocol

 1 client->mesg
 2 server->ack
 3 client->get ack
 4 client->next mesg

 2 server->timeout->resend
 3 client->get resend
 4 client->mesg

 2 server->timeout or ack
 3 client->timeout
 4 client->mesg

 therefore need packet sequence in ack
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

#define SA          struct sockaddr
#define TIMEVAL     struct timeval
#define MAXLINE     1400
#define SERV_PORT   5566
#define SERVER_PATH "/Users/Davis/received.tar"
#define CLIENT_PATH "/tmp/input.tar"

struct ack_type
{
    int sequence;
};

struct packet_type
{
    int     sequence;
    int     fragment;
    size_t  size;
    char    recvline[ MAXLINE + 1 ];
};

static void sig_alrm( int signo )
{
    return;
}

void zero_ack( ack_type& ack )
{
    ack.sequence = 0;
}

void dg_echo( int sockfd, SA* pcliaddr, socklen_t clilen )
{
    int         sequence = 1;
    socklen_t   len;
    packet_type recvpacket;
    ack_type    sendack;
    FILE*       fp = fopen( SERVER_PATH, "wb" );

    printf( "receive begin\n" );

    signal( SIGALRM, sig_alrm );
    siginterrupt( SIGALRM, 1 );

    while( true )
    {
        zero_ack( sendack );

        len = clilen;

        alarm( 1 );

        if( recvfrom( sockfd, &recvpacket, sizeof( recvpacket ), 0, pcliaddr, &len ) < 0 )
        {
            printf( "server socket timeout\n" );
            sendack.sequence = sequence - 1;
            //printf( "server sendack : %d\n", sendack.sequence );
            sendto( sockfd, &sendack, sizeof( sendack ), 0, pcliaddr, clilen );
        }
        else
        {
            alarm( 0 );

            //printf( "server recvmesg: %d %d %d\n", recvpacket.sequence, recvpacket.fragment, recvpacket.size );
            if( recvpacket.sequence == sequence )
            {
                fwrite( recvpacket.recvline, 1, recvpacket.size, fp );

                sendack.sequence = sequence++;
            }
            else
                sendack.sequence = sequence - 1;

            //printf( "server sendack : %d\n", sendack.sequence );
            sendto( sockfd, &sendack, sizeof( sendack ), 0, pcliaddr, clilen );

            if( recvpacket.fragment == 0 )
                break;
        }
    }

    printf( "receive over\n" );
}

int main( int argc, char** argv )
{
    int                 sockfd, servport;
    struct sockaddr_in  servaddr, cliaddr;

    if( argc > 1 )
        servport = atoi( argv[1] );
    else
        servport = SERV_PORT;

    sockfd = socket( AF_INET, SOCK_DGRAM, 0 );
    bzero( &servaddr, sizeof( servaddr ) );
    servaddr.sin_family         = AF_INET;
    servaddr.sin_addr.s_addr    = htonl( INADDR_ANY );
    servaddr.sin_port           = htons( servport );

    bind( sockfd, ( SA* ) &servaddr, sizeof( servaddr ) );

    dg_echo( sockfd, ( SA* ) &cliaddr, sizeof( cliaddr ) );

    return 0;
}



