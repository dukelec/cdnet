CDNET is a high layer protocol for CDBUS
=======================================

1. [Select Format](#select-format)
2. [Basic Format](#basic-format)
3. [Standard Format](#standard-format)
4. [Port 0](#port-0)
5. [Examples](#examples)


## Select Format
First byte:

| FIELD   | DESCRIPTION                                       |
|-------- |---------------------------------------------------| 
| [7]     | 0: basic format; 1: standard format               |

All devices have to support the basic format, the standard format is optional.


## Basic Format
First byte:

| FIELD   | DESCRIPTION                                       |
|-------- |---------------------------------------------------| 
| [7]     | Always 0: basic format                            |
| [6]     | 0: request; 1: reply                              |

### Request
First byte:

| FIELD   | DESCRIPTION                                       |
|-------- |---------------------------------------------------| 
| [7]     | Always 0: basic format                            |
| [6]     | Always 0: request                                 |
| [5:0]   | dst_port, range 0~63                              |

Note: The port is equivalent to UDP port, but can be simply understood as commands.

The second byte and after: command parameters.

### Reply
First byte:

| FIELD   | DESCRIPTION                                       |
|-------- |---------------------------------------------------| 
| [7]     | Always 0: basic format                            |
| [6]     | Always 1: reply                                   |
| [5]     | 0: not share; 1: [4:0] shared as first data byte  |
| [4:0]   | Not care, or first data byte (must ≤ 31)          |

E.g.: reply `[0x40, 0x0c]` is the same as `[0x6c]`.
```
0x40: 'b0100_0000
0x0c: 'b0000_1100
----
0x6c: 'b0110_1100
```

The first byte shared part, and the second byte and after: reply status and/or datas.

## Standard Format
First byte:

| FIELD   | DESCRIPTION                                       |
|-------- |---------------------------------------------------| 
| [7]     | Always 1: standard format                         |
| [6]     | FULL_ADDR                                         |
| [5]     | MULTICAST                                         |
| [4]     | FRAGMENT                                          |
| [3]     | COMPRESSED                                        |
| [2]     | FURTHER_PROT                                      |
| [1]     | SRC_PORT_16                                       |
| [0]     | DST_PORT_16                                       |

### FULL_ADDR
0: local network, use the MAC address from frame header is enough;  
1: cross multi-network, append 4 bytes after first byte:
`[src_net_id, src_net_ip, dst_net_id, dst_net_ip]`

Note: MAC address equal to IP address in same network.

### MULTICAST
0: not multicast;  
1: multicast:  
 * If FULL_ADDR == 0: append 1 byte for `multicast-id`
 * If FULL_ADDR == 1:
   - `dst_net_id` is used for `multicast-range`, 255 for all network
   - `dst_net_ip` is used for `multicast-id`

### FRAGMENT
0: not fragment;  
1: fragment packet: append 1 byte for `fragment-id`,
bit7 of `fragment-id`: 0 means more fragments follow; 1 means last fragment.

The futher append bytes for `FURTHER_PROT`, `src_port` and `dst_port` only carried in first fragment.

### COMPRESSED
0: not compressed;  
1: compressed.

### FURTHER_PROT
0: default UDP protocol;  
1: append 1 byte for more protocols: (only used for PC)
 - 1: TCP
 - 2: ICMP

Note: Unused bits may be used to indicate the compression algorithm or other purpose.

### SRC_PORT_16
0: append 1 byte for `src_port`;  
1: append 2 bytes for `src_port` (little endian).

### DST_PORT_16
0: append 1 byte for `dst_port`;  
1: append 2 bytes for `dst_port` (little endian).


## Port 0
All device on the bus should provide the port 0 service,
which handled the common functions ralated to the bus,
e.g.: provide device info, set bond rate, set device address, software flow control and receive bus error notifications.

Tips: UDP port 0 on the PC can be mapped to other ports, e.g. port `0xcd00` in `cdbus_tun`.

## Examples

Request device info:
 - local network
 - request: `0x0c` -> `0x0d` (MAC address)
 - replay: `0x0d` -> `0x0c`

Reply the device info string: `"M: c1; S: 1234"`,
expressed in hexadecimal: `[0x4d, 0x3a, 0x20, 0x63, 0x31, 0x3b, 0x20, 0x53, 0x3a, 0x20, 0x31, 0x32, 0x33, 0x34]`.

Note: Conventions: `M: model; S: serial string; HW: hardware version; SW: software version`.

The basic format:
 * Request:
   - CDNET packet: `[0x00]` (`dst_port` = `0x00`, no arguments)
   - CDBUS frame: `[0x0c, 0x0d, 0x01, 0x00, crc_l, crc_h]`
 * Reply:
   - CDNET packet: `[0x40, 0x4d, 0x3a, 0x20 ... 0x34]` (`0x40`: first data byte not shared with the head)
   - CDBUS frame: `[0x0d, 0x0c, 0x0f, 0x40, 0x4d, 0x3a, 0x20 ... 0x34, crc_l, crc_h]`

The standard format:
 * Request: (port `0xff` is used here as an example, for PC, it's best to choose a larger port number, e.g.: `0xcdcd`)
   - CDNET packet: `[0x80, 0xff, 0x00]` (`src_port` = `0xff`, `dst_port` = `0x00`, no arguments)
   - CDBUS frame: `[0x0c, 0x0d, 0x03, 0x80, 0xff, 0x00, crc_l, crc_h]`
 * Reply:
   - CDNET packet: `[0x80, 0x00, 0xff, 0x4d, 0x3a, 0x20 ... 0x34]` (`src_port` = `0x00`, `dst_port` = `0xff`)
   - CDBUS frame: `[0x0d, 0x0c, 0x11, 0x80, 0x00, 0xff, 0x4d, 0x3a, 0x20 ... 0x34, crc_l, crc_h]`


### Code Examples

How to use this library refer to `app_skeleton.c` under `arch/*/`;  
How to control CDCTL-Bx refer to `dev/cdctl_bx.c & .h`.

