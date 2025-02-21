
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
    printf("c_iflag:  %o\n", options->c_iflag & 0xfff);
    printf("c_oflag:  %o\n", options->c_oflag & 0xfffff);
    printf("c_cflag:  %o\n", options->c_cflag & 0xffffff);
    printf("c_lflag:  %o\n", options->c_lflag & 0xffffffff);
    printf("c_ispeed: %o\n", options->c_ispeed);
    printf("c_ospeed: %o\n", options->c_ospeed);
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
    printf("Device connected\n");

    struct termios options;

    if (tcgetattr(*out_fd, &initial_options) < 0)
    {
        printf("Could not read current device configuration.\n");
        close(*out_fd);
        return ERR_UNEXPECTED;
    }
    bzero(&options, sizeof(options));

    options.c_cflag        = (CLOCAL | CREAD | baud_rate | CRTSCTS | CS8);
    options.c_iflag        = IGNPAR | IGNCR;
    options.c_oflag        = 0;
    options.c_lflag        = ICANON;
    options.c_cc[VEOF]     = 4;            /* Ctrl-d */
    options.c_cc[VEOL]     = 0;            /* '\0' */
    options.c_cc[VEOL2]    = 0; /* '\0' */ // expected num of bytes before returning
    options.c_cc[VSWTC]    = 0;            /* '\0' */
    options.c_cc[VINTR]    = 0;            /* Ctrl-c */
    options.c_cc[VQUIT]    = 0;            /* Ctrl-\ */
    options.c_cc[VERASE]   = 0;            /* del */
    options.c_cc[VKILL]    = 0;            /* @ */
    options.c_cc[VTIME]    = 3;            /* inter-character timer unused */
    options.c_cc[VMIN]     = min_in_bytes; /* block for min_in_bytes char at least*/
    options.c_cc[VSTART]   = 0;            /* Ctrl-q */
    options.c_cc[VSTOP]    = 0;            /* Ctrl-s */
    options.c_cc[VSUSP]    = 0;            /* Ctrl-z */
    options.c_cc[VREPRINT] = 0;            /* Ctrl-r */
    options.c_cc[VDISCARD] = 0;            /* Ctrl-u */
    options.c_cc[VWERASE]  = 0;            /* Ctrl-w */
    options.c_cc[VLNEXT]   = 0;            /* Ctrl-v */

    cfsetispeed(&options, baud_rate);
    cfsetospeed(&options, baud_rate);

    _usb_utils_print_termios_struct(&options);

    // write port configuration to driver
    if (tcsetattr(*out_fd, TCSAFLUSH, &options))
    {
        printf("tcsetattr failed\n");
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
        printf("Failed to write to port.\n");
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
        printf("User's interruption\n");
        return ERR_INTERRUPTION;
    }
    printf("Got: %zd byte(s)\n", *out_bytes_read_p);
    if (*out_bytes_read_p < 0)
    {
        perror("failed to read from port");
        return ERR_UNEXPECTED;
    }
    buffer[*out_bytes_read_p] = 0;
    return ERR_ALL_GOOD;
}
