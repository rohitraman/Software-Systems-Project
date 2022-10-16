#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>

#include "./constants.h"
#include <time.h>

struct Account {
    int accNo;
    int customers[2];
    int isRegularAccount;
    int active;
    long int balance;
    int transactions[10];
};
struct Transaction {
    int transactionID;
    int accountNumber;
    int operation;
    long int oldBalance;
    long int newBalance;
    time_t transactionTime;
};
struct Customer {
    int id;
    char name[25];
    char gender;
    int age;
    char login[30];
    char password[30];
    int account;
};
int getAccountDetails(int connFD, struct Account *customerAccount) {
    int readBytes;
    char inputBuffer[SIZE], outputBuffer[10000], buffer[SIZE];

    int accNo, accountFD;
    struct Account account;

    if (customerAccount == NULL) {
        write(connFD, GET_ACCOUNT_NUMBER, strlen(GET_ACCOUNT_NUMBER));

        bzero(inputBuffer, sizeof(inputBuffer));
        read(connFD, inputBuffer, sizeof(inputBuffer));

        accNo = atoi(inputBuffer);
    }
    else
        accNo = customerAccount->accNo;

    accountFD = open(ACCOUNT_FILE, O_RDONLY);
    if (accountFD == -1) {
        bzero(outputBuffer, sizeof(outputBuffer));
        strcpy(outputBuffer, "Error opening account file.");
        strcat(outputBuffer, "^");
        write(connFD, outputBuffer, strlen(outputBuffer));
        read(connFD, inputBuffer, sizeof(inputBuffer));
        return 0;
    }

    int offset = lseek(accountFD, accNo * sizeof(struct Account), SEEK_SET);
    if (offset == -1 && errno == EINVAL) {
        bzero(outputBuffer, sizeof(outputBuffer));
        strcpy(outputBuffer, ACCOUNT_ID_DOESNT_EXIST);
        strcat(outputBuffer, "^");
        write(connFD, outputBuffer, strlen(outputBuffer));
        read(connFD, inputBuffer, sizeof(inputBuffer));
        return 0;
    }
    else if (offset == -1) {
        printf("Error seeking to required account record!");
        return 0;
    }

    struct flock lock;
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = offset;
    lock.l_len = sizeof(struct Account);
    lock.l_pid = getpid();

    fcntl(accountFD, F_SETLKW, &lock);

    readBytes = read(accountFD, &account, sizeof(struct Account));
    if (readBytes == -1) {
        printf("Error reading account record from file!");
        return 0;
    }

    lock.l_type = F_UNLCK;
    fcntl(accountFD, F_SETLK, &lock);

    if (customerAccount != NULL) {
        *customerAccount = account;
        return 1;
    }

    bzero(outputBuffer, sizeof(outputBuffer));
    sprintf(outputBuffer, ACCOUNT_DETAILS, account.accNo, (account.isRegularAccount ? "Regular" : "Joint"), (account.active) ? "Active" : "Deactivated");
    if (account.active == 1) {
        sprintf(buffer, "\nAccount Balance:₹ %ld", account.balance);
        strcat(outputBuffer, buffer);
    }

    sprintf(buffer, "\nPrimary Customer ID: %d", account.customers[0]);
    strcat(outputBuffer, buffer);
    if (account.customers[1] != -1) {
        sprintf(buffer, "\nSecondary Customer ID: %d", account.customers[1]);
        strcat(outputBuffer, buffer);
    }

    strcat(outputBuffer, "\n^");

    write(connFD, outputBuffer, strlen(outputBuffer));
    read(connFD, inputBuffer, sizeof(inputBuffer));

    return 1;
}

