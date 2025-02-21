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
    else {
        printf("`%s` FIFO is already there.\n", fifo_path_char_p);
    }
    if (chown(fifo_path_char_p, 1000, 1000) < 0)
    {
        printf("Failed to set FIFO `%s` ownership.\n", fifo_path_char_p);
        exit(ERR_FATAL);
    }
}

Error fifo_utils_wait_for_fifo_in(char* out_fifo_in_buffer_p, ssize_t *bytes_read)
{
    int fifo_fd = open(FIFO_IN, O_RDONLY);
    if (fifo_fd < 0)
    {
        printf("Failed to open FIFO `%s`.\n", FIFO_IN);
        return ERR_FATAL;
    }
    *bytes_read = read(fifo_fd, out_fifo_in_buffer_p, COMMUNICATION_BUFF_IN_SIZE);
    if (*bytes_read < 0)
    {
        printf("Failed to read from FIFO `%s`.\n", FIFO_IN);
        return ERR_FATAL;
    }
    close(fifo_fd);
    printf("Received: `%s`.\n", out_fifo_in_buffer_p);
    return ERR_ALL_GOOD;
}
