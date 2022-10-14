#include <sys/ipc.h>
#include <sys/sem.h>
#include "./common.h"

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short  *array;
    struct seminfo  *__buf;
} arg;

int addCustomer(int connFD, int isPrimary, int newAccNumber) {
    int writeBytes;
    char inputBuffer[SIZE], outputBuffer[SIZE];

    struct Customer newCust, prevCust;

    int customerFD = open(CUSTOMER_FILE, O_RDONLY);
    if (customerFD == -1 && errno == ENOENT) {
        newCust.id = 0;
    } else if (customerFD == -1) {
        printf("Error while opening customer file");
        return -1;
    } else {
        int offset = lseek(customerFD, -sizeof(struct Customer), SEEK_END);

        struct flock lock;
        lock.l_type = F_RDLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = offset;
        lock.l_len = sizeof(struct Customer);
        lock.l_pid = getpid();

        fcntl(customerFD, F_SETLKW, &lock);

        read(customerFD, &prevCust, sizeof(struct Customer));

        lock.l_type = F_UNLCK;
        fcntl(customerFD, F_SETLK, &lock);
        close(customerFD);
        newCust.id = prevCust.id + 1;
    }

    if (isPrimary == 1)
        sprintf(outputBuffer, "%s%s", ADMIN_CREATE_CUSTOMER_PRIMARY, ADMIN_CREATE_CUSTOMER_NAME);
    else
        sprintf(outputBuffer, "%s%s", ADMIN_CREATE_CUSTOMER_SECONDARY, ADMIN_CREATE_CUSTOMER_NAME);

    write(connFD, outputBuffer, sizeof(outputBuffer));
    read(connFD, inputBuffer, sizeof(inputBuffer));

    strcpy(newCust.name, inputBuffer);

    write(connFD, ADMIN_CREATE_CUSTOMER_GENDER, strlen(ADMIN_CREATE_CUSTOMER_GENDER));

    bzero(inputBuffer, sizeof(inputBuffer));
    read(connFD, inputBuffer, sizeof(inputBuffer));
    if (inputBuffer[0] == 'M' || inputBuffer[0] == 'F')
        newCust.gender = inputBuffer[0];
    else {
        write(connFD, ADMIN_CREATE_CUSTOMER_WRONG_GENDER_MESSAGE, strlen(ADMIN_CREATE_CUSTOMER_WRONG_GENDER_MESSAGE));
        read(connFD, inputBuffer, sizeof(inputBuffer));
        return 0;
    }

    bzero(outputBuffer, sizeof(outputBuffer));
    strcpy(outputBuffer, ADMIN_CREATE_CUSTOMER_AGE);
    write(connFD, outputBuffer, strlen(outputBuffer));

    bzero(inputBuffer, sizeof(inputBuffer));
    read(connFD, inputBuffer, sizeof(inputBuffer));

    int custAge = atoi(inputBuffer);
    if (custAge == 0) {
        bzero(outputBuffer, sizeof(outputBuffer));
        strcpy(outputBuffer, ERRON_INPUT_FOR_NUMBER);
        write(connFD, outputBuffer, strlen(outputBuffer));
        read(connFD, inputBuffer, sizeof(inputBuffer));
        return 0;
    }
    newCust.age = custAge;

    newCust.account = newAccNumber;

    strcpy(newCust.login, newCust.name);
    strcat(newCust.login, "-");
    sprintf(outputBuffer, "%d", newCust.id);
    strcat(newCust.login, outputBuffer);

    strcpy(newCust.password, GEN_PASSWORD);

    customerFD = open(CUSTOMER_FILE, O_CREAT | O_APPEND | O_WRONLY, S_IRWXU);
    if (customerFD == -1) {
        printf("Error while creating / opening customer file!");
        return 0;
    }
    writeBytes = write(customerFD, &newCust, sizeof(newCust));
    if (writeBytes == -1) {
        printf("Error while writing customer record to file!");
        return 0;
    }

    close(customerFD);

    bzero(outputBuffer, sizeof(outputBuffer));
    sprintf(outputBuffer, "%s%s-%d\n%s%s", ADMIN_CREATE_CUSTOMER_GEN_LOGIN, newCust.name, newCust.id, ADMIN_CREATE_CUSTOMER_GEN_PASSWORD, GEN_PASSWORD);
    strcat(outputBuffer, "^");
    write(connFD, outputBuffer, strlen(outputBuffer));
    read(connFD, inputBuffer, sizeof(inputBuffer));

    return newCust.id;
}
void addAccount(int connFD) {
    int writeBytes;
    char inputBuffer[SIZE], outputBuffer[SIZE];

    struct Account newAcc, prevAcc;

    int accountFD = open(ACCOUNT_FILE, O_RDONLY);
    if (accountFD == -1 && errno == ENOENT) {
        newAcc.accNo = 0;
    } else if (accountFD == -1) {
        printf("Error while opening account file");
        return;
    } else {
        int offset = lseek(accountFD, -sizeof(struct Account), SEEK_END);

        struct flock lock;
        lock.l_type = F_RDLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = offset;
        lock.l_len = sizeof(struct Customer);
        lock.l_pid = getpid();
       
        fcntl(accountFD, F_SETLKW, &lock);
        read(accountFD, &prevAcc, sizeof(struct Account));
       
        lock.l_type = F_UNLCK;
        fcntl(accountFD, F_SETLK, &lock);
        close(accountFD);
        newAcc.accNo = prevAcc.accNo + 1;
    }
    write(connFD, ADMIN_CREATE_ACCOUNT_TYPE, strlen(ADMIN_CREATE_ACCOUNT_TYPE));
    bzero(inputBuffer, sizeof(inputBuffer));
    read(connFD, &inputBuffer, sizeof(inputBuffer));

    newAcc.isRegularAccount = atoi(inputBuffer) == 1 ? 1 : 0;

    newAcc.customers[0] = addCustomer(connFD, 1, newAcc.accNo);

    if (newAcc.isRegularAccount)
        newAcc.customers[1] = -1;
    else
        newAcc.customers[1] = addCustomer(connFD, 0, newAcc.accNo);

    newAcc.active = 1;
    newAcc.balance = 0;
    memset(newAcc.transactions, -1, MAX_TRANSACTIONS * sizeof(int));
    accountFD = open(ACCOUNT_FILE, O_CREAT | O_APPEND | O_WRONLY, S_IRWXU);
    if (accountFD == -1) {
        printf("Error while creating / opening account file!");
        return;
    }
    writeBytes = write(accountFD, &newAcc, sizeof(struct Account));
    if (writeBytes == -1) {
        printf("Error while writing Account record to file!");
        return;
    }
    close(accountFD);
    bzero(outputBuffer, sizeof(outputBuffer));
    sprintf(outputBuffer, "%s%d", ADMIN_CREATE_ACCOUNT_NUMBER, newAcc.accNo);
    strcat(outputBuffer, "^");
    write(connFD, outputBuffer, sizeof(outputBuffer));
    read(connFD, inputBuffer, sizeof(read));
    return;
}

