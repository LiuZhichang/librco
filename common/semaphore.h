//
// Created by root on 2022/4/1.
//

#ifndef RCO_SEMAPHORE_H
#define RCO_SEMAPHORE_H

#include <semaphore.h>
#include <cstdint>
#include <cassert>

class Semaphore {
public:
    Semaphore(uint32_t count = 0) {
        assert(sem_init(&semaphore, 0, count) == 0);
    }

    ~Semaphore() {
        sem_destroy(&semaphore);
    }

    void wait() {
        assert(sem_wait(&semaphore) == 0);
    }

    void notify() {
        assert(sem_post(&semaphore) == 0);
    }

private:
    sem_t semaphore;
};


#endif //RCO_SEMAPHORE_H
