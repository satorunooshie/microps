#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

#include "platform.h"

#include "util.h"

struct irq_entry {
    struct irq_entry *next;
    unsigned int irq; // interrupt number(IRQ number).
    int (*handler)(unsigned int irq, void *dev); // interrupt handler(function pointer).
    int flags; // when set to INTR_FLAGS_SHARED, the IRQ number will be able to be shared.
    char name[16]; // identifier of interrupt for debug.
    void *dev; // the origin of interrupt(void pointer can be used for not only struct net_device).
};

/* NOTE: if you want to add/delete the entries after intr_run(), you need to protect these lists with mutext. */
static struct irq_entry *irqs; // list of interrupt entries.

static sigset_t sigmask; // signal mask for interrupt thread.

static pthread_t tid; // thread ID of interrupt thread.
static pthread_barrier_t barrier; // barrier for synchronization.

int intr_request_irq(unsigned int irq, int (*handler)(unsigned int irq, void *dev), int flags, const char *name, void *dev)
{
    struct irq_entry *entry;

    debugf("irq=%u, flags=%d, name=%s", irq, flags, name);
    for (entry = irqs; entry; entry = entry->next) {
        if (entry->irq == irq) {
            // if the IRQ number is already registered, check the flags.
            // if the flags are not shared, return error.
            if (entry->flags ^ INTR_IRQ_SHARED || flags ^ INTR_IRQ_SHARED) {
                errorf("conflicts with already registered IRQs");
                return (-1);
            }
        }
    }
    // add new entry to IRQ list.
    entry = memory_alloc(sizeof(*entry));
    if (!entry) {
        errorf("memory_alloc() failed");
        return (-1);
    }
    entry->irq = irq;
    entry->handler = handler;
    entry->flags = flags;
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->dev = dev;
    // add top of list.
    entry->next = irqs;
    irqs = entry;
    // add new signal to signal mask.
    sigaddset(&sigmask, irq);
    debugf("registered: irq=%u, name=%s", irq, name);
    return (0);
}

int intr_raise_irq(unsigned int irq)
{
    // send signal to interrupt thread.
    return pthread_kill(tid, (int)irq);
}

// entry point of interrupt thread.
// Since the signal handler, which is executed asynchronously when a signal is received, greatly limits the processing,
// a exclusive thread is started for interrupt processing to wait for the occurrence of a signal and process it.
static void *intr_thread(void *arg)
{
    int terminate = 0, sig, err;
    struct irq_entry *entry;

    debugf("start");
    // wait for all interrupt entries to be registered.
    pthread_barrier_wait(&barrier);
    while (!terminate) {
        // wait for signal.
        err = sigwait(&sigmask, &sig);
        if (err) {
            errorf("sigwait() failed: %s", strerror(err));
            break;
        }
        switch (sig) {
            case SIGHUP: // signal that notifies the end of interrupt thread.
                terminate = 1;
                break;
            default: // signal for device interrupt.
                for (entry = irqs; entry; entry = entry->next) {
                    // call handler of the entry that matches the IRQ number.
                    if (entry->irq == (unsigned int)sig) {
                        debugf("irq=%u, name=%s", entry->irq, entry->name);
                        if (entry->handler(entry->irq, entry->dev)) {
                            errorf("handler() failed");
                        }
                    }
                }
                break;
        }
    }
    debugf("terminated");
    return (NULL);
}

int intr_run(void)
{
    int err;

    // setting for signal mask.
    err = pthread_sigmask(SIG_BLOCK, &sigmask, NULL);
    if (err) {
        errorf("pthread_sigmask() failed: %s", strerror(err));
        return (-1);
    }
    // run interrupt thread.
    err = pthread_create(&tid, NULL, intr_thread, NULL);
    if (err) {
        errorf("pthread_create() failed: %s", strerror(err));
        return (-1);
    }
    // wait for interrupt thread to be ready.
    // (stop thread untill barrier count reaches the number of threads that wait for barrier.)
    pthread_barrier_wait(&barrier);
    return (0);
}

void intr_shutdown(void)
{
    if (pthread_equal(tid, pthread_self()) != 0) {
        // thread not created.
        return;
    }
    // send signal to interrupt thread.
    pthread_kill(tid, SIGHUP);
    // wait for interrupt thread to be terminated.
    pthread_join(tid, NULL);
}

int intr_init(void)
{
    // set main thread ID as initial value of interrupt thread ID.
    tid = pthread_self();
    // initialize barrier and set count 2.
    pthread_barrier_init(&barrier, NULL, 2);
    // initialize signal mask.
    sigemptyset(&sigmask);
    // add SIGHUP for notifying the end of interrupt thread.
    sigaddset(&sigmask, SIGHUP);
    return (0);
}