void deleteAccount(int connFD) {
    char inputBuffer[SIZE], outputBuffer[SIZE];
    struct Account acc;

    write(connFD, ADMIN_DELETED_ACCOUNT_NO, strlen(ADMIN_DELETED_ACCOUNT_NO));
    bzero(inputBuffer, sizeof(inputBuffer));
    read(connFD, inputBuffer, sizeof(inputBuffer));

    int accountNumber = atoi(inputBuffer);

    int accountFD = open(ACCOUNT_FILE, O_RDONLY);
    if (accountFD == -1) {
        bzero(outputBuffer, sizeof(outputBuffer));
        strcpy(outputBuffer, ACCOUNT_ID_DOESNT_EXIT);
        write(connFD, outputBuffer, strlen(outputBuffer));
        read(connFD, inputBuffer, sizeof(inputBuffer));
        return;
    }

    int offset = lseek(accountFD, accountNumber * sizeof(struct Account), SEEK_SET);
    if (errno == EINVAL) {
        bzero(outputBuffer, sizeof(outputBuffer));
        strcpy(outputBuffer, ACCOUNT_ID_DOESNT_EXIT);
        write(connFD, outputBuffer, strlen(outputBuffer));
        read(connFD, inputBuffer, sizeof(inputBuffer));
        return;
    } else if (offset == -1) {
        printf("Error while seeking to required account record!");
        return;
    }

    struct flock lock;
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = offset;
    lock.l_len = sizeof(struct Customer);
    lock.l_pid = getpid();
    fcntl(accountFD, F_SETLKW, &lock);
    read(accountFD, &acc, sizeof(struct Account));

    lock.l_type = F_UNLCK;
    fcntl(accountFD, F_SETLK, &lock);

    close(accountFD);

    bzero(outputBuffer, sizeof(outputBuffer));
    if (acc.balance == 0) {
        acc.active = 0;
        accountFD = open(ACCOUNT_FILE, O_WRONLY);
        if (accountFD == -1) {
            printf("Error opening Account file in write mode!");
            return;
        }

        offset = lseek(accountFD, accountNumber * sizeof(struct Account), SEEK_SET);

        lock.l_type = F_WRLCK;
        lock.l_start = offset;
        fcntl(accountFD, F_SETLKW, &lock);
        write(accountFD, &acc, sizeof(struct Account));
        
        lock.l_type = F_UNLCK;
        fcntl(accountFD, F_SETLK, &lock);
        strcpy(outputBuffer, ADMIN_DELETED_ACCOUNT_SUCCESS_MESSAGE);
    } else
        strcpy(outputBuffer, ADMIN_DELETED_ACCOUNT_FAILURE_MESSAGE);
    write(connFD, outputBuffer, strlen(outputBuffer));
    read(connFD, inputBuffer, sizeof(inputBuffer));
}

