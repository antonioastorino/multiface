# MULTIFACE

## Description
Create a FIFO to interface with other processes, interprets FIFO instructions
and talk to an external Serial Device.

```
   User               Multiface           Serial Device 
    .       fifo_in               USB          .
    |    ---------->      |                    .
    .                     |    ---------->     |
    .      fifo_out       |    <----------     |
    |    <----------      |                    .
```

Reading from FIFO is a blocking operation running on a separate thread. If any
valid instruction is received, the main thread is asked to take action.

Writing to Serial Device is expected to trigger a response (e.g., ACK or ERR).
Therefore, reading after sending a message is done in non-blocking mode, until
data is received or the reading operation timeouts.

## Usage
As an example, an ESP32 can run the script in `slave/`

```bash
cd slave
platformio run -t upload
```

From the root folder, run
```bash
./tools/build_and_run.sh /dev/<device_name>
```
where `<device_name>` is that used to communicate with the ESP32, for example
`ttyUSB0`.

Then `echo` your instructions to the FIFO created by the application. As an
example, the `POLL` call has been implemented:
- run the application
- use `echo -n POLL >artifacts/fifo_in
This will make the application write a message to the Serial Device and wait
for its response.

## Limitations
Tested on Raspberry Pi 4 and ESP32 only. With minor changes, it can run on any
uC supported by Platform IO and any OS.

