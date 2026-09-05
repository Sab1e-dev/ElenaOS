/**
 * @file esh_builtin_ui.c
 * @brief ESH command for driving the normal LVGL pointer input path
 */

#include "esh_builtin_commands.h"
#include "eos_config.h"
#include "eos_touch.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define UI_DEFAULT_SWIPE_DURATION_MS (300U)
#define UI_MAX_SWIPE_DURATION_MS (5000U)

static int ui_help(esh_cmd_ctx_t *ctx)
{
    /* esh_printf() deliberately rejects output larger than its bounded
     * formatting buffer.  Keep help readable by emitting short records. */
    if (esh_printf(ctx, "usage: ui <control|tap|down|move|up|swipe|help> ...\r\n") != EOS_OK
        || esh_printf(ctx, "  ui control acquire|release\r\n") != EOS_OK
        || esh_printf(ctx, "  ui tap <x> <y>\r\n") != EOS_OK || esh_printf(ctx, "  ui down <x> <y>\r\n") != EOS_OK
        || esh_printf(ctx, "  ui move <x> <y>\r\n") != EOS_OK || esh_printf(ctx, "  ui up <x> <y>\r\n") != EOS_OK
        || esh_printf(ctx, "  ui swipe <x1> <y1> <x2> <y2> [duration_ms]\r\n") != EOS_OK
        || esh_printf(ctx,
                      "coordinates are logical LVGL pixels: x=0..%u, y=0..%u\r\n",
                      (unsigned)(EOS_DISPLAY_WIDTH - 1U),
                      (unsigned)(EOS_DISPLAY_HEIGHT - 1U))
               != EOS_OK)
    {
        return -1;
    }
    return 0;
}

static int ui_control(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    if ((argc == 2) || ((argc == 3) && (strcmp(argv[2], "help") == 0)))
    {
        return (int)esh_printf(ctx, "usage: ui control <acquire|release>\r\n");
    }

    if (argc != 3)
    {
        return (int)esh_printf(ctx, "usage: ui control <acquire|release>\r\n");
    }

    if (strcmp(argv[2], "acquire") == 0)
    {
        if (!eos_touch_control_acquire())
        {
            return (int)esh_printf(ctx, "ERR: touch input is not initialized\r\n");
        }
        return (int)esh_printf(ctx, "ui: screen control acquired\r\n");
    }

    if (strcmp(argv[2], "release") == 0)
    {
        eos_touch_control_release();
        return (int)esh_printf(ctx, "ui: screen control released\r\n");
    }

    return (int)esh_printf(ctx, "ui: unknown control operation '%s'; use 'ui control help'\r\n", argv[2]);
}

static int ui_parse_i32(esh_cmd_ctx_t *ctx, const char *text, const char *name, int32_t *value)
{
    char *end;
    long parsed;

    if ((text == NULL) || (*text == '\0'))
    {
        (void)esh_printf(ctx, "ui: invalid %s: value is empty\r\n", name);
        return 0;
    }

    errno = 0;
    parsed = strtol(text, &end, 10);
    if ((errno == ERANGE) || (end == text) || (*end != '\0') || (parsed < INT32_MIN) || (parsed > INT32_MAX))
    {
        (void)esh_printf(ctx, "ui: invalid %s: %s\r\n", name, text);
        return 0;
    }

    *value = (int32_t)parsed;
    return 1;
}

static int ui_parse_duration(esh_cmd_ctx_t *ctx, const char *text, uint32_t *duration_ms)
{
    char *end;
    unsigned long parsed;

    if ((text == NULL) || (*text == '\0'))
    {
        (void)esh_printf(ctx, "ui: duration must be 1..%u ms\r\n", UI_MAX_SWIPE_DURATION_MS);
        return 0;
    }

    errno = 0;
    parsed = strtoul(text, &end, 10);
    if ((errno == ERANGE) || (end == text) || (*end != '\0') || (parsed == 0UL) || (parsed > UI_MAX_SWIPE_DURATION_MS))
    {
        (void)esh_printf(ctx, "ui: duration must be 1..%u ms: %s\r\n", UI_MAX_SWIPE_DURATION_MS, text);
        return 0;
    }

    *duration_ms = (uint32_t)parsed;
    return 1;
}

static int ui_check_coordinates(esh_cmd_ctx_t *ctx, int32_t x, int32_t y)
{
    if ((x < 0) || (x >= (int32_t)EOS_DISPLAY_WIDTH) || (y < 0) || (y >= (int32_t)EOS_DISPLAY_HEIGHT))
    {
        (void)esh_printf(ctx,
                         "ui: coordinate out of range: (%ld,%ld), x=0..%u y=0..%u\r\n",
                         (long)x,
                         (long)y,
                         (unsigned)(EOS_DISPLAY_WIDTH - 1U),
                         (unsigned)(EOS_DISPLAY_HEIGHT - 1U));
        return 0;
    }
    return 1;
}

