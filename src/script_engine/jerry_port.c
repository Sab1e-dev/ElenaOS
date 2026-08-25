/**
 * @file jerry_port.c
 * @brief JerryScript Port — implemented inside the ElenixOS kernel
 *
 * All JerryScript port functions are provided by the ElenixOS kernel
 * so that every target (simulator, real hardware) gets them through
 * the same kernel APIs:
 *
 *   - Time:       eos_time_get()          (service/time)
 *   - File I/O:   eos_fs_*()             (port/file_system)
 *   - Memory:     eos_malloc / eos_free  (kernel/memory)
 *   - Sleep:      eos_delay()            (port/eos_port.h)
 *   - Logging:    EOS_LOG_*()            (service/log)
 *
 * Simulators / hardware platforms only need to register the appropriate
 * device back-ends (eos_dev_time_register, eos_fs_set_root, …) at startup.
 */

/* Includes ---------------------------------------------------*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "jerryscript-port.h"
#include "eos_port.h"
#include "eos_service_time.h"
#include "eos_fs_port.h"
#include "eos_mem.h"
#include "eos_log.h"
#include "script_engine_core.h"

/* Macros and Definitions -------------------------------------*/

#define LOG_BUF_SIZE 1024

/* Variables --------------------------------------------------*/

/* Days per month (non-leap year) */
static const uint8_t _days_in_month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static char _log_buf[LOG_BUF_SIZE];
static size_t _log_buf_len = 0;

/* Function Implementations -----------------------------------*/

/* Portable calendar helpers (no POSIX required) --------------*/