void modifyCustomerInfo(int connFD) {
    char inputBuffer[SIZE], outputBuffer[SIZE];
    struct Customer customer;
    int custID, offset;

    write(connFD, ADMIN_CUSTOMER_ID_MESSAGE, strlen(ADMIN_CUSTOMER_ID_MESSAGE));
    bzero(inputBuffer, sizeof(inputBuffer));
    read(connFD, inputBuffer, sizeof(inputBuffer));

    custID = atoi(inputBuffer);

    int customerFD = open(CUSTOMER_FILE, O_RDONLY);
    if (customerFD == -1) {
        bzero(outputBuffer, sizeof(outputBuffer));
        strcpy(outputBuffer, CUSTOMER_ID_DOESNT_EXIST);
        strcat(outputBuffer, "^");
        write(connFD, outputBuffer, strlen(outputBuffer));
        read(connFD, inputBuffer, sizeof(inputBuffer));
        return;
    }
    
    offset = lseek(customerFD, custID * sizeof(struct Customer), SEEK_SET);
    if (errno == EINVAL) {
        bzero(outputBuffer, sizeof(outputBuffer));
        strcpy(outputBuffer, CUSTOMER_ID_DOESNT_EXIST);
        strcat(outputBuffer, "^");
        write(connFD, outputBuffer, strlen(outputBuffer));
        read(connFD, inputBuffer, sizeof(inputBuffer));
        return;
    } else if (offset == -1) {
        printf("Error while seeking to required customer record!");
        return;
    }

    struct flock lock;
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = offset;
    lock.l_len = sizeof(struct Customer);
    lock.l_pid = getpid();

    fcntl(customerFD, F_SETLKW, &lock);

    read(customerFD, &customer, sizeof(struct Customer));

    lock.l_type = F_UNLCK;
    fcntl(customerFD, F_SETLK, &lock);

    close(customerFD);
    write(connFD, ADMIN_MODIFY_CUSTOMER_MENU, strlen(ADMIN_MODIFY_CUSTOMER_MENU));
    read(connFD, inputBuffer, sizeof(inputBuffer));

    int opt = atoi(inputBuffer);
    if (opt == 0) {
        bzero(outputBuffer, sizeof(outputBuffer));
        strcpy(outputBuffer, ERRON_INPUT_FOR_NUMBER);
        write(connFD, outputBuffer, strlen(outputBuffer));
        read(connFD, inputBuffer, sizeof(inputBuffer));
        return;
    }

    bzero(inputBuffer, sizeof(inputBuffer));
    switch (opt) {
        case 1:
            write(connFD, ADMIN_MODIFY_CUSTOMER_NEW_NAME, strlen(ADMIN_MODIFY_CUSTOMER_NEW_NAME));
            read(connFD, &inputBuffer, sizeof(inputBuffer));
            strcpy(customer.name, inputBuffer);
            break;
        case 2:
            write(connFD, ADMIN_MODIFY_CUSTOMER_NEW_AGE, strlen(ADMIN_MODIFY_CUSTOMER_NEW_AGE));
            read(connFD, &inputBuffer, sizeof(inputBuffer));
            int newAge = atoi(inputBuffer);
            if (newAge == 0) {
                bzero(outputBuffer, sizeof(outputBuffer));
                strcpy(outputBuffer, ERRON_INPUT_FOR_NUMBER);
                write(connFD, outputBuffer, strlen(outputBuffer));
                read(connFD, inputBuffer, sizeof(inputBuffer));
                return;
            }
            customer.age = newAge;
            break;
        case 3:
            write(connFD, ADMIN_MODIFY_CUSTOMER_NEW_GENDER, strlen(ADMIN_MODIFY_CUSTOMER_NEW_GENDER));
            read(connFD, &inputBuffer, sizeof(inputBuffer));
            customer.gender = inputBuffer[0];
            break;
        default:
            bzero(outputBuffer, sizeof(outputBuffer));
            strcpy(outputBuffer, INVALID_MENU_CHOICE);
            write(connFD, outputBuffer, strlen(outputBuffer));
            read(connFD, inputBuffer, sizeof(inputBuffer));
            return;
    }

    customerFD = open(CUSTOMER_FILE, O_WRONLY);
    if (customerFD == -1) {
        printf("Error while opening customer file");
        return;
    }
    offset = lseek(customerFD, custID * sizeof(struct Customer), SEEK_SET);

    lock.l_type = F_WRLCK;
    lock.l_start = offset;
    fcntl(customerFD, F_SETLKW, &lock);

    write(customerFD, &customer, sizeof(struct Customer));

    lock.l_type = F_UNLCK;
    fcntl(customerFD, F_SETLKW, &lock);

    close(customerFD);

    write(connFD, ADMIN_MODIFY_CUSTOMER_SUCCESS_MESSAGE, strlen(ADMIN_MODIFY_CUSTOMER_SUCCESS_MESSAGE));
    read(connFD, inputBuffer, sizeof(inputBuffer));

    return;
}


