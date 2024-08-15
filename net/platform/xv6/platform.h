#ifndef PLATFORM_H
#define PLATFORM_H

#include "types.h"
#include "defs.h"

#include "std.h"

/*
 * Memory
 */

#include "mmu.h"

static inline void *
memory_alloc(size_t size)
{
    void *p;

    if (PGSIZE < size) {
        return NULL;
    }
    p = kalloc();
    if (p) {
        memset(p, 0, size);
    }
    return p;
}

static inline void
memory_free(void *ptr)
{
    kfree(ptr);
}

/*
 * Mutex
 */

#include "param.h"
#include "spinlock.h"
#include "proc.h"

typedef struct spinlock mutex_t;

#define MUTEX_INITIALIZER {0}

static inline int
mutex_init(mutex_t *mutex)
{
    initlock(mutex, "");
    return 0;
}

static inline int
mutex_lock(mutex_t *mutex)
{
    acquire(mutex);
    return 0;
}

static inline int
mutex_unlock(mutex_t *mutex)
{
    release(mutex);
    return 0;
}

/*
 * Interrupt
 */

#include "traps.h"

#define INTR_IRQ_SOFTIRQ T_SOFT
#define INTR_IRQ_EVENT T_EVENT

static inline int
intr_raise_irq(unsigned int irq)
{
    asm volatile("int %0" : : "i" (irq));
    return 0;
}

static inline int
intr_init(void)
{
    return 0;
}

static inline int
intr_run(void)
{
    return 0;
}

static inline void
intr_shutdown(void)
{
    return;
}

/*
 * Scheduler
 */



#endif
