#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include "util.h"
#include "net.h"

#include "driver/loopback.h"

#include "test.h"

static volatile sig_atomic_t terminate;

// asynchronous signal handler.
// https://www.jpcert.or.jp/sc-rules/c-sig30-c.html
// write volatile sig_atomic_t.
static void on_signal(int s)
{
    (void)s;
    terminate = 1;
}

int main(int argc, char *argv[])
{
    struct net_device *dev;

    // set signal handler.
    signal(SIGINT, on_signal);
    // initialize protocol stack.
    if (net_init() == -1) {
        errorf("net_init() failed");
        return (-1);
    }
    dev = loopback_init();
    if (!dev) {
        errorf("loopback_init() failed");
        return (-1);
    }
    // start protocol stack.
    if (net_run() == -1) {
        errorf("net_run() failed");
        return (-1);
    }
    // when CTRL+C is pressed, terminate.
    while (!terminate) {
        // write packet to device every 1 second.
        // for now, cannot generate packet, so use test data.
        if (net_device_output(dev, 0x0800, test_data, sizeof(test_data), NULL) == -1) {
            errorf("net_device_output() failed");
            break;
        }
        sleep(1);
    }
    // shutdown protocol stack.
    net_shutdown();
    return (0);
}
