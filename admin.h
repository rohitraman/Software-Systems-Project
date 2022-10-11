#include "./common.h"

int adminLoginHandler(int connFD);
int addAccount(int connFD);
int addCustomer(int connFD, int isPrimary, int newAccNumber);
int deleteAccount(int connFD);
int modifyCustomerInfo(int connFD);


int adminLoginHandler(int connFD) {
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
            // write(connFD, inputBuffer, strlen(inputBuffer));
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
            default:
                write(connFD, ADMIN_LOGOUT, strlen(ADMIN_LOGOUT));
                _exit(0);
            }

        }
    }
    else {
        return 0;
    }
    return 1;
}

int addAccount(int connFD) {
    int writeBytes;
    char inputBuffer[SIZE], outputBuffer[SIZE];

    struct Account newAcc, prevAcc;

    int accountFD = open(ACCOUNT_FILE, O_RDONLY);
    if (accountFD == -1 && errno == ENOENT) {
        newAcc.accountNumber = 0;
    }
    else if (accountFD == -1) {
        printf("Error while opening account file");
        return 0;
    }
    else {
        int offset = lseek(accountFD, -sizeof(struct Account), SEEK_END);

        struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};
        fcntl(accountFD, F_SETLKW, &lock);

        read(accountFD, &prevAcc, sizeof(struct Account));

        lock.l_type = F_UNLCK;
        fcntl(accountFD, F_SETLK, &lock);

        close(accountFD);

        newAcc.accountNumber = prevAcc.accountNumber + 1;
    }
    write(connFD, ADMIN_ADD_ACCOUNT_TYPE, strlen(ADMIN_ADD_ACCOUNT_TYPE));
    bzero(inputBuffer, sizeof(inputBuffer));
    read(connFD, &inputBuffer, sizeof(inputBuffer));

    newAcc.isRegularAccount = atoi(inputBuffer) == 1 ? 1 : 0;

    newAcc.owners[0] = addCustomer(connFD, 1, newAcc.accountNumber);

    if (newAcc.isRegularAccount)
        newAcc.owners[1] = -1;
    else
        newAcc.owners[1] = addCustomer(connFD, 0, newAcc.accountNumber);

    newAcc.active = 1;
    newAcc.balance = 0;

    memset(newAcc.transactions, -1, MAX_TRANSACTIONS * sizeof(int));

    accountFD = open(ACCOUNT_FILE, O_CREAT | O_APPEND | O_WRONLY, S_IRWXU);
    if (accountFD == -1) {
        printf("Error while creating / opening account file!");
        return 0;
    }

    writeBytes = write(accountFD, &newAcc, sizeof(struct Account));
    if (writeBytes == -1) {
        printf("Error while writing Account record to file!");
        return 0;
    }

    close(accountFD);

    bzero(outputBuffer, sizeof(outputBuffer));
    sprintf(outputBuffer, "%s%d", ADMIN_ADD_ACCOUNT_NUMBER, newAcc.accountNumber);
    write(connFD, outputBuffer, sizeof(outputBuffer));
    read(connFD, inputBuffer, sizeof(read));
    return 1;
}

