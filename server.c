
//To compile: gcc server.c -o server -lsqlite3


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <sqlite3.h>


#define PORT 2911 // port used by server

extern int errno;

typedef struct thData
{
  int idThread; 
  int cl;       // descriptor returned by accept()
} thData;

static void *treat(void *);
void respond(void *);

// function that finds the amount of the product in the database
int CheckAmountCallback(void *NotUsed, int argc, char **argv, char **azColName) 
{
  // this function will be called for each row in the result set
  for (int i = 0; i < argc; i++)
  {
    // assuming the order of columns is Name, Type
    if (strcmp(azColName[i], "Type") == 0)
    { 
      *(int *)NotUsed = 1; // 1 represents meat
    }
  }
  return 0;
}

// function that finds the type of the client in the database
int CheckTypeCallback(void *NotUsed, int argc, char **argv, char **azColName) 
{
  // this function will be called for each row in the result set
  for (int i = 0; i < argc; i++)
  {
    // assuming the order of columns is Name, Type
    if (strcmp(azColName[i], "Type") == 0)
    {
      // check value of the 'Type' column and set the corresponding int variable
      if (strcmp(argv[i], "Donor") == 0)
      {
        *(int *)NotUsed = 1; // 1 represents donor
      }
      else if (strcmp(argv[i], "Receiver") == 0)
      {
        *(int *)NotUsed = 0; // 0 represents receiver
      }
    }
  }
  return 0;
}



// function that modifies a variable if SELECT has returned any results.
int CheckExistenceCallback(void *NotUsed, int argc, char **argv, char **azColName) 
{
  *(int *)NotUsed = 1;
  return 0;
}

// function that handles user login based on given username.
// return value: 0 = receiver, = 1 donor, -1 = not found
int HandleLogin(char* usernameGiven)
{

  sqlite3 *db;
  char *errMsg = 0;
  int rc;

  // Open the SQLite database
  rc = sqlite3_open("BDfwr.db", &db);

  if (rc)
  {
    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
    return rc;
  }

  // SQL query to retrieve information based on the given username
  char query[200];
  sprintf(query, "SELECT * FROM Clients WHERE Name='%s'", usernameGiven);

  int userType = -1; // Will be set based on the query result
  rc = sqlite3_exec(db, query, CheckTypeCallback, &userType, &errMsg);

  if (rc != SQLITE_OK)
  {
    fprintf(stderr, "SQL error: %s\n", errMsg);
    sqlite3_free(errMsg);
  }
  else
  {
    sqlite3_close(db); // close database
    return(userType);
  }
       
}


int main()
{

  printf("[server] Server is now running. Clients may use the 'help' command for a list of commands.\n");
  struct sockaddr_in server;
  struct sockaddr_in from;

  int sd; // socket descriptor
  int pid;
  pthread_t th[100];
  int i = 0;

  if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    perror("[server] Error at socket().\n");
    return errno;
  }

  int on = 1;
  setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)); // allows reuse of local addresses

  bzero(&server, sizeof(server));
  bzero(&from, sizeof(from));

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY); // any address
  server.sin_port = htons(PORT); 

  if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1) // bind socket
  {
    perror("[server] Error at bind().\n");
    return errno;
  }

  if (listen(sd, 2) == -1)
  {
    perror("[server] Error at listen().\n");
    return errno;
  }

  char addressBuffer[INET_ADDRSTRLEN]; // for printing address
  bzero(&addressBuffer, sizeof(addressBuffer));
  inet_ntop(AF_INET, &server.sin_addr, addressBuffer, sizeof(addressBuffer));
  printf("[server] Address: %s\n", addressBuffer);

  // concurrent management of client programs, achieved using threads
  while (1)
  {

    int client;
    thData *td; 
    int length = sizeof(from);

    printf("[server] Port: %d...\n", PORT);
    fflush(stdout);

    if ((client = accept(sd, (struct sockaddr *)&from, &length)) < 0)
    {
      perror("[server] Error at accept().\n");
      continue;
    }

    // connection made, waiting for message

    td = (struct thData *)malloc(sizeof(struct thData));
    td->idThread = i++;
    td->cl = client;

    pthread_create(&th[i], NULL, &treat, td);

  }
};

static void *treat(void *arg)
{
  struct thData tdL;
  tdL = *((struct thData *)arg);

  printf("[Thread %d] Awaiting input...\n", tdL.idThread);
  fflush(stdout);
  pthread_detach(pthread_self());
  respond((struct thData *)arg);

  // close the connection, since we will no longer be working with this client
  close((intptr_t)arg);
  return (NULL);
};

