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

#define RANDOM_TEST     if( rand() % 100 >= 8 );
#define SA              struct sockaddr
#define TIMEVAL         struct timeval
#define TIMEOUT         40000
#define TIME_US         1000000
#define WINDOW_SIZE     4096
#define SLIDE_SIZE      2048
#define MAXLINE         1479
#define SLEEP_USEC      150
#define SEQ_KEEP        5
#define SERV_PORT       5566
#define SERVER_PATH     "/tmp/output_file"
#define CLIENT_PATH     "/tmp/a_big_file"

int packetcounter = 0;

enum seq_type
{
    NONE    = -3,
    HEADER  = -2,
    FINISH  = -1,
    NORMAL
};

enum slot_type
{
    EMPTY,
    QUEUE,
    USED
};

struct file_type
{
    unsigned int    fslice;             //file slice size
    TIMEVAL         sendtime;           //packet create time

    file_type( unsigned int fslice ) : fslice( fslice ) {}
}__attribute__( ( packed ) );;

struct set_type
{
    unsigned int    seq;        //sequence
    int             RTT;        //round time trip

    set_type()
    {
        seq = NONE;
    }
}__attribute__( ( packed ) );;

struct packet_type
{
    unsigned int    seq;                //sequence
    unsigned int    tsize;              //data in file
    unsigned int    psize;              //packet carry data size
    char            data[ MAXLINE + 1 ];    //packet carry data

    packet_type( unsigned int seq, unsigned int tsize, unsigned int psize, char data_in[ MAXLINE + 1 ] ) :
    seq( seq ), tsize( tsize ), psize( psize )
    {
        memcpy( data, data_in, psize );
    }
}__attribute__( ( packed ) );;

struct ack_type
{
    unsigned int    seq;    //sequence
    unsigned int    tsize;  //data in file
    unsigned int    wfrom;  //window from

    ack_type()
    {
        seq = NONE;
        tsize = -1;
        wfrom = 0;
    }
}__attribute__( ( packed ) );;


struct window_slot
{
    packet_type*    packet;
    slot_type       used;
};

int HASH( unsigned int number )
{
    return number / MAXLINE + ( number % MAXLINE != 0 );
}

int time_sub( TIMEVAL A, TIMEVAL B )
{
    return ( A.tv_sec - B.tv_sec ) * TIME_US + ( A.tv_usec - B.tv_usec );
}

static void sig_alrm( int signo )
{
    return;
}

