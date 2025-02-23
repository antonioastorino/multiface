#define TEST 0
#define MEMORY_CHECK 0

#define UNUSED(x) (void)(x)

// ---------- ERROR ----------
#ifndef __APP_DEFINED__
#define __APP_DEFINED__
#endif /* __APP_DEFINED__ */

typedef enum
{
    ERR_FATAL = -1,
    ERR_ALL_GOOD,
    ERR_INVALID,
    ERR_UNDEFINED,
    ERR_INFALLIBLE,
    ERR_UNEXPECTED,
    ERR_FORBIDDEN,
    ERR_TIMEOUT,
    ERR_OUT_OF_RANGE,
    ERR_PERMISSION_DENIED,
    ERR_INTERRUPTION,
    ERR_NULL,
    ERR_PARSE_STRING_TO_INT,
    ERR_PARSE_STRING_TO_LONG_INT,
    ERR_PARSE_STRING_TO_FLOAT,
    ERR_EMPTY_STRING,
    ERR_JSON_INVALID,
    ERR_JSON_MISSING_ENTRY,
    ERR_TYPE_MISMATCH,
    ERR_FS_INTERNAL,
    ERR_TCP_INTERNAL,
    ERR_NOT_FOUND,
    __APP_DEFINED__
} Error;

#define is_err(_expr) (_expr != ERR_ALL_GOOD)
#define is_ok(_expr) (_expr == ERR_ALL_GOOD)

// ---------- LOGGER ----------

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define LEVEL_TRACE 5
#define LEVEL_DEBUG 4
#define LEVEL_INFO 3
#define LEVEL_WARNING 2
#define LEVEL_ERROR 1
#define LEVEL_NO_LOGS 0
#ifndef LOG_LEVEL
#define LOG_LEVEL LEVEL_TRACE
#endif

// Ensure that errors are not printed to stderr during tests as they would cause the unit test
// to fail.

#if TEST == 1
#define log_out stdout
#define log_err stdout
void test_logger(void);
#else /* TEST == 1 */
#define log_out get_log_out_file()
#define log_err get_log_err_file()
#endif

#define return_on_err(_expr)                                                                       \
    {                                                                                              \
        Error _res = _expr;                                                                        \
        if (_res != ERR_ALL_GOOD)                                                                  \
        {                                                                                          \
            LOG_WARNING("Error propagated from here.");                                            \
            return _res;                                                                           \
        }                                                                                          \
    }

#if LOG_LEVEL > LEVEL_NO_LOGS
#define DATE_TIME_STR_LEN 26
void logger_init(const char*, const char*);
pthread_mutex_t* logger_get_out_mut_p(void);
pthread_mutex_t* logger_get_err_mut_p(void);

FILE* get_log_out_file(void);
FILE* get_log_err_file(void);

void get_date_time(char* date_time_str);

#define log_header_o(TYPE)                                                                         \
    char date_time_str[DATE_TIME_STR_LEN];                                                         \
    get_date_time(date_time_str);                                                                  \
    pthread_mutex_lock(logger_get_out_mut_p());                                                    \
    fprintf(                                                                                       \
        log_out,                                                                                   \
        "[%5s] <%d> %s %s:%d | ",                                                                  \
        #TYPE,                                                                                     \
        getpid(),                                                                                  \
        date_time_str,                                                                             \
        __FILENAME__,                                                                              \
        __LINE__);

#define log_header_e(TYPE)                                                                         \
    char date_time_str[DATE_TIME_STR_LEN];                                                         \
    get_date_time(date_time_str);                                                                  \
    pthread_mutex_lock(logger_get_err_mut_p());                                                    \
    fprintf(                                                                                       \
        log_err,                                                                                   \
        "[%5s] <%d> %s %s:%d | ",                                                                  \
        #TYPE,                                                                                     \
        getpid(),                                                                                  \
        date_time_str,                                                                             \
        __FILENAME__,                                                                              \
        __LINE__);

