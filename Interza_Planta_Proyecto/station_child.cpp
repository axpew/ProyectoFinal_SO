#include "station_child.h"
#include "ipc_common.h"

#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstdio>
#include <signal.h>
#include <time.h>
#include <semaphore.h>

extern "C" void _child_entry(int idx, int seed) {
    if (!open_ipc()) {
        fprintf(stderr, "Child %d: cannot open ipc\n", idx);
        _exit(1);
    }

    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd == -1) { perror("child shm_open"); _exit(1); }
    ShmState* s = (ShmState*)mmap(NULL, sizeof(ShmState), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    ::close(fd);
    if (s == MAP_FAILED) { perror("child mmap"); _exit(1); }

    sem_t* sem_stage = open_sem_stage(idx);
    sem_t* sem_ack   = open_sem_ack(idx);
    sem_t* sem_trans = open_sem_transition();  // Semáforo de transición

    srand(seed ^ idx);

    while (s->running) {
        if (sem_stage) {
            sem_wait(sem_stage);
        } else {
            usleep(100000);
            continue;
        }

        if (!s->running) break;
        if (s->station_paused[idx]) continue;

        // *** ADQUIRIR MUTEX DE TRANSICIÓN ***
        if (sem_trans) sem_wait(sem_trans);

        ProductInfo currentProduct;

        if (idx == 0) {
            currentProduct.productId = s->next_product_id++;
        } else {
            currentProduct = s->product_in_station[idx - 1];
            if (currentProduct.productId <= 0) {
                if (sem_trans) sem_post(sem_trans);  // Liberar mutex
                continue;
            }
        }

        s->product_in_station[idx] = currentProduct;

        // Limpiar anterior
        if (idx > 0) {
            s->product_in_station[idx - 1].productId = 0;
        }

        // *** LIBERAR MUTEX DE TRANSICIÓN ***
        if (sem_trans) sem_post(sem_trans);

        // Procesar (fuera del mutex)
        int work_ms = 800 + (rand() % 800);
        usleep(work_ms * 1000);

        s->station_done[idx] = 1;

        if (sem_ack) sem_wait(sem_ack);

        if (idx + 1 < NUM_STATIONS) {
            sem_t* next = open_sem_stage(idx + 1);
            if (next) sem_post(next);
        } else {
            // Última estación: limpiar con mutex
            if (sem_trans) sem_wait(sem_trans);
            s->product_in_station[idx].productId = 0;
            if (sem_trans) sem_post(sem_trans);
        }

        if (idx == 0) {
            usleep(300000);
            if (!s->station_paused[idx] && s->running) {
                if (sem_stage) sem_post(sem_stage);
            }
        }
    }

    if (sem_trans) sem_close(sem_trans);
    munmap(s, sizeof(ShmState));
    _exit(0);
}