int addCustomer(int connFD, int isPrimary, int newAccNumber) {
    int readBytes, writeBytes;
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

        struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Customer), getpid()};
        fcntl(customerFD, F_SETLKW, &lock);

        read(customerFD, &prevCust, sizeof(struct Customer));

        lock.l_type = F_UNLCK;
        fcntl(customerFD, F_SETLK, &lock);
        close(customerFD);
        newCust.id = prevCust.id + 1;
    }

    if (isPrimary == 1)
        sprintf(outputBuffer, "%s%s", ADMIN_ADD_CUSTOMER_PRIMARY, ADMIN_ADD_CUSTOMER_NAME);
    else
        sprintf(outputBuffer, "%s%s", ADMIN_ADD_CUSTOMER_SECONDARY, ADMIN_ADD_CUSTOMER_NAME);

    write(connFD, outputBuffer, sizeof(outputBuffer));
    read(connFD, inputBuffer, sizeof(inputBuffer));

    strcpy(newCust.name, inputBuffer);

    write(connFD, ADMIN_ADD_CUSTOMER_GENDER, strlen(ADMIN_ADD_CUSTOMER_GENDER));

    bzero(inputBuffer, sizeof(inputBuffer));
    read(connFD, inputBuffer, sizeof(inputBuffer));
    if (inputBuffer[0] == 'M' || inputBuffer[0] == 'F')
        newCust.gender = inputBuffer[0];
    else {
        writeBytes = write(connFD, ADMIN_ADD_CUSTOMER_WRONG_GENDER, strlen(ADMIN_ADD_CUSTOMER_WRONG_GENDER));
        readBytes = read(connFD, inputBuffer, sizeof(inputBuffer)); // Dummy read
        return 0;
    }

    bzero(outputBuffer, sizeof(outputBuffer));
    strcpy(outputBuffer, ADMIN_ADD_CUSTOMER_AGE);
    write(connFD, outputBuffer, strlen(outputBuffer));

    bzero(inputBuffer, sizeof(inputBuffer));
    read(connFD, inputBuffer, sizeof(inputBuffer));

    int customerAge = atoi(inputBuffer);
    if (customerAge == 0) {
        bzero(outputBuffer, sizeof(outputBuffer));
        strcpy(outputBuffer, ERRON_INPUT_FOR_NUMBER);
        write(connFD, outputBuffer, strlen(outputBuffer));
        read(connFD, inputBuffer, sizeof(inputBuffer));
        return 0;
    }
    newCust.age = customerAge;

    newCust.account = newAccNumber;

    strcpy(newCust.login, newCust.name);
    strcat(newCust.login, "-");
    sprintf(outputBuffer, "%d", newCust.id);
    strcat(newCust.login, outputBuffer);

    char hashedPassword[SIZE];
    strcpy(hashedPassword, crypt(AUTOGEN_PASSWORD, SALT_BAE));
    strcpy(newCust.password, hashedPassword);

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
    sprintf(outputBuffer, "%s%s-%d\n%s%s", ADMIN_ADD_CUSTOMER_AUTOGEN_LOGIN, newCust.name, newCust.id, ADMIN_ADD_CUSTOMER_AUTOGEN_PASSWORD, AUTOGEN_PASSWORD);
    write(connFD, outputBuffer, strlen(outputBuffer));
    read(connFD, inputBuffer, sizeof(inputBuffer));

    return newCust.id;
}

int deleteAccount(int connFD) {
    char inputBuffer[SIZE], outputBuffer[SIZE];
    struct Account acc;

    write(connFD, ADMIN_DEL_ACCOUNT_NO, strlen(ADMIN_DEL_ACCOUNT_NO));
    bzero(inputBuffer, sizeof(inputBuffer));
    read(connFD, inputBuffer, sizeof(inputBuffer));

    int accountNumber = atoi(inputBuffer);

    int accountFD = open(ACCOUNT_FILE, O_RDONLY);
    if (accountFD == -1) {
        bzero(outputBuffer, sizeof(outputBuffer));
        strcpy(outputBuffer, ACCOUNT_ID_DOESNT_EXIT);
        write(connFD, outputBuffer, strlen(outputBuffer));
        read(connFD, inputBuffer, sizeof(inputBuffer));
        return 0;
    }

    int offset = lseek(accountFD, accountNumber * sizeof(struct Account), SEEK_SET);
    if (errno == EINVAL) {
        bzero(outputBuffer, sizeof(outputBuffer));
        strcpy(outputBuffer, ACCOUNT_ID_DOESNT_EXIT);
        write(connFD, outputBuffer, strlen(outputBuffer));
        read(connFD, inputBuffer, sizeof(inputBuffer)); // Dummy read
        return 0;
    }
    else if (offset == -1) {
        printf("Error while seeking to required account record!");
        return 0;
    }

    struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};
    fcntl(accountFD, F_SETLKW, &lock);
    read(accountFD, &acc, sizeof(struct Account));

    lock.l_type = F_UNLCK;
    fcntl(accountFD, F_SETLK, &lock);

    close(accountFD);

    bzero(outputBuffer, sizeof(outputBuffer));
    if (acc.balance == 0)
    {
        // No money, hence can close account
        acc.active = 0;
        accountFD = open(ACCOUNT_FILE, O_WRONLY);
        if (accountFD == -1) {
            printf("Error opening Account file in write mode!");
            return 0;
        }

        offset = lseek(accountFD, accountNumber * sizeof(struct Account), SEEK_SET);

        lock.l_type = F_WRLCK;
        lock.l_start = offset;
        fcntl(accountFD, F_SETLKW, &lock);
        write(accountFD, &acc, sizeof(struct Account));
        
        lock.l_type = F_UNLCK;
        fcntl(accountFD, F_SETLK, &lock);
        strcpy(outputBuffer, ADMIN_DEL_ACCOUNT_SUCCESS);
    }
    else
        strcpy(outputBuffer, ADMIN_DEL_ACCOUNT_FAILURE);
    write(connFD, outputBuffer, strlen(outputBuffer));
    read(connFD, inputBuffer, sizeof(inputBuffer)); // Dummy read

    return 1;
}