static int ui_report_result(esh_cmd_ctx_t *ctx,
                            const char *operation,
                            eos_touch_inject_result_t result,
                            int32_t x,
                            int32_t y)
{
    switch (result)
    {
        case EOS_TOUCH_INJECT_OK:
            return (int)esh_printf(ctx, "ui: %s queued at (%ld,%ld)\r\n", operation, (long)x, (long)y);
        case EOS_TOUCH_INJECT_NOT_READY:
            return (int)esh_printf(ctx, "ui: touch input is not initialized\r\n");
        case EOS_TOUCH_INJECT_BUSY:
            return (int)esh_printf(ctx, "ui: touch injection is busy; wait for release\r\n");
        case EOS_TOUCH_INJECT_NO_ACTIVE:
            return (int)esh_printf(ctx, "ui: no active touch; use 'ui down' first\r\n");
        case EOS_TOUCH_INJECT_QUEUE_FULL:
            return (int)esh_printf(ctx, "ui: touch sample queue is full\r\n");
        default:
            return (int)esh_printf(ctx, "ui: invalid touch input\r\n");
    }
}

int esh_builtin_cmd_ui(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    int32_t x1;
    int32_t y1;
    int32_t x2;
    int32_t y2;
    uint32_t duration_ms = UI_DEFAULT_SWIPE_DURATION_MS;
    eos_touch_inject_result_t result;

    if ((argc == 1) || ((argc == 2) && (strcmp(argv[1], "help") == 0)))
    {
        return ui_help(ctx);
    }
    if (argc < 2)
    {
        return ui_help(ctx);
    }

    if (strcmp(argv[1], "control") == 0)
    {
        return ui_control(ctx, argc, argv);
    }

    if ((strcmp(argv[1], "tap") == 0) || (strcmp(argv[1], "down") == 0) || (strcmp(argv[1], "move") == 0)
        || (strcmp(argv[1], "up") == 0))
    {
        if (eos_touch_control_get_state() != EOS_TOUCH_CONTROL_CONTROLLED)
        {
            return (int)esh_printf(ctx, "ERR: screen control not acquired\r\n");
        }
        if (argc != 4)
        {
            return (int)esh_printf(ctx, "ui: usage: ui %s <x> <y>\r\n", argv[1]);
        }
        if (!ui_parse_i32(ctx, argv[2], "x", &x1) || !ui_parse_i32(ctx, argv[3], "y", &y1)
            || !ui_check_coordinates(ctx, x1, y1))
        {
            return -1;
        }

        if (strcmp(argv[1], "tap") == 0)
        {
            result = eos_touch_inject_tap(x1, y1);
        }
        else if (strcmp(argv[1], "down") == 0)
        {
            result = eos_touch_inject_down(x1, y1);
        }
        else if (strcmp(argv[1], "move") == 0)
        {
            result = eos_touch_inject_move(x1, y1);
        }
        else
        {
            result = eos_touch_inject_up(x1, y1);
        }
        return ui_report_result(ctx, argv[1], result, x1, y1);
    }

    if (strcmp(argv[1], "swipe") == 0)
    {
        if (eos_touch_control_get_state() != EOS_TOUCH_CONTROL_CONTROLLED)
        {
            return (int)esh_printf(ctx, "ERR: screen control not acquired\r\n");
        }
        if ((argc != 6) && (argc != 7))
        {
            return (int)esh_printf(ctx, "ui: usage: ui swipe <x1> <y1> <x2> <y2> [duration_ms]\r\n");
        }
        if (!ui_parse_i32(ctx, argv[2], "x1", &x1) || !ui_parse_i32(ctx, argv[3], "y1", &y1)
            || !ui_parse_i32(ctx, argv[4], "x2", &x2) || !ui_parse_i32(ctx, argv[5], "y2", &y2)
            || !ui_check_coordinates(ctx, x1, y1) || !ui_check_coordinates(ctx, x2, y2))
        {
            return -1;
        }
        if ((argc == 7) && !ui_parse_duration(ctx, argv[6], &duration_ms))
        {
            return -1;
        }

        result = eos_touch_inject_swipe(x1, y1, x2, y2, duration_ms);
        if (result == EOS_TOUCH_INJECT_OK)
        {
            return (int)esh_printf(ctx,
                                   "ui: swipe queued (%ld,%ld)->(%ld,%ld) duration=%u ms\r\n",
                                   (long)x1,
                                   (long)y1,
                                   (long)x2,
                                   (long)y2,
                                   (unsigned)duration_ms);
        }
        return ui_report_result(ctx, "swipe", result, x2, y2);
    }

    return (int)esh_printf(ctx, "ui: unknown operation '%s'; use 'ui help'\r\n", argv[1]);
}
