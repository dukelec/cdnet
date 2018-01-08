CDNET is a high layer protocol for CDBUS
=======================================

1. [CDNET Levels](#cdnet-levels)
2. [Level 0 Format](#level-0-format)
3. [Level 1 Format](#level-1-format)
4. [Level 2 Format](#level-2-format)
5. [Port 0](#port-0)
6. [Examples](#examples)


## CDNET Levels

CDNET protocol has three different levels, select by bit7 and bit6 of first byte:

| bit7 | bit6   | DESCRIPTION                                                   |
|------|------- |---------------------------------------------------------------|
| 0    | x      | Level 0: The simplest one, for single network communication   |
| 1    | 0      | Level 1: Support cross network and multi-cast communication   |
| 1    | 1      | Level 2: Raw TCP/IP communication between PCs                 |

All devices have to support the Level 0, the Level 1 and Level 2 are optional.

## Level 0 Format
First byte:

| FIELD   | DESCRIPTION                                       |
|-------- |---------------------------------------------------| 
| [7]     | Always 0: Level 0                                 |
| [6]     | 0: request; 1: reply                              |

### Request
First byte:

| FIELD   | DESCRIPTION                                       |
|-------- |---------------------------------------------------| 
| [7]     | Always 0: Level 0                                 |
| [6]     | Always 0: request                                 |
| [5:0]   | dst_port, range 0~63                              |

Note: The port is equivalent to UDP port, but can be simply understood as commands.

The second byte and after: command parameters.

### Reply
First byte:

| FIELD   | DESCRIPTION                                       |
|-------- |---------------------------------------------------| 
| [7]     | Always 0: Level 0                                 |
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

## Level 1 Format
First byte:

| FIELD   | DESCRIPTION                                       |
|-------- |---------------------------------------------------| 
| [7]     | Always 1                                          |
| [6]     | Always 0                                          |
| [5]     | MULTI_NET                                         |
| [4]     | MULTICAST                                         |
| [3]     | Reserved                                          |
| [2:0]   | PORT_SIZE                                         |

Notes: All fields reserved in this document must be 0.

### MULTI_NET & MULTICAST

| MULTI_NET | MULTICAST | DESCRIPTION                                                                       |
|-----------|-----------|-----------------------------------------------------------------------------------| 
| 0         | 0         | Local net: append 0 byte                                                          |
| 0         | 1         | Local net multicast: append 2 bytes `[multicast-id]`                              |
| 1         | 0         | Cross net: append 4 bytes: `[src_net, src_id, dst_net, dst_id]`                   |
| 1         | 1         | Cross net multicast: append 4 bytes: `[src_net, src_id, multicast-id]`            |

Notes:
 - Address from frame header equal to the `id` in same network.
 - Broadcast could simply not use the MULTICAST bit.

When communication with PC:
 - The `net` is equal to byte 8 of IPv6 Unique Local Address.
 - The `id` is equal to byte 16 of IPv6 Unique Local Address.

### PORT_SIZE:

| bit2 | bit1 | bit0   | SRC_PORT      | DST_PORT      |
|------|------|--------|-------------------------------|
| 0    | 0    | 0      | default port  | 1 byte        |
| 0    | 0    | 1      | default port  | 2 bytes       |
| 0    | 1    | 0      | 1 byte        | default port  |
| 0    | 1    | 1      | 2 bytes       | default port  |
| 1    | 0    | 0      | 1 byte        | 1 byte        |
| 1    | 0    | 1      | 1 byte        | 2 bytes       |
| 1    | 1    | 0      | 2 bytes       | 1 byte        |
| 1    | 1    | 1      | 2 bytes       | 2 bytes       |

Notes:
 - Default port is `0xcdcd` for convention, it does not take up space,
otherwise append 1 or 2 byte(s) for the src and dst port.
 - CDNET is little endian.


## Level 2 Format
First byte:

| FIELD   | DESCRIPTION                                       |
|-------- |---------------------------------------------------| 
| [7]     | Always 1                                          |
| [6]     | Always 1                                          |
| [5]     | FRAGMENT                                          |
| [4]     | FRAGMENT_END                                      |
| [3]     | COMPRESSED                                        |
| [2:0]   | Reserved                                          |

### FRAGMENT
0: not fragment;  
1: fragment packet: append 1 byte for `fragment-id`.

### FRAGMENT_END:
0: more fragments follow;  
1: last fragment.


## Port 0
All device on the bus should provide the port 0 service,
which handled the common functions ralated to the bus,
e.g.: provide device info, set bond rate, set device address, software flow control and receive bus error notifications.

Tips: UDP port 0 on the PC can be mapped to other ports, e.g. port `0xcd00` in `cdbus_tun`.

## Examples

Request device info:
 - local network
 - request: `0x0c` -> `0x0d` (MAC address)
 - reply: `0x0d` -> `0x0c`

Reply the device info string: `"M: c1; S: 1234"`,
expressed in hexadecimal: `[0x4d, 0x3a, 0x20, 0x63, 0x31, 0x3b, 0x20, 0x53, 0x3a, 0x20, 0x31, 0x32, 0x33, 0x34]`.

Note: Conventions: `M: model; S: serial string; HW: hardware version; SW: software version`.

The Level 0 Format:
 * Request:
   - CDNET packet: `[0x00]` (`dst_port` = `0x00`, no arguments)
   - CDBUS frame: `[0x0c, 0x0d, 0x01, 0x00, crc_l, crc_h]`
 * Reply:
   - CDNET packet: `[0x40, 0x4d, 0x3a, 0x20 ... 0x34]` (`0x40`: first data byte not shared with the head)
   - CDBUS frame: `[0x0d, 0x0c, 0x0f, 0x40, 0x4d, 0x3a, 0x20 ... 0x34, crc_l, crc_h]`

The Level 1 Format:
 * Request:
   - CDNET packet: `[0x80, 0x00]` (`src_port` = default, `dst_port` = `0x00`, no arguments)
   - CDBUS frame: `[0x0c, 0x0d, 0x02, 0x80, 0x00, crc_l, crc_h]`
 * Reply:
   - CDNET packet: `[0x82, 0x00, 0x4d, 0x3a, 0x20 ... 0x34]` (`src_port` = `0x00`, `dst_port` = default)
   - CDBUS frame: `[0x0d, 0x0c, 0x10, 0x82, 0x00, 0x4d, 0x3a, 0x20 ... 0x34, crc_l, crc_h]`


### Code Examples

How to use this library refer to `app_skeleton.c` under `arch/*/`;  
How to control CDCTL-Bx refer to `dev/cdctl_bx.c & .h`.

