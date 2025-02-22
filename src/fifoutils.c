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

Error fifo_utils_wait_for_fifo_in(SizedBuffer* fifo_buffer_p)
{
    int fifo_fd = open(FIFO_IN, O_RDONLY);
    if (fifo_fd < 0)
    {
        printf("Failed to open FIFO `%s`.\n", FIFO_IN);
        return ERR_FATAL;
    }
    fifo_buffer_p->size = read(fifo_fd, fifo_buffer_p->buffer, COMMUNICATION_BUFF_IN_SIZE);
    if (fifo_buffer_p->size < 0)
    {
        printf("Failed to read from FIFO `%s`.\n", FIFO_IN);
        return ERR_FATAL;
    }
    close(fifo_fd);
    printf("Received: `%s`.\n", fifo_buffer_p->buffer);
    return ERR_ALL_GOOD;
}
