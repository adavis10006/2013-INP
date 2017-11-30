/*
// 9817024 Davis
// UDP Client

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
    int     sequence;
};

struct packet_type
{
    int     sequence;
    int     fragment;
    size_t  size;
    char    sendline[ MAXLINE + 1 ];
};

static void sig_alrm( int signo )
{
    return;
}

void zero_packet( packet_type& packet )
{
    packet.sequence = 0;
    packet.fragment = 0;
    bzero( packet.sendline, sizeof( packet.sendline ) );
}

long get_data_size( FILE* fp )
{
    long fsize;

    fseek( fp, 0, SEEK_END );
    fsize = ftell( fp );

    rewind( fp );

    return fsize;
}

void dg_cli( int sockfd, const SA* pservaddr, socklen_t servlen )
{
    int         size, sequence = 0, timeout = 0;
    packet_type sendpacket;
    ack_type    recvack;
    bool        resend = false;
    FILE*       fp = fopen( CLIENT_PATH, "rb" );
    long        remainsize = get_data_size( fp );

    printf( "transfer begin\n" );

    signal( SIGALRM, sig_alrm );
    siginterrupt( SIGALRM, 1 );

    while( true )
    {
        if( !resend )
            if( remainsize == 0 )
                break;
            else
            {
                zero_packet( sendpacket );

                size = fread( sendpacket.sendline, 1, MAXLINE, fp );
                remainsize -= size;
                sendpacket.sequence = ++sequence;
                sendpacket.fragment = remainsize > 0 ? 1 : 0;
                sendpacket.size     = size;

            }
        else
            if( remainsize == 0 )
            {
                timeout++;
                if( timeout > 10 )
                    break;
            }

        //printf( "client sendmesg: %d %d\n", sequence, size );
        sendto( sockfd, &sendpacket, sizeof( sendpacket ), 0, pservaddr, servlen );

        alarm( 1 );

        if ( recvfrom( sockfd, &recvack, sizeof( recvack ), 0, NULL, NULL ) < 0 )
        {
            resend = true;
            printf( "client socket timeout\n" );
        }
        else
        {
            alarm( 0 );

            //printf( "client recvack : %d\n", recvack.sequence );
            if( recvack.sequence == sequence )
            {
                resend = false;
                if( remainsize == 0 )
                    break;
            }
            else
                resend = true;
        }
    }

    printf( "transfer done\n" );

}

int main( int argc, char** argv )
{
    int                 sockfd, servport;
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

    dg_cli( sockfd, ( SA* ) &servaddr, sizeof( servaddr ) );

    gettimeofday( &recvtime, NULL );

    timecost.tv_sec = recvtime.tv_sec - sendtime.tv_sec;
    timecost.tv_usec = recvtime.tv_usec - sendtime.tv_usec;
    printf( "transfer time: %.3f s\n", double ( timecost.tv_sec * 1000000 + timecost.tv_usec ) / 1000000 );

    return 0;
}

