# APOLLO61 macOS HID findings

## Confirmed device

The APOLLO61 keyboard is visible to macOS through IOKit HID.

```text
product:      APOLLO61-LY
manufacturer: Apple Inc.
serial:       DC:2C:26:0F:A7:E2
transport:    Bluetooth
vid:pid:      0x05ac:0x024f
version:      0x010a
primary:      usage=0x0001:0x0006
```

The VID/PID matches the Windows `device.xml` bundled in the installer.

## Report sizes

```text
input:   42 bytes
output:  42 bytes
feature: 1 byte
```

The Windows app dynamically loads `hid.dll` and contains a method that calls
`HidD_SetFeature(handle, buffer, 0x41)`, so the original configuration protocol
likely uses 65-byte feature reports. The current macOS Bluetooth interface only
advertises a 1-byte feature report, which may be too limited for configuration.

Test the board in wired USB mode next and compare report sizes. A wired
interface with a 65-byte feature report is the most likely target for lighting
and keymap writes.

## Wired USB result

Wired mode exposes two APOLLO interfaces:

```text
USB DEVICE / hfd.cn / USB / 0x05ac:0x024f / usage 0x000c:0x0001
input=16 output=1 feature=1

USB DEVICE / hfd.cn / USB / 0x05ac:0x024f / usage 0x0001:0x0006
input=8 output=1 feature=64
```

The second interface is the likely configuration target. Windows
`HidD_SetFeature(..., 0x41)` maps to report ID 0 plus 64 bytes of payload.

## Report IDs observed

The normal keyboard interface appears under report ID `1`.

The likely vendor/control channel is:

```text
input  report_id=177, payload-like size 41 bytes
output report_id=178, payload-like size 41 bytes
```

That maps neatly to the max 42-byte report size: one report ID byte plus
41 bytes of payload.

## Windows protocol notes

Static analysis of `DeviceDriver.exe` found a low-level sender that calls:

```text
HidD_SetFeature(handle, buffer, 0x41)
```

The wrapper keeps report ID byte `0` and copies caller payload bytes into the
remaining 64 bytes. On macOS the equivalent is:

```text
IOHIDDeviceSetReport(..., kIOHIDReportTypeFeature, 0, payload, 64)
```

Candidate command prefixes seen near sender call sites:

```text
04 18  start/prepare
04 13  sync stage, often with byte 8 = 12
04 02  end/apply/commit
04 f0  post-transfer step
04 11  profile/key-ish stage
04 15  large data transfer setup
04 17  status/LED-ish stage
```

Confirmed responses from the wired keyboard:

```text
04 18              -> 04 18 00 01 ...  ack
04 17 ... 01       -> 04 17 00 01 ...  ack, parameter echoed at byte 8
04 13 ... 12       -> 04 13 00 01 ...  ack, parameter echoed at byte 8
04 11 ... 0a       -> 04 11 00 01 ...  ack, parameter echoed at byte 8
04 02 alone        -> 04 02 00 ff ...  done/closed
04 02 in sequence  -> 04 02 00 00 ...  accepted, zero status
04 f0 alone        -> 04 f0 00 01 ...  ack
04 19 alone        -> 04 19 00 01 ...  ack
04 15 page-count 0 -> 04 15 00 01 ...  ack
```

## Transfer shapes

The Windows app has several larger transfer paths. These should not be sent
until the payload format is fully understood:

```text
04 13 ... 12 + 0x4c0-byte block + 04 02 + 04 f0
04 11 ... 0a + 0x2c0-byte block + 04 02 + 04 f0
04 19 + 04 15 page-count + variable block ending aa 55 + 04 02
04 17 ... 01 + 64-byte packet beginning 0a and ending aa 55 + 04 02
```

The compact `04 17` path builds a packet with byte `0x0a` at offset 0 and
`aa 55` at offsets 62-63. Based on nearby UI/config strings, this is more
likely gaming-mode or lock-state related than the main RGB lighting transfer.

## Lighting modes

The bundled English language file lists these lighting modes:

```text
200 Static
201 SingleOn
202 SingleOff
203 Glittering
204 Falling
205 Colourful
206 Breath
207 Spectrum
208 Outward
209 Scrolling
210 Rolling
211 Rotating
212 Explode
213 Launch
214 Ripples
215 Flowing
216 Pulsating
217 Tilt
218 Shuttle
219 UserDefine
```

The `t_light_data` table stores `mode`, `colortype`, and seven command bytes
named `byte2` through `byte7`. The `t_key_rgb_data` table stores per-key RGB
values for a selected lighting mode.

## Next protocol task

We need the APOLLO61 command format for output report `178`. Good next steps:

1. Capture Windows traffic while the original app changes lighting/profile data.
2. Compare `DeviceDriver.exe` call sites around `HidD_SetFeature`, `WriteFile`,
   and `DeviceIoControl`.
3. Try read-only status polling first, if a candidate command can be identified.

Do not send guessed reports to the keyboard casually. A malformed vendor report
could change persistent keyboard state.
