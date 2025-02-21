#include <sched.h> /* To set the priority on linux */
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

#include "usbutils.c"

void signal_handler(int signum)
{
    printf("Process interrupted by signal `%d`.\n", signum);
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        printf("Missing serial device\n");
        exit(1);
    }
    struct sigaction sa = {.sa_handler = signal_handler};
    sigaction(SIGINT, &sa, 0);
    sigaction(SIGTERM, &sa, 0);
    int serial_fd = 0;
    usb_utils_open_serial_port(argv[1], B115200, 0, &serial_fd);
    return ERR_ALL_GOOD;
}
