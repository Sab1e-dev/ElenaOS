/**
 * @file esh_command.c
 * @brief ESH command table access
 */

#include "esh_command.h"

/* Includes ---------------------------------------------------*/
#if defined(__APPLE__) && defined(__MACH__)
#include <mach-o/getsect.h>
#include <mach-o/loader.h>
#include <stdbool.h>
#include <stdint.h>
#endif

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

#if ESH_USE_LINKER_SECTION && !defined(__APPLE__)
extern const esh_command_t __esh_cmd_start[] __attribute__((weak));
extern const esh_command_t __esh_cmd_end[] __attribute__((weak));
#endif
#if ESH_USE_LINKER_SECTION && defined(__APPLE__) && defined(__MACH__)
extern const struct mach_header_64 _mh_execute_header;

static const esh_command_t *s_macho_command_begin;
static const esh_command_t *s_macho_command_end;
static bool s_macho_command_scanned;
#endif
extern const esh_command_t *_esh_builtin_command_begin(void);
extern const esh_command_t *_esh_builtin_command_end(void);

/* Function Implementations -----------------------------------*/

#if ESH_USE_LINKER_SECTION && defined(__APPLE__) && defined(__MACH__)
static bool _esh_macho_scan_commands(void)
{
    unsigned long section_size = 0;
    const uint8_t *section_data = getsectiondata(&_mh_execute_header, "__DATA", "esh_cmd", &section_size);

    s_macho_command_scanned = true;
    if (!section_data || section_size == 0 || section_size % sizeof(esh_command_t) != 0)
    {
        return false;
    }

    s_macho_command_begin = (const esh_command_t *)section_data;
    s_macho_command_end = s_macho_command_begin + section_size / sizeof(esh_command_t);
    return true;
}

static bool _esh_macho_commands_available(void)
{
    if (!s_macho_command_scanned)
    {
        return _esh_macho_scan_commands();
    }

    return s_macho_command_begin && s_macho_command_end;
}
#endif

const esh_command_t *esh_command_begin(void)
{
#if ESH_USE_LINKER_SECTION && !defined(__APPLE__)
    if (__esh_cmd_start && __esh_cmd_end && __esh_cmd_end >= __esh_cmd_start)
    {
        return __esh_cmd_start;
    }

    return _esh_builtin_command_begin();
#elif ESH_USE_LINKER_SECTION && defined(__APPLE__) && defined(__MACH__)
    if (_esh_macho_commands_available())
    {
        return s_macho_command_begin;
    }

    return _esh_builtin_command_begin();
#else
    return _esh_builtin_command_begin();
#endif
}

const esh_command_t *esh_command_end(void)
{
#if ESH_USE_LINKER_SECTION && !defined(__APPLE__)
    if (__esh_cmd_start && __esh_cmd_end && __esh_cmd_end >= __esh_cmd_start)
    {
        return __esh_cmd_end;
    }

    return _esh_builtin_command_end();
#elif ESH_USE_LINKER_SECTION && defined(__APPLE__) && defined(__MACH__)
    if (_esh_macho_commands_available())
    {
        return s_macho_command_end;
    }

    return _esh_builtin_command_end();
#else
    return _esh_builtin_command_end();
#endif
}
