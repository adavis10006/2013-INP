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

#define RANDOM_TEST     if( rand() % 100 >= 8 );
#define SA              struct sockaddr
#define TIMEVAL         struct timeval
#define TIMEOUT         40000
#define TIME_US         1000000
#define WINDOW_SIZE     4096
#define SLIDE_SIZE      2048
#define MAXLINE         1479
#define SLEEP_USEC      150
#define SEQ_KEEP        1
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
    unsigned int    seq;                    //sequence
    unsigned int    tsize;                  //data in file
    unsigned int    psize;                  //packet carry data size
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
    }
}__attribute__( ( packed ) );;

struct window_slot
{
    packet_type*    packet;
    slot_type       used;
};

static void sig_alrm( int signo )
{
    return;
}

int HASH( unsigned int number )
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

int time_sub( TIMEVAL A, TIMEVAL B )
{
    return ( A.tv_sec - B.tv_sec ) * TIME_US + ( A.tv_usec - B.tv_usec );
}

void transfer( int sockfd, SA* pservaddr, socklen_t servlen )
{
    std::vector< window_slot >  sendlist( WINDOW_SIZE );
    int                         slot = -1, from = 0;
    file_type*                  recvfile = new file_type( 0 );
    set_type                    recvset;
    ack_type                    recvack;
    FILE*                       fp = fopen( CLIENT_PATH, "rb" );
    unsigned int                fsize = get_fsize( fp ), tsize = 0;
    bool                        timer = false;
    int                         timeout, slide, window;


    for( int i = 0; i < WINDOW_SIZE; i++ )
    {
        sendlist[ i ].packet = NULL;
        sendlist[ i ].used = EMPTY;
    }

    signal( SIGALRM, sig_alrm );
    siginterrupt( SIGALRM, 1 );

    printf( "[CLIENT] Transfer begin!!\n" );

    printf( "[CLIENT] File slice: %d\n", HASH( fsize ) );
//set  file header
    printf( "[CLIENT] Header trasfer begin!!\n" );

    while( true )
    {
        recvfile->fslice = HASH( fsize );
        gettimeofday( &recvfile->sendtime, NULL );

        RANDOM_TEST
        sendto( sockfd, recvfile, sizeof( *recvfile ), 0, pservaddr, servlen );
        packetcounter++;

        ualarm( TIMEOUT, 0 );
        timer = true;

        if( recvfrom( sockfd, &recvset, sizeof( recvset ), 0, NULL, NULL ) > 0 )
        {
            timeout = recvset.RTT * 0.4;
            slide = ( recvset.RTT / SLEEP_USEC ) * 1.5 + 1;
            if( slide > SLIDE_SIZE )
                slide = SLIDE_SIZE;
            window = slide * 2;

            printf( "[CLIENT] RTT: %d Set Timeout: %d Window: %d Slide: %d\n", recvset.RTT, timeout, window, slide );

            ualarm( 0, 0 );
            timer = false;

            printf( "[CLIENT] Header trasfer done!!\n" );

            break;
        }
        else
            printf( "[CLIENT] Header timeout!!\n" );
    }

//done file header
//start transfer
    printf( "[CLIENT] Packet transfer begin!!\n" );

    while( true )
    {
        if( tsize != fsize )
        {
            char            buffer[ MAXLINE + 1 ];
            unsigned int    psize;

            slot = ( slot + 1 ) % window;
            delete( sendlist[ slot ].packet );
            sendlist[ slot ].packet = NULL;

            psize = fread( buffer, 1, MAXLINE, fp );
            sendlist[ slot ].packet = new packet_type( slot, tsize, psize, buffer );
            sendlist[ slot ].used   = QUEUE;
            tsize += psize;

            if( !timer )
            {
                ualarm( timeout, 0 );
                timer = true;
            }

            RANDOM_TEST
//printf("[normal] send packet %6d from: %6d slot: %6d\n", sendlist[ slot ].packet->seq, from, slot );
            sendto( sockfd, sendlist[ slot ].packet, sizeof( *sendlist[ slot ].packet ), 0, pservaddr, servlen );
            usleep( SLEEP_USEC );
            packetcounter++;
        }

        if( tsize != fsize && slot != ( from + slide - 1 ) % window )
                continue;
//tsize == fsize || slot == ( from + slide - 1 ) % window
        else
            while( true )
                if( recvfrom( sockfd, &recvack, sizeof( recvack ), 0, NULL, NULL ) > 0 )
                {
                    if( recvack.seq == FINISH )
                    {
                        printf( "[CLIENT] Transfer complete and try to shutdown!!\n" );

                        goto shutdown;
                    }

                    if( sendlist[ recvack.seq ].used == QUEUE && sendlist[ recvack.seq ].packet->tsize == recvack.tsize )
                    {
                        ualarm( 0, 0 );
                        ualarm( timeout, 0 );
                        sendlist[ recvack.seq ].used = USED;

                        if( recvack.seq == from )
                        {
                            while( sendlist[ from ].used == USED )
                            {
                                sendlist[ from ].used = EMPTY;
                                if( from == slot )
                                {
                                    from = ( from + 1 ) % window;
                                    ualarm( 0, 0 );
                                    timer = false;

                                    break;
                                }
                                else
                                    from = ( from + 1 ) % window;
                            }

                            break;
                        }

                        if( ( recvack.wfrom - from > 0 && recvack.wfrom - from < slide ) || ( recvack.wfrom + window - from  > 0 && recvack.wfrom + window - from < slide ) )
                            while( from != recvack.wfrom )
                            {
                                sendlist[ from ].used = EMPTY;
                                if( from == slot )
                                {
                                    from = ( from + 1 ) % window;
                                    ualarm( 0, 0 );
                                    timer = false;

                                    break;
                                }
                                else
                                    from = ( from + 1 ) % window;
                            }
                    }
                }
                else
                {
                    ualarm( 0, 0 );
                    ualarm( timeout, 0 );

                    for( int i = from; i != ( slot + 1 ) % window; i = ( i + 1 ) % window )
                    {
                        if( sendlist[ i ].used == USED )
                            continue;

                        RANDOM_TEST
//printf("[resend] send packet %6d from: %6d slot: %6d\n", sendlist[ i ].packet->seq, from, slot );
                        sendto( sockfd, sendlist[ i ].packet, sizeof( *sendlist[ i ].packet ), 0, pservaddr, servlen );
                        usleep( SLEEP_USEC );
                        packetcounter++;
                    }
                }
    }
shutdown:
    printf( "[CLIENT] Packet transfer done!!\n" );
//done transfer
    delete( recvfile );

    for( int i = 0; i < WINDOW_SIZE; i++ )
        delete( sendlist[ i ].packet );

    while( sendlist.size() != 0 )
        sendlist.pop_back();

    fclose( fp );
    close( sockfd );

    printf( "[CLIENT] Transfer complete!!\n" );
}

int main( int argc, char** argv )
{
    srand( time( NULL ) );
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
