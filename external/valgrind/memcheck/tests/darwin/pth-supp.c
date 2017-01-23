
#include <pthread.h>

int main() {
   pthread_rwlock_t mutex;
   pthread_rwlock_init(&mutex, NULL);
   return 0;
}
