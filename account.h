#define MAX_TRANSACTIONS 10

struct Account {
    int accNo;
    int customers[2];
    int isRegularAccount;
    int active;
    long int balance;
    int transactions[MAX_TRANSACTIONS];
};