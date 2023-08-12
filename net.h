#ifndef NET_H
#define NET_H

#include <stddef.h>
#include <stdint.h>

#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif

#define NET_DEVICE_TYPE_DUMMY    0x0000
#define NET_DEVICE_TYPE_LOOPBACK 0x0001
#define NET_DEVICE_TYPE_ETHERNET 0x0002

#define NET_DEVICE_FLAG_UP          0x0001
#define NET_DEVICE_FLAG_LOOPBACK    0x0010
#define NET_DEVICE_FLAG_BROADCAST   0x0020
#define NET_DEVICE_FLAG_POINTOPOINT 0x0030
#define NET_DEVICE_FLAG_NEED_ARP    0x0100

#define NET_DEVICE_ADDR_LEN 16

#define NET_DEVICE_IS_UP(x) ((x)->flags & NET_DEVICE_FLAG_UP)
#define NET_DEVICE_STATE(x) (NET_DEVICE_IS_UP(x) ? "UP" : "DOWN")

struct net_device {
    struct net_device *next; // next device in the list.
    unsigned int index;
    char name[IFNAMSIZ];
    uint16_t type; // device type reference to NET_DEVICE_TYPE_*.
    // device state.
    uint16_t mtu; // maximum transmission unit.
    uint16_t flags; // device flags reference to NET_DEVICE_FLAG_*.
    uint16_t addr_len;
    uint16_t header_len;
    // hardware address.
    uint8_t addr[NET_DEVICE_ADDR_LEN];
    // address size is different for different device type, so we use big enough buffer to store it.
    // device without address won't set value.
    union {
        uint8_t peer[NET_DEVICE_ADDR_LEN];
        uint8_t broadcast[NET_DEVICE_ADDR_LEN];
    };
    struct net_device_ops *ops;
    void *priv; // private data.
};

// store function pointer for device operations.
// transmit function is required, others are optional.
struct net_device_ops {
    int (*open)(struct net_device *dev);
    int (*close)(struct net_device *dev);
    int (*transmit)(struct net_device *dev, uint16_t type, const uint8_t *data, size_t len, const void *dst);
};

extern struct net_device *net_device_alloc(void);
extern int net_device_register(struct net_device *dev);
extern int net_device_output(struct net_device *dev, uint16_t type, const uint8_t *data, size_t len, const void *dst);

extern int net_input_handler(uint16_t type, const uint8_t *data, size_t len, struct net_device *dev);

extern int net_run(void);
extern void net_shutdown(void);
extern int net_init(void);

#endif
