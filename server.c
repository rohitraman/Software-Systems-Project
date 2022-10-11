#include <stdio.h>
#include <errno.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "./constants.h"
#include "./admin.h"
#include "./customer.h"


void main()
{
    int sockFD, nsd;
    struct sockaddr_in serverAddress, clientAddress;

    sockFD = socket(AF_INET, SOCK_STREAM, 0);
    if (sockFD == -1) {
        perror("Error while creating server socket!");
        _exit(0);
    }

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8081);
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(sockFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
    listen(sockFD, 10);

    int clientSize;
    while (1) {
        clientSize = (int)sizeof(clientAddress);
        nsd = accept(sockFD, (struct sockaddr *)&clientAddress, &clientSize);
        if (nsd == -1) {
            perror("Error while connecting to client!");
            close(sockFD);
        }
        else {
            if (!fork()) {
                char readBuffer[1000];
                int opt;

                write(nsd, INITIAL_MESSAGE, strlen(INITIAL_MESSAGE));

                bzero(readBuffer, sizeof(readBuffer));
                read(nsd, readBuffer, sizeof(readBuffer));

                opt = atoi(readBuffer);
                switch (opt)
                {
                case 1:
                    adminLoginHandler(nsd);
                    break;
                case 2:
                    customer_operation_handler(nsd);
                    break;
                default:
                    break;
                }

                close(nsd);
                _exit(0);
            }
        }
    }

    close(sockFD);
}


