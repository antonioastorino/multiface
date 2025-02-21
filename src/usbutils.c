
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
    printf("c_iflag:  0%o\n", options->c_iflag & 0777777);
    printf("c_oflag:  0%o\n", options->c_oflag & 0777777);
    printf("c_cflag:  0%o\n", options->c_cflag & 0777777);
    printf("c_lflag:  0%o\n", options->c_lflag & 0777777);
    printf("c_ispeed: 0%o\n", options->c_ispeed);
    printf("c_ospeed: 0%o\n", options->c_ospeed);
    printf("c_cc[VEOF]    : %d\n", options->c_cc[VEOF]);
    printf("c_cc[VEOL]    : %d\n", options->c_cc[VEOL]);
    printf("c_cc[VEOL2]   : %d\n", options->c_cc[VEOL2]);
    printf("c_cc[VSWTC]   : %d\n", options->c_cc[VSWTC]);
    printf("c_cc[VINTR]   : %d\n", options->c_cc[VINTR]);
    printf("c_cc[VQUIT]   : %d\n", options->c_cc[VQUIT]);
    printf("c_cc[VERASE]  : %d\n", options->c_cc[VERASE]);
    printf("c_cc[VKILL]   : %d\n", options->c_cc[VKILL]);
    printf("c_cc[VTIME]   : %d\n", options->c_cc[VTIME]);
    printf("c_cc[VMIN]    : %d\n", options->c_cc[VMIN]);
    printf("c_cc[VSTART]  : %d\n", options->c_cc[VSTART]);
    printf("c_cc[VSTOP]   : %d\n", options->c_cc[VSTOP]);
    printf("c_cc[VSUSP]   : %d\n", options->c_cc[VSUSP]);
    printf("c_cc[VREPRINT]: %d\n", options->c_cc[VREPRINT]);
    printf("c_cc[VDISCARD]: %d\n", options->c_cc[VDISCARD]);
    printf("c_cc[VWERASE] : %d\n", options->c_cc[VWERASE]);
    printf("c_cc[VLNEXT]  : %d\n", options->c_cc[VLNEXT]);
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
    printf("Initial options:\n");
    _usb_utils_print_termios_struct(&initial_options);

    bzero(&options, sizeof(options));

    options.c_cflag        = CLOCAL | CREAD | CRTSCTS | CS8;
    options.c_iflag        = IGNPAR | IGNCR | BRKINT | ICRNL | IMAXBEL;
    options.c_oflag        = OPOST | ONLCR;
    options.c_lflag        = ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE | IEXTEN;
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

    cfsetospeed(&options, baud_rate);
    cfsetispeed(&options, baud_rate);

    printf("Final options:\n");
    _usb_utils_print_termios_struct(&options);

    // write port configuration to driver
    if (tcsetattr(*out_fd, TCSAFLUSH, &options))
    {
        printf("tcsetattr failed\n");
        close(*out_fd);
        return ERR_UNEXPECTED;
    }

    //_usb_utils_flush(*out_fd);
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
