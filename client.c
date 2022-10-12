#include <stdio.h>
#include <errno.h>
#include <fcntl.h> 
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <string.h>


void main() {
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

    char buffer[1000];

    do {
        bzero(inputBuffer, sizeof(inputBuffer));
        bzero(buffer, sizeof(buffer));
        readBytes = read(sockFD, inputBuffer, sizeof(inputBuffer));
        if (readBytes == -1)
            printf("Read error");
        else if (readBytes == 0)
            printf("No read available from server\n");
        else if (strchr(inputBuffer, '^') != NULL) {
            // Makes sure that, to return to main menu without being stuck, we use some identifier to make sure that it returns to main menu, else it is stuck
            strncpy(buffer, inputBuffer, strlen(inputBuffer) - 1);
            printf("%s\n", buffer);
            write(sockFD, "^", strlen("^"));
        } else if (strchr(inputBuffer, '$') != NULL) {
            // Used for terminating connection
            strncpy(buffer, inputBuffer, strlen(inputBuffer) - 2);
            printf("%s\n", buffer);
            printf("Closing the connection to the server now!\n");
            break;
        }
        else {
            bzero(outputBuffer, sizeof(outputBuffer));
            // Hiding password
            if (strchr(inputBuffer, '#') != NULL)
                strcpy(outputBuffer, getpass(inputBuffer));
            else {
                // Take normal inputs
                printf("%s\n", inputBuffer);
                scanf("%[^\n]%*c", outputBuffer);
            }

            writeBytes = write(sockFD, outputBuffer, strlen(outputBuffer));
            if (writeBytes == -1) {
                printf("Error while writing to client socket!");
                printf("Closing the connection to the server now!\n");
                break;
            }
        }
    } while (readBytes > 0);

    close(sockFD);
}
