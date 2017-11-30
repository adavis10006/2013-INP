//
//  Client.cpp
//  INP-ConcurrentChatRoom
//
//  Created by 陳上奎 on 12/11/7.
//  Copyright (c) 2012年 陳上奎. All rights reserved.
//

#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
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

bool quit = false;

void wait_connect( char servip[ 1024 ], int& servport, int& sockfd, struct sockaddr_in& servaddr, struct hostent* hen );
void try_connect( char servip[ 1024 ], int& servport, int& sockfd, struct sockaddr_in& servaddr, struct hostent* hen );


void sig_chld( int signo )
{
    pid_t pid;
    int stat;

    while( ( pid = waitpid( -1, &stat, WNOHANG ) ) > 0 );
    exit( 0 );
}

void wait_connect( char servip[ 1024 ], int& servport, int& sockfd, struct sockaddr_in& servaddr, struct hostent* hen )
{
    printf( "It is not connected to any server\n" );
    printf( "Please use /connect <IP address> <Port number>\n");

    char buf[ MAXLINE ];
    while( fgets( buf, MAXLINE, stdin ) != NULL )
    {
        if( strcmp( buf, "/quit\n" ) == 0 )
        {
            close( sockfd );
            exit( 0 );
        }

        if( sscanf( buf, "/connect %s %d", servip, &servport ) == 2 )
        {
            try_connect( servip, servport, sockfd, servaddr, hen );
            break;
        }
    }
}

void try_connect( char servip[ 1024 ], int& servport, int& sockfd, struct sockaddr_in& servaddr, struct hostent* hen )
{
    hen = gethostbyname( servip );
    if( !hen )
        wait_connect( servip, servport, sockfd, servaddr, hen );

    sockfd = socket( AF_INET, SOCK_STREAM, 0 );

    bzero( &servaddr, sizeof( servaddr ) );
    servaddr.sin_family         = AF_INET;
    servaddr.sin_port           = htons( servport );
    memcpy( &servaddr.sin_addr.s_addr, hen->h_addr_list[ 0 ], hen->h_length );

    if( connect( sockfd, ( SA* ) &servaddr, sizeof( servaddr ) ) < 0 )
        wait_connect( servip, servport, sockfd, servaddr, hen );
}

void send_msg( FILE* fp, int sockfd )
{
    char sendline[ MAXLINE ];
    while( fgets( sendline, MAXLINE, fp ) != NULL )
    {
        if( strcmp( sendline, "/quit\n" ) == 0 )
        {
            quit = true;
            exit( 0 );
        }

        write( sockfd, sendline, strlen( sendline ) );
        bzero( sendline, MAXLINE );
    }
}

void recv_msg( int sockfd )
{
    char recvline[ MAXLINE + 1 ];
    while( !quit )
        if( read( sockfd, recvline, MAXLINE ) > 0 )
        {
            /*
            if( strncmp( recvline, "/serv", 5 ) == 0 )
                printf( "[Server] %s", recvline + 6 );
            else if( strncmp( recvline, "/msg", 4 ) == 0 )
                printf( "%s", recvline + 5 );
            */
            printf( "%s", recvline );
            bzero( recvline, MAXLINE + 1 );
        }
}

int main(int argc, const char* argv[])
{
    char                servip[ 1024 ];
    int                 servport;

    int                 sockfd;
    struct sockaddr_in  servaddr;
    struct hostent*     hen;

    if( argc == 3 )
    {
        strcpy( servip, argv[ 1 ] );
        servport    = atoi( argv[ 2 ] );
        try_connect( servip, servport, sockfd, servaddr, hen );
    }
    else
        wait_connect( servip, servport, sockfd, servaddr, hen );

    signal( SIGCHLD, sig_chld );

    if( fork() == 0 )
    {
        send_msg( stdin, sockfd );
        close( sockfd );
    }
    else
    {
        recv_msg( sockfd );
        close( sockfd );
    }

    while( !quit );
    return 0;
}