#define log_footer_o(...)                                                                          \
    fprintf(log_out, __VA_ARGS__);                                                                 \
    fprintf(log_out, "\n");                                                                        \
    fflush(log_out);                                                                               \
    pthread_mutex_unlock(logger_get_out_mut_p());

#define log_footer_e(...)                                                                          \
    fprintf(log_err, __VA_ARGS__);                                                                 \
    fprintf(log_err, "\n");                                                                        \
    fflush(log_err);                                                                               \
    pthread_mutex_unlock(logger_get_err_mut_p());

#define PRINT_SEPARATOR()                                                                          \
    {                                                                                              \
        char date_time_str[DATE_TIME_STR_LEN];                                                     \
        get_date_time(date_time_str);                                                              \
        pthread_mutex_lock(logger_get_out_mut_p());                                                \
        fprintf(log_out, "------- <%d> %s -------\n", getpid(), date_time_str);                    \
        pthread_mutex_unlock(logger_get_out_mut_p());                                              \
    }

#else /* LOG_LEVEL > LEVEL_NO_LOGS */
#define get_date_time(something)
#define PRINT_SEPARATOR()
#endif /* LOG_LEVEL > LEVEL_NO_LOGS */

#if LOG_LEVEL >= LEVEL_ERROR
#define LOG_ERROR(...)                                                                             \
    {                                                                                              \
        log_header_e(ERROR);                                                                       \
        log_footer_e(__VA_ARGS__);                                                                 \
    }

#define LOG_PERROR(...)                                                                            \
    {                                                                                              \
        log_header_e(ERROR);                                                                       \
        fprintf(log_err, "`%s` | ", strerror(errno));                                              \
        log_footer_e(__VA_ARGS__);                                                                 \
    }
#else
#define LOG_ERROR(...)
#endif

#if LOG_LEVEL >= LEVEL_WARNING
#define LOG_WARNING(...)                                                                           \
    {                                                                                              \
        log_header_e(WARN);                                                                        \
        log_footer_e(__VA_ARGS__);                                                                 \
    }
#else
#define LOG_WARNING(...)
#endif

#if LOG_LEVEL >= LEVEL_INFO
#define LOG_INFO(...)                                                                              \
    {                                                                                              \
        log_header_o(INFO);                                                                        \
        log_footer_o(__VA_ARGS__);                                                                 \
    }
#else
#define LOG_INFO(...)
#endif

#if LOG_LEVEL >= LEVEL_DEBUG
#define LOG_DEBUG(...)                                                                             \
    {                                                                                              \
        log_header_o(DEBUG);                                                                       \
        log_footer_o(__VA_ARGS__);                                                                 \
    }
#else
#define LOG_DEBUG(...)
#endif

#if LOG_LEVEL >= LEVEL_TRACE
#define LOG_TRACE(...)                                                                             \
    {                                                                                              \
        log_header_o(TRACE);                                                                       \
        log_footer_o(__VA_ARGS__);                                                                 \
    }
#else
#define LOG_TRACE(...)
#endif

// ---------- ASSERT ----------
#if TEST == 1

void ASSERT_(bool, const char*, const char*, int);
void ASSERT_OK_(Error, const char*, const char*, int);
void ASSERT_ERR_(Error, const char*, const char*, int);

void ASSERT_EQ_int(int, int, const char*, const char*, int);
void ASSERT_EQ_uint8(uint8_t, uint8_t, const char*, const char*, int);
void ASSERT_EQ_uint16(uint16_t, uint16_t, const char*, const char*, int);
void ASSERT_EQ_uint32(uint32_t, uint32_t, const char*, const char*, int);
void ASSERT_EQ_uint(size_t, size_t, const char*, const char*, int);
void ASSERT_EQ_bool(bool v1, bool v2, const char*, const char*, int);
void ASSERT_EQ_float(float, float, const char*, const char*, int);
void ASSERT_EQ_double(double, double, const char*, const char*, int);
void ASSERT_EQ_char_p(const char*, const char*, const char*, const char*, int);