static inline bool _is_leap_year(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/**
 * @brief Convert a calendar date to days since 1970-01-01.
 *
 * Pure integer arithmetic — works on bare metal.
 */
static uint64_t _datetime_to_days_since_epoch(const eos_datetime_t *dt)
{
    uint64_t days = 0;

    /* Full years */
    for (int y = 1970; y < (int)dt->year; y++)
    {
        days += _is_leap_year(y) ? 366 : 365;
    }

    /* Months of current year */
    for (int m = 1; m < (int)dt->month; m++)
    {
        days += _days_in_month[m - 1];
        if (m == 2 && _is_leap_year(dt->year))
        {
            days++;
        }
    }

    /* Days of current month */
    days += (uint64_t)(dt->day - 1);

    return days;
}

/**
 * @brief Convert eos_datetime_t → Unix epoch seconds (UTC).
 *
 * Assumes the input datetime is expressed in UTC.
 */
static uint64_t _datetime_to_epoch_utc(const eos_datetime_t *dt)
{
    uint64_t days = _datetime_to_days_since_epoch(dt);
    return days * 86400ULL + (uint64_t)dt->hour * 3600ULL + (uint64_t)dt->min * 60ULL + (uint64_t)dt->sec;
}

/* Date / Time ------------------------------------------------*/

double jerry_port_current_time(void)
{
    eos_datetime_t dt = eos_time_get();

#if defined(__unix__) || defined(__APPLE__) || defined(__EMSCRIPTEN__) || defined(_WIN32) || defined(__MINGW32__)
    /*
     * POSIX / simulator path:
     * eos_time_get() returns LOCAL broken-down time (see eos_port_time.c).
     * mktime() converts local broken-down → UTC time_t.
     */
    struct tm tm_info;
    memset(&tm_info, 0, sizeof(tm_info));
    tm_info.tm_year = (int)dt.year - 1900;
    tm_info.tm_mon = (int)dt.month - 1;
    tm_info.tm_mday = (int)dt.day;
    tm_info.tm_hour = (int)dt.hour;
    tm_info.tm_min = (int)dt.min;
    tm_info.tm_sec = (int)dt.sec;
    tm_info.tm_isdst = -1;

    time_t utc_sec = mktime(&tm_info);
    return (double)utc_sec * 1000.0 + (double)dt.ms;
#else
    /*
     * Bare-metal path:
     * eos_time_get() returns UTC.  Convert calendar → epoch directly.
     */
    uint64_t utc_sec = _datetime_to_epoch_utc(&dt);
    return (double)utc_sec * 1000.0 + (double)dt.ms;
#endif
}

int32_t jerry_port_local_tza(double unix_ms)
{
    time_t t = (time_t)(unix_ms / 1000.0);

#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || defined(__EMSCRIPTEN__)
    /*
     * BSD / glibc / macOS: struct tm has tm_gmtoff (seconds east of UTC).
     * ECMAScript LocalTZA expects offset to ADD to UTC to get local time
     * → positive for East, negative for West → matches tm_gmtoff.
     */
    struct tm tm_local;
    memset(&tm_local, 0, sizeof(tm_local));
    if (localtime_r(&t, &tm_local) != NULL)
    {
        return (int32_t)(tm_local.tm_gmtoff * 1000);
    }
    return 0;

#elif defined(_WIN32) || defined(__MINGW32__)
    /*
     * Windows: compute offset by comparing local ↔ UTC broken-down times.
     */
    struct tm tm_local, tm_utc;
    memset(&tm_local, 0, sizeof(tm_local));
    memset(&tm_utc, 0, sizeof(tm_utc));
    if (localtime_s(&tm_local, &t) != 0 || gmtime_s(&tm_utc, &t) != 0)
    {
        return 0;
    }
    time_t local_sec = _mkgmtime(&tm_local);
    time_t utc_sec = _mkgmtime(&tm_utc);
    return (int32_t)((local_sec - utc_sec) * 1000);

#else
    /*
     * Bare metal: no OS timezone info available.
     * Return 0 (UTC).  A platform with a known fixed offset can override
     * this file or define EOS_DEFAULT_TZ_OFFSET_MS at build time.
     */
    (void)t;
#ifdef EOS_DEFAULT_TZ_OFFSET_MS
    return EOS_DEFAULT_TZ_OFFSET_MS;
#else
    return 0;
#endif
#endif
}

/* Process / Lifecycle ----------------------------------------*/

void jerry_port_init(void)
{
    /* ElenixOS initialises eos_init() before the script engine starts. */
}

void JERRY_ATTR_NORETURN jerry_port_fatal(jerry_fatal_code_t code)
{
    EOS_LOG_E("[JerryScript] Fatal error: code=%d\r\n", (int)code);

    if (!script_engine_is_fatal_scope_active())
    {
        /*
         * longjmp to an invalid setjmp frame is undefined behaviour.
         * After script_engine_run() returns, its stack frame is gone.
         * This should never happen, but if it does, abort is safer
         * than corrupting memory via longjmp to a dead frame.
         */
        abort();
    }

    if (script_engine_is_fatal_recovering())
    {
        /* Prevent infinite longjmp loop if setjmp recovery itself faults */
        abort();
    }
    script_engine_set_fatal_recovering(true);

    script_engine_fatal_longjmp((int)(code != 0 ? code : -1));
    /* unreachable */
    while (1)
    {
    }
}

void jerry_port_sleep(uint32_t sleep_time)
{
#if defined(__EMSCRIPTEN__)
    (void)sleep_time;
    /* No-op on WASM — the browser event loop cannot block synchronously. */
#elif defined(__unix__) || defined(__APPLE__) || defined(_WIN32) || defined(__MINGW32__)
    eos_delay(sleep_time);
#else
    /*
     * Bare metal: busy-wait or delegate to the RTOS tick.
     * eos_delay() is the kernel's portable delay and will do the
     * right thing per platform (e.g. vTaskDelay on FreeRTOS).
     */
    eos_delay(sleep_time);
#endif
}

/* Logging / Console ------------------------------------------*/

/**
 * JerryScript delivers a single logical log line as multiple calls to
 * jerry_port_log(), one per fragment.  The final fragment is terminated
 * by '\n'.  If we forward every fragment to EOS_LOG_I immediately, each
 * one gets its own "[INFO]" prefix and newline, mangling the output.
 *
 * We therefore accumulate fragments into a static buffer and only flush
 * when a fragment ends with '\n' (i.e. the message is complete).
 */
void jerry_port_log(const char *message_p)
{
    if (!message_p)
    {
        return;
    }

    size_t msg_len = strlen(message_p);
    if (msg_len == 0)
    {
        return;
    }

    /* If the buffer can't hold this fragment, flush first. */
    size_t remaining = LOG_BUF_SIZE - _log_buf_len - 1; /* reserve 1 for NUL */
    if (msg_len > remaining)
    {
        _log_buf[_log_buf_len] = '\0';
        EOS_LOG_I("%s", _log_buf);
        _log_buf_len = 0;
        remaining = LOG_BUF_SIZE - 1;
    }

    /*
     * Append fragment.  Truncate if it's larger than the entire buffer
     * (should never happen for JerryScript diagnostic output).
     */
    size_t copy_len = msg_len;
    if (copy_len > remaining)
    {
        copy_len = remaining;
    }
    memcpy(_log_buf + _log_buf_len, message_p, copy_len);
    _log_buf_len += copy_len;
    _log_buf[_log_buf_len] = '\0';

    /* '\n' signals end of the logical line — flush. */
    if (_log_buf_len > 0 && _log_buf[_log_buf_len - 1] == '\n')
    {
        /* Strip trailing newline — EOS_LOG_I adds its own. */
        _log_buf[_log_buf_len - 1] = '\0';
        EOS_LOG_I("%s", _log_buf);
        _log_buf_len = 0;
    }
}

void jerry_port_print_buffer(const jerry_char_t *buffer_p, jerry_size_t buffer_size)
{
    if (buffer_p && buffer_size > 0)
    {
        fwrite(buffer_p, 1, (size_t)buffer_size, stdout);
        fflush(stdout);
    }
}

/* Line input (jerry-ext REPL only, not used by core) ---------*/

jerry_char_t *jerry_port_line_read(jerry_size_t *out_size_p)
{
    (void)out_size_p;
    return NULL; /* No interactive line input */
}

void jerry_port_line_free(jerry_char_t *buffer_p)
{
    (void)buffer_p;
}

/* File system — Paths ----------------------------------------*/

/**
 * @brief Normalise a file path through ElenixOS's virtual filesystem.
 *
 * eos_fs_realpath() prepends the sandbox root (set via eos_fs_set_root())
 * on simulator, or passes through on real hardware (root = "/").
 */
jerry_char_t *jerry_port_path_normalize(const jerry_char_t *path_p, jerry_size_t path_size)
{
    if (!path_p || path_size == 0)
    {
        return NULL;
    }

    /* Copy input to a NUL-terminated string */
    char *input = eos_malloc(path_size + 1);
    if (!input)
    {
        return NULL;
    }
    memcpy(input, path_p, path_size);
    input[path_size] = '\0';

    /* Resolve through the kernel's virtual → real path mapping */
    char resolved[EOS_FS_PATH_MAX];
    const char *rp = eos_fs_realpath(input, resolved, sizeof(resolved));
    eos_free(input);

    if (!rp)
    {
        return NULL;
    }

    size_t rp_len = strlen(rp);
    jerry_char_t *out = eos_malloc(rp_len + 1);
    if (!out)
    {
        return NULL;
    }
    memcpy(out, rp, rp_len + 1);
    return out;
}

void jerry_port_path_free(jerry_char_t *path_p)
{
    if (path_p)
    {
        eos_free(path_p);
    }
}

jerry_size_t jerry_port_path_base(const jerry_char_t *path_p)
{
    if (!path_p)
    {
        return 0;
    }

    const char *p = (const char *)path_p;
    const char *last = p;
    while (*p)
    {
        if (*p == '/')
        {
            last = p + 1;
        }
        p++;
    }
    return (jerry_size_t)(last - (const char *)path_p);
}

/* File system — Source file reading --------------------------*/

jerry_char_t *jerry_port_source_read(const char *file_name_p, jerry_size_t *out_size_p)
{
    if (!file_name_p || !out_size_p)
    {
        return NULL;
    }

    /*
     * Open through the ElenixOS file-system port.
     * eos_fs_open_read() internally calls eos_fs_realpath() so the
     * virtual-filesystem root (simulator sandbox) is always respected.
     */
    eos_file_t fp = eos_fs_open_read(file_name_p);
    if (fp == EOS_FILE_INVALID)
    {
        EOS_LOG_E("jerry_port_source_read: cannot open \"%s\"", file_name_p);
        return NULL;
    }

    uint32_t file_size = 0;
    if (eos_fs_size(fp, &file_size) != EOS_OK || file_size == 0)
    {
        EOS_LOG_E("jerry_port_source_read: bad size for \"%s\"", file_name_p);
        eos_fs_close(fp);
        return NULL;
    }

    jerry_char_t *buf = eos_malloc(file_size + 1);
    if (!buf)
    {
        eos_fs_close(fp);
        return NULL;
    }

    int bytes_read = eos_fs_read(fp, buf, file_size);
    eos_fs_close(fp);

    if (bytes_read != (int)file_size)
    {
        EOS_LOG_E("jerry_port_source_read: short read %d/%" PRIu32 " for \"%s\"", bytes_read, file_size, file_name_p);
        eos_free(buf);
        return NULL;
    }

    buf[file_size] = '\0';
    *out_size_p = (jerry_size_t)file_size;
    return buf;
}

void jerry_port_source_free(jerry_char_t *buffer_p)
{
    if (buffer_p)
    {
        eos_free(buffer_p);
    }
}
