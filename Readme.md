CDNET: An Optional High-Layer Protocol for CDBUS
=======================================

Protocol status: Stable [Version 1.0]

The CDBUS frame containing a CDNET package is as follows:  
`[src, dst, len] + [CDNET package] + [crc_l, crc_h]`  
`[src, dst, len] + [CDNET header, payload] + [crc_l, crc_h]`


## CDNET Levels

CDNET protocol offers two levels, determined by bit7 of the first header byte:

| Bit7 | DESCRIPTION                                                   |
|------|---------------------------------------------------------------|
| 0    | Level 0: Simplified communication                             |
| 1    | Level 1: Standard communication                               |

You can select one or more levels for your application as required.

The CDNET is little endian.


## Level 0 Format

### Request
A single-byte header:

| FIELD   | DESCRIPTION                                       |
|-------- |---------------------------------------------------|
| [7]     | Always 0: Level 0                                 |
| [6]     | Always 0: request                                 |
| [5:0]   | dst_port, range 0 ~ 0x3f                          |

Note: The port is analogous to UDP port and can be thought of simply as command.

### Reply
A single-byte header:

| FIELD   | DESCRIPTION                                       |
|-------- |---------------------------------------------------|
| [7]     | Always 0: Level 0                                 |
| [6]     | Always 1: reply                                   |
| [5:0]   | Low bits of first payload byte                    |

The first payload byte omits bits [7:6], which were intended to be `b10`.

E.g.: Header `0x42` implies `0x82` as the first byte of the payload.



## Level 1 Format
The first byte of the header:

| FIELD   | DESCRIPTION                     |
|-------- |---------------------------------|
| [7]     | Always 1                        |
| [5]     | MULTI_NET                       |
| [4]     | MULTICAST                       |
| [2:1]   | PORT_SIZE                       |
| [6][3][0] | _Reserved as 0_               |

### MULTI_NET & MULTICAST

| MULTI_NET | MULTICAST | DESCRIPTION                                                               |
|-----------|-----------|---------------------------------------------------------------------------|
| 0         | 0         | Local net: append 0 byte                                                  |
| 0         | 1         | Local net multicast: append 2 bytes `[mh, ml]`                            |
| 1         | 0         | Cross net: append 4 bytes: `[src_net, src_mac, dst_net, dst_mac]`         |
| 1         | 1         | Cross net multicast: append 4 bytes: `[src_net, src_mac, mh, ml]`         |

Notes:
 - mh + ml: multicast_id, ml is mapped to mac layer multicast address (h: high byte, l: low byte);
 - Could simply use MULTI_NET = 0 and MULTICAST = 0 for local net multicast and broadcast.
 - Implementations of MULTI_NET and MULTICAST are optional.

### PORT_SIZE:

| Bit2 | Bit1 | SRC_PORT      | DST_PORT      |
|------|------|---------------|---------------|
| 0    | 0    | Default port  | 1-byte        |
| 0    | 1    | 1-byte        | Default port  |
| 1    | 0    | 1-byte        | 1-byte        |
| 1    | 1    | 2-byte        | 2-byte        |

Notes:
 - The default port is `0xcdcd` for convention and doesn't consume space.
 - Append bytes for `src_port` before `dst_port`.
 - Implementation of 2-byte port is optional.



## Recommendation for first byte of payload

For Request and Report, the first byte definition:

| FIELD   | DESCRIPTION                         |
|-------- |-------------------------------------|
| [7]     | is_reply, always 0                  |
| [6]     | not_reply                           |
| [5:0]   | sub command                         |

For Reply, the first byte definition:

| FIELD   | DESCRIPTION                     |
|-------- |---------------------------------|
| [7]     | is_reply, always 1              |
| [6:0]   | status, 0 means no error        |



## Port Allocation Recommendation

Ports 0 to 0xf are recommended for general purposes.
Specifically, port 1 is designated for device information queries.
While all ports in this section are optional, it is advisable to implement the basic function of port 1 (read device_info).


### Port 1