void adminLoginHandler(int connFD) {
    if (loginHandler(1, connFD, NULL) == 1) {
        char outputBuffer[SIZE];
        bzero(outputBuffer, sizeof(outputBuffer));
        strcpy(outputBuffer, ADMIN_LOGIN_SUCCESS);
        while (1) {
            char inputBuffer[SIZE];
            strcat(outputBuffer, "\n");
            strcat(outputBuffer, ADMIN_MENU);
            write(connFD, outputBuffer, strlen(outputBuffer));
            bzero(outputBuffer, sizeof(outputBuffer));
            bzero(inputBuffer, sizeof(inputBuffer));
            read(connFD, inputBuffer, sizeof(inputBuffer));
            int opt = atoi(inputBuffer);
            switch (opt) {
            case 1:
                getCustomerDetails(connFD, -1);
                break;
            case 2:
                getAccountDetails(connFD, NULL);
                break;
            case 3:
                addAccount(connFD);
                break;
            case 4:
                deleteAccount(connFD);
                break;
            case 5:
                modifyCustomerInfo(connFD);
                break;
            case 6:
                write(connFD, ADMIN_LOGOUT_MESSAGE, strlen(ADMIN_LOGOUT_MESSAGE));
                _exit(0);
            default:
                write(connFD, WRONG_OPTION_SELECTED, strlen(WRONG_OPTION_SELECTED));
                break;
            }

        }
    }
}
void lockCriticalSection(struct sembuf *semOp, int semID) {
    semOp->sem_flg = SEM_UNDO;
    semOp->sem_op = -1;
    semOp->sem_num = 0;
    semop(semID, semOp, 1);
}

