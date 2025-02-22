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
#include <string.h>

#define ERR_ALL_GOOD (0)
#define ERR_UNEXPECTED (1)
#define ERR_INTERRUPTION (2)
#define ERR_INVALID (3)
#define ERR_TIMEOUT (4)
#define ERR_FATAL (-1)

#define COMMUNICATION_BUFF_IN_SIZE (4096)
#define UNUSED(x) (void)(x)

#define FIFO_IN "artifacts/fifo_in"
#define FIFO_OUT "artifacts/fifo_out"

typedef int Error;
typedef struct
{
    char buffer[COMMUNICATION_BUFF_IN_SIZE];
    ssize_t size;
} SizedBuffer;

int g_serial_fd              = 0;
SizedBuffer g_fifo_input     = {0};
volatile bool g_should_close = false;
volatile bool g_should_react = false;

#include "usbutils.c"
#include "fifoutils.c"

void signal_handler(int signum)
{
    printf("Process interrupted by signal `%d`.\n", signum);
    g_should_close = true;
}

void* fifo_reader(void* unused)
{
    UNUSED(unused);
    printf("Thread running");
    while (!g_should_close)
    {
        fifo_utils_wait_for_fifo_in(&g_fifo_input);
        g_should_react = true;
    }
    return NULL;
}

// Used to gracefully close the thread currently in blocking reading of FIFO_IN. By writing a dummy
// string into FIFO_IN, the reading is unblocked and the thread can exit.
Error send_dummy_string_to_fifo_in(void)
{
    int fifo_fd = open(FIFO_IN, O_WRONLY);
    if (fifo_fd < 0)
    {
        printf("Failed to open FIFO IN for reading file `%s`.\n", FIFO_IN);
        return ERR_FATAL;
    }
    else
    {
        if (write(fifo_fd, "dummy", 5) < 0)
        {
            printf("Failed to write to FIFO OUT `%s`.\n", FIFO_IN);
            return ERR_FATAL;
        }
    }
    close(fifo_fd);
    return ERR_ALL_GOOD;
}

int main(int argc, char* argv[])
{
    ssize_t bytes_read = 0;
    pthread_t fifo_thread;
    pthread_attr_t pthread_attr;
    const char* message                                   = NULL;
    char serial_input_buffer[COMMUNICATION_BUFF_IN_SIZE]  = {0};
    char serial_output_buffer[COMMUNICATION_BUFF_IN_SIZE] = {0};
    ssize_t serial_bytes_to_write                         = 0;
    bool should_send_serial_message                       = false;
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
    usb_utils_open_serial_port(argv[1], B115200, &g_serial_fd);
    while (!g_should_close)
    {
        if (g_should_react)
        {
            g_should_react = false;
            if (strncmp(g_fifo_input.buffer, "POLL", (size_t)g_fifo_input.size) == 0)
            {
                message               = "give me a long string!\n";
                serial_bytes_to_write = strlen(message);
                printf("size to send %lu\n", serial_bytes_to_write);
                memcpy(serial_output_buffer, message, serial_bytes_to_write);
                should_send_serial_message = true;
            }
            bzero((void*)g_fifo_input.buffer, g_fifo_input.size);

            if (should_send_serial_message)
            {
                should_send_serial_message = false;
                if (usb_utils_write_port(g_serial_fd, serial_output_buffer, serial_bytes_to_write)
                    != ERR_ALL_GOOD)
                {
                    printf("This should not happen\n");
                    exit(ERR_FATAL);
                }
                if (usb_utils_read_port(g_serial_fd, serial_input_buffer, &bytes_read)
                    == ERR_ALL_GOOD)
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
        }
        else
        {
            printf("Waiting for FIFO message\n");
            sleep(1);
        }
    }

    printf("Joining\n");
    if (send_dummy_string_to_fifo_in() != ERR_ALL_GOOD)
    {
        pthread_cancel(fifo_thread);
    }
    printf("should react = %d\n", g_should_react);
    printf("should close = %d\n", g_should_close);
    pthread_join(fifo_thread, NULL);
    return ERR_ALL_GOOD;
}
