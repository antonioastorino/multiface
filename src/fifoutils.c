void fifo_utils_make_fifo(const char* fifo_path_char_p)
{
    struct stat st;
    if (stat(fifo_path_char_p, &st) < 0)
    {
        printf("Trying to create FIFO `%s`.\n", fifo_path_char_p);
        if (mkfifo(fifo_path_char_p, 0777) < 0)
        {
            printf("Failed to create FIFO `%s`.\n", fifo_path_char_p);
            exit(ERR_FATAL);
        }
        printf("FIFO `%s` successfully created.\n", fifo_path_char_p);
    }
    else if ((st.st_mode & S_IFMT) != S_IFIFO)
    {
        printf("`%s` should be a FIFO.\n", fifo_path_char_p);
        exit(ERR_FATAL);
    }
    else
    {
        printf("`%s` FIFO is already there.\n", fifo_path_char_p);
    }
#ifdef __linux__
    if (chown(fifo_path_char_p, 1000, 1000) < 0)
    {
        printf("Failed to set FIFO `%s` ownership.\n", fifo_path_char_p);
        exit(ERR_FATAL);
    }
#endif /* __linux__ */
}

void fifo_utils_flush_fifo_in(void)
{
    printf("Flushing FIFO IN\n");
    char c;
    ssize_t tmp = 0;
    int fifo_fd = open(FIFO_IN, O_RDONLY | O_NONBLOCK);
    if (fifo_fd < 0)
    {
        printf("Failed to open FIFO `%s`.\n", FIFO_IN);
    }

    while ((tmp = read(fifo_fd, &c, 1)) != 0)
    {
        printf("%c", c);
    }
    printf(" - tmp %zd", tmp);
    printf("\n");
    close(fifo_fd);
}

Error fifo_utils_wait_for_fifo_in(SizedBuffer* fifo_buffer_p, int fifo_fd)
{
    ssize_t bytes_read = 0;
    char c;
    ssize_t tmp;
    while (bytes_read < COMMUNICATION_BUFF_IN_SIZE)
    {
        tmp = read(fifo_fd, &c, 1);
        if (tmp < 0)
        {
            printf("Failed to read from FIFO `%s`.\n", FIFO_IN);
            return ERR_FATAL;
        }
        else if (tmp == 0)
        {
            break;
        }
        else
        {
            fifo_buffer_p->buffer[bytes_read] = c;
            bytes_read++;
            if (c == '\n')
            {
                break;
            }
        }
    }
    fifo_buffer_p->size = bytes_read;
    if (bytes_read)
    {
        printf("Received: `%s`, bytes: %lu.\n", fifo_buffer_p->buffer, bytes_read);
    }
    return ERR_ALL_GOOD;
}
