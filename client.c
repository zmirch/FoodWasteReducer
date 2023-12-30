#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>


extern int errno;

int port; // the port we will use to connect to the server

int main(int argc, char *argv[])
{
  int sd;
  struct sockaddr_in server;	// structure used for connect
  char inputBuff[300];

  // make sure syntax is respected
  if (argc != 3)
  {
    printf ("[client] Syntax: %s <server address> <port>\n", argv[0]);
    return -1;
  }

  port = atoi (argv[2]);

  if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
  {
    perror ("[client] Error at socket().\n");
    return errno;
  }

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(argv[1]);
  server.sin_port = htons (port);

  if (connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
  {
    perror ("[client] Error at connect().\n");
    return errno;
  }

    
  while(1)
  {
    // handle input
    printf ("\n[client] Input: ");
    fflush (stdout);

    inputBuff[0] = '\0';
    int commLength = 0;
    commLength = read(0, inputBuff, sizeof(inputBuff));

    inputBuff[commLength - 1] = '\0';

    // if input is "quit", close the client
    if (strcmp(inputBuff, "quit") == 0) 
    {
      write (sd, inputBuff, sizeof(inputBuff));
      break;
    }

    // send message to server
    if (write (sd, inputBuff, sizeof(inputBuff)) <= 0)
    {
      perror ("\n[client] Error at write() to server.\n");
      return errno;
    }

    // read server's response
    if (read (sd, inputBuff,sizeof(inputBuff)) < 0)
    {
      perror ("\n[client] Error at read() from server.\n");
      return errno;
    }

    //print server's response
    printf ("\n[server] ");
    printf(inputBuff);
    printf("\n");
    strcpy(inputBuff, "");
  }

  close (sd);

}


