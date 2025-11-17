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
    // open ipc resources created by parent
    if (!open_ipc()) {
        fprintf(stderr, "Child %d: cannot open ipc\n", idx);
        _exit(1);
    }

    // map shared memory pointer (ipc_common keeps internal mapping)
    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd == -1) { perror("child shm_open"); _exit(1); }
    ShmState* s = (ShmState*)mmap(NULL, sizeof(ShmState), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    ::close(fd);
    if (s == MAP_FAILED) { perror("child mmap"); _exit(1); }

    sem_t* sem_stage = open_sem_stage(idx);
    sem_t* sem_ack   = open_sem_ack(idx);

    srand(seed ^ idx);

    while (s->running) {
        // respect pause flag
        if (s->station_paused[idx]) {
            usleep(200000);
            continue;
        }

        // wait for stage semaphore (permission to start this piece)
        if (sem_stage) {
            sem_wait(sem_stage);
        } else {
            usleep(100000);
            continue;
        }

        if (!s->running) break;

        // simulate processing
        int work_ms = 800 + (rand() % 800); // un poco más de trabajo para notar animación
        usleep(work_ms * 1000);

        // mark done for GUI to animate
        s->station_done[idx] = 1;

        // wait GUI ack (GUI will sem_post ack after animation)
        if (sem_ack) {
            sem_wait(sem_ack);
        }

        // after ack, if not last station, post next stage semaphore
        if (idx + 1 < NUM_STATIONS) {
            sem_t* next = open_sem_stage(idx + 1);
            if (next) sem_post(next);
        }

        // IMPORTANT: allow station 0 to continuously create new jobs
        // by reposting its own stage semaphore after finishing.
        // This means station 0 will keep producing as long as it's not paused.
        if (idx == 0) {
            // repost its own stage so another product can start immediately
            if (sem_stage) sem_post(sem_stage);
        }

        // GUI will reset station_done[idx] to 0 after handling
    }

    munmap(s, sizeof(ShmState));
    _exit(0);
}