void unlockCriticalSection(struct sembuf *semOp, int semID) {
    semOp->sem_op = 1;
    semop(semID, semOp, 1);
}
void writeTransactionToArray(int *transArray, int ID) {
    int i = 0;
    while (transArray[i] != -1)
        i++;
    if (i >= MAX_TRANSACTIONS) {
        for (i = 1; i < MAX_TRANSACTIONS; i++)
            transArray[i - 1] = transArray[i];
        transArray[i - 1] = ID;
    } else {
        transArray[i] = ID;
    }
}

int writeTransactionToFile(int accountNumber, long int oldBalance, long int newBalance, int operation) {
    struct Transaction newTrans;
    newTrans.accountNumber = accountNumber;
    newTrans.oldBalance = oldBalance;
    newTrans.newBalance = newBalance;
    newTrans.operation = operation;
    newTrans.transactionTime = time(NULL);

    int transFD = open(TRANSACTION_FILE, O_CREAT | O_APPEND | O_RDWR, S_IRWXU);

    int offset = lseek(transFD, -sizeof(struct Transaction), SEEK_END);
    if (offset >= 0) {
        struct Transaction prevTransaction;
        read(transFD, &prevTransaction, sizeof(struct Transaction));

        newTrans.transactionID = prevTransaction.transactionID + 1;
    }
    else
        newTrans.transactionID = 0;

    write(transFD, &newTrans, sizeof(struct Transaction));

    return newTrans.transactionID;
}
void retreiveBalance(int connFD, int semID, struct Customer loggedInCustomer) {
    char buffer[SIZE];
    struct Account account;
    account.accNo = loggedInCustomer.account;
    if (getAccountDetails(connFD, &account)) {
        if (account.active) {
            sprintf(buffer, BALANCE_DETAILS, account.balance);
            write(connFD, buffer, strlen(buffer));
        }
        else
            write(connFD, ACCOUNT_DEACTIVATED_MESSAGE, strlen(ACCOUNT_DEACTIVATED_MESSAGE));
        read(connFD, buffer, sizeof(buffer));
    }
    else {
        return;
    }
}

void depositAmount(int connFD, int semID, struct Customer loggedInCustomer) {
    char inputBuffer[SIZE], outputBuffer[SIZE];

    struct Account account;
    account.accNo = loggedInCustomer.account;

    long int amtToDeposit = 0;

    struct sembuf semOp;
    lockCriticalSection(&semOp, semID);

    if (getAccountDetails(connFD, &account)) {
        if (account.active) {
            write(connFD, DEPOSIT_AMOUNT_MESSAGE, strlen(DEPOSIT_AMOUNT_MESSAGE));

            bzero(inputBuffer, sizeof(inputBuffer));
            read(connFD, inputBuffer, sizeof(inputBuffer));

            amtToDeposit = atol(inputBuffer);
            if (amtToDeposit > 0) {
                int newTransID = writeTransactionToFile(account.accNo, account.balance, account.balance + amtToDeposit, 1);
                writeTransactionToArray(account.transactions, newTransID);

                account.balance += amtToDeposit;

                int accountFD = open(ACCOUNT_FILE, O_WRONLY);
                int offset = lseek(accountFD, account.accNo * sizeof(struct Account), SEEK_SET);

                struct flock lock;
                lock.l_type = F_WRLCK;
                lock.l_whence = SEEK_SET;
                lock.l_start = offset;
                lock.l_len = sizeof(struct Account);
                lock.l_pid = getpid();

                fcntl(accountFD, F_SETLKW, &lock);

                write(accountFD, &account, sizeof(struct Account));            
                lock.l_type = F_UNLCK;
                fcntl(accountFD, F_SETLK, &lock);

                write(connFD, DEPOSIT_AMOUNT_SUCCESS_MESSAGE, strlen(DEPOSIT_AMOUNT_SUCCESS_MESSAGE));
                read(connFD, inputBuffer, sizeof(inputBuffer));
                unlockCriticalSection(&semOp, semID);
                return;
            }
            else {
                write(connFD, DEPOSIT_AMOUNT_INVALID_MESSAGE, strlen(DEPOSIT_AMOUNT_INVALID_MESSAGE));
            }
        }
        else {
            write(connFD, ACCOUNT_DEACTIVATED_MESSAGE, strlen(ACCOUNT_DEACTIVATED_MESSAGE));
        }
        read(connFD, inputBuffer, sizeof(inputBuffer));
        unlockCriticalSection(&semOp, semID);
    }
    else {
        unlockCriticalSection(&semOp, semID);
        return;
    }
}

