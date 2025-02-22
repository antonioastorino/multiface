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

int g_serial_fd                     = 0;
SizedBuffer g_fifo_input            = {0};
volatile bool g_should_close        = false;
volatile bool g_should_process_fifo = false;
pthread_mutex_t process_fifo;

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
        pthread_mutex_lock(&process_fifo);
        g_should_process_fifo = true;
        pthread_mutex_unlock(&process_fifo);
        while (g_should_process_fifo && !g_should_close)
        {
            usleep(1000);
            // Wake up quickly after the data has been processed
        }
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
    pthread_t fifo_thread;
    pthread_attr_t pthread_attr;
    const char* message             = NULL;
    SizedBuffer serial_input        = {0};
    SizedBuffer serial_output       = {0};
    bool should_send_serial_message = false;
    pthread_attr_init(&pthread_attr);
    if (pthread_create(&fifo_thread, &pthread_attr, fifo_reader, NULL) < 0)
    {
        perror("Create thread");
        exit(ERR_FATAL);
    }
    if (pthread_mutex_init(&process_fifo, NULL) < 0)
    {
        printf("Failed to initialize mutex\n");
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
        if (g_should_process_fifo)
        {
            pthread_mutex_lock(&process_fifo);
            if (strncmp(g_fifo_input.buffer, "POLL", (size_t)g_fifo_input.size) == 0)
            {
                message            = "give me a long string!\n";
                serial_output.size = strlen(message);
                printf("size to send %lu\n", serial_output.size);
                memcpy(serial_output.buffer, message, serial_output.size);
                should_send_serial_message = true;
            }
            bzero((void*)g_fifo_input.buffer, g_fifo_input.size);

            if (should_send_serial_message)
            {
                should_send_serial_message = false;
                if (usb_utils_write_port(g_serial_fd, &serial_output) != ERR_ALL_GOOD)
                {
                    printf("This should not happen\n");
                    exit(ERR_FATAL);
                }
                if (usb_utils_read_port(g_serial_fd, &serial_input) == ERR_ALL_GOOD)
                {
                    if (serial_input.size)
                    {
                        printf("Read: %s", serial_input.buffer);
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
            g_should_process_fifo = false;
            pthread_mutex_unlock(&process_fifo);
            // Wait a bit in case there is still stuff in FIFO_IN 
            usleep(10000);
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
    printf("should process fifo = %d\n", g_should_process_fifo);
    printf("should close =        %d\n", g_should_close);
    pthread_join(fifo_thread, NULL);
    pthread_mutex_destroy(&process_fifo);
    return ERR_ALL_GOOD;
}
