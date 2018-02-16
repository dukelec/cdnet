CDNET is a high layer protocol for CDBUS
=======================================

1. [CDNET Levels](#cdnet-levels)
2. [Level 0 Format](#level-0-format)
3. [Level 1 Format](#level-1-format)
4. [Level 2 Format](#level-2-format)
5. [Specific Ports](#specific-ports)
6. [Examples](#examples)


## CDNET Levels

CDNET protocol has three different levels, select by bit7 and bit6 of first byte:

| bit7 | bit6   | DESCRIPTION                                                   |
|------|------- |---------------------------------------------------------------|
| 0    | x      | Level 0: The simplest one, for single network communication   |
| 1    | 0      | Level 1: Support cross network and multi-cast communication   |
| 1    | 1      | Level 2: Raw, e.g. TCP/IP communication between PCs           |

You can select one or more levels for your application as needed.

The CDNET is little endian.


## Level 0 Format

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
| [3]     | SEQ_NO                                            |
| [2:0]   | PORT_SIZE                                         |

Notes: All fields reserved in this document must be 0.

### MULTI_NET & MULTICAST

| MULTI_NET | MULTICAST | DESCRIPTION                                                                       |
|-----------|-----------|-----------------------------------------------------------------------------------| 
| 0         | 0         | Local net: append 0 byte                                                          |
| 0         | 1         | Local net multicast: append 2 bytes `[multicast-id]`                              |
| 1         | 0         | Cross net: append 4 bytes: `[src_net, src_mac, dst_net, dst_mac]`                 |
| 1         | 1         | Cross net multicast: append 4 bytes: `[src_net, src_mac, multicast-id]`           |

Notes:
 - Broadcast could simply not use the MULTICAST bit.

When communication with PC:
 - The `net` is mapped to byte 8 of IPv6 Unique Local Address.
 - The `mac` is mapped to byte 16 of IPv6 Unique Local Address.

### SEQ_NO
0: no sequence number;  
1: append 1 byte sequence number, see [Port 0](#port-0).


### PORT_SIZE:

| bit2 | bit1 | bit0   | SRC_PORT      | DST_PORT      |
|------|------|--------|---------------|---------------|
| 0    | 0    | 0      | default port  | 1 byte        |
| 0    | 0    | 1      | default port  | 2 bytes       |
| 0    | 1    | 0      | 1 byte        | default port  |
| 0    | 1    | 1      | 2 bytes       | default port  |
| 1    | 0    | 0      | 1 byte        | 1 byte        |
| 1    | 0    | 1      | 1 byte        | 2 bytes       |
| 1    | 1    | 0      | 2 bytes       | 1 byte        |
| 1    | 1    | 1      | 2 bytes       | 2 bytes       |

Notes:
 - Default port is `0xcdcd` for convention, it does not take up space.
 - Append bytes for `src_port` first then `dst_port`.


## Level 2 Format
First byte:

| FIELD   | DESCRIPTION                                       |
|-------- |---------------------------------------------------| 
| [7]     | Always 1                                          |
| [6]     | Always 1                                          |
| [5]     | FRAGMENT                                          |
| [4]     | FRAGMENT_END                                      |
| [3]     | SEQ_NO                                            |
| [2]     | COMPRESSED                                        |
| [1:0]   | Reserved                                          |

### FRAGMENT
0: not fragment;  
1: fragment packet, must select `SEQ_NO`.

### FRAGMENT_END:
0: more fragments follow;  
1: last fragment.

### SEQ_NO
0: no sequence number;  
1: append 1 byte sequence number, see [Port 0](#port-0).


## Specific Ports

Ports 0 through 9 are cdnet-specific (undefined ports are reserved),
Port 10 and later are freely assigned by user.

All specific ports are optional,
and user can only implement some of the port features.
It is recommended to implement at least the basic part of port 1 (device info).

### Port 0

Used together with header's `SEQ_NO` for flow control and ensure data integrity.  
Bit 7 of `SEQ_NO` is set indicating that an `ACK` is required.  
The `SEQ_NO[6:0]` auto increase when `SEQ_NO` is selected in the header.  
Do not select `SEQ_NO` for port 0 communication.

Port 0 communications:
```
Write [] (empty) to port 0: check the RX free space and the SEQ_NO corresponding to the requester,
return: [FREE_PKT, CUR_SEQ_NO] (2 bytes), CUR_SEQ_NO bit 7 indicates that there is no record.

Write [0x00, SET_SEQ_NO] to port 0: set the SEQ_NO which corresponding to the requester,
return: [] (empty).

Report [0x80, FREE_PKT, ACK_SEQ_NO] to port 0 if ACK is required,
no return.

Report [0x81, FREE_PKT, CUR_SEQ_NO] to port 0 if wrong sequence detected and droped (optional),
no return.
```

Example: (Device A send packets to device B)

Device B maintain a `SEQ_NO` record list for each remote device, when the list is full, drop the oldest record.

Before the first transmission, or the record has been dropped, device A should init the `SEQ_NO` for B:
```
Send [0x00, 0x00] from A default port to B port 0,
Return [] (empty) from B port 0 to A default port.
```

Send multipule packets from A to B:
```
[0x88, 0x00, ...] // first byte: level 1 header, second byte: SEQ_NO
[0x88, 0x01, ...]
...
[0x88, 0x0e, ...]
[0x88, 0x8f, ...] // require ACK
  
[0x88, 0x10, ...]
[0x88, 0x11, ...]
...
[0x88, 0x9f, ...] // require ACK

Wait for first ACK before continue send.
```

Return `ACK` from B to A:
```
Send [0x80, FREE_PKT, 0x0f] from B default port to A port 0.
```

### Port 1

Provide device info.

Write `[]` (empty) to port 1: check `device_info` string,  
Return `device_info` string: (any sequence, must contain at least model field)  
 - conventions: `M: model; S: serial string; HW: hardware version; SW: software version`.

Write `filter_string` to port 1: search device by string, (optional)  
Return `device_info` string if `device_info` contain `filter_string`.


### Port 2

Set device baud rate.
```
Type of baud_rate is uint32_t, e.g. 115200;
Type of interface is uint8_t.

Write [0x00, baud_rate]: set current interface to same baud rate, or
Write [0x00, baud_rate_low, baud_rate_high]: set current interface to multi baud rate,
return: [] (empty) // change baud rate after return

Write [0x08, interface, baud_rate], or
Write [0x08, interface, baud_rate_low, baud_rate_high]: set baud rate for the interface,
return: [] (empty)

Write [0x08, interface]: check baud rate of the interface,
return: [baud_rate] or [baud_rate_low, baud_rate_high]
```

### Port 3

Set device address.
```
Type of mac and net is uint8_t;
Type of intf is uint8_t;
Type of max_time_ms is uint8_t.

Write [0x00, mac]: change mac address of current interface,
return: [] (empty) // change mac address after return

Write [0x01, net]: change net id of current interface,
return: [] (empty) // change net id after return

Write [0x01]: check net id of current interface,
return: [net]

Write [0x08, intf, mac]: set mac address for the interface,
return: [] (empty)

Write [0x09, intf, net]: set net id for the interface,
return: [] (empty)

Write [0x08, intf]: check mac address of the interface,
return: [mac]

Write [0x09, intf]: check net id of the interface,
return: [net]

For mac address auto allocation:

Write [0x00, select_start, select_end, max_time_ms]:
  if current mac address in the range [select_start, select_end], then
    after wait a random time in the range [0, max_time_ms]:
      (if detect bus busy during wait, re-generate the random time and re-wait after bus idle)
      return the device_info string
  else
    ignore

Write [0x00, select_start, select_end, target_start, target_end]:
  if current address in the range [select_start, select_end], then
    update current interface to a random mac address between [target_start, target_end]
  else
    ignore
no return.
```


## Examples

Request device info:
 - local network
 - request: `0x0c` -> `0x0d` (MAC address)
 - reply: `0x0d` -> `0x0c`

Reply the device info string: `"M: c1; S: 1234"`,
expressed in hexadecimal: `[0x4d, 0x3a, 0x20, 0x63, 0x31, 0x3b, 0x20, 0x53, 0x3a, 0x20, 0x31, 0x32, 0x33, 0x34]`.

The Level 0 Format:
 * Request:
   - CDNET packet: `[0x01]` (`dst_port` = `0x01`, no arguments)
   - CDBUS frame: `[0x0c, 0x0d, 0x01, 0x00, crc_l, crc_h]`
 * Reply:
   - CDNET packet: `[0x40, 0x4d, 0x3a, 0x20 ... 0x34]` (`0x40`: first data byte not shared with the head)
   - CDBUS frame: `[0x0d, 0x0c, 0x0f, 0x40, 0x4d, 0x3a, 0x20 ... 0x34, crc_l, crc_h]`

The Level 1 Format:
 * Request:
   - CDNET packet: `[0x80, 0x01]` (`src_port` = default, `dst_port` = `0x01`, no arguments)
   - CDBUS frame: `[0x0c, 0x0d, 0x02, 0x80, 0x01, crc_l, crc_h]`
 * Reply:
   - CDNET packet: `[0x82, 0x01, 0x4d, 0x3a, 0x20 ... 0x34]` (`src_port` = `0x01`, `dst_port` = default)
   - CDBUS frame: `[0x0d, 0x0c, 0x10, 0x82, 0x01, 0x4d, 0x3a, 0x20 ... 0x34, crc_l, crc_h]`


### Code Examples

How to use this library refer to `cdnet_bridge` and `cdnet_tun` project;  
How to control CDCTL-Bx refer to `dev/cdctl_bx_xxx`.

