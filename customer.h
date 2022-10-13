#include <sys/ipc.h>
#include <sys/sem.h>

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short  *array;
    struct seminfo  *__buf;
} arg;

int lockCriticalSection(struct sembuf *semOp, int semID) {
    semOp->sem_flg = SEM_UNDO;
    semOp->sem_op = -1;
    semOp->sem_num = 0;
    semop(semID, semOp, 1);
    return 1;
}

int unlockCriticalSection(struct sembuf *semOp, int semID) {
    semOp->sem_op = 1;
    semop(semID, semOp, 1);
    return 1;
}
void writeTransactionToArray(int *transactionArray, int ID) {
    int i = 0;
    while (transactionArray[i] != -1)
        i++;
    if (i >= MAX_TRANSACTIONS) {
        for (i = 1; i < MAX_TRANSACTIONS; i++)
            transactionArray[i - 1] = transactionArray[i];
        transactionArray[i - 1] = ID;
    } else {
        transactionArray[i] = ID;
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
int retreiveBalance(int connFD, int semID, struct Customer loggedInCustomer) {
    char buffer[1000];
    struct Account account;
    account.accNo = loggedInCustomer.account;
    if (getAccountDetails(connFD, &account)) {
        if (account.active) {
            sprintf(buffer, "You have ₹ %ld in your account!^", account.balance);
            write(connFD, buffer, strlen(buffer));
        }
        else
            write(connFD, ACCOUNT_DEACTIVATED_MESSAGE, strlen(ACCOUNT_DEACTIVATED_MESSAGE));
        read(connFD, buffer, sizeof(buffer));
    }
    else {
        return 0;
    }
}

int depositAmount(int connFD, int semID, struct Customer loggedInCustomer) {
    char inputBuffer[1000], outputBuffer[1000];

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
                return 1;
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
        return 0;
    }
}

int withdrawAmount(int connFD, int semID, struct Customer loggedInCustomer) {
    char inputBuffer[1000], outputBuffer[1000];
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

                return 1;
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
        return 0;
    }
}

int changePassword(int connFD, int semID, struct Customer loggedInCustomer) {
    int readBytes, writeBytes;
    char inputBuffer[1000], outputBuffer[1000], newPassword[1000];

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
                return 0;
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

            return 1;
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

    return 0;
}
int customerLoginHandler(int connFD) {
    struct Customer loggedInCustomer;
    if (loginHandler(0, connFD, &loggedInCustomer) == 1) {
        char inputBuffer[1000], outputBuffer[1000];

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
                    return 0;
                default:
                    write(connFD, WRONG_OPTION_SELECTED, strlen(WRONG_OPTION_SELECTED));
                    break;
            }
        }
    }
    else {
        return 0;
    }
    return 1;
}
