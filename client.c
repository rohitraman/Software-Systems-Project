#include <stdio.h>
#include <errno.h>
#include <fcntl.h> 
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <string.h>


void main()
{
    int sockFD;
    struct sockaddr_in serverAddress;
    struct sockaddr server;

    sockFD = socket(AF_INET, SOCK_STREAM, 0);

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8081);
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    connect(sockFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress));

    char inputBuffer[1000], outputBuffer[1000];
    int readBytes, writeBytes;

    char tempBuffer[1000];

    do
    {
        bzero(inputBuffer, sizeof(inputBuffer)); // Empty the read buffer
        bzero(tempBuffer, sizeof(tempBuffer));
        readBytes = read(sockFD, inputBuffer, sizeof(inputBuffer));
        if (readBytes == -1)
            perror("Error while reading from client socket!");
        else if (readBytes == 0)
            printf("No error received from server! Closing the connection to the server now!\n");
        else if (strchr(inputBuffer, '^') != NULL)
        {
            // Skip read from client
            strncpy(tempBuffer, inputBuffer, strlen(inputBuffer) - 1);
            printf("%s\n", tempBuffer);
            writeBytes = write(sockFD, "^", strlen("^"));
            if (writeBytes == -1)
            {
                perror("Error while writing to client socket!");
                break;
            }
        }
        else if (strchr(inputBuffer, '$') != NULL)
        {
            // Server sent an error message and is now closing it's end of the connection
            strncpy(tempBuffer, inputBuffer, strlen(inputBuffer) - 2);
            printf("%s\n", tempBuffer);
            printf("Closing the connection to the server now!\n");
            break;
        }
        else
        {
            bzero(outputBuffer, sizeof(outputBuffer)); // Empty the write buffer

            if (strchr(inputBuffer, '#') != NULL)
                strcpy(outputBuffer, getpass(inputBuffer));
            else
            {
                printf("%s\n", inputBuffer);
                scanf("%[^\n]%*c", outputBuffer); // Take user input!
            }

            writeBytes = write(sockFD, outputBuffer, strlen(outputBuffer));
            if (writeBytes == -1)
            {
                perror("Error while writing to client socket!");
                printf("Closing the connection to the server now!\n");
                break;
            }
        }
    } while (readBytes > 0);

    close(sockFD);
}
