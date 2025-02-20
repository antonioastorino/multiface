#include "prj_setup.h"
#include "usbutils.h"
#include <fcntl.h>
#include <stdio.h>
#include <strings.h>
#include <sys/ioctl.h>

void _usb_utils_flush(int fd)
{
    // play with DTR
    int iFlags;

    // turn on DTR
    iFlags = TIOCM_DTR;
    ioctl(fd, TIOCMBIS, &iFlags);
    // turn off DTR
    iFlags = TIOCM_DTR;
    ioctl(fd, TIOCMBIC, &iFlags);
    tcflush(fd, TCIFLUSH);
}

void _usb_utils_print_termios_struct(struct termios* options)
{
#define placeholder "%o"
    LOG_DEBUG("c_iflag:  " placeholder, options->c_iflag & 0xfff);
    LOG_DEBUG("c_oflag:  " placeholder, options->c_oflag & 0xfffff);
    LOG_DEBUG("c_cflag:  " placeholder, options->c_cflag & 0xffffff);
    LOG_DEBUG("c_lflag:  " placeholder, options->c_lflag & 0xffffffff);
    LOG_DEBUG("c_ispeed: " placeholder, options->c_ispeed);
    LOG_DEBUG("c_ospeed: " placeholder, options->c_ospeed);
    UNUSED(options);
}

struct termios initial_options;

Error usb_utils_open_serial_port(
    const char* device,
    const speed_t baud_rate,
    const int min_in_bytes,
    int* out_fd)
{
    *out_fd = open(device, O_RDWR | O_NOCTTY);
    if (*out_fd == -1)
    {
        perror(device);
        return ERR_UNEXPECTED;
    }
    if (!isatty(*out_fd))
    {
        perror("Not a TTY device");
        return ERR_INVALID;
    }
    LOG_TRACE("Device connected\n");

    struct termios options;

    if (tcgetattr(*out_fd, &initial_options) < 0)
    {
        LOG_ERROR("Could not read current device configuration.");
        close(*out_fd);
        return ERR_UNEXPECTED;
    }
    bzero(&options, sizeof(options));

    options.c_cflag        = (CLOCAL | CREAD | baud_rate | CRTSCTS | CS8);
    options.c_iflag        = IGNPAR | IGNCR;
    options.c_oflag        = 0;
    options.c_lflag        = ICANON;
    options.c_cc[VSWTC]    = 0;            /* '\0' */
    options.c_cc[VINTR]    = 0;            /* Ctrl-c */
    options.c_cc[VQUIT]    = 0;            /* Ctrl-\ */
    options.c_cc[VERASE]   = 0;            /* del */
    options.c_cc[VKILL]    = 0;            /* @ */
    options.c_cc[VEOF]     = 4;            /* Ctrl-d */
    options.c_cc[VTIME]    = 0;            /* inter-character timer unused */
    options.c_cc[VMIN]     = min_in_bytes; /* block for min_in_bytes char at least*/
    options.c_cc[VSTART]   = 0;            /* Ctrl-q */
    options.c_cc[VSTOP]    = 0;            /* Ctrl-s */
    options.c_cc[VSUSP]    = 0;            /* Ctrl-z */
    options.c_cc[VEOL]     = 0;            /* '\0' */
    options.c_cc[VREPRINT] = 0;            /* Ctrl-r */
    options.c_cc[VDISCARD] = 0;            /* Ctrl-u */
    options.c_cc[VWERASE]  = 0;            /* Ctrl-w */
    options.c_cc[VLNEXT]   = 0;            /* Ctrl-v */
    options.c_cc[VEOL2]    = 0; /* '\0' */ // expected num of bytes before returning

    cfsetispeed(&options, baud_rate);
    cfsetospeed(&options, baud_rate);

    _usb_utils_print_termios_struct(&options);

    // write port configuration to driver
    if (tcsetattr(*out_fd, TCSAFLUSH, &options))
    {
        LOG_ERROR("tcsetattr failed");
        close(*out_fd);
        return ERR_UNEXPECTED;
    }

    _usb_utils_flush(*out_fd);
    return ERR_ALL_GOOD;
}

void usb_utils_close_serial_port(int fd)
{
    tcsetattr(fd, TCSAFLUSH, &initial_options);
    close(fd);
}

// Writes bytes to the serial port, returning 0 on success and -1 on failure.
Error usb_utils_write_port(const int fd, const char* buffer, size_t size)
{
    ssize_t result = write(fd, buffer, size);
    if (result != (ssize_t)size)
    {
        LOG_ERROR("Failed to write to port.");
        return ERR_UNEXPECTED;
    }
    return ERR_ALL_GOOD;
}

Error usb_utils_read_port(const int fd, char* buffer, ssize_t* out_bytes_read_p)
{
    // Leave one empty spot in the array to ensure that even if the buffer is full it can be
    // null-terminated
    *out_bytes_read_p = read(fd, buffer, COMMUNICATION_BUFF_IN_SIZE - 1);
    if (errno == EINTR)
    {
        LOG_WARNING("User's interruption");
        return ERR_INTERRUPTION;
    }
    LOG_TRACE("Got: %zd byte(s)\n", *out_bytes_read_p);
    if (*out_bytes_read_p < 0)
    {
        perror("failed to read from port");
        return ERR_UNEXPECTED;
    }
    buffer[*out_bytes_read_p] = 0;
    return ERR_ALL_GOOD;
}
