//
//  Server.cpp
//  INP-ConcurrentChatRoom
//
//  Created by 陳上奎 on 12/11/7.
//  Copyright (c) 2012年 陳上奎. All rights reserved.
//

#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SA          struct sockaddr
#define MAXLINE     4096

struct client_info
{
    bool                    initial;
    int                     sockfd;
    char                    name[ 13 ];
    struct  sockaddr_in     addr;
};

void sig_chld( int signo )
{
    pid_t pid;
    int stat;

    while( ( pid = waitpid( -1, &stat, WNOHANG ) ) > 0 );
    exit( 0 );
}

void request_name( char recvline[ MAXLINE ], int sockfd, struct client_info* client, int place, int& number )
{
    char servip[ 1024 ];
    char senderrorline1[] = "/serv This name has been used by others.\n";
    char senderrorline2[] = "/serv Username can only consists of 2~12 digits or English letters.\n";
    char sendsuccline1[ MAXLINE ];
    char sendsuccline2[ MAXLINE ];

    sscanf( recvline, "%s", recvline );
//--Server send error msg ( length error )--
    if( strlen( recvline ) < 2 || strlen( recvline ) > 12 )
    {
        write( sockfd, senderrorline2, strlen( senderrorline2 ) );
        return;
    }
    else
//--Server send error msg ( same error )--
        for( int i = 0; i <= number; i++ )
            if( client[ i ].initial && client[ i ].sockfd > 0 && strcmp( client[ i ].name, recvline ) == 0 )
            {
                write( sockfd, senderrorline1, strlen( senderrorline2 ) );
                return;
            }

    strncpy( client[ place ].name, recvline, sizeof( recvline ) );

    struct sockaddr_in  servaddr;
    socklen_t           len = sizeof( servaddr );
    getsockname( sockfd, ( SA* ) &servaddr, &len );

    sprintf( sendsuccline1, "/serv Hello %s, welcome! Server IP: %s:%d\n", client[ place ].name, inet_ntop( AF_INET, &servaddr.sin_addr, servip, sizeof( servip ) ), ntohs( servaddr.sin_port ) );
    sprintf( sendsuccline2, "/serv %s is online.\n", client[ place ].name );

//--Server Send succ to user--
    write( sockfd, sendsuccline1, strlen( sendsuccline1 ) );
//--Server broadcast succ--
    for( int i = 0; i <= number; i++ )
    {
        if( !client[ i ].initial || ( sockfd = client[ i ].sockfd ) < 0 || i == place )
            continue;
        write( sockfd, sendsuccline2, strlen( sendsuccline2 ) );
    }
    client[ place ].initial = true;
}

void change_name( char name[ 13 ], int sockfd, struct client_info* client, int place, int& number )
{
    char senderrorline1[ MAXLINE ];
    char senderrorline2[] = "/serv Username can only consists of 2~12 digits or English letters.\n";
    char sendsuccline1[ MAXLINE ];
    char sendsuccline2[ MAXLINE ];

//--Server send error msg ( length error )--
    if( strlen( name ) < 2 || strlen( name ) > 12 )
    {
        write( sockfd, senderrorline2, strlen( senderrorline2 ) );
        return;
    }
//--Server send error msg ( same error )--
    for( int i = 0; i <= number; i++ )
        if( place != i && client[ i ].initial && client[ i ].sockfd > 0 && strcmp( client[ i ].name, name ) == 0 )
        {
            sprintf( sendsuccline1, "/serv %s has been used by others.\n", name );

            write( sockfd, senderrorline1, strlen( senderrorline2 ) );
            return;
        }

    char tempname[ 13 ];
    strncpy( tempname, client[ place ].name, sizeof( client[ place ].name ) );
    strncpy( client[ place ].name, name, sizeof( name ) );

    sprintf( sendsuccline1, "/serv You're now known as %s.\n", client[ place ].name );
    sprintf( sendsuccline2, "/serv %s is now known as %s.\n", tempname, client[ place ].name );

    //--Server Send succ to user--
    write( sockfd, sendsuccline1, strlen( sendsuccline1 ) );
    //--Server broadcast succ--
    for( int i = 0; i <= number; i++ )
    {
        if( !client[ i ].initial || ( sockfd = client[ i ].sockfd ) < 0 || i == place )
            continue;
        write( sockfd, sendsuccline2, strlen( sendsuccline2 ) );
    }
}