Provide device info.
```
Types:
 - mac_start and mac_end: uint8_t
 - max_time: uint16_t (unit: ms)
 - "string": variable-length string, including empty string
   (excluding the terminating null byte '\0')


Read device_info string:
  Write [0x00]
  Return [0x80, "device_info"]

Search devices by filters (for resolving mac conflicts):
  Write [0x10, max_time, mac_start, mac_end, "string"]
  Return [0x80, "device_info"] after a random time in the range [0, max_time]
    only if "device_info" contains "string" (always true for an empty string)
    and the current mac address is in the range [mac_start, mac_end] (inclusive)
  Not return otherwise
    and reject any subsequent modify mac (or save config) commands
```
Example of `device_info`:  
  `M: model; S: serial id; HW: hardware version; SW: software version` ...  
Field order is not important; it is recommended to include the model field at least.



## CDNET Address String Formats

```
             local link     unique local    multicast
level0:       00:NN:MM
level1:       80:NN:MM        a0:NN:MM       f0:MH:ML
```

Notes:
  - NN: net_id, MM: mac_addr, MH+ML: multicast_id (H: high byte, L: low byte).
  - The string address is analogous to IPv6 address.


## Examples

Request device info:  
(Local network, requester's mac address: `0x0c`, target's mac address: `0x0d`)

Reply the device info string: `"M: c1; S: 1234"`,
expressed in hexadecimal: `[0x4d, 0x3a, 0x20, 0x63, 0x31, 0x3b, 0x20, 0x53, 0x3a, 0x20, 0x31, 0x32, 0x33, 0x34]`.

The Level 0 Format:
 * Request:
   - CDNET socket: `[00:00:0c]:0xcdcd` -> `[00:00:0d]:1`: `0x00` (net id: `0`)
   - CDNET packet: `[0x01,  0x00]` (`dst_port`: `0x01`)
   - CDBUS frame: `[0x0c, 0x0d, 0x02,  0x01, 0x00,  crc_l, crc_h]`
 * Reply:
   - CDNET socket: `[00:00:0d]:1` -> `[00:00:0c]:0xcdcd`: `0x80` + `"M: c1; S: 1234"`
   - CDNET packet: `[0x40, 0x4d, 0x3a, 0x20 ... 0x34]` (first `0x40` implies payload byte `0x80`)
   - CDBUS frame: `[0x0d, 0x0c, 0x0f,  0x40, 0x4d, 0x3a, 0x20 ... 0x34,  crc_l, crc_h]`

The Level 1 Format:
 * Request:
   - CDNET socket: `[80:00:0c]:0xcdcd` -> `[80:00:0d]:1`: `0x00`
   - CDNET packet: `[0x80, 0x01,  0x00]` (`src_port`: default, `dst_port`: `0x01`)
   - CDBUS frame: `[0x0c, 0x0d, 0x03,  0x80, 0x01, 0x00,  crc_l, crc_h]`
 * Reply:
   - CDNET socket: `[80:00:0d]:1` -> `[80:00:0c]:0xcdcd`: `0x80` + `"M: c1; S: 1234"`
   - CDNET packet: `[0x82, 0x01,  0x80, 0x4d, 0x3a, 0x20 ... 0x34]` (`src_port`: `0x01`, `dst_port`: default)
   - CDBUS frame: `[0x0d, 0x0c, 0x11,  0x82, 0x01, 0x80, 0x4d, 0x3a, 0x20 ... 0x34,  crc_l, crc_h]`



### Sequence & Flow Control

For transferring large data, such as transmitting a jpg image in the `cdcam` project, the sub command is defined as follows:

```
[5:4] FRAGMENT: 00: not fragment, 01: first, 10: more, 11: last, [3:0]: cnt
```

Here, the `cnt` value corresponds to 0 for the first packet, and increments by 1 for each subsequent packet.

If flow control is required, for example, we can group every 12 packets as a transmission set,
where only the last packet in each set has `not_reply` set to 0. Start by transmitting 2 sets of packets,
wait for a reply regarding the last packet in the first set, and then append another set of packets.

For sub commands that do not have a `cnt` definition like the one mentioned above,
if we want to ensure the uniqueness of a command, such as ensuring that a command can only be executed once,
we can also achieve this by cyclically sending commands through multiple source port numbers.

This way, if a command does not receive a reply, resend the command.
The receiving port checks whether the source port number of the received command matches that of the previous command;
if they match, the command is not executed again.