int getCustomerDetails(int connFD, int custID) {
    int readBytes;
    char inputBuffer[SIZE], outputBuffer[SIZE], buffer[SIZE];

    struct Customer cust;
    int custFD;
    
    struct flock lock;
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = sizeof(struct Account);
    lock.l_pid = getpid();

    if (custID == -1) {
        write(connFD, GET_CUSTOMER_ID, strlen(GET_CUSTOMER_ID));
        bzero(inputBuffer, sizeof(inputBuffer));
        read(connFD, inputBuffer, sizeof(inputBuffer));
        custID = atoi(inputBuffer);
    }

    custFD = open(CUSTOMER_FILE, O_RDONLY);
    if (custFD == -1) {
        bzero(outputBuffer, sizeof(outputBuffer));
        strcpy(outputBuffer, CUSTOMER_ID_DOESNT_EXIST);
        strcat(outputBuffer, "^");
        write(connFD, outputBuffer, strlen(outputBuffer));
        read(connFD, inputBuffer, sizeof(inputBuffer));
        return 0;
    }
    int offset = lseek(custFD, custID * sizeof(struct Customer), SEEK_SET);
    if (errno == EINVAL) {
        bzero(outputBuffer, sizeof(outputBuffer));
        strcpy(outputBuffer, CUSTOMER_ID_DOESNT_EXIST);
        strcat(outputBuffer, "^");
        write(connFD, outputBuffer, strlen(outputBuffer));
        read(connFD, inputBuffer, sizeof(inputBuffer));
        return 0;
    } else if (offset == -1) {
        printf("Error while seeking to required customer record!");
        return 0;
    }
    lock.l_start = offset;

    fcntl(custFD, F_SETLKW, &lock);

    readBytes = read(custFD, &cust, sizeof(struct Customer));
    if (readBytes == -1) {
        printf("Error reading customer record from file!");
        return 0;
    }

    lock.l_type = F_UNLCK;
    fcntl(custFD, F_SETLK, &lock);

    bzero(outputBuffer, sizeof(outputBuffer));
    sprintf(outputBuffer, CUSTOMER_DETAILS, cust.id, cust.name, cust.gender, cust.age, cust.account, cust.login);

    strcat(outputBuffer, "\n\nGoing to the main menu...^");

    write(connFD, outputBuffer, strlen(outputBuffer));

    read(connFD, inputBuffer, sizeof(inputBuffer));
    return 1;
}