int modifyCustomerInfo(int connFD) {
    char inputBuffer[SIZE], outputBuffer[SIZE];
    struct Customer customer;
    int customerID;
    int offset;

    write(connFD, ADMIN_MOD_CUSTOMER_ID, strlen(ADMIN_MOD_CUSTOMER_ID));
    bzero(inputBuffer, sizeof(inputBuffer));
    read(connFD, inputBuffer, sizeof(inputBuffer));

    customerID = atoi(inputBuffer);

    int customerFD = open(CUSTOMER_FILE, O_RDONLY);
    if (customerFD == -1) {
        bzero(outputBuffer, sizeof(outputBuffer));
        strcpy(outputBuffer, CUSTOMER_ID_DOESNT_EXIT);
        strcat(outputBuffer, "^");
        write(connFD, outputBuffer, strlen(outputBuffer));
        read(connFD, inputBuffer, sizeof(inputBuffer));
        return 0;
    }
    
    offset = lseek(customerFD, customerID * sizeof(struct Customer), SEEK_SET);
    if (errno == EINVAL) {
        bzero(outputBuffer, sizeof(outputBuffer));
        strcpy(outputBuffer, CUSTOMER_ID_DOESNT_EXIT);
        strcat(outputBuffer, "^");
        write(connFD, outputBuffer, strlen(outputBuffer));
        read(connFD, inputBuffer, sizeof(inputBuffer));
        return 0;
    } else if (offset == -1) {
        printf("Error while seeking to required customer record!");
        return 0;
    }

    struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Customer), getpid()};

    fcntl(customerFD, F_SETLKW, &lock);

    read(customerFD, &customer, sizeof(struct Customer));

    lock.l_type = F_UNLCK;
    fcntl(customerFD, F_SETLK, &lock);

    close(customerFD);
    write(connFD, ADMIN_MOD_CUSTOMER_MENU, strlen(ADMIN_MOD_CUSTOMER_MENU));
    read(connFD, inputBuffer, sizeof(inputBuffer));

    int choice = atoi(inputBuffer);
    if (choice == 0) {
        bzero(outputBuffer, sizeof(outputBuffer));
        strcpy(outputBuffer, ERRON_INPUT_FOR_NUMBER);
        write(connFD, outputBuffer, strlen(outputBuffer));
        read(connFD, inputBuffer, sizeof(inputBuffer));
        return 0;
    }

    bzero(inputBuffer, sizeof(inputBuffer));
    switch (choice) {
        case 1:
            write(connFD, ADMIN_MOD_CUSTOMER_NEW_NAME, strlen(ADMIN_MOD_CUSTOMER_NEW_NAME));
            read(connFD, &inputBuffer, sizeof(inputBuffer));
            strcpy(customer.name, inputBuffer);
            break;
        case 2:
            write(connFD, ADMIN_MOD_CUSTOMER_NEW_AGE, strlen(ADMIN_MOD_CUSTOMER_NEW_AGE));
            read(connFD, &inputBuffer, sizeof(inputBuffer));
            int updatedAge = atoi(inputBuffer);
            if (updatedAge == 0) {
                bzero(outputBuffer, sizeof(outputBuffer));
                strcpy(outputBuffer, ERRON_INPUT_FOR_NUMBER);
                write(connFD, outputBuffer, strlen(outputBuffer));
                read(connFD, inputBuffer, sizeof(inputBuffer));
                return 0;
            }
            customer.age = updatedAge;
            break;
        case 3:
            write(connFD, ADMIN_MOD_CUSTOMER_NEW_GENDER, strlen(ADMIN_MOD_CUSTOMER_NEW_GENDER));
            read(connFD, &inputBuffer, sizeof(inputBuffer));
            customer.gender = inputBuffer[0];
            break;
        default:
            bzero(outputBuffer, sizeof(outputBuffer));
            strcpy(outputBuffer, INVALID_MENU_CHOICE);
            write(connFD, outputBuffer, strlen(outputBuffer));
            read(connFD, inputBuffer, sizeof(inputBuffer));
            return 0;
    }

    customerFD = open(CUSTOMER_FILE, O_WRONLY);
    if (customerFD == -1) {
        printf("Error while opening customer file");
        return 0;
    }
    offset = lseek(customerFD, customerID * sizeof(struct Customer), SEEK_SET);

    lock.l_type = F_WRLCK;
    lock.l_start = offset;
    fcntl(customerFD, F_SETLKW, &lock);

    write(customerFD, &customer, sizeof(struct Customer));

    lock.l_type = F_UNLCK;
    fcntl(customerFD, F_SETLKW, &lock);

    close(customerFD);

    write(connFD, ADMIN_MOD_CUSTOMER_SUCCESS, strlen(ADMIN_MOD_CUSTOMER_SUCCESS));
    read(connFD, inputBuffer, sizeof(inputBuffer));

    return 1;
}