void respond(void *arg)
{

  char buf[300];
  char currentUsername[100];

  int readLength = 0, isLoggedIn = 0,
      isDonor = 0, isReceiver = 0;
  struct thData tdL;
  tdL = *((struct thData *)arg);

  do
  {
    readLength = read(tdL.cl, buf, sizeof(buf));
    buf[readLength] = '\0';

    // login
    if (strstr(buf, "login ") != NULL)
    {
      if (!isLoggedIn)
      {
        char usernameGiven[100];
        strcpy(usernameGiven, buf + 6);
        int loginResult = HandleLogin(usernameGiven); // return value: 0 = receiver, = 1 donor, -1 = not found
        switch(loginResult)
        {
          case 0: // receiver
            printf("[Thread %d] %s has logged in as a receiver.\n", tdL.idThread, usernameGiven);
            strcpy(buf, "Successfully logged in as a receiver!");
            isLoggedIn = 1;
            isReceiver = 1;
            strcpy(currentUsername, usernameGiven);
            break;
          case 1: // donor
            printf("[Thread %d] %s has logged in as a donor.\n", tdL.idThread, usernameGiven);
            strcpy(buf, "Successfully logged in as a donor!");
            isLoggedIn = 1;
            isDonor = 1;
            strcpy(currentUsername, usernameGiven);
            break;
          case -1: // not found
            printf("[Thread %d] Error: %s is not in the database or has an unknown type.\n", tdL.idThread, usernameGiven);
            strcpy(buf, "Error: Login unsuccessful. The name is not in our database.");
            break;
        }
      }
      else
      {
        strcpy(buf, "Error: You are already logged in.");
      }
    }
    // logout
    else if (strcmp(buf, "logout") == 0)
    {
      if (isLoggedIn)
      {
        strcpy(buf, "Logged out successfully.");
        printf("[Thread %d] User %s has logged out.\n", tdL.idThread, currentUsername);
        isLoggedIn = 0;
        isDonor = 0;
        isReceiver = 0;
      }
      else
      {
        strcpy(buf, "Error: You cannot log out as you are not currently logged in.");
      }
    }
    // help
    else if (strcmp(buf, "help") == 0)
    {
      strcpy(buf, "\n\nCommands:\nlogin <username>\nlogout\nsignup <d/r> <username>\n\t//(d for donor, r for receiver)\ndonate <meat/produce/dairy/bread> <amount>\n\t//(only donor clients)\nrequest <meat/produce/dairy/bread> <amount>\n\t//(only receiver clients. not implemented yet.)\nhelp\nquit");
    }
    // quit
    else if (strcmp(buf, "quit") == 0)
    {
      continue;
    }
    // donate
    else if (strstr(buf, "donate ") != NULL) // donate <foodtype> <amount>
    {
      if (isLoggedIn)
      {
        if (isDonor)
        {
          char *aux = strtok(buf, " ");
          int step = 1;
          char productType[20], productAmount[5];
          while(aux)
          {
            
            switch (step)
            {
              case 1: // reading the first word of the command
                if(strcmp(aux, "donate") == 0) 
                  step++;
                break;
              case 2:
                if((strcmp(aux, "meat") == 0)||
                  (strcmp(aux, "dairy") == 0)||
                  (strcmp(aux, "produce") == 0)||
                  (strcmp(aux, "bread") == 0))
                {
                  strcpy(productType, aux);
                  step++;
                }
                else
                {
                  strcpy(productType, "ERROR");
                  step++;
                }
                break;
              case 3:
                strcpy(productAmount, aux);
                step++;
                break;
              default:
                break;
            }           
            aux = strtok(NULL, " ");
          }

          sqlite3 *db;
          char *errMsg = 0;
          int rc;


          rc = sqlite3_open("BDfwr.db", &db);

          

          if (rc)
          {
            fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
            return rc;
          }

          char donateQuery[200];
          sprintf(donateQuery, "INSERT INTO Products VALUES('%s',%s,'%s')", productType, productAmount, currentUsername);

          if(strcmp(productType, "ERROR") == 0)
          {
            strcpy(buf, "Error: Syntax is as follows: donate <meat/produce/dairy/bread> <amount>. Use the 'help' command for more details.");
          }
          else
          {
            rc = sqlite3_exec(db, donateQuery, 0, 0, &errMsg);

            if (rc != SQLITE_OK)
            {
              fprintf(stderr, "SQL error: %s\n", errMsg);
              sqlite3_free(errMsg);
            }
            else
            {
              printf("[Thread %d] User has donated %s kg of products.\n", tdL.idThread, productAmount);
              strcpy(buf, "Successfully inserted donated goods into database.");
            }
          }

          
          sqlite3_close(db);
        }
        else
        {
          strcpy(buf, "Error: You do not have access to this command, as you are not a donor.");
        }
      }
      else
      {
        strcpy(buf, "Error: You are not logged in.");
      }
    }
    // request
    else if (strstr(buf, "request ") != NULL)
    {
      if (isLoggedIn)
      {
        if (isReceiver)
        {
          char *aux = strtok(buf, " ");
          int step = 1, requestedAmount;
          char productType[20], productAmount[5];
          while(aux)
          {
            
            switch (step)
            {
              case 1: // reading the first word of the command
                if(strcmp(aux, "request") == 0) 
                  step++;
                break;
              case 2:
                if((strcmp(aux, "meat") == 0)||
                  (strcmp(aux, "dairy") == 0)||
                  (strcmp(aux, "produce") == 0)||
                  (strcmp(aux, "bread") == 0))
                {
                  strcpy(productType, aux);
                  step++;
                }
                else
                {
                  strcpy(productType, "ERROR");
                  step++;
                }
                break;
              case 3:
                strcpy(productAmount, aux);
                step++;
                break;
              default:
                break;
            }           
            aux = strtok(NULL, " ");
          }

          sqlite3 *db;
          char *errMsg = 0;
          int rc;

          rc = sqlite3_open("BDfwr.db", &db);

          if (rc)
          {
            fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
            return rc;
          }

          char requestQuery[200], alterQuery[200];

          requestedAmount = atoi(productAmount);

          // then, if amount < available amount, ALTER TABLE to remove amount from that row.

          //else, remove that entire row and redo the search.

          if(strcmp(productType, "ERROR") == 0)
          {
            strcpy(buf, "Error: Syntax is as follows: request <meat/produce/dairy/bread> <amount>. Use the 'help' command for more details.");
          }
          else
          { 
            if(requestedAmount > 0)
            {
              // first, we check if there is ANY amount of said product
              sprintf(requestQuery, "SELECT * FROM Products WHERE Category = '%s'", productType);
              int resultCount = 0;
              rc = sqlite3_exec(db, requestQuery, CheckExistenceCallback, &resultCount, &errMsg);

              if (rc != SQLITE_OK)
              {
                fprintf(stderr, "SQL error: %s\n", errMsg);
                sqlite3_free(errMsg);
              }
              else
              {
                if (resultCount > 0) // if there is a donation in the database of this product category
                {
                  int resultAmount = 0;
                  sprintf(requestQuery, "SELECT Amount FROM Products WHERE Category = '%s' LIMIT 1", productType);
                  //rc = sqlite3_exec(db, requestQuery, CheckAmountCallback, &resultAmount, &errMsg);

                  int retval, idx;
                  sqlite3_stmt* stmt = NULL;
                  retval = sqlite3_prepare_v2(db, requestQuery, -1, &stmt, 0);

                  if (retval != SQLITE_OK)
                  {
                    fprintf(stderr, "Cannot bind parameter: %s\n", sqlite3_errmsg(db));
                    return rc;
                  }

                  rc = sqlite3_step(stmt);

                  if (rc == SQLITE_ROW) 
                  {
                    // fetch the result
                    resultAmount = sqlite3_column_int(stmt, 0);
                    printf("Amount of product available: %d. Amount of product requested: %d.\n", resultAmount, requestedAmount);
                    //snprintf(buf, sizeof(buf), "%d", resultAmount);

                    // check size
                    if (requestedAmount < resultAmount)
                    {
                      sprintf(alterQuery, "UPDATE Products SET Amount = %d WHERE Category = '%s' LIMIT 1", resultAmount - requestedAmount, productType); // WHERE DonationID = ....

                      rc = sqlite3_exec(db, alterQuery, 0, 0, &errMsg);
                      if (rc != SQLITE_OK)
                      {
                        fprintf(stderr, "SQL error: %s\n", errMsg);
                        sqlite3_free(errMsg);
                      }
                      else
                      {
                        strcpy(buf, "Request succeeded. Database has been updated accordingly.");
                      }
                    }
                    else if (requestedAmount == resultAmount)
                    {
                      sprintf(alterQuery, "DELETE FROM Products WHERE Category = '%s' LIMIT 1", productType);

                      rc = sqlite3_exec(db, alterQuery, 0, 0, &errMsg);
                      if (rc != SQLITE_OK)
                      {
                        fprintf(stderr, "SQL error: %s\n", errMsg);
                        sqlite3_free(errMsg);
                      }
                      else
                      {
                        strcpy(buf, "Request succeeded. Database has been updated accordingly.");
                      }
                      
                    }
                    else
                    {
                      strcpy(buf, "Not enough product available. Please try again later.");
                    }

                  } else if (rc == SQLITE_DONE) {
                    printf("No rows found.\n");
                  } else {
                    fprintf(stderr, "Error executing statement: %s\n", sqlite3_errmsg(db));
                  }
                  sqlite3_finalize(stmt);
                }
              }

            }
            else
            {
              strcpy(buf, "Error: Product amount must be a positive integer.");
            }
            
          }

          sqlite3_close(db);
        }
        else
        {
          strcpy(buf, "Error: You do not have access to this command, as you are not a receiver.");
        }
      }
      else
      {
        strcpy(buf, "Error: You are not logged in.");
      }
    }
    // signup
    else if (strstr(buf, "signup ") != NULL)
    {

      if (!isLoggedIn)
      {
        char type; // convention: type 'r' -> receiver, type 'd' -> donor
        char aux[100];
        strcpy(aux, buf + 7);
        type = aux[0];
        char signupUsername[100];
        strcpy(signupUsername, buf + 9);

        sqlite3 *db;
        char *errMsg = 0;
        int rc;

        // Open the SQLite database
        rc = sqlite3_open("BDfwr.db", &db);

        if (rc)
        {
          fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
          return rc;
        }

        // Check if the username already exists in the database
        char checkQuery[200];
        sprintf(checkQuery, "SELECT * FROM Clients WHERE Name='%s'", signupUsername);

        int resultCount = 0;
        rc = sqlite3_exec(db, checkQuery, CheckExistenceCallback, &resultCount, &errMsg);

        if (rc != SQLITE_OK)
        {
          fprintf(stderr, "SQL error: %s\n", errMsg);
          sqlite3_free(errMsg);
        }
        else
        {
          if (resultCount > 0)
          {
            printf("Username '%s' is already taken.\n", signupUsername);
            strcpy(buf, "Error: Chosen username is already in our database.");
          }
          else
          {
            // If the username is not taken, insert it as a donor
            char insertQuery[200];

            switch (type)
            {

            case 'r': // receiver

              sprintf(insertQuery, "INSERT INTO Clients (Name, Type) VALUES ('%s', 'Receiver')", signupUsername);
              rc = sqlite3_exec(db, insertQuery, 0, 0, &errMsg);
              if (rc != SQLITE_OK)
              {
                fprintf(stderr, "SQL error: %s\n", errMsg);
                sqlite3_free(errMsg);
              }
              else
              {
                printf("Username '%s' has been successfully inserted as a Receiver.\n", signupUsername);
                strcpy(buf, "Signed up successfully as a receiver! You are now logged in.");
                isLoggedIn = 1;
                isReceiver = 1;
              }
              break;

            case 'd': // donor

              sprintf(insertQuery, "INSERT INTO Clients (Name, Type) VALUES ('%s', 'Donor')", signupUsername);
              rc = sqlite3_exec(db, insertQuery, 0, 0, &errMsg);
              if (rc != SQLITE_OK)
              {
                fprintf(stderr, "SQL error: %s\n", errMsg);
                sqlite3_free(errMsg);
              }
              else
              {
                printf("Username '%s' has been successfully inserted as a Donor.\n", signupUsername);
                strcpy(buf, "Signed up successfully as a donor! You are now logged in.");
                isLoggedIn = 1;
                isDonor = 1;
              }
              break;

            default: // invalid syntax

              strcpy(buf, "Error: Syntax is as follows: signup <r/d> <username>\n(r for receiver, d for donor)");
              break;
            }
          }
        }
        sqlite3_close(db); // Close the database
      }
      else
      {
        strcpy(buf, "Error: You are already logged in, you cannot sign up for an account.");
      }
    }
    // invalid command
    else
    {
      printf("[Thread %d] Invalid command: %s\n", tdL.idThread, buf);
      strcpy(buf, "Error: Unknown command. Type 'help' for information on available commands.");
    }

    if (write(tdL.cl, buf, sizeof(buf)) <= 0)
    {
      printf("[Thread %d] ", tdL.idThread);
      perror("[Thread] Error at write().\n");
    }

  } while (readLength);
}