int getTransactionDetails(int connFD, int accNo) {
    char inputBuffer[SIZE], outputBuffer[10000], buffer[SIZE];

    int bytes;
    struct Account account;

    if (accNo == -1) {
        write(connFD, GET_ACCOUNT_NUMBER, strlen(GET_ACCOUNT_NUMBER));

        bzero(inputBuffer, sizeof(inputBuffer));
        read(connFD, inputBuffer, sizeof(inputBuffer));

        account.accNo = atoi(inputBuffer);
    }
    else
        account.accNo = accNo;

    if (getAccountDetails(connFD, &account) == 1) {
        int i;

        struct Transaction trans;
        struct tm* transactionTime;

        bzero(outputBuffer, sizeof(inputBuffer));

        int transFD = open(TRANSACTION_FILE, O_RDONLY);
        if (transFD == -1) {
            printf("Error while opening transaction file!");
            write(connFD, TRANSACTIONS_NOT_FOUND, strlen(TRANSACTIONS_NOT_FOUND));
            read(connFD, inputBuffer, sizeof(inputBuffer));
            return 0;
        }

        for (i = 0; i < MAX_TRANSACTIONS && account.transactions[i] != -1; i++) {
            int offset = lseek(transFD, account.transactions[i] * sizeof(struct Transaction), SEEK_SET);
            if (offset == -1) {
                printf("Error while seeking to required transaction record!");
                return 0;
            }

            struct flock lock;
            lock.l_type = F_RDLCK;
            lock.l_whence = SEEK_SET;
            lock.l_start = offset;
            lock.l_len = sizeof(struct Transaction);
            lock.l_pid = getpid();

            fcntl(transFD, F_SETLKW, &lock);

            bytes = read(transFD, &trans, sizeof(struct Transaction));
            if (bytes == -1) {
                printf("Error reading records from file!");
                return 0;
            }

            lock.l_type = F_UNLCK;
            fcntl(transFD, F_SETLK, &lock);
            time_t temp = trans.transactionTime;
            transactionTime = localtime(&temp);

            bzero(buffer, sizeof(buffer));
            sprintf(buffer, TRANSACTION_DETAILS, (i + 1), transactionTime -> tm_hour, transactionTime -> tm_min, transactionTime -> tm_mday, transactionTime -> tm_mon, transactionTime -> tm_year + 1900, (trans.operation == 1 ? "Credit" : "Debit"), trans.newBalance);

            if (strlen(outputBuffer) == 0)
                strcpy(outputBuffer, buffer);
            else
                strcat(outputBuffer, buffer);
        }

        close(transFD);

        if (strlen(outputBuffer) == 0) {
            write(connFD, TRANSACTIONS_NOT_FOUND, strlen(TRANSACTIONS_NOT_FOUND));
            read(connFD, inputBuffer, sizeof(inputBuffer));
            return 0;
        } else {
            strcat(outputBuffer, "^");
            write(connFD, outputBuffer, strlen(outputBuffer));
            read(connFD, inputBuffer, sizeof(inputBuffer));
        }
    }
}
int loginHandler(int isAdmin, int connFD, struct Customer *ptrToCustomer) {
    struct Customer cust;
    char inputBuffer[SIZE], outputBuffer[SIZE], buffer[SIZE];
    int custID, found = 0;

    if (isAdmin == 1)
        strcpy(outputBuffer, ADMIN_LOGIN_SUCCESS);
    else
        strcpy(outputBuffer, CUSTOMER_LOGIN_WELCOME_MESSAGE);

    strcat(outputBuffer, "\n");
    strcat(outputBuffer, USERNAME);
    write(connFD, outputBuffer, strlen(outputBuffer));
    read(connFD, inputBuffer, sizeof(inputBuffer));

    if (isAdmin == 1) {
        if (strcmp(inputBuffer, ADMIN_USERNAME) == 0)
            found = 1;
    } else {
        strcpy(buffer, inputBuffer);
        char* token = strtok(buffer, "-");
        token = strtok(NULL, "-"); // strtok() stores the previous stored string as a static reference
        custID = atoi(token);

        int customerFD = open(CUSTOMER_FILE, O_RDONLY);
        if (customerFD == -1) {
            printf("Error opening customer file in read mode!");
            return 0;
        }

        int offset = lseek(customerFD, custID * sizeof(struct Customer), SEEK_SET);
        if (offset >= 0) {
            struct flock lock;
            lock.l_type = F_RDLCK;
            lock.l_whence = SEEK_SET;
            lock.l_start = custID * sizeof(struct Customer);
            lock.l_len = sizeof(struct Customer);
            lock.l_pid = getpid();

            fcntl(customerFD, F_SETLKW, &lock);
            read(customerFD, &cust, sizeof(struct Customer));

            lock.l_type = F_UNLCK;
            fcntl(customerFD, F_SETLK, &lock);

            if (strcmp(cust.login, inputBuffer) == 0)
                found = 1;
            close(customerFD);
        }
        else {
            write(connFD, CUSTOMER_LOGIN_ID_DOESNT_EXIST, strlen(CUSTOMER_LOGIN_ID_DOESNT_EXIST));
            return 0;
        }
    }

    if (found == 1) {
        write(connFD, PASSWORD, strlen(PASSWORD));

        bzero(inputBuffer, sizeof(inputBuffer));
        read(connFD, inputBuffer, sizeof(inputBuffer));

        if (isAdmin == 1) {
            if (strcmp(inputBuffer, ADMIN_PASSWORD) == 0)
                return 1;
        }
        else {
            if (strcmp(inputBuffer, cust.password) == 0) {
                *ptrToCustomer = cust;
                return 1;
            }
        }
        write(connFD, INVALID_PASSWORD, strlen(INVALID_PASSWORD));
        return 0;
    }
    else {
        write(connFD, INVALID_USERNAME, strlen(INVALID_USERNAME));
        return 0;
    }

    return 0;
}

