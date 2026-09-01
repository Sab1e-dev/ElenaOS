/**
 * @file esh_builtin_debug.c
 * @brief Debug and diagnostic ESH commands
 */

#include "esh_builtin_commands.h"

/* Includes ---------------------------------------------------*/
#include <inttypes.h>
#include <string.h>
#include "eos_activity.h"
#include "eos_log.h"
#include "eos_mem.h"
#include "eos_recent_apps.h"
#include "jerryscript.h"
#include "spm.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Prototypes ----------------------------------------*/

/* Function Implementations -----------------------------------*/

static const char *_log_level_name(eos_log_level_t level)
{
    static const char *const names[] = {"debug", "info", "warn", "error"};

    return level <= EOS_LOG_LEVEL_ERROR ? names[level] : "unknown";
}

static bool _parse_log_level(const char *text, eos_log_level_t *level)
{
    eos_log_level_t candidate;

    if (!text || !level)
    {
        return false;
    }

    for (candidate = EOS_LOG_LEVEL_DEBUG; candidate <= EOS_LOG_LEVEL_ERROR; candidate++)
    {
        if (strcmp(text, _log_level_name(candidate)) == 0)
        {
            *level = candidate;
            return true;
        }
    }

    return false;
}

int esh_builtin_cmd_log(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    eos_log_level_t level;

    if (!ctx || !argv)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (argc == 1 || (argc == 2 && strcmp(argv[1], "show") == 0) || (argc == 2 && strcmp(argv[1], "level") == 0)
        || (argc == 3 && strcmp(argv[1], "level") == 0 && strcmp(argv[2], "get") == 0))
    {
        level = eos_log_get_level();
        return (int)esh_printf(ctx, "log level: %s\r\n", _log_level_name(level));
    }

    if (argc == 3 && strcmp(argv[1], "level") == 0)
    {
        if (!_parse_log_level(argv[2], &level))
        {
            return (int)esh_printf(ctx, "log: invalid level: %s\r\n", argv[2]);
        }

        if (eos_log_set_level(level) != EOS_OK)
        {
            return (int)esh_printf(ctx, "log: cannot set level\r\n");
        }

        return (int)esh_printf(ctx, "log level: %s\r\n", _log_level_name(level));
    }

    return (int)esh_printf(ctx, "log: usage: log [show|level [get|debug|info|warn|error]]\r\n");
}

static int _print_lvgl_memory(esh_cmd_ctx_t *ctx)
{
    lv_mem_monitor_t monitor;
    char total[64];
    char free_size[64];
    char largest_free[64];
    char max_used[64];

    lv_mem_monitor(&monitor);
    if (!esh_builtin_format_bytes(monitor.total_size, total, sizeof(total))
        || !esh_builtin_format_bytes(monitor.free_size, free_size, sizeof(free_size))
        || !esh_builtin_format_bytes(monitor.free_biggest_size, largest_free, sizeof(largest_free))
        || !esh_builtin_format_bytes(monitor.max_used, max_used, sizeof(max_used)))
    {
        return EOS_ERR_IO;
    }

    if (esh_printf(ctx, "LVGL heap\r\n") != EOS_OK || esh_printf(ctx, "  total: %s\r\n", total) != EOS_OK
        || esh_printf(ctx, "  free:  %s\r\n", free_size) != EOS_OK
        || esh_printf(ctx, "  largest free: %s\r\n", largest_free) != EOS_OK
        || esh_printf(ctx, "  max used: %s\r\n", max_used) != EOS_OK)
    {
        return EOS_ERR_IO;
    }

    return esh_printf(ctx,
                      "  used: %u%%, fragmentation: %u%%, used blocks: %zu, free blocks: %zu\r\n",
                      monitor.used_pct,
                      monitor.frag_pct,
                      monitor.used_cnt,
                      monitor.free_cnt);
}

static int _print_jerry_memory(esh_cmd_ctx_t *ctx)
{
    jerry_heap_stats_t stats = {0};
    char total[64];
    char allocated[64];
    char peak[64];

    if (!jerry_feature_enabled(JERRY_FEATURE_HEAP_STATS) || !jerry_heap_stats(&stats))
    {
        return (int)esh_printf(ctx, "JerryScript heap: unavailable\r\n");
    }

    if (!esh_builtin_format_bytes(stats.size, total, sizeof(total))
        || !esh_builtin_format_bytes(stats.allocated_bytes, allocated, sizeof(allocated))
        || !esh_builtin_format_bytes(stats.peak_allocated_bytes, peak, sizeof(peak)))
    {
        return EOS_ERR_IO;
    }

    if (esh_printf(ctx, "JerryScript\r\n") != EOS_OK || esh_printf(ctx, "  heap: %s\r\n", total) != EOS_OK
        || esh_printf(ctx, "  allocated: %s\r\n", allocated) != EOS_OK)
    {
        return EOS_ERR_IO;
    }

    return esh_printf(ctx, "  peak allocated: %s\r\n", peak);
}

