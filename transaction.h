#include <time.h>

struct Transaction {
    int transactionID;
    int accountNumber;
    int operation;
    long int oldBalance;
    long int newBalance;
    time_t transactionTime;
};