void withdrawAmount(int connFD, int semID, struct Customer loggedInCustomer) {
    char inputBuffer[SIZE], outputBuffer[SIZE];
    int readBytes, writeBytes;

    struct Account account;
    account.accNo = loggedInCustomer.account;

    long int withdrawAmount = 0;

    struct sembuf semOp;
    lockCriticalSection(&semOp, semID);

    if (getAccountDetails(connFD, &account)) {
        if (account.active) {
            write(connFD, WITHDRAW_AMOUNT_MESSAGE, strlen(WITHDRAW_AMOUNT_MESSAGE));

            bzero(inputBuffer, sizeof(inputBuffer));
            read(connFD, inputBuffer, sizeof(inputBuffer));
            
            withdrawAmount = atol(inputBuffer);

            if (withdrawAmount > 0 && account.balance - withdrawAmount >= 0) {
                int newTransID = writeTransactionToFile(account.accNo, account.balance, account.balance - withdrawAmount, 0);
                writeTransactionToArray(account.transactions, newTransID);

                account.balance -= withdrawAmount;

                int accountFD = open(ACCOUNT_FILE, O_WRONLY);
                int offset = lseek(accountFD, account.accNo * sizeof(struct Account), SEEK_SET);

                struct flock lock;
                lock.l_type = F_WRLCK;
                lock.l_whence = SEEK_SET;
                lock.l_start = offset;
                lock.l_len = sizeof(struct Account);
                lock.l_pid = getpid();

                fcntl(accountFD, F_SETLKW, &lock);
                write(accountFD, &account, sizeof(struct Account));

                lock.l_type = F_UNLCK;
                fcntl(accountFD, F_SETLK, &lock);

                write(connFD, WITHDRAW_AMOUNT_SUCCESS_MESSAGE, strlen(WITHDRAW_AMOUNT_SUCCESS_MESSAGE));
                read(connFD, inputBuffer, sizeof(inputBuffer));

                unlockCriticalSection(&semOp, semID);

                return;
            }
            else
                writeBytes = write(connFD, WITHDRAW_AMOUNT_INVALID_MESSAGE, strlen(WITHDRAW_AMOUNT_INVALID_MESSAGE));
        }
        else
            write(connFD, ACCOUNT_DEACTIVATED_MESSAGE, strlen(ACCOUNT_DEACTIVATED_MESSAGE));
        read(connFD, inputBuffer, sizeof(inputBuffer)); // Dummy read

        unlockCriticalSection(&semOp, semID);
    }
    else {
        unlockCriticalSection(&semOp, semID);
        return;
    }
}

