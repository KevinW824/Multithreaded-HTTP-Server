#include <semaphore.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct queue {
    void **buf;
    int head;
    int tail;
    int size;
    int count;
    sem_t *mutex;
    sem_t *not_empty;
    sem_t *not_full;
} queue_t;

queue_t *queue_new(int size) {
    queue_t *q = (queue_t *) malloc(sizeof(queue_t));
    q->buf = (void **) malloc(size * sizeof(void *));
    q->head = 0;
    q->tail = 0;
    q->size = size;
    q->count = 0;
    q->mutex = (sem_t *) malloc(sizeof(sem_t));
    q->not_empty = (sem_t *) malloc(sizeof(sem_t));
    q->not_full = (sem_t *) malloc(sizeof(sem_t));
    sem_init(q->mutex, 0, 1);
    sem_init(q->not_empty, 0, 0);
    sem_init(q->not_full, 0, size);
    return q;
}

void queue_delete(queue_t **q) {
    if (*q) {
        free((*q)->buf);
        sem_destroy((*q)->mutex);
        sem_destroy((*q)->not_empty);
        sem_destroy((*q)->not_full);
        free((*q)->mutex);
        free((*q)->not_empty);
        free((*q)->not_full);
        free(*q);
        *q = NULL;
    }
}

bool queue_push(queue_t *q, void *elem) {
    if (q == NULL || elem == NULL)
        return false;
    sem_wait(q->not_full);
    sem_wait(q->mutex);
    q->buf[q->tail] = elem;
    q->tail = (q->tail + 1) % q->size;
    q->count++;
    sem_post(q->mutex);
    sem_post(q->not_empty);
    return true;
}

bool queue_pop(queue_t *q, void **elem) {
    if (q == NULL || elem == NULL)
        return false;
    sem_wait(q->not_empty);
    sem_wait(q->mutex);
    *elem = q->buf[q->head];
    q->head = (q->head + 1) % q->size;
    q->count--;
    sem_post(q->mutex);
    sem_post(q->not_full);
    return true;
}
