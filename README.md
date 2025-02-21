# MULTIFACE

## Description
Create a FIFO to interface with other processes, interprets FIFO instructions
and talk to an external Serial Device.

Reading from FIFO is a blocking operation running on an separate thread. If any
valid instruction is received, the main thread is asked to take actions.

Writing to Serial Device is expected to trigger a response (e.g., ACK or ERR).
Therefore, reading after sending a message is done in non-blocking mode, until
data is received or the reading operation timeouts.

## Usage
As an example, an ESP32 can be run the script in `slave/`

```bash
cd slave
platformio run -t upload
```

From the root folder, run
```bash
./tools/build_and_run.sh /dev/<device_name>
```
where `<device_name>` is that used to communicate with the ESP32, for example
`/ttyUSB0`.

Then `echo` your instructions to the FIFO created by the application.

## Limitations
Tested on Raspberry Pi 4 and ESP32 only. With minor changes it can run on any
uC supported by Platform IO and any OS.

