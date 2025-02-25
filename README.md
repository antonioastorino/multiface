# MULTIFACE

## Description
MULTIFACE creates a FIFO to interface with other processes, interprets FIFO instructions
and talks to an external Serial Device based on those instructions.

```
   User               Multiface           Serial Device 
    .       fifo_in               USB          .
    |    ━━━━━━━━━━>      |                    .
    .                     |    ━━━━━━━━━━>     |
    .      fifo_out       |    <━━━━━━━━━━     |
    |    <━━━━━━━━━━      |                    .
```

Reading from FIFO is a blocking operation running on a separate thread. If any
valid instruction is received, the main thread is asked to take action.
FIFO instructions should be terminated with a `'\n'`. This way, multiple instructions
can be sent at once but processed sequentially. Also, if 2 or more FIFO instructions
pile up (concatenate), they will still be processed correctly (see examples below).

Writing to Serial Device is expected to trigger a response (e.g., ACK or ERR).
Therefore, reading after sending a message is done in non-blocking mode, until
data is received or the reading operation times out.

## Usage
As an example, an ESP32 or Arduino UNO can run the program `slave/src/main.cpp`:

```bash
cd slave
platformio run -t upload -e <env>
```
where `<env>` can be `esp32dev` or `uno`.

From the root folder, run

```bash
./tools/build_and_run.sh <device_name>
```
where `<device_name>` is that used to communicate with the uC, for example 
`/dev/ttyUSBx` on Linux or `/dev/cu.usbmodemxxxx` on MacOS.

Then `echo` your instructions to the FIFO created by the application. As an
example, the `POLL` call has been implemented:
- run the application
- use `echo POLL >artifacts/fifo_in`

This instruction tells the MULTIFACE to send `"give me a long string!"` to the Serial Device
and wait for its (long) response.

As mentioned earlier, the following commands produce the same results:

```bash
echo -e "POLL\nPOLL\nPOLL\n" >artifacts/fifo_in
```
or
```bash
for i in {0..3}; do echo -e "POLL\n" >artifacts/fifo_in; sleep 1; done
```

A one-second sleep has been added to the second command ensure that each FIFO instruction is
processed before the next one is sent.

## Supported devices 
### Operating Systems 
Tested on
- Raspberry Pi OS
- MacOS

### Micro Controllers 
Supports
- ESP32 WROOM 32D
- Arduino UNO Rev 3
