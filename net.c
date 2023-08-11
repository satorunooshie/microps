#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "platform.h"

#include "util.h"
#include "net.h"

/* NOTE: if you want to add/delete the entries after net_run(), you need to protect the list with a lock. */
static struct net_device *devices; // device list.

struct net_device *net_device_alloc(void)
{
    struct net_device *dev;

    dev = memory_alloc(sizeof(*dev));
    if (!dev) {
        errorf("memory_alloc() failed");
        return (NULL);
    }
    return (dev);
}

/* NOTE: must not be called after net_run() */
int net_device_register(struct net_device *dev)
{
    static unsigned int index = 0;

    dev->index = index++;
    // create device name(net0, net1, net2, ...).
    snprintf(dev->name, sizeof(dev->name), "net%d", dev->index);
    // append to the device list.
    dev->next = devices;
    devices = dev;
    infof("registered, dev=%s, type=0x%04x", dev->name, dev->type);
    return (0);
}

static int net_device_open(struct net_device *dev)
{
    // check if the device is already opened.
    if (NET_DEVICE_IS_UP(dev)) {
        errorf("already opened, dev=%s", dev->name);
        return (-1);
    }
    // skip if the device has no open() method.
    if (dev->ops->open) {
        if (dev->ops->open(dev) == -1) {
            errorf("open() failed, dev=%s", dev->name);
            return (-1);
        }
    }
    // set the device state to UP.
    dev->flags |= NET_DEVICE_FLAG_UP;
    infof("opened, dev=%s, state=%s", dev->name, NET_DEVICE_STATE(dev));
    return (0);
}

static int net_device_close(struct net_device *dev)
{
    // check if the device is already closed.
    if (!NET_DEVICE_IS_UP(dev)) {
        errorf("not opened, dev=%s", dev->name);
        return (-1);
    }
    // skip if the device has no close() method.
    if (dev->ops->close) {
        if (dev->ops->close(dev) == -1) {
            errorf("close() failed, dev=%s", dev->name);
            return (-1);
        }
    }
    // set the device state to DOWN.
    dev->flags &= ~NET_DEVICE_FLAG_UP;
    infof("closed, dev=%s, state=%s", dev->name, NET_DEVICE_STATE(dev));
    return (0);
}

int net_device_output(struct net_device *dev, uint16_t type, const uint8_t *data, size_t len, const void *dst)
{
    // cannot send if the device is not opened.
    if (!NET_DEVICE_IS_UP(dev)) {
        errorf("not opened, dev=%s", dev->name);
        return (-1);
    }
    // check if the packet is too large.
    // the packet size must be less than or equal to the MTU.
    // NOTE: adjust the packet size to the MTU at the upper layer.
    // MTU value differs depending on the data link and basic MTU value of Ethernet is 1500.
    if (len > dev->mtu) {
        errorf("too large packet, dev=%s, len=%zu, mtu=%u", dev->name, len, dev->mtu);
        return (-1);
    }
    debugf("dev=%s, type=0x%04x, len=%zu", dev->name, type, len);
    debugdump(data, len);
    // call the device's output function.
    if (dev->ops->transmit(dev, type, data, len, dst) == -1) {
        errorf("transmit() failed, dev=%s, len=%zu", dev->name, len);
        return (-1);
    }
    return (0);
}

// net_input_handler delivers the received packet to protocol stack.
int net_input_handler(uint16_t type, const uint8_t *data, size_t len, struct net_device *dev)
{
    // for now, it's okay to check whether this method is called from the device or not.
    debugf("dev=%s, type=0x%04x, len=%zu", dev->name, type, len);
    debugdump(data, len);
    return (0);
}

int net_run(void)
{
    struct net_device *dev;

    debugf("open all devices...");
    for (dev = devices; dev; dev = dev->next) {
        if (net_device_open(dev) == -1) {
            errorf("net_device_open() failed, dev=%s", dev->name);
        }
    }
    debugf("running...");
    return (0);
}

void net_shutdown(void)
{
    struct net_device *dev;

    debugf("close all devices...");
    for (dev = devices; dev; dev = dev->next) {
        if (net_device_close(dev) == -1) {
            errorf("net_device_close() failed, dev=%s", dev->name);
        }
    }
    debugf("shutting down...");
}

int net_init(void)
{
    infof("initialized");
    return (0);
}
