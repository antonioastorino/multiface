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

#define COMMUNICATION_BUFF_IN_SIZE (4096)

#define FIFO_IN "artifacts/fifo_in"
#define FIFO_OUT "artifacts/fifo_out"

typedef struct
{
    char buffer[COMMUNICATION_BUFF_IN_SIZE];
    ssize_t size;
} SizedBuffer;

int g_serial_fd                     = 0;
SizedBuffer g_fifo_input            = {0};
volatile bool g_should_close        = false;
volatile bool g_should_process_fifo = false;

#define LOG_LEVEL LEVEL_TRACE
#include "mylib.c"
#include "usbutils.c"
#include "fifoutils.c"

void signal_handler(int signum)
{
    printf("Process interrupted by signal `%d`.\n", signum);
    g_should_close = true;
}

void* fifo_reader_thread(void* mutex)
{
    pthread_mutex_t* busy_mutex_p = (pthread_mutex_t*)(mutex);
    printf("Thread running");
    int fifo_fd = open(FIFO_IN, O_RDONLY);
    if (fifo_fd < 0)
    {
        printf("Failed to open FIFO `%s`.\n", FIFO_IN);
        exit(ERR_FATAL);
    }
    while (!g_should_close)
    {
        fifo_utils_wait_for_fifo_in(&g_fifo_input, fifo_fd);
        if (g_fifo_input.size)
        {
            pthread_mutex_lock(busy_mutex_p);
            g_should_process_fifo = true;
            pthread_mutex_unlock(busy_mutex_p);
            while (g_should_process_fifo && !g_should_close)
            {
                usleep(1000);
                // Wake up quickly after the data has been processed
            }
        }
        usleep(1000);
    }
    return NULL;
}

void start_fifo_reader_thread(pthread_t* inout_thread_p, pthread_mutex_t* inout_busy_mutex_p)
{
    pthread_attr_t pthread_attr;
    pthread_attr_init(&pthread_attr);
    if (pthread_create(inout_thread_p, &pthread_attr, fifo_reader_thread, inout_busy_mutex_p) < 0)
    {
        perror("Create thread");
        exit(ERR_FATAL);
    }
    pthread_attr_destroy(&pthread_attr);
    if (pthread_mutex_init(inout_busy_mutex_p, NULL) < 0)
    {
        LOG_ERROR("Failed to initialize mutex");
        exit(ERR_FATAL);
    }
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
    if (argc < 2)
    {
        printf("Missing serial device\n");
        exit(1);
    }
    logger_init(NULL, NULL);
    LOG_INFO("Logger initialized");
    pthread_t fifo_thread;
    pthread_mutex_t fifo_busy_mutex;
    const char* message             = NULL;
    SizedBuffer serial_input        = {0};
    SizedBuffer serial_output       = {0};
    bool should_send_serial_message = false;
    // ---- set up FIFO stuff ----
    fifo_utils_make_fifo(FIFO_IN);
    fifo_utils_make_fifo(FIFO_OUT);
    fifo_utils_flush_fifo_in();

    start_fifo_reader_thread(&fifo_thread, &fifo_busy_mutex);

    usb_utils_open_serial_port(argv[1], B115200, &g_serial_fd);

    struct sigaction sa = {.sa_handler = signal_handler};
    sigaction(SIGINT, &sa, 0);
    sigaction(SIGTERM, &sa, 0);
    while (!g_should_close)
    {
        if (g_should_process_fifo)
        {
            pthread_mutex_lock(&fifo_busy_mutex);
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
            g_should_process_fifo = false;
            pthread_mutex_unlock(&fifo_busy_mutex);
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
    pthread_mutex_destroy(&fifo_busy_mutex);
    return ERR_ALL_GOOD;
}