void ASSERT_NE_int(int, int, const char*, const char*, int);
void ASSERT_NE_uint8(uint8_t, uint8_t, const char*, const char*, int);
void ASSERT_NE_uint16(uint16_t, uint16_t, const char*, const char*, int);
void ASSERT_NE_uint(size_t, size_t, const char*, const char*, int);
void ASSERT_NE_bool(bool v1, bool v2, const char*, const char*, int);
void ASSERT_NE_float(float, float, const char*, const char*, int);
void ASSERT_NE_double(double, double, const char*, const char*, int);
void ASSERT_NE_char_p(const char*, const char*, const char*, const char*, int);

#define PRINT_BANNER()                                                                             \
    printf("\n");                                                                                  \
    for (size_t i = 0; i < strlen(__FUNCTION__) + 12; i++)                                         \
    {                                                                                              \
        printf("=");                                                                               \
    }                                                                                              \
    printf("\n-- TEST: %s --\n", __FUNCTION__);                                                    \
    for (size_t i = 0; i < strlen(__FUNCTION__) + 12; i++)                                         \
    {                                                                                              \
        printf("=");                                                                               \
    }                                                                                              \
    printf("\n");                                                                                  \
    size_t test_counter_ = 0;

#define ASSERT(value, message) ASSERT_(value, message, __FILE__, __LINE__)
#define ASSERT_OK(value, message) ASSERT_OK_(value, message, __FILE__, __LINE__)
#define ASSERT_ERR(value, message) ASSERT_ERR_(value, message, __FILE__, __LINE__)
// clang-format off
#define ASSERT_EQ(value_1, value_2, message)      \
    _Generic((value_1),                           \
        int           : ASSERT_EQ_int,            \
        int16_t       : ASSERT_EQ_int,            \
        uint8_t       : ASSERT_EQ_uint8,          \
        uint16_t      : ASSERT_EQ_uint16,         \
        uint32_t      : ASSERT_EQ_uint32,         \
        size_t        : ASSERT_EQ_uint,           \
        bool          : ASSERT_EQ_bool,           \
        float         : ASSERT_EQ_float,          \
        double        : ASSERT_EQ_double,         \
        char*         : ASSERT_EQ_char_p,         \
        const char*   : ASSERT_EQ_char_p          \
    )(value_1, value_2, message, __FILE__, __LINE__)

#define ASSERT_NE(value_1, value_2, message)      \
    _Generic((value_1),                           \
        int           : ASSERT_NE_int,            \
        uint8_t       : ASSERT_NE_uint8,          \
        uint16_t      : ASSERT_NE_uint16,         \
        size_t        : ASSERT_NE_uint,           \
        bool          : ASSERT_NE_bool,           \
        float         : ASSERT_NE_float,          \
        double        : ASSERT_NE_double,         \
        char*         : ASSERT_NE_char_p,         \
        const char*   : ASSERT_NE_char_p          \
    )(value_1, value_2, message, __FILE__, __LINE__)

// clang-format on

#define PRINT_TEST_TITLE(title) printf("\n------- T:%lu < %s > -------\n", ++test_counter_, title);
#endif

// ---------- LOGGER ----------
#if LOG_LEVEL > LEVEL_NO_LOGS

static FILE* log_out_file_p = NULL;
static FILE* log_err_file_p = NULL;
static pthread_mutex_t log_out_mutex;
static pthread_mutex_t log_err_mutex;

FILE* get_log_out_file(void) { return log_out_file_p == NULL ? stdout : log_out_file_p; }
FILE* get_log_err_file(void) { return log_err_file_p == NULL ? stderr : log_err_file_p; }

pthread_mutex_t* logger_get_out_mut_p(void) { return &log_out_mutex; }
pthread_mutex_t* logger_get_err_mut_p(void) { return &log_err_mutex; }