int main(int argc, const char* argv[])
{
    int                 servport;
    int                 maxi, maxfd, listenfd, connfd, sockfd;
    int                 nready;
    client_info         client[ FD_SETSIZE ];
    ssize_t             n;
    fd_set              rset, allset;
    char                buf[ MAXLINE ], tempbuf[ MAXLINE ];
    socklen_t           clilen;
    struct sockaddr_in  cliaddr, servaddr;

    if( argc != 2 )
    {
        printf( "usage: ./Server <Port number>\n" );
        exit( 0 );
    }
    else
        servport = atoi( argv[ 1 ] );

    listenfd = socket( AF_INET, SOCK_STREAM, 0 );

    bzero( &servaddr, sizeof( servaddr ) );
    servaddr.sin_family         = AF_INET;
    servaddr.sin_addr.s_addr    = htonl( INADDR_ANY );
    servaddr.sin_port           = htons( servport );

    bind( listenfd, ( SA* ) &servaddr, sizeof( servaddr ) );

    listen( listenfd, 10 );

    maxfd   = listenfd;
    maxi    = -1;
    for( int i = 0; i < FD_SETSIZE; i++ )
    {
        client[ i ].initial = false;
        client[ i ].sockfd  = -1;
    }
    FD_ZERO( &allset );
    FD_SET( listenfd, &allset );

//--Server Start Listening--
    printf( "Server listen\n" );

    signal( SIGCHLD, sig_chld );

    while( true )
    {
        rset    = allset;
        nready  = select( maxfd + 1, &rset, NULL, NULL, NULL );

        if( FD_ISSET( listenfd, &rset ) )
        {
            clilen  = sizeof( cliaddr );
            connfd  = accept( listenfd, ( SA* ) &cliaddr, &clilen );

//--Client first connect--
            int i;
            for( i = 0; i < FD_SETSIZE; i++ )
                if( client[ i ].sockfd < 0 )
                {
                    client[ i ].sockfd  = connfd;
                    client[ i ].addr    = cliaddr;
                    break;
                }
            if( i == FD_SETSIZE )
                exit( 0 );
            FD_SET( connfd, &allset );
//--Server request username--
            char sendline[] ="/serv What's your name?\n";
            write( connfd, sendline, strlen( sendline ) );
            if( connfd > maxfd )
                maxfd = connfd;
            if( i > maxi )
                maxi = i;

            if( --nready <= 0 )
                continue;
        }

        for( int i = 0; i <= maxi; i++ )
        {
            if( ( sockfd = client[ i ].sockfd ) < 0 )
                continue;
            if( FD_ISSET( sockfd, &rset ) )
            {
                bzero( buf, MAXLINE );
//--Server send Broadcast leave msg--
                if( read( sockfd, buf, MAXLINE ) == 0 )
                {
                    close( sockfd );
                    FD_CLR( sockfd, &allset );
                    client[ i ].initial = false;
                    client[ i ].sockfd  = -1;

                    bzero( tempbuf, MAXLINE );
                    snprintf( tempbuf, sizeof( tempbuf ), "/serv %s is offline.\n", client[ i ].name );

                    int broadcastfd;
                    for( int num = 0; num <= maxi; num++ )
                    {
                        if( !client[ num ].initial || ( broadcastfd = client[ num ].sockfd ) < 0 )
                            continue;
                        write( broadcastfd, tempbuf, sizeof( tempbuf ) );
                    }
                }
                else
                {
//--Server first request username--
                    if( !client[ i ].initial )
                        request_name( buf, sockfd, client, i, maxi );
//--Server change username--
                    else if( strncmp( buf, "/nick", 5 ) == 0 )
                    {
                        char newname[ 13 ];
                        sscanf( buf, "/nick %s", newname );
                        change_name( newname, sockfd, client, i, maxi );
                    }
//--Server send user list--
                    else if( strncmp( buf, "/who", 4 ) == 0 )
                    {
                        for( int num = 0; num <= maxi; num++ )
                        {
                            char clientip[ 1024 ];
                            if( !( client[ num ].initial ) || client[ num ].sockfd < 0 )
                                continue;
                            bzero( tempbuf, MAXLINE );
                            snprintf( tempbuf, sizeof( tempbuf ), "/serv %s %s:%d\n", client[ num ].name, inet_ntop( AF_INET, &client[ num ].addr.sin_addr, clientip, sizeof( clientip ) ), ntohs( client[ num ].addr.sin_port ) );

                            write( sockfd, tempbuf, sizeof( tempbuf ) );
                        }
                    }
//--Server broadcast msg--
                    else
                    {
                        bzero( tempbuf, MAXLINE );
                        snprintf( tempbuf, sizeof( tempbuf ), "/msg %s SAID: %s", client[ i ].name, buf );

                        int broadcastfd;
                        for( int num = 0; num <= maxi; num++ )
                        {
                            if( !client[ num ].initial || ( broadcastfd = client[ num ].sockfd ) < 0 )
                                continue;
                            write( broadcastfd, tempbuf, sizeof( tempbuf ) );
                        }
                    }
                }

                if( --nready <= 0 )
                    break;
            }
        }
    }

    return 0;
}

