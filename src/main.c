#include <sched.h> /* To set the priority on linux */
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <pthread.h>

#define ERR_ALL_GOOD (0)
#define ERR_UNEXPECTED (1)
#define ERR_INTERRUPTION (2)
#define ERR_INVALID (3)
#define ERR_FATAL (-1)

#define COMMUNICATION_BUFF_IN_SIZE (4096)

#define FIFO_IN "artifacts/fifo_in"
#define FIFO_OUT "artifacts/fifo_out"

typedef int Error;
bool g_should_close = false;
bool g_should_react = false;

#include "usbutils.c"
#include "fifoutils.c"

void signal_handler(int signum)
{
    printf("Process interrupted by signal `%d`.\n", signum);
    g_should_close = true;
}

void* fifo_reader(void* unused)
{
    char fifo_input_buffer[COMMUNICATION_BUFF_IN_SIZE] = {0};
    printf("Thread running");
    while (true)
    {
        fifo_utils_wait_for_fifo_in(fifo_input_buffer);
        g_should_react = true;
    }
    return NULL;
}

int main(int argc, char* argv[])
{
    char serial_input_buffer[COMMUNICATION_BUFF_IN_SIZE]  = {0};
    char serial_output_buffer[COMMUNICATION_BUFF_IN_SIZE] = {0};
    char fifo_output_buffer[COMMUNICATION_BUFF_IN_SIZE]   = {0};
    ssize_t bytes_read                                    = 0;
    ssize_t bytes_to_write                                = 0;
    pthread_t fifo_thread;
    pthread_attr_t pthread_attr;
    pthread_attr_init(&pthread_attr);
    if (pthread_create(&fifo_thread, &pthread_attr, fifo_reader, NULL) < 0)
    {
        perror("Create thread");
        exit(ERR_FATAL);
    }
    pthread_attr_destroy(&pthread_attr);

    if (argc < 2)
    {
        printf("Missing serial device\n");
        exit(1);
    }
    fifo_utils_make_fifo(FIFO_IN);
    fifo_utils_make_fifo(FIFO_OUT);
    struct sigaction sa = {.sa_handler = signal_handler};
    sigaction(SIGINT, &sa, 0);
    sigaction(SIGTERM, &sa, 0);
    int serial_fd = 0;
    usb_utils_open_serial_port(argv[1], B115200, 0, &serial_fd);

    while (!g_should_close)
    {
        if (g_should_react)
        {
            g_should_react = false;
            if (usb_utils_read_port(serial_fd, serial_input_buffer, &bytes_read) == ERR_ALL_GOOD)
            {
                if (bytes_read)
                {
                    printf("Read: %s", serial_input_buffer);
                }
                else
                {
                    printf("Got an empty answer\n");
                }
            }
            else
            {
                printf("Timeout\n");
            }
        }
        else
        {
            printf("Waiting for FIFO message\n");
            sleep(1);
        }
    }

    printf("Joining\n");
    pthread_cancel(fifo_thread);
    printf("should react = %d\n", g_should_react);
    printf("should close = %d\n", g_should_close);
    pthread_join(fifo_thread, NULL);
    return ERR_ALL_GOOD;
}