void _logger_open_out_file(const char* log_out_file_path_str)
{
    log_out_file_p = fopen(log_out_file_path_str, "a");
    if (log_out_file_p == NULL)
    {
        perror("Fatal error: could not open logger out file.");
        exit(-1);
    }
    LOG_INFO("Logger OUT file opened.");
}
void _logger_open_err_file(const char* log_err_file_path_str)
{
    log_err_file_p = fopen(log_err_file_path_str, "a");
    if (log_err_file_p == NULL)
    {
        perror("Fatal error: could not open logger out file.");
        exit(-1);
    }
    LOG_INFO("Logger ERR file opened.");
}

void logger_init(const char* log_out_file_path_str, const char* log_err_file_path_str)
{
    static bool logger_initialized = false;
    if (logger_initialized)
    {
        LOG_ERROR("Logger already initialized.");
        return;
    }
    LOG_INFO("Initializing logger.");
    pthread_mutex_init(&log_out_mutex, NULL);
    pthread_mutex_init(&log_err_mutex, NULL);
    logger_initialized = true;

    if (log_out_file_path_str != NULL && log_err_file_path_str != NULL)
    {
        _logger_open_out_file(log_out_file_path_str);
        if (strcmp(log_out_file_path_str, log_err_file_path_str) == 0)
        {
            log_err_file_p = log_out_file_p;
            LOG_INFO("Logger ERR file matches logger OUT file.");
        }
        else
        {
            _logger_open_err_file(log_err_file_path_str);
        }
    }
    if (log_out_file_path_str == NULL)
    {
        log_out_file_p = stdout;
        LOG_INFO("Logger OUT set to standard out.");
    }
    else if (log_out_file_p == NULL)
    {
        _logger_open_out_file(log_out_file_path_str);
    }

    if (log_err_file_path_str == NULL)
    {
        log_err_file_p = stderr;
        LOG_INFO("Logger ERR set to standard error.");
    }
    else if (log_err_file_p == NULL)
    {
        _logger_open_err_file(log_err_file_path_str);
    }
}

void get_date_time(char* date_time_str)
{
    time_t ltime;
    struct tm result;
    ltime = time(NULL);
    localtime_r(&ltime, &result);
    // The string must be at least 26 character long. The returned value contains a \n\0 at
    // the end.
    asctime_r(&result, date_time_str);
    // Overwrite the \n to avoid a new line.
    date_time_str[24] = 0;
}

#endif /* LOG_LEVEL > LEVEL_NO_LOGS */
#if TEST == 1
void test_logger(void)
{
    PRINT_BANNER()
    PRINT_TEST_TITLE("Logging all levels");
    PRINT_SEPARATOR();
    LOG_TRACE("Log trace.");
    LOG_DEBUG("Log debug.");
    LOG_INFO("Log info.");
    LOG_WARNING("Log warning.");
    LOG_ERROR("Log error.");
}
#endif /* TEST == 1 */

// ---------- ASSERT ----------

#define PRINT_PASS_MESSAGE(message) printf("> \x1B[32mPASS\x1B[0m\t %s\n", message)

#define PRINT_FAIL_MESSAGE_(message, filename, line_number)                                        \
    fprintf(stderr, "> \x1B[31mFAIL\x1B[0m\t %s\n", message);                                      \
    fprintf(stderr, "> Err - Test failed.\n%s:%d : false assertion\n", filename, line_number)

#define PRINT_FAIL_MESSAGE_EQ(message, filename, line_number)                                      \
    fprintf(stderr, "> \x1B[31mFAIL\x1B[0m\t %s\n", message);                                      \
    fprintf(stderr, "> Err - Test failed.\n%s:%d : left != right\n", filename, line_number)

#define PRINT_FAIL_MESSAGE_NE(message, filename, line_number)                                      \
    fprintf(stderr, "> \x1B[31mFAIL\x1B[0m\t %s\n", message);                                      \
    fprintf(stderr, "> Err - Test failed.\n%s:%d : left == right\n", filename, line_number)

void ASSERT_(bool value, const char* message, const char* filename, int line_number)
{
    if (value)
    {
        PRINT_PASS_MESSAGE(message);
    }
    else
    {
        PRINT_FAIL_MESSAGE_(message, filename, line_number);
        fprintf(stderr, "The value is `false`\n");
    }
}

