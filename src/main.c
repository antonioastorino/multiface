#include <sched.h> /* To set the priority on linux */
#include <signal.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <poll.h>

#define COMMUNICATION_BUFF_IN_SIZE (4096)

#define FIFO_IN "artifacts/fifo_in"
#define FIFO_OUT "artifacts/fifo_out"

typedef struct
{
    char buffer[COMMUNICATION_BUFF_IN_SIZE];
    ssize_t size;
} SizedBuffer;

int g_serial_fd              = 0;
SizedBuffer g_fifo_input     = {0};
volatile bool g_should_close = false;

#define LOG_LEVEL LEVEL_TRACE
#include "mylib.c"
#include "usbutils.c"
#include "fifoutils.c"

void signal_handler(int signum)
{
    printf("Process interrupted by signal `%d`.\n", signum);
    g_should_close = true;
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        printf("Missing serial device\n");
        exit(1);
    }
    logger_init(NULL, NULL);
    LOG_INFO("Logger initialized");
    const char* message             = NULL;
    SizedBuffer serial_input        = {0};
    SizedBuffer serial_output       = {0};
    bool should_send_serial_message = false;

    fifo_utils_make_fifo(FIFO_IN);
    fifo_utils_make_fifo(FIFO_OUT);
    int fifo_in_fd = open(FIFO_IN, O_RDWR | O_NONBLOCK);
    if (fifo_in_fd < 0)
    {
        printf("Failed to open FIFO `%s`.\n", FIFO_IN);
        exit(ERR_FATAL);
    }
    struct pollfd polled_fd = {
        .fd      = fifo_in_fd,
        .events  = POLLIN,
        .revents = POLLERR,
    };

    usb_utils_open_serial_port(argv[1], B115200, &g_serial_fd);

    struct sigaction sa = {.sa_handler = signal_handler};
    sigaction(SIGINT, &sa, 0);
    sigaction(SIGTERM, &sa, 0);
    while (!g_should_close)
    {
        // Wait for a POLLIN event or keep processing the buffer (.size  != 0)
        if (poll(&polled_fd, 1, 500) > 0 && (polled_fd.revents & POLLIN))
        {
            if (fifo_utils_read_line(&g_fifo_input, fifo_in_fd) == ERR_FATAL)
            {
                exit(ERR_FATAL);
            }
            if (strncmp(g_fifo_input.buffer, "POLL\n", 5) == 0)
            {
                message            = "give me a long string!\n";
                serial_output.size = strlen(message);
                LOG_TRACE("size to send %lu", serial_output.size);
                memcpy(serial_output.buffer, message, serial_output.size);
                should_send_serial_message = true;
            }
            bzero((void*)g_fifo_input.buffer, g_fifo_input.size);

            if (should_send_serial_message)
            {
                should_send_serial_message = false;
                if (usb_utils_write_port(g_serial_fd, &serial_output) != ERR_ALL_GOOD)
                {
                    LOG_ERROR("This should not happen");
                    exit(ERR_FATAL);
                }
                if (usb_utils_read_port(g_serial_fd, &serial_input) == ERR_ALL_GOOD)
                {
                    if (serial_input.size)
                    {
                        LOG_INFO("Read: %s", serial_input.buffer);
                    }
                    else
                    {
                        LOG_WARNING("Got an empty answer");
                    }
                }
                else
                {
                    printf("Timeout\n");
                }
            }
        }
        else
        {
            printf("Waiting for FIFO message\n");
        }
    }

    printf("should close =        %d\n", g_should_close);
    return ERR_ALL_GOOD;
}
