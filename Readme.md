CDNET: Optional High-Layer Protocol for CDBUS
=======================================

Protocol status: Stable [Version 2.0]

A CDBUS frame carrying a CDNET packet is structured as:  
`[src, dst, len] + [CDNET package] + [crc_l, crc_h]`  
or equivalently:  
`[src, dst, len] + [CDNET header, payload] + [crc_l, crc_h]`

More documentation: [wiki](https://github.com/dukelec/cdnet/wiki).


## CDNET Levels

CDNET defines two protocol levels, determined by bit7 of the first header byte:

| Bit7 | DESCRIPTION                                                   |
|------|---------------------------------------------------------------|
| 0    | Level 0: Simplified communication                             |
| 1    | Level 1: Standard communication                               |

Applications may support either or both levels.

CDNET uses little-endian byte order.


## Level 0 Format

Header: 2 bytes

First byte:

| FIELD   | DESCRIPTION                                       |
|-------- |---------------------------------------------------|
| [7]     | Always 0 (indicates level 0)                      |
| [6:0]   | src_port, range `00` ~ `7f`                       |

Second byte:

| FIELD   | DESCRIPTION                                       |
|-------- |---------------------------------------------------|
| [7]     | _Reserved (0)_                                    |
| [6:0]   | dst_port, range `00` ~ `7f`                       |


Note: Port functions like a UDP port.


## Level 1 Format

Header – first byte:

| FIELD   | DESCRIPTION                     |
|-------- |---------------------------------|
| [7]     | Always 1 (indicates level 1)    |
| [6]     | _Reserved (0)_                  |
| [5]     | MULTI_NET                       |
| [4]     | MULTICAST                       |
| [3:2]   | _Reserved (0)_                  |
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
 - `mh`/`ml`: multicast_id (`ml` maps to MAC-layer multicast, `mh`: high byte, `ml`: low byte)
 - For simplicity, MULTI_NET = 0 and MULTICAST = 0 may also be used for local multicast/broadcast
 - Support for MULTI_NET and MULTICAST is optional

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
 - 2-byte port support is optional
 - Append order:
   * MULTI_NET & MULTICAST bytes first, PORT_SIZE bytes last
   * `src_port` bytes first, `dst_port` bytes last


## Port Allocation Recommendation

Ports `00` ~ `0f` are recommended for general-purpose use.  
Port `01` is reserved for device information queries.

The assignments in this section are optional, but implementing the basic function of port `01` (read `device_info`) is recommended.

Port numbers ≥ 0x40 are typically used as ephemeral ports.


### Port 01

Provide device info.

```
Types:
 - mac_start, mac_end: uint8_t
 - max_time: uint16_t (unit: ms)
 - "string": variable-length string (may be empty, without terminating '\0')


Read device_info:
  Write:    [None]
  Return:   ["device_info"]

Search devices by filters (for MAC conflict resolution):
  Write:    [0x10, max_time, mac_start, mac_end, "string"]
  Return:   ["device_info"]

  Return after a random delay in range [0, max_time] only if:
   - "device_info" contains "string" (always true if empty)
   - current MAC is in range [mac_start, mac_end] (inclusive)
  Otherwise:
   - no reply
   - reject subsequent modify-MAC / save-config commands
```

Example of `device_info`:  
  `M: model; S: serial id; HW: hardware version; SW: software version` ...  
The model field should be included at minimum.



## CDNET Address String Formats

```
           localhost      local link     unique local    multicast
            10:00:00
level0:                    00:NN:MM
level1:                    80:NN:MM        a0:NN:MM       f0:MH:ML
```

Notes:
  - NN: net_id, MM: mac_addr, MH/ML: multicast_id (H: high byte, L: low byte)
  - String address format is analogous to IPv6


## Examples

Request device info:  
(Local network, requester's MAC address: `0x0c`, target's MAC address: `0x0d`)

Reply the device info string: `"M: c1; S: 1234"`,
expressed in hexadecimal: `[4d, 3a, 20, 63, 31, 3b, 20, 53, 3a, 20, 31, 32, 33, 34]`.

The Level 0 Format:
 * Request:
   - CDNET socket: `[00:00:0c]:40` -> `[00:00:0d]:01`: `[None]`
   - CDNET packet: `[40, 01]`
   - CDBUS frame: `[0c, 0d, 02,  40, 01,  crc_l, crc_h]`
 * Reply:
   - CDNET socket: `[00:00:0d]:01` -> `[00:00:0c]:40`: `["M: c1; S: 1234"]`
   - CDNET packet: `[01, 40,  4d, 3a, 20 ... 34]`
   - CDBUS frame: `[0d, 0c, 10,  01, 40,  4d, 3a, 20 ... 34,  crc_l, crc_h]`

The Level 1 Format:
 * Request:
   - CDNET socket: `[80:00:0c]:40` -> `[80:00:0d]:01`: `[None]`
   - CDNET packet: `[80, 40, 01]`
   - CDBUS frame: `[0c, 0d, 03,  80, 40, 01,  crc_l, crc_h]`
 * Reply:
   - CDNET socket: `[80:00:0d]:01` -> `[80:00:0c]:40`: `["M: c1; S: 1234"]`
   - CDNET packet: `[80, 01, 40,  4d, 3a, 20 ... 34]`
   - CDBUS frame: `[0d, 0c, 11,  80, 01, 40,  4d, 3a, 20 ... 34,  crc_l, crc_h]`


### Sequence & Flow Control

For commands, the ephemeral port may change each time.
This prevents re-execution during retransmission and ensures correct request–response matching.

To improve throughput for bulk data transfer:

 1. Send up to two packet groups at a time; each group contains multiple packets, and only the last packet requires a reply.
 2. After receiving the reply for the previous group, if no error occurred, send one more group.

Example:
 - 5 packets per group
 - Ephemeral port format:
   * Bit7: 0 = normal reply, 1 = no reply
   * Bit[2:0]: auto-increment sequence `cnt`
 - On sequence error: set error flag and discard data

<img src="docs/img/send_file.svg">
