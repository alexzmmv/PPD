#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define NUM_ACCOUNTS 10
#define INITIAL_BALANCE 1000
#define NUM_THREADS 10
#define TRANSFERS_PER_THREAD 100
#define MAX_TRANSFER_AMOUNT 100

typedef struct {
    int id;
    int balance;
    pthread_mutex_t mtx;
} BankAccount;

BankAccount accounts[NUM_ACCOUNTS];
int initial_total;

int total_balance() {
    int sum = 0;
    for (int i = 0; i < NUM_ACCOUNTS; ++i) {
        sum += accounts[i].balance;
    }
    return sum;
}

void lock_accounts(int id1, int id2) {
    if (id1 < id2) {
        pthread_mutex_lock(&accounts[id1].mtx);
        pthread_mutex_lock(&accounts[id2].mtx);
    } else {
        pthread_mutex_lock(&accounts[id2].mtx);
        pthread_mutex_lock(&accounts[id1].mtx);
    }
}

void unlock_accounts(int id1, int id2) {
    if (id1 < id2) {
        pthread_mutex_unlock(&accounts[id2].mtx);
        pthread_mutex_unlock(&accounts[id1].mtx);
    } else {
        pthread_mutex_unlock(&accounts[id1].mtx);
        pthread_mutex_unlock(&accounts[id2].mtx);
    }
}

void* transfer_thread(void* arg) {
    unsigned int seed = time(NULL);
    for (int i = 0; i < TRANSFERS_PER_THREAD; ++i) {
        int from, to;
        do {
            from = rand_r(&seed) % NUM_ACCOUNTS;
            to = rand_r(&seed) % NUM_ACCOUNTS;
        } while (from == to);
        int amount = (rand_r(&seed) % MAX_TRANSFER_AMOUNT) + 1;
        usleep(amount);
        //lock_accounts(from, to);
        if (accounts[from].balance >= amount) {
            accounts[from].balance -= amount;
            accounts[to].balance += amount;
        }
        //unlock_accounts(from, to);
    }
    return NULL;
}

int main() {
    for (int i = 0; i < NUM_ACCOUNTS; ++i) {
        accounts[i].id = i;
        accounts[i].balance = INITIAL_BALANCE;
        pthread_mutex_init(&accounts[i].mtx, NULL);
    }
    initial_total = total_balance();
    printf("Initial balance: %d\n", initial_total);

    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_create(&threads[i], NULL, transfer_thread, (void*)(uintptr_t)i);
    }
    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], NULL);
    }

    int final_total = total_balance();
    printf("Final balance: %d\n", final_total);

    for (int i = 0; i < NUM_ACCOUNTS; ++i) {
        pthread_mutex_destroy(&accounts[i].mtx);
    }
    return 0;
}