void changePassword(int connFD, int semID, struct Customer loggedInCustomer) {
    char inputBuffer[SIZE], outputBuffer[SIZE], newPassword[SIZE];

    struct sembuf semOp = {0, -1, SEM_UNDO};
    semop(semID, &semOp, 1);

    write(connFD, PASSWORD_CHANGE_OLD_PASS, strlen(PASSWORD_CHANGE_OLD_PASS));

    bzero(inputBuffer, sizeof(inputBuffer));
    read(connFD, inputBuffer, sizeof(inputBuffer));

    if (strcmp(inputBuffer, loggedInCustomer.password) == 0) {
        write(connFD, PASSWORD_CHANGE_NEW_PASS_MESSAGE, strlen(PASSWORD_CHANGE_NEW_PASS_MESSAGE));
        bzero(inputBuffer, sizeof(inputBuffer));
        read(connFD, inputBuffer, sizeof(inputBuffer));
        strcpy(newPassword, inputBuffer);

        write(connFD, PASSWORD_CHANGE_NEW_PASS_REENTER_MESSAGE, strlen(PASSWORD_CHANGE_NEW_PASS_REENTER_MESSAGE));
        bzero(inputBuffer, sizeof(inputBuffer));
        read(connFD, inputBuffer, sizeof(inputBuffer));

        if (strcmp(inputBuffer, newPassword) == 0) {
            strcpy(loggedInCustomer.password, newPassword);

            int custFD = open(CUSTOMER_FILE, O_WRONLY);
            if (custFD == -1) {
                printf("Error opening customer file!");
                return;
            }

            int offset = lseek(custFD, loggedInCustomer.id * sizeof(struct Customer), SEEK_SET);

            struct flock lock;
            lock.l_type = F_WRLCK;
            lock.l_whence = SEEK_SET;
            lock.l_start = offset;
            lock.l_len = sizeof(struct Account);
            lock.l_pid = getpid();
            
            fcntl(custFD, F_SETLKW, &lock);

            write(custFD, &loggedInCustomer, sizeof(struct Customer));

            lock.l_type = F_UNLCK;
            fcntl(custFD, F_SETLK, &lock);

            close(custFD);

            write(connFD, PASSWORD_CHANGE_SUCCESS_MESSAGE, strlen(PASSWORD_CHANGE_SUCCESS_MESSAGE));
            read(connFD, inputBuffer, sizeof(inputBuffer));

            unlockCriticalSection(&semOp, semID);

            return;
        }
        else {
            write(connFD, PASSWORD_CHANGE_NEW_PASS_INVALID_MESSAGE, strlen(PASSWORD_CHANGE_NEW_PASS_INVALID_MESSAGE));
            read(connFD, inputBuffer, sizeof(inputBuffer));
        }
    }
    else {
        write(connFD, PASSWORD_CHANGE_OLD_PASS_INVALID_MESSAGE, strlen(PASSWORD_CHANGE_OLD_PASS_INVALID_MESSAGE));
        read(connFD, inputBuffer, sizeof(inputBuffer));
    }

    unlockCriticalSection(&semOp, semID);

    return;
}
void customerLoginHandler(int connFD) {
    struct Customer loggedInCustomer;
    if (loginHandler(0, connFD, &loggedInCustomer) == 1) {
        char inputBuffer[SIZE], outputBuffer[SIZE];
        int semKey = ftok(".", loggedInCustomer.account);

        int semID = semget(semKey, 1, 0);
        if (semID == -1) {
            semID = semget(semKey, 1, IPC_CREAT | 0700);
            arg.val = 1;
            semctl(semID, 0, SETVAL, arg);
        }

        strcpy(outputBuffer, CUSTOMER_LOGIN_SUCCESS_MESSAGE);
        while (1) {
            strcat(outputBuffer, "\n");
            strcat(outputBuffer, CUSTOMER_MENU);
            write(connFD, outputBuffer, strlen(outputBuffer));
            bzero(outputBuffer, sizeof(outputBuffer));

            bzero(inputBuffer, sizeof(inputBuffer));
            read(connFD, inputBuffer, sizeof(inputBuffer));
            
            int opt = atoi(inputBuffer);
            switch (opt)
            {
                case 1:
                    getCustomerDetails(connFD, loggedInCustomer.id);
                    break;
                case 2:
                    depositAmount(connFD, semID, loggedInCustomer);
                    break;
                case 3:
                    withdrawAmount(connFD, semID, loggedInCustomer);
                    break;
                case 4:
                    retreiveBalance(connFD, semID, loggedInCustomer);
                    break;
                case 5:
                    getTransactionDetails(connFD, loggedInCustomer.account);
                    break;
                case 6:
                    changePassword(connFD, semID, loggedInCustomer);
                    break;
                case 7: 
                    write(connFD, CUSTOMER_LOGOUT_MESSAGE, strlen(CUSTOMER_LOGOUT_MESSAGE));
                    return;
                default:
                    write(connFD, WRONG_OPTION_SELECTED, strlen(WRONG_OPTION_SELECTED));
                    break;
            }
        }
    }
    else {
        return;
    }
    return;
}
