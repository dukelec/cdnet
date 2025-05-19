CDNET: An Optional High-Layer Protocol for CDBUS
=======================================

Protocol status: Stable [Version 2.0]

A CDBUS frame carrying a CDNET packet is structured as follows:  
`[src, dst, len] + [CDNET package] + [crc_l, crc_h]`  
or equivalently:  
`[src, dst, len] + [CDNET header, payload] + [crc_l, crc_h]`


## CDNET Levels

CDNET defines two protocol levels, determined by bit 7 of the first header byte:

| Bit7 | DESCRIPTION                                                   |
|------|---------------------------------------------------------------|
| 0    | Level 0: Simplified communication                             |
| 1    | Level 1: Standard communication                               |

Applications may support either or both levels, depending on requirements.

CDNET uses little-endian byte order.


## Level 0 Format

The header is always 2 bytes.

First byte:

| FIELD   | DESCRIPTION                                       |
|-------- |---------------------------------------------------|
| [7]     | Always 0 (indicates level 0)                      |
| [6:0]   | src_port, range 0 ~ 127                           |

Second byte:

| FIELD   | DESCRIPTION                                       |
|-------- |---------------------------------------------------|
| [7]     | _Reserved as 0_                                   |
| [6:0]   | dst_port, range 0 ~ 127                           |


Note: The port number is similar to the UDP port number.


## Level 1 Format
The first byte of the header:

| FIELD   | DESCRIPTION                     |
|-------- |---------------------------------|
| [7]     | Always 1 (indicates level 1)    |
| [6]     | _Reserved as 0_                 |
| [5]     | MULTI_NET                       |
| [4]     | MULTICAST                       |
| [3:2]   | _Reserved as 0_                 |
| [1]     | SRC_PORT_SIZE                   |
| [0]     | DST_PORT_SIZE                   |

### MULTI_NET & MULTICAST

| MULTI_NET | MULTICAST | DESCRIPTION                                                               |
|-----------|-----------|---------------------------------------------------------------------------|
| 0         | 0         | Local net: append 0 byte                                                  |
| 0         | 1         | Local net multicast: append 2 bytes `[mh, ml]`                            |
| 1         | 0         | Cross net: append 4 bytes: `[src_net, src_mac, dst_net, dst_mac]`         |
| 1         | 1         | Cross net multicast: append 4 bytes: `[src_net, src_mac, mh, ml]`         |

Notes:
 - mh/ml: multicast_id, ml is mapped to mac layer multicast address (h: high byte, l: low byte);
 - Could simply use MULTI_NET = 0 and MULTICAST = 0 for local net multicast and broadcast.
 - Implementations of MULTI_NET and MULTICAST are optional.

### PORT_SIZE:

| Bit1 | SRC_PORT      |
|------|---------------|
| 0    | 1-byte        |
| 1    | 2-byte        |

| Bit0 | DST_PORT      |
|------|---------------|
| 0    | 1-byte        |
| 1    | 2-byte        |

Notes:
 - Append bytes for `src_port` before `dst_port`.
 - Implementation of 2-byte port is optional.


## Port Allocation Recommendation

Ports 0 to 15 are recommended for general-purpose use.  
Specifically, port 1 is designated for device information queries.  
While the port assignments mentioned in this section are optional, it is recommended to implement the basic function of port 1 (`read_device_info`).

Port numbers ≥ 64 are typically used as ephemeral ports.


### Port 1

Provide device info.
```
Types:
 - mac_start and mac_end: uint8_t
 - max_time: uint16_t (unit: ms)
 - "string": variable-length string, including empty string
   (excluding the terminating null byte '\0')


Read device_info string:
  Write None
  Return ["device_info"]

Search devices by filters (for resolving mac conflicts):
  Write [0x10, max_time, mac_start, mac_end, "string"]
  Return ["device_info"] after a random time in the range [0, max_time]
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
           localhost      local link     unique local    multicast
            10:00:00
level0:                    00:NN:MM
level1:                    80:NN:MM        a0:NN:MM       f0:MH:ML
```

Notes:
  - NN: net_id, MM: mac_addr, MH/ML: multicast_id (H: high byte, L: low byte).
  - The string address is analogous to IPv6 address.


## Examples

Request device info:  
(Local network, requester's mac address: `0x0c`, target's mac address: `0x0d`)

Reply the device info string: `"M: c1; S: 1234"`,
expressed in hexadecimal: `[0x4d, 0x3a, 0x20, 0x63, 0x31, 0x3b, 0x20, 0x53, 0x3a, 0x20, 0x31, 0x32, 0x33, 0x34]`.

The Level 0 Format:
 * Request:
   - CDNET socket: `[00:00:0c]:64` -> `[00:00:0d]:1`: `None`
   - CDNET packet: `[0x40, 0x01]` (`src_port`: `0x40`, `dst_port`: `0x01`)
   - CDBUS frame: `[0x0c, 0x0d, 0x02,  0x40, 0x01,  crc_l, crc_h]`
 * Reply:
   - CDNET socket: `[00:00:0d]:1` -> `[00:00:0c]:64`: `"M: c1; S: 1234"`
   - CDNET packet: `[0x01, 0x40,  0x4d, 0x3a, 0x20 ... 0x34]`
   - CDBUS frame: `[0x0d, 0x0c, 0x10,  0x01, 0x40,  0x4d, 0x3a, 0x20 ... 0x34,  crc_l, crc_h]`

The Level 1 Format:
 * Request:
   - CDNET socket: `[80:00:0c]:64` -> `[80:00:0d]:1`: `None`
   - CDNET packet: `[0x80, 0x40, 0x01]` (`src_port`: `0x40`, `dst_port`: `0x01`)
   - CDBUS frame: `[0x0c, 0x0d, 0x03,  0x80, 0x40, 0x01,  crc_l, crc_h]`
 * Reply:
   - CDNET socket: `[80:00:0d]:1` -> `[80:00:0c]:64`: `"M: c1; S: 1234"`
   - CDNET packet: `[0x80, 0x01, 0x40,  0x4d, 0x3a, 0x20 ... 0x34]` (`src_port`: `0x01`, `dst_port`: `0x40`)
   - CDBUS frame: `[0x0d, 0x0c, 0x11,  0x80, 0x01, 0x40,  0x4d, 0x3a, 0x20 ... 0x34,  crc_l, crc_h]`


### Sequence & Flow Control

For different commands, `SRC_PORT` can increment sequentially within a defined range. This allows the receiver to avoid re-executing commands during retransmissions and ensures correct matching between requests and responses.

For large data transfers, a high bit in `SRC_PORT` can be used as a `not_reply` flag. When set, the receiver does not send a reply.  
The lower bits of `SRC_PORT` increment to indicate the sequence.

We can begin by sending two groups of packets, each requiring a reply only for the last packet. Upon receiving a reply, the next group can be sent. Each group contains multiple packets.
