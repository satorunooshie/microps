#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "platform.h"

#include "util.h"
#include "net.h"

#define DUMMY_MTU UINT16_MAX /* maximum size of IP datagram */

#define DUMMY_IRQ INTR_IRQ_BASE

static int dummy_transmit(struct net_device *dev, uint16_t type, const uint8_t *data, size_t len, const void *dst)
{
    debugf("dev=%s, type=0x%04x, len=%zu\n", dev->name, type, len);
    debugdump(data, len);
    // drop data.
    intr_raise_irq(DUMMY_IRQ);
    return (0);
}

static int dummy_isr(unsigned int irq, void *id)
{
    // for now, it's okay to check whether this method is called from the device or not.
    debugf("irq=%u, dev=%s", irq, ((struct net_device *)id)->name);
    return (0);
}

// set pointer of function implemented by test driver.
static struct net_device_ops dummy_ops = {
    .transmit = dummy_transmit, // set only transmit function.
};

// input: none, never receive data.
// output: drop data.
struct net_device *dummy_init(void)
{
    struct net_device *dev;

    // generate device.
    dev = net_device_alloc();
    if (!dev) {
        errorf("net_device_alloc() failed");
        return (NULL);
    }
    dev->type = NET_DEVICE_TYPE_DUMMY;
    dev->mtu = DUMMY_MTU;
    dev->header_len = 0;
    dev->addr_len = 0;
    dev->ops = &dummy_ops;
    // register device.
    if (net_device_register(dev) == -1) {
        errorf("net_device_register() failed");
        return (NULL);
    }
    intr_request_irq(DUMMY_IRQ, dummy_isr, INTR_IRQ_SHARED, dev->name, dev);
    debugf("initialized, dev=%s", dev->name);
    return (dev);
}
