// Threaded program for the tests where a SIF removes main's timer: TAU's
// setup code must stay in main, or the threads' timers fail at run time.
#include <pthread.h>

void keep_routine() {
    volatile int sum = 0;
    for (int i = 0; i < 1000; ++i) {
        sum = sum + i;
    }
}

void excluded_routine() {
    keep_routine();
}

void *worker(void *) {
    keep_routine();
    excluded_routine();
    return nullptr;
}

int main() {
    pthread_t threads[4];
    for (pthread_t &thread : threads) {
        pthread_create(&thread, nullptr, worker, nullptr);
    }
    for (pthread_t &thread : threads) {
        pthread_join(thread, nullptr);
    }
    keep_routine();
    return 0;
}