void receive( int sockfd, SA* pcliaddr, socklen_t clilen )
{
    bool*                       hash_table;
    int                         n;
    file_type*                  recvfile = new file_type( 0 );
    set_type                    sendset;
    std::vector< window_slot >  recvlist( WINDOW_SIZE );
    int                         from = 0;
    packet_type*                recvpacket = new packet_type( NONE, 0, 0, new char[ MAXLINE + 1 ] );
    ack_type                    sendack;
    FILE*                       fp = fopen( SERVER_PATH, "wb" );
    int                         timeout, window, slide;

    for( int i = 0; i < WINDOW_SIZE; i++ )
    {
        recvlist[ i ].packet   = NULL;
        recvlist[ i ].used     = EMPTY;
    }

    signal( SIGALRM, sig_alrm );
    siginterrupt( SIGALRM, 1 );

    printf( "[SERVER] Receive begin!!\n" );
//set file header
    printf( "[SERVER] Header wait!!\n" );
    while( true )
    {
        if( recvfrom( sockfd, recvfile, sizeof( file_type ), 0, pcliaddr, &clilen ) > 0 )
        {
            TIMEVAL recvtime;

            gettimeofday( &recvtime, NULL );

            hash_table = new bool[ recvfile->fslice ];

            for( int i = 0; i < recvfile->fslice; i++ )
                hash_table[ i ] = false;

            sendset.seq     = HEADER;
            sendset.RTT     = 2 * time_sub( recvtime, recvfile->sendtime );

            timeout = sendset.RTT * 0.4;
            slide = ( sendset.RTT / SLEEP_USEC ) * 1.5 + 1;
            if( slide > SLIDE_SIZE )
                slide = SLIDE_SIZE;
            window = slide * 2;

            printf( "[SERVER] RTT: %d Set Timeout: %d Window: %d Slide: %d\n", sendset.RTT, timeout, window, slide );

            RANDOM_TEST
            sendto( sockfd, &sendset, sizeof( sendset ), 0, pcliaddr, clilen );
            packetcounter++;

            printf( "[SERVER] Header receive!!\n" );
            break;
        }
    }
//done file header
//start receive
    printf( "[SERVER] Packet wait!!\n" );

    for( int i = from; i < ( from + slide ) % window; i++ )
        recvlist[ i ].used = QUEUE;

    while( true )
    {
wait_next_packet:
        if( ( n = recvfrom( sockfd, recvpacket, sizeof( packet_type ), 0, pcliaddr, &clilen ) ) > 0 )
        {
            if( n != sizeof( packet_type ) )
            {
                RANDOM_TEST
                sendto( sockfd, &sendset, sizeof( sendset ), 0, pcliaddr, clilen );
                packetcounter++;

                continue;
            }

            if( recvlist[ recvpacket->seq ].used == EMPTY )
            {
                sendack.seq     = recvpacket->seq;
                sendack.tsize   = recvpacket->tsize;
                sendack.wfrom   = from;

                RANDOM_TEST
//printf("[outsld] send ack %6d %6d %6d\n", sendack.seq, from, ( from + SLIDE_SIZE - 1 ) % WINDOW_SIZE );
                sendto( sockfd, &sendack, sizeof( sendack ), 0, pcliaddr, clilen );
                packetcounter++;
            }

            if( recvlist[ recvpacket->seq ].used == QUEUE )
            {
                recvlist[ recvpacket->seq ].packet  = new packet_type( recvpacket->seq, recvpacket->tsize, recvpacket->psize, recvpacket->data );
                recvlist[ recvpacket->seq ].used    = USED;

                if( recvpacket->seq == from )
                    while( recvlist[ from ].used == USED )
                    {
                        //fseek( fp, recvpacket->tsize, SEEK_SET );
                        fwrite( recvlist[ from ].packet->data, 1, recvlist[ from ].packet->psize, fp );
                        hash_table[ HASH( recvlist[ from ].packet->tsize ) ] = true;

                        delete( recvlist[ from ].packet );
                        recvlist[ from ].packet = NULL;
                        recvlist[ from ].used   = EMPTY;

                        from = ( from + 1 ) % window;
                        recvlist[ ( from + slide - 1 ) % window ].used = QUEUE;
                    }

                sendack.seq     = recvpacket->seq;
                sendack.tsize   = recvpacket->tsize;
                sendack.wfrom   = from;

                RANDOM_TEST
//printf("[normal] send ack %6d %6d %6d\n", sendack.seq, from, ( from + SLIDE_SIZE - 1 ) % WINDOW_SIZE );
                sendto( sockfd, &sendack, sizeof( sendack ), 0, pcliaddr, clilen );
                packetcounter++;
            }
        }

        for( int i = 0; i < recvfile->fslice; i++ )
            if ( !hash_table[ i ] )
                goto wait_next_packet;

        break;
    }
//done receive
//start final check( tell client not to send packet if timeout then think client is shutdown )
    printf( "[SERVER] Final packet transfer begin!!\n" );
    while( true )
    {
        sendack.seq = FINISH;

        ualarm( timeout, 0 );
        RANDOM_TEST
        sendto( sockfd, &sendack, sizeof( sendack ), 0, pcliaddr, clilen );
        packetcounter++;

        if( recvfrom( sockfd, recvpacket, sizeof( packet_type ), 0, pcliaddr, &clilen ) <= 0 )
            break;
        else
            ualarm( 0, 0 );
    }

    printf( "[SERVER] Final packet transfer done!!\n" );
    printf( "[SERVER] Receive complete!!\n" );

//done final check
    printf( "[SERVER] Start clean packet memory!!\n" );

    delete( recvpacket );
    delete( recvfile );
    delete( hash_table );

    printf( "[SERVER] Start clean window memory!!\n" );

    for( int i = 0; i < WINDOW_SIZE; i++ )
        delete( recvlist[ i ].packet );

    printf( "[SERVER] Start close file!!\n" );

    fclose( fp );
}

int main( int argc, char** argv )
{
    srand( time( NULL ) );
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
