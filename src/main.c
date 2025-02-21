#include <sched.h> /* To set the priority on linux */
#include <stdbool.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <stdlib.h>
#include <errno.h>


#define ERR_ALL_GOOD (0)
#define ERR_UNEXPECTED (1)
#define ERR_INTERRUPTION (2)
#define ERR_INVALID (3)  
#define COMMUNICATION_BUFF_IN_SIZE (4096)

typedef int Error;
bool g_should_close = false;

#include "usbutils.c"

void signal_handler(int signum)
{
    printf("Process interrupted by signal `%d`.\n", signum);
    g_should_close = true;
}

int main(int argc, char* argv[])
{
    char input_buffer[COMMUNICATION_BUFF_IN_SIZE] = {0};
    ssize_t bytes_read = 0;
    if (argc < 2)
    {
        printf("Missing serial device\n");
        exit(1);
    }
    struct sigaction sa = {.sa_handler = signal_handler};
    sigaction(SIGINT, &sa, 0);
    sigaction(SIGTERM, &sa, 0);
    int serial_fd = 0;
    usb_utils_open_serial_port(argv[1], 115200, 0, &serial_fd);
    while (!g_should_close) {
        usb_utils_read_port(serial_fd, input_buffer, &bytes_read);
        printf("Read: %s\n", input_buffer);
    }
    return ERR_ALL_GOOD;
}