void ASSERT_OK_(Error result, const char* message, const char* filename, int line_number)
{
    if (is_ok(result))
    {
        PRINT_PASS_MESSAGE(message);
    }
    else
    {
        PRINT_FAIL_MESSAGE_(message, filename, line_number);
        fprintf(stderr, "The value is `false`\n");
    }
}

void ASSERT_ERR_(Error result, const char* message, const char* filename, int line_number)
{
    if (is_err(result))
    {
        PRINT_PASS_MESSAGE(message);
    }
    else
    {
        PRINT_FAIL_MESSAGE_(message, filename, line_number);
        fprintf(stderr, "The value is `false`\n");
    }
}

void ASSERT_EQ_int(
    int value_1,
    int value_2,
    const char* message,
    const char* filename,
    int line_number)
{
    if (value_1 == value_2)
    {
        PRINT_PASS_MESSAGE(message);
    }
    else
    {
        PRINT_FAIL_MESSAGE_EQ(message, filename, line_number);
        fprintf(stderr, "Left : `%d`\nRight: `%d`\n", value_1, value_2);
    }
}

void ASSERT_EQ_uint8(
    uint8_t value_1,
    uint8_t value_2,
    const char* message,
    const char* filename,
    int line_number)
{
    if (value_1 == value_2)
    {
        PRINT_PASS_MESSAGE(message);
    }
    else
    {
        PRINT_FAIL_MESSAGE_EQ(message, filename, line_number);
        fprintf(stderr, "Left : `%u`\nRight: `%u`\n", value_1, value_2);
    }
}

void ASSERT_EQ_uint16(
    uint16_t value_1,
    uint16_t value_2,
    const char* message,
    const char* filename,
    int line_number)
{
    if (value_1 == value_2)
    {
        PRINT_PASS_MESSAGE(message);
    }
    else
    {
        PRINT_FAIL_MESSAGE_EQ(message, filename, line_number);
        fprintf(stderr, "Left : `%hu`\nRight: `%hu`\n", value_1, value_2);
    }
}

void ASSERT_EQ_uint32(
    uint32_t value_1,
    uint32_t value_2,
    const char* message,
    const char* filename,
    int line_number)
{
    if (value_1 == value_2)
    {
        PRINT_PASS_MESSAGE(message);
    }
    else
    {
        PRINT_FAIL_MESSAGE_EQ(message, filename, line_number);
        fprintf(stderr, "Left : `%u`\nRight: `%u`\n", value_1, value_2);
    }
}

void ASSERT_EQ_uint(
    size_t value_1,
    size_t value_2,
    const char* message,
    const char* filename,
    int line_number)
{
    if (value_1 == value_2)
    {
        PRINT_PASS_MESSAGE(message);
    }
    else
    {
        PRINT_FAIL_MESSAGE_EQ(message, filename, line_number);
        fprintf(stderr, "Left : `%lu`\nRight: `%lu`\n", value_1, value_2);
    }
}

void ASSERT_EQ_bool(
    bool value_1,
    bool value_2,
    const char* message,
    const char* filename,
    int line_number)
{
    if (value_1 == value_2)
    {
        PRINT_PASS_MESSAGE(message);
    }
    else
    {
        PRINT_FAIL_MESSAGE_EQ(message, filename, line_number);
        fprintf(
            stderr,
            "Left : `%s`\nRight: `%s`\n",
            value_1 ? "true" : "false",
            value_2 ? "true" : "false");
    }
}
void ASSERT_EQ_float(
    float value_1,
    float value_2,
    const char* message,
    const char* filename,
    int line_number)
{
    if (value_1 == value_2)
    {
        PRINT_PASS_MESSAGE(message);
    }
    else
    {
        PRINT_FAIL_MESSAGE_EQ(message, filename, line_number);
        fprintf(stderr, "Left : `%f`\nRight: `%f`\n", value_1, value_2);
    }
}

