//
// Server.cpp
// 9817024 Davis
//

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
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
#define SOCKET_SET  struct socket_buffer
#define TIMEVAL     struct timeval
#define MAXLINE     1024
#define MAXBUF      2000000
#define MAXCLIENT   3
#define SERVPORT    5566

struct socket_buffer
{
    int     sockfd;
    char    buffer[ MAXBUF + 1 ];
    char    *input, *output;
};

int main( int argc, char* argv[] )
{
    int                 portnum, sockfd, connfd;
    socklen_t           clilen;
    struct sockaddr_in  cliaddr, servaddr;

    if( argc == 2 )
        portnum = atoi( argv[ 1 ] );
    else
        portnum = SERVPORT;

   if( ( sockfd = socket( AF_INET, SOCK_STREAM, 0 ) ) < 0 )
        printf( "socket create error!!!\n" );

    memset( &servaddr, 0, sizeof( servaddr ) );
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl( INADDR_ANY );
    servaddr.sin_port = htons( portnum );

    if( bind( sockfd, ( SA * ) &servaddr, sizeof( servaddr ) ) < 0 )
        printf( "bind create error!!!\n" );

    if( listen( sockfd, MAXCLIENT ) < 0 )
        printf( "listen create error!!!\n" );

    int         val, maxi, maxfd, nready;
    ssize_t     n, nread, nwrite;
    fd_set      rset, wset, allset;
    SOCKET_SET  client[ MAXCLIENT ];

    val = fcntl( sockfd, F_GETFL, 0 );
    fcntl( sockfd, F_SETFL, val | O_NONBLOCK );

    FD_ZERO( &allset );
    FD_SET( sockfd, &allset );
    maxfd = sockfd;
    maxi = -1;
    for( int i = 0; i < MAXCLIENT; i++ )
        client[ i ].sockfd = -1;

    printf( "server start!!!\n" );

    while( true )
    {
        FD_ZERO( &rset );
        FD_ZERO( &wset );
        FD_SET( sockfd, &rset );
        for( int i = 0; i <= maxi; i++ )
        {
            if( client[ i ].sockfd > 0 )
            {
                if( client[ i ].output < &client[ i ].buffer[ MAXBUF ] )
                    FD_SET( client[ i ].sockfd, &rset );
                if( client[ i ].input != client[ i ].output )
                    FD_SET( client[ i ].sockfd, &wset );
            }
        }

        nready = select( maxfd + 1, &rset, &wset, NULL, NULL );

        if( FD_ISSET( sockfd, &rset ) )
        {
            clilen = sizeof( cliaddr ) ;
            connfd = accept( sockfd, ( SA* ) &cliaddr , &clilen );

            printf( "new connect in fd:%d!!!\n", connfd );

            int check;
            for( check = 0; check < MAXCLIENT; check++ )
            {
                if( client[ check ].sockfd < 0 )
                {
                    val = fcntl( connfd, F_GETFL, 0 );
                    fcntl( connfd, F_SETFL, val | O_NONBLOCK );

                    client[ check ].sockfd = connfd;
                    bzero( client[ check ].buffer, MAXBUF + 1 );
                    client[ check ].input = client[ check ].output = client[ check ].buffer;
                    break;
                }
            }

            if( check == MAXCLIENT )
                printf( "too many client!!!\n" );
            FD_SET( connfd, &allset );
            if( connfd > maxfd )
                maxfd = connfd;
            if( check > maxi )
                maxi = check;

            if( --nready <= 0 )
                continue;
        }

        for( int i = 0; i <= maxi; i++ )
        {
            if( ( connfd = client[ i ].sockfd ) < 0 )
                continue;

            if( FD_ISSET( connfd, &rset ) )
            {
                if( ( nread = read( connfd, client[ i ].output, &client[ i ].buffer[ MAXBUF ] - client[ i ].output ) ) < 0 )
                {
                    if( errno != EWOULDBLOCK )
                        printf( "read error socket!\n" );
                }
                else if( nread == 0 )
                {
                    close( connfd );
                    FD_CLR( connfd, &allset );
                    FD_CLR( connfd, &rset );
                    FD_CLR( connfd, &wset );
                    client[ i ].sockfd = -1;
                }
                else
                {
                    printf("read: %d from: %d\n", nread, i );

                    for( int j = 0; j <= maxi; j++ )
                    {
                        if( client[ j ].sockfd != -1 )
                        {
                            if( i != j )
                            {
                                memcpy( client[ j ].output, client[ i ].output, int( nread ) );
                                client[ j ].output += int( nread );
                                printf( "read: %d %d %d\n", j, client[ j ].output - client[ j ].buffer, &client[ j ].buffer[ MAXBUF ] - client[ j ].buffer );

                            }
                        }
                    }
                    client[ i ].output += int( nread );
                    printf( "read: %d %d %d\n", i, client[ i ].output - client[ i ].buffer, &client[ i ].buffer[ MAXBUF ] - client[ i ].buffer );
                }
                if( --nready <= 0 )
                    break;
            }

            if( FD_ISSET( connfd, &wset ) && ( ( n = client[ i ].output - client[ i ].input ) > 0 ) )
            {
                printf("write: %d to: %d\n", n, i );
                if( ( nwrite = write( connfd, client[ i ].input, n ) ) < 0 )
                {
                    if( errno != EWOULDBLOCK )
                        printf( "write error socket!\n" );
                }
                else
                {
                    client[ i ].input += int( nwrite );
                    printf( "write: %d %d %d\n", i, client[ i ].output - client[ i ].buffer, &client[ i ].buffer[ MAXBUF ] - client[ i ].buffer );
                    if( client[ i ].output == client[ i ].input )
                    {
                        bzero( client[ i ].buffer, MAXBUF + 1 );
                        client[ i ].input = client[ i ].output = client[ i ].buffer;
                    }
                    printf( "write after: %d %d %d\n", i, client[ i ].output - client[ i ].buffer, &client[ i ].buffer[ MAXBUF ] - client[ i ].buffer );
                }
                if( --nready <= 0 )
                    break;
            }
        }
    }

    return 0;
}