int esh_builtin_cmd_mem(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    char used[64];
    char free_size[64];
    const char *mode;

    if (!ctx || !argv)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (argc > 2)
    {
        return (int)esh_printf(ctx, "mem: usage: mem [lvgl|jerry|pools]\r\n");
    }

    mode = argc == 2 ? argv[1] : "all";
    if (strcmp(mode, "all") == 0 || strcmp(mode, "pools") == 0)
    {
        if (!esh_builtin_format_bytes(eos_mem_get_used_bytes(), used, sizeof(used))
            || !esh_builtin_format_bytes(eos_mem_get_free_bytes(), free_size, sizeof(free_size)))
        {
            return EOS_ERR_IO;
        }

        if (esh_printf(ctx, "ElenixOS memory\r\n  used: %s\r\n  free: %s\r\n", used, free_size) != EOS_OK)
        {
            return EOS_ERR_IO;
        }
    }

    if (strcmp(mode, "all") == 0 || strcmp(mode, "lvgl") == 0)
    {
        if (_print_lvgl_memory(ctx) != EOS_OK)
        {
            return EOS_ERR_IO;
        }
    }

    if (strcmp(mode, "all") == 0 || strcmp(mode, "jerry") == 0)
    {
        if (_print_jerry_memory(ctx) != EOS_OK)
        {
            return EOS_ERR_IO;
        }
    }

    if (strcmp(mode, "all") != 0 && strcmp(mode, "lvgl") != 0 && strcmp(mode, "jerry") != 0
        && strcmp(mode, "pools") != 0)
    {
        return (int)esh_printf(ctx, "mem: unknown view: %s\r\n", mode);
    }

    return EOS_OK;
}

static const char *_activity_type_name(eos_activity_type_t type)
{
    static const char *const names[EOS_ACTIVITY_TYPE_COUNT] = {
        [EOS_ACTIVITY_TYPE_NULL] = "null",
        [EOS_ACTIVITY_TYPE_APP] = "app",
        [EOS_ACTIVITY_TYPE_INPUT_PAGE] = "input-page",
        [EOS_ACTIVITY_TYPE_APP_LIST] = "app-list",
        [EOS_ACTIVITY_TYPE_WATCHFACE] = "watchface",
        [EOS_ACTIVITY_TYPE_WATCHFACE_LIST] = "watchface-list",
        [EOS_ACTIVITY_TYPE_LOCK_SCREEN] = "lock-screen",
        [EOS_ACTIVITY_TYPE_RECENT_APPS] = "recent-apps",
    };

    return type < EOS_ACTIVITY_TYPE_COUNT && names[type] ? names[type] : "unknown";
}

static uint32_t _activity_substack_depth(eos_activity_t *root)
{
    uint32_t depth = 0U;

    while (root)
    {
        depth++;
        root = eos_activity_get_app_substack_next(root);
    }
    return depth;
}

int esh_builtin_cmd_stack(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    eos_activity_t *current;
    eos_activity_t *app_root;
    uint32_t depth;

    (void)argv;

    if (!ctx || argc > 2)
    {
        return EOS_ERR_INVALID_ARG;
    }

    current = eos_activity_get_current();
    app_root = current ? eos_activity_get_app_root(current) : NULL;
    depth = _activity_substack_depth(app_root);

    if (esh_printf(ctx,
                   "task stack watermark: unavailable (platform API)\r\n"
                   "activity: %s\r\n"
                   "app sub-stack depth: %" PRIu32 "\r\n"
                   "recent app entries: %" PRIu32 "\r\n",
                   current ? _activity_type_name(eos_activity_get_type(current)) : "none",
                   depth,
                   eos_recent_apps_count())
        != EOS_OK)
    {
        return EOS_ERR_IO;
    }

    return EOS_OK;
}

static const char *_script_type_name(script_pkg_type_t type)
{
    switch (type)
    {
        case SCRIPT_TYPE_APPLICATION:
            return "application";
        case SCRIPT_TYPE_WATCHFACE:
            return "watchface";
        default:
            return "unknown";
    }
}

int esh_builtin_cmd_crashlog(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    const spm_crash_state_t *state;

    (void)argv;

    if (!ctx || argc != 1)
    {
        return (int)esh_printf(ctx, "crashlog: usage: crashlog\r\n");
    }

    state = spm_get_crash_state();
    if (!state || !state->has_crash)
    {
        return (int)esh_printf(ctx, "crashlog: no crash recorded\r\n");
    }

    if (esh_printf(ctx, "id: %s\r\n", state->script_id) != EOS_OK
        || esh_printf(ctx, "type: %s\r\n", _script_type_name(state->script_type)) != EOS_OK)
    {
        return EOS_ERR_IO;
    }

    return esh_printf(ctx, "error: %s\r\n", state->error_info);
}