void ASSERT_EQ_double(
    double value_1,
    double value_2,
    const char* message,
    const char* filename,
    int line_number)
{
    if (value_1 == value_2)
    {
        PRINT_PASS_MESSAGE(message);
    }
    else
    {
        PRINT_FAIL_MESSAGE_EQ(message, filename, line_number);
        fprintf(stderr, "Left : `%lf`\nRight: `%lf`\n", value_1, value_2);
    }
}

void ASSERT_EQ_char_p(
    const char* value_1,
    const char* value_2,
    const char* message,
    const char* filename,
    int line_number)
{
    if (!strcmp(value_1, value_2))
    {
        PRINT_PASS_MESSAGE(message);
    }
    else
    {
        PRINT_FAIL_MESSAGE_EQ(message, filename, line_number);
        fprintf(stderr, "Left : `%s`\nRight: `%s`\n", value_1, value_2);
    }
}

void ASSERT_NE_int(
    int value_1,
    int value_2,
    const char* message,
    const char* filename,
    int line_number)
{
    if (value_1 != value_2)
    {
        PRINT_PASS_MESSAGE(message);
    }
    else
    {
        PRINT_FAIL_MESSAGE_NE(message, filename, line_number);
        fprintf(stderr, "Left : `%d`\nRight: `%d`\n", value_1, value_2);
    }
}

void ASSERT_NE_uchar(
    uint8_t value_1,
    uint8_t value_2,
    const char* message,
    const char* filename,
    int line_number)
{
    if (value_1 != value_2)
    {
        PRINT_PASS_MESSAGE(message);
    }
    else
    {
        PRINT_FAIL_MESSAGE_NE(message, filename, line_number);
        fprintf(stderr, "Left : `%d`\nRight: `%d`\n", value_1, value_2);
    }
}

void ASSERT_NE_uint(
    size_t value_1,
    size_t value_2,
    const char* message,
    const char* filename,
    int line_number)
{
    if (value_1 != value_2)
    {
        PRINT_PASS_MESSAGE(message);
    }
    else
    {
        PRINT_FAIL_MESSAGE_NE(message, filename, line_number);
        fprintf(stderr, "Left : `%lu`\nRight: `%lu`\n", value_1, value_2);
    }
}

void ASSERT_NE_bool(
    bool value_1,
    bool value_2,
    const char* message,
    const char* filename,
    int line_number)
{
    if (value_1 != value_2)
    {
        PRINT_PASS_MESSAGE(message);
    }
    else
    {
        PRINT_FAIL_MESSAGE_NE(message, filename, line_number);
        fprintf(
            stderr,
            "Left : `%s`\nRight: `%s`\n",
            value_1 ? "true" : "false",
            value_2 ? "true" : "false");
    }
}
void ASSERT_NE_float(
    float value_1,
    float value_2,
    const char* message,
    const char* filename,
    int line_number)
{
    if (value_1 == value_2)
    {
        PRINT_PASS_MESSAGE(message);
    }
    else
    {
        PRINT_FAIL_MESSAGE_NE(message, filename, line_number);
        fprintf(stderr, "Left : `%f`\nRight: `%f`\n", value_1, value_2);
    }
}

void ASSERT_NE_double(
    double value_1,
    double value_2,
    const char* message,
    const char* filename,
    int line_number)
{
    if (value_1 == value_2)
    {
        PRINT_PASS_MESSAGE(message);
    }
    else
    {
        PRINT_FAIL_MESSAGE_NE(message, filename, line_number);
        fprintf(stderr, "Left : `%lf`\nRight: `%lf`\n", value_1, value_2);
    }
}

void ASSERT_NE_char_p(
    const char* value_1,
    const char* value_2,
    const char* message,
    const char* filename,
    int line_number)
{
    if (!strcmp(value_1, value_2))
    {
        PRINT_PASS_MESSAGE(message);
    }
    else
    {
        PRINT_FAIL_MESSAGE_NE(message, filename, line_number);
        fprintf(stderr, "Left : `%s`\nRight: `%s`\n", value_1, value_2);
    }
}
