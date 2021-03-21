CDNET is a high layer protocol for CDBUS
=======================================

1. [CDNET Levels](#cdnet-levels)
2. [Level 0 Format](#level-0-format)
3. [Level 1 Format](#level-1-format)
4. [Level 2 Format](#level-2-format)
5. [Port Allocation Recommendation](#port-allocation-recommendation)
6. [Examples](#examples)
7. [More Resources](#more-resources)


CDBUS frame:  
[src, dst, len] + [CDNET payload] + [crc_l, crc_h]


## CDNET Levels

CDNET protocol has three different levels, select by bit7 and bit6 of first byte:

| Bit7 | Bit6   | DESCRIPTION                                                   |
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
| [5:0]   | dst_port, range 0 ~ 0x3f                          |

The second byte and after: command parameters.

Note: The port is equivalent to UDP port, but can be simply understood as commands.

### Reply
First byte:

| FIELD   | DESCRIPTION                                       |
|-------- |---------------------------------------------------|
| [7]     | Always 0: Level 0                                 |
| [6]     | Always 1: reply                                   |
| [5]     | 0: not share; 1: [4:0] shared for first data byte |
| [4:0]   | Not care, or part of first data byte              |

Default: `SHARE_MASK` is 0xe0, `SHARE_LEFT` is 0x80,
only allow share if (`first data byte` & `SHARE_MASK`) == `SHARE_LEFT`.

E.g.: reply `[0x40, 0x8c]` is the same as `[0x6c]`.
```
0x40: 'b0100_0000
0x8c: 'b1000_1100
----
0x6c: 'b0110_1100
```

The first byte shared part, and the second byte and after: reply status and/or datas.


### Recommendation for level 0 and 1:
For request and report, the first parameter byte is sub command with two flags:

| FIELD   | DESCRIPTION                         |
|-------- |-------------------------------------|
| [7]     | is_reply, always 0                  |
| [6]     | not_reply, e.g. 1 for report packet |
| [5:0]   | sub command                         |

For reply, the first data byte is status with one flag:

| FIELD   | DESCRIPTION                     |
|-------- |---------------------------------|
| [7]     | is_reply, always 1              |
| [6:0]   | status, 0 means no error        |


## Level 1 Format
First byte:

| FIELD   | DESCRIPTION                     |
|-------- |---------------------------------|
| [7]     | Always 1                        |
| [6]     | Always 0                        |
| [5]     | MULTI_NET                       |
| [4]     | MULTICAST                       |
| [3]     | SEQUENCE                        |
| [2:0]   | PORT_SIZE                       |

### MULTI_NET & MULTICAST

| MULTI_NET | MULTICAST | DESCRIPTION                                                               |
|-----------|-----------|---------------------------------------------------------------------------|
| 0         | 0         | Local net: append 0 byte                                                  |
| 0         | 1         | Local net multicast: append 2 bytes `[mh, ml]`                            |
| 1         | 0         | Cross net: append 4 bytes: `[src_net, src_mac, dst_net, dst_mac]`         |
| 1         | 1         | Cross net multicast: append 4 bytes: `[src_net, src_mac, mh, ml]`         |

Notes:
 - mh + ml: multicast_id, ml is mapped to mac layer multicast address;
 - Could simply use MULTI_NET = 0 and MULTICAST = 0 for local net multicast and broadcast.

### SEQUENCE
0: No sequence number;  
1: Append 1 byte `SEQ_NUM`, see [Port 0](#port-0).


### PORT_SIZE:

| Bit2 | Bit1 | Bit0   | SRC_PORT      | DST_PORT      |
|------|------|--------|---------------|---------------|
| 0    | 0    | 0      | Default port  | 1 byte        |
| 0    | 0    | 1      | Default port  | 2 bytes       |
| 0    | 1    | 0      | 1 byte        | Default port  |
| 0    | 1    | 1      | 2 bytes       | Default port  |
| 1    | 0    | 0      | 1 byte        | 1 byte        |
| 1    | 0    | 1      | 1 byte        | 2 bytes       |
| 1    | 1    | 0      | 2 bytes       | 1 byte        |
| 1    | 1    | 1      | 2 bytes       | 2 bytes       |

Notes:
 - Default port is `0xcdcd` for convention, it does not take up space.
 - Append bytes for `src_port` first then `dst_port`.


## Level 2 Format
First byte:

| FIELD   | DESCRIPTION                     |
|-------- |---------------------------------|
| [7]     | Always 1                        |
| [6]     | Always 1                        |
| [5:4]   | FRAGMENT                        |
| [3]     | SEQUENCE                        |
| [2:0]   | User-defined flag               |

### FRAGMENT:

| Bit5 | Bit4   | DST_PORT                  |
|------|--------|---------------------------|
| 0    | 0      | Not fragment              |
| 0    | 1      | First fragment            |
| 1    | 0      | More fragment             |
| 1    | 1      | Last fragment             |

Note:
 - `SEQUENCE` must be selected when using fragments.
 - There is no need to reset the `SEQ_NUM` when starting the fragmentation.

### SEQUENCE
0: No sequence number;  
1: Append 1 byte `SEQ_NUM`, see [Port 0](#port-0).


## Port Allocation Recommendation

It is recommended that ports 0 to 0xf be used for general purpose.
Among them, port 0 is used for sequence control of cdnet, and port 1 is used for device information query.

All ports in this section are optional, but it is recommended to implement the first function of port 1 (read device_info).

### Port 0

Used together with header's `SEQUENCE` for flow control and ensure data integrity.  
The `SEQ_NUM[6:0]` auto increase when `SEQUENCE` is selected in the header.  
Bit 7 of `SEQ_NUM` is set indicating that a report is required.  
Do not select `SEQUENCE` for port 0 communication.

Port 0 communications:
```
Check the SEQ_NUM:
  Write [0x00]
  Return: [0x80, SEQ_NUM] (no record found if bit 7 set)

Set the SEQ_NUM:
  Write [0x20, SEQ_NUM]
  Return: [0x80]

Report SEQ_NUM:
  Report [0x40, SEQ_NUM] (report to the src port of the relevant packet)
```

Example:  
(The data format is cdnet payload. dft is short for default (0xcdcd). (p) marks the location of the port.)

```
Device A                    Device B                 Description                    Port flow
        (p)
[0x80, 0x00, 0x20, 0x00] >>         (p)              Set SEQ_NUM at first time      (dft -> 0x0)
                         << [0x82, 0x00, 0x80]       Port 0 reply                   (dft <- 0x0)
              (p)
[0x88, 0x00, 0x10, ...]  >>                          Start send data                (dft -> 0x10)
[0x88, 0x01, 0x10, ...]  >>                                                         (dft -> 0x10)
[0x88, 0x82, 0x10, ...]  >>                          Require report at SEQ_NUM 2    (dft -> 0x10)
[0x88, 0x03, 0x10, ...]  >>                                                         (dft -> 0x10)
[0x88, 0x04, 0x10, ...]  >>         (p)                                             (dft -> 0x10)
              (p)        << [0x82, 0x00, 0x40, 0x03] Report after receive SEQ_NUM 2 (dft <- 0x0)
[0x88, 0x85, 0x10, ...]  >>         (p)              Require report at SEQ_NUM 5    (dft -> 0x10)
                         << [0x82, 0x00, 0x40, 0x06] Report after receive SEQ_NUM 5 (dft <- 0x0)
              (p)
[0x88, 0x86, 0x11, ...]  >>         (p)              Require report at SEQ_NUM 6    (dft -> 0x11)
                         << [0x82, 0x00, 0x40, 0x07] Report after receive SEQ_NUM 6 (dft <- 0x0)
                         << [0x82, 0x11, ...]        Reply without SEQ by default   (dft <- 0x11)
```


### Port 1

Provide device info.
```
Type of mac_start and mac_end is uint8_t;
Type of max_time is uint16_t, unit: ms;
Type of "string" is any length of string, include empty;
All strings excluding the terminating null byte ('\0').

Read device_info string:
  Write [0x00]
  Return [0x80, "device_info"]

Search device by filters (used to resolve mac conflicts):
  Write [0x10, max_time, mac_start, mac_end, "string"]
  Return [0x80, "device_info"] after a random time in range [0, max_time]
    only if "device_info" contain "string" (always true for empty string) and
    current mac address is in the range [mac_start, mac_end], mac_end included
  Not return otherwise
    and reject any subsequent modify mac (or save config) commands
```
Example of `device_info`:  
  `M: model; S: serial string; HW: hardware version; SW: software version` ...  
Do not care about the field order, and should at least include the model field.


## Examples

Request device info:  
(Local network, requester's MAC address: `0x0c`, target's MAC address: `0x0d`)

Reply the device info string: `"M: c1; S: 1234"`,
expressed in hexadecimal: `[0x4d, 0x3a, 0x20, 0x63, 0x31, 0x3b, 0x20, 0x53, 0x3a, 0x20, 0x31, 0x32, 0x33, 0x34]`.

The Level 0 Format:
 * Request:
   - CDNET socket: `[00:00:0c]:0xcdcd` -> `[00:00:0d]:1`: `0x00` (net id: `0`)
   - CDNET packet: `[0x01, 0x00]` (`dst_port` = `0x01`)
   - CDBUS frame: `[0x0c, 0x0d, 0x02, 0x01, 0x00, crc_l, crc_h]`
 * Reply:
   - CDNET socket: `[00:00:0d]:1` -> `[00:00:0c]:0xcdcd`: `0x80` + `"M: c1; S: 1234"`
   - CDNET packet: `[0x60, 0x4d, 0x3a, 0x20 ... 0x34]` (first `0x60` is `0x40` + `0x80`)
   - CDBUS frame: `[0x0d, 0x0c, 0x0f, 0x60, 0x4d, 0x3a, 0x20 ... 0x34, crc_l, crc_h]`

The Level 1 Format:
 * Request:
   - CDNET socket: `[80:00:0c]:0xcdcd` -> `[80:00:0d]:1`: `0x00`
   - CDNET packet: `[0x80, 0x01, 0x00]` (`src_port` = default, `dst_port` = `0x01`)
   - CDBUS frame: `[0x0c, 0x0d, 0x03, 0x80, 0x01, 0x00, crc_l, crc_h]`
 * Reply:
   - CDNET socket: `[80:00:0d]:1` -> `[80:00:0c]:0xcdcd`: `0x80` + `"M: c1; S: 1234"`
   - CDNET packet: `[0x82, 0x01, 0x80, 0x4d, 0x3a, 0x20 ... 0x34]` (`src_port` = `0x01`, `dst_port` = default)
   - CDBUS frame: `[0x0d, 0x0c, 0x11, 0x82, 0x01, 0x80, 0x4d, 0x3a, 0x20 ... 0x34, crc_l, crc_h]`


### Code Examples

How to use this library refer to `cd_mdrv_step` (stepper motor controller), `cdbus_bridge` or `cdnet_tun` projects;  
How to control CDCTL-Bx/Hx refer to `dev/cdctl_xxx`.


## More Resources

Python library for CDNET: ../pycdnet

CDNET address string format: [parser/cdnet.h](parser/cdnet.h)

