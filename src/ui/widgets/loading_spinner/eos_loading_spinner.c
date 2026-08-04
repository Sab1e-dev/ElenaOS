/**
 * @file eos_loading_spinner.c
 * @brief Apple-style radial loading spinner implementation
 */

#include "eos_loading_spinner.h"

/* Includes ---------------------------------------------------*/
#include <string.h>
#define EOS_LOG_TAG "LoadSpinner"
#include "eos_log.h"
#include "eos_mem.h"

/* Macros and Definitions -------------------------------------*/

#define _SPINNER_ACTIVE_COLOR 0xFFFFFF /**< Filled bar color */
#define _SPINNER_IDLE_COLOR 0x404040 /**< Idle bar color */
#define _SPINNER_DEFAULT_SIZE 56 /**< Default LVGL pixel size */

/* Forward Declarations ---------------------------------------*/
static void _spinner_draw_infinite(eos_loading_spinner_t *spinner, lv_obj_t *obj, lv_layer_t *layer);
static void _spinner_rotate_anim_cb(void *var, int32_t v);

/* Function Implementations -----------------------------------*/

/**
 * @brief Compute overall visual percent from all phase contributions.
 *
 * Total = sum of (phase_pct / 100 x phase_span) for all phases.
 *
 * Each phase has equal weight. The last phase absorbs the integer-division
 * remainder so the total always spans exactly 0-100 even when
 * 100 % phase_count != 0.
 */
static uint16_t _compute_overall_pct(const eos_loading_spinner_t *s)
{
    if (s->phase_count == 0 || s->phase_progress == NULL)
    {
        return 0;
    }

    uint16_t base_span = 100 / s->phase_count;
    uint16_t remainder = (uint16_t)(100 - (uint32_t)base_span * s->phase_count);
    uint32_t total = 0;

    for (uint16_t i = 0; i < s->phase_count; i++)
    {
        uint16_t p = s->phase_progress[i] > 100 ? 100 : s->phase_progress[i];
        uint16_t span = base_span + (i == s->phase_count - 1 ? remainder : 0);
        total += (uint32_t)p * span / 100;
    }

    return total > 100 ? 100 : (uint16_t)total;
}

/* Drawing ----------------------------------------------------*/

/**
 * @brief Report the extra draw size needed outside the widget bounds.
 *
 * The radial bars extend to outer_r = d * OUTER_RATIO / 100, which may
 * exceed the widget's own area.  Tell LVGL to expand the clip area so
 * the bars are not truncated to the 56×56 widget rect.
 */
static void _spinner_refr_ext_draw_size_cb(lv_event_t *e)
{
    eos_loading_spinner_t *spinner = (eos_loading_spinner_t *)lv_event_get_user_data(e);
    lv_obj_t *obj = lv_event_get_target(e);

    if (!spinner || !obj)
    {
        return;
    }

    int32_t w = lv_obj_get_width(obj);
    int32_t h = lv_obj_get_height(obj);

    if (w < 8 || h < 8)
    {
        return;
    }

    int32_t d = w < h ? w : h;
    int32_t outer_r = (d * spinner->outer_ratio) / 100;
    int32_t overflow = outer_r - d / 2;

    if (overflow > 0)
    {
        lv_event_set_ext_draw_size(e, overflow);
    }
}

/**
 * @brief Draw infinite-mode dots with comet-tail effect.
 *
 * Draws N dots arranged on a ring, rotating continuously.
 * Each dot is a filled circle (via lv_draw_arc with a full 360 deg
 * sweep at the dot's screen position). The leading dot (i=0) is
 * bright white and full-size; trailing dots progressively dim and
 * shrink via color and width interpolation. No alpha blending is
 * used for the comet tail — all dimming is through RGB color.
 */
static void _spinner_draw_infinite(eos_loading_spinner_t *spinner, lv_obj_t *obj, lv_layer_t *layer)
{
    /* Absolute screen coordinates */
    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    int32_t w = lv_area_get_width(&coords);
    int32_t h = lv_area_get_height(&coords);

    if (w < 8 || h < 8)
    {
        return;
    }

    /* Circle center and ring radius */
    int32_t cx = coords.x1 + w / 2;
    int32_t cy = coords.y1 + h / 2;
    int32_t d = w < h ? w : h;
    int32_t inner_r = (d * spinner->inner_ratio) / 100;
    int32_t outer_r = (d * spinner->outer_ratio) / 100;
    int32_t ring_r = (inner_r + outer_r) / 2;

    uint16_t n = spinner->bar_count;
    if (n < 2)
    {
        return;
    }

    /* Leading dot at 12 o'clock; rotation/10 converts 0.1deg to degrees.
     * 180 - angle maps so 0 -> 12 o'clock, rotation advances clockwise. */
    int16_t lead_angle = (int16_t)(180 - (spinner->rotation / 10));

    /* Sequential dot emergence with overlapping window.
     * emerge_progress sweeps from 0 to (n + EMERGE_WINDOW_DOTS) * _EMERGE_UNITS.
     * At any moment, up to EMERGE_WINDOW_DOTS are in transition,
     * creating a smooth sweeping fade-in around the ring. */
#define _EMERGE_UNITS 384
#define _EMERGE_WINDOW_DOTS 5
    uint16_t emerge_end = (uint16_t)((n + _EMERGE_WINDOW_DOTS) * _EMERGE_UNITS);
    bool emerging = spinner->emerge_progress < emerge_end;

    for (uint16_t i = 0; i < n; i++)
    {
        /* Dots trail counter-clockwise behind the leading dot */
        int16_t dot_angle = lead_angle + (int16_t)((int32_t)i * 360 / n);
        dot_angle = ((dot_angle % 360) + 360) % 360;

        int32_t sin_a = lv_trigo_sin(dot_angle);
        int32_t cos_a = lv_trigo_cos(dot_angle);

        /* Dot center position on the ring */
        int32_t dot_cx = cx + (ring_r * sin_a) / LV_TRIGO_SIN_MAX;
        int32_t dot_cy = cy + (ring_r * cos_a) / LV_TRIGO_SIN_MAX;

        /* Comet-tail brightness: white -> dark gray.
         * Uses 0-255 range over (n-1) steps for smooth per-dot gradient. */
        uint8_t c = (uint8_t)(255 - (uint32_t)i * 237 / (n - 1));

        /* Comet-tail diameter in 8.8 fixed-point.
         * Even diameters only (LVGL limitation at small sizes).
         * Two-circle blending with opacity simulates fractional sizing:
         *   d_lo = nearest even <= ideal;  d_hi = d_lo + 2
         *   frac  = position between them (0..255)
         *   Draw d_lo at 100%, d_hi at frac% for smooth size gradient. */
        uint32_t d_ideal = 2 * 256; /* minimum 2 px */
        if (spinner->bar_width > 0)
        {
            uint32_t max_d = (uint32_t)spinner->bar_width * 256;
            uint32_t min_d = 2 * 256;
            d_ideal = max_d - (uint32_t)i * (max_d - min_d) / (n - 1);
        }
        uint16_t d_lo = (uint16_t)((d_ideal / 256) & ~1U);
        if (d_lo < 2)
            d_lo = 2;
        uint16_t d_hi = d_lo + 2;
        /* frac: 0=at d_lo, 255=almost at d_hi */
        uint8_t d_blend = 0;
        if (d_hi <= (uint16_t)spinner->bar_width + 2)
        {
            uint32_t offset = d_ideal - (uint32_t)d_lo * 256; /* 0..511 */
            d_blend = (uint8_t)((offset * 255) / 512);
        }
        bool blend_up = (d_blend > 0);
        uint16_t dot_radius = d_lo / 2;
        uint16_t dot_radius_hi = d_hi / 2;

        /* Emerge: offset of emergence front past this dot (in _EMERGE_UNITS).
         * ahead <= 0 -> not visible, ahead >= WINDOW*UNITS -> full comet-tail,
         * otherwise linear fade from ~25% to 100%. */
        uint8_t emerge_opa = 255;
        if (emerging)
        {
            int32_t ahead = (int32_t)spinner->emerge_progress - (int32_t)i * _EMERGE_UNITS;
            if (ahead <= 0)
            {
                continue;
            }
            int32_t window_units = (int32_t)_EMERGE_WINDOW_DOTS * _EMERGE_UNITS;
            if (ahead < window_units)
            {
                emerge_opa = (uint8_t)(64 + ((uint32_t)ahead * (255 - 64)) / (uint32_t)window_units);
            }
        }
        /* Apply emerge opacity to color, then apply global dim */
        uint8_t c_final = (uint8_t)(((uint16_t)c * emerge_opa) / 255);
        c_final = (uint8_t)((uint16_t)c_final * spinner->dim_opa / LV_OPA_COVER);
        lv_color_t color = lv_color_make(c_final, c_final, c_final);

        /* Global dim opacity for arc drawing */
        lv_opa_t base_opa = (lv_opa_t)((uint16_t)emerge_opa * spinner->dim_opa / LV_OPA_COVER);

        /* Draw the primary (lower even) circle */
        {
            lv_draw_arc_dsc_t arc_dsc;
            lv_draw_arc_dsc_init(&arc_dsc);
            arc_dsc.center.x = dot_cx;
            arc_dsc.center.y = dot_cy;
            arc_dsc.radius = dot_radius;
            arc_dsc.width = d_lo;
            arc_dsc.start_angle = 0;
            arc_dsc.end_angle = 3600;
            arc_dsc.color = color;
            arc_dsc.rounded = 1;
            arc_dsc.opa = base_opa;
            lv_draw_arc(layer, &arc_dsc);
        }

        /* Blend-up circle for sub-pixel size smoothing */
        if (blend_up)
        {
            lv_opa_t blend_opa = (lv_opa_t)((uint16_t)base_opa * d_blend / 255);
            lv_draw_arc_dsc_t arc_dsc;
            lv_draw_arc_dsc_init(&arc_dsc);
            arc_dsc.center.x = dot_cx;
            arc_dsc.center.y = dot_cy;
            arc_dsc.radius = dot_radius_hi;
            arc_dsc.width = d_hi;
            arc_dsc.start_angle = 0;
            arc_dsc.end_angle = 3600;
            arc_dsc.color = color;
            arc_dsc.rounded = 1;
            arc_dsc.opa = blend_opa;
            lv_draw_arc(layer, &arc_dsc);
        }
    }
}

#undef _EMERGE_UNITS
#undef _EMERGE_WINDOW_DOTS

/**
 * @brief Draw radial bars in LV_EVENT_DRAW_MAIN_END callback.
 *
 * Draws bar_count radial lines from inner_r to outer_r around the
 * widget center. Active bars (i < active_bars) are white; idle bars
 * are gray. Uses fixed-point trig to avoid math.h/double.
 */
static void _spinner_draw_cb(lv_event_t *e)
{
    eos_loading_spinner_t *spinner = (eos_loading_spinner_t *)lv_event_get_user_data(e);
    lv_obj_t *obj = lv_event_get_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);

    if (!spinner || !obj)
    {
        return;
    }

    /* Infinite mode: delegate to dot-based drawing */
    if (spinner->infinite)
    {
        _spinner_draw_infinite(spinner, obj, layer);
        return;
    }

    /* Absolute screen coordinates — required by LV_EVENT_DRAW_MAIN_END */
    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    int32_t w = lv_area_get_width(&coords);
    int32_t h = lv_area_get_height(&coords);

    if (w < 8 || h < 8)
    {
        return;
    }

    /* Circle center and radii in absolute screen coordinates */
    int32_t cx = coords.x1 + w / 2;
    int32_t cy = coords.y1 + h / 2;
    int32_t d = w < h ? w : h;
    int32_t inner_r = (d * spinner->inner_ratio) / 100;
    int32_t outer_r = (d * spinner->outer_ratio) / 100;

    /* How many bars are "filled" (white), rest are idle (gray) */
    uint16_t active_bars = (uint16_t)(((uint32_t)spinner->display_pct * spinner->bar_count) / 100);

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.width = spinner->bar_width;
    line_dsc.opa = LV_OPA_COVER;

    for (uint16_t i = 0; i < spinner->bar_count; i++)
    {
        /* Bar 0 at 12 o'clock, fill clockwise (12→3→6→9) */
        int16_t angle = (int16_t)(180 - ((int32_t)i * 360) / spinner->bar_count);
        int32_t sin_a = lv_trigo_sin(angle);
        int32_t cos_a = lv_trigo_cos(angle);

        line_dsc.color = (i < active_bars) ? lv_color_hex(_SPINNER_ACTIVE_COLOR) : lv_color_hex(_SPINNER_IDLE_COLOR);

        line_dsc.p1.x = cx + (inner_r * sin_a) / LV_TRIGO_SIN_MAX;
        line_dsc.p1.y = cy + (inner_r * cos_a) / LV_TRIGO_SIN_MAX;
        line_dsc.p2.x = cx + (outer_r * sin_a) / LV_TRIGO_SIN_MAX;
        line_dsc.p2.y = cy + (outer_r * cos_a) / LV_TRIGO_SIN_MAX;

        lv_draw_line(layer, &line_dsc);
    }
}

/* Display update (with optional animation) -------------------*/

/**
 * @brief Animation execution callback — sets display_pct and invalidates.
 */
static void _spinner_anim_exec_cb(void *var, int32_t v)
{
    eos_loading_spinner_t *spinner = (eos_loading_spinner_t *)var;

    spinner->display_pct = (uint16_t)v;
    if (spinner->root && lv_obj_is_valid(spinner->root))
    {
        lv_obj_invalidate(spinner->root);
    }
}

/**
 * @brief Update the displayed overall percent, optionally animated.
 *
 * If anim_duration_ms > 0, starts a smooth lv_anim from current
 * display_pct to target. Otherwise sets the value instantly.
 */
static void _spinner_set_display(eos_loading_spinner_t *s, uint16_t target)
{
    if (target > 100)
    {
        target = 100;
    }

    if (s->display_pct == target)
    {
        return;
    }

    if (s->anim_duration_ms > 0)
    {
        /* Kill any in-flight transition, then start a new one */
        lv_anim_delete(s, NULL);
        lv_anim_init(&s->anim);
        lv_anim_set_var(&s->anim, s);
        lv_anim_set_exec_cb(&s->anim, _spinner_anim_exec_cb);
        lv_anim_set_values(&s->anim, (int32_t)s->display_pct, (int32_t)target);
        lv_anim_set_duration(&s->anim, s->anim_duration_ms);
        lv_anim_set_path_cb(&s->anim, lv_anim_path_ease_out);
        lv_anim_start(&s->anim);
    }
    else
    {
        s->display_pct = target;
        if (s->root && lv_obj_is_valid(s->root))
        {
            lv_obj_invalidate(s->root);
        }
    }
}

/**
 * @brief Recompute overall percent from phases and update the display.
 */
static void _spinner_recompute_and_redraw(eos_loading_spinner_t *s)
{
    uint16_t total = _compute_overall_pct(s);
    _spinner_set_display(s, total);
}

/* Lifecycle --------------------------------------------------*/

/**
 * @brief Free all heap resources owned by the spinner.
 */
static void _spinner_free_internal(eos_loading_spinner_t *spinner)
{
    if (!spinner)
    {
        return;
    }

    lv_anim_delete(spinner, NULL);

    if (spinner->phase_progress)
    {
        eos_free(spinner->phase_progress);
        spinner->phase_progress = NULL;
    }

    spinner->root = NULL;
    eos_free(spinner);
}

/**
 * @brief LV_EVENT_DELETE callback — self-frees the context when the root
 *        object is deleted (e.g. parent container removed).
 */
static void _spinner_delete_cb(lv_event_t *e)
{
    eos_loading_spinner_t *spinner = (eos_loading_spinner_t *)lv_event_get_user_data(e);

    if (spinner)
    {
        /* Unregister from root so _spinner_free_internal won't try to
         * interact with the now-dying object. */
        spinner->root = NULL;
    }

    _spinner_free_internal(spinner);
}

/* Public API -------------------------------------------------*/

eos_loading_spinner_t *eos_loading_spinner_create(lv_obj_t *parent, uint16_t bar_count, uint16_t phase_count)
{
    if (!parent)
    {
        EOS_LOG_E("parent is NULL");
        return NULL;
    }

    /* Clamp bar count: 0 = default, then min..max */
    if (bar_count == 0)
    {
        bar_count = EOS_LOADING_SPINNER_DEFAULT_BARS;
    }
    if (bar_count < EOS_LOADING_SPINNER_MIN_BARS)
    {
        bar_count = EOS_LOADING_SPINNER_MIN_BARS;
    }
    if (bar_count > EOS_LOADING_SPINNER_MAX_BARS)
    {
        bar_count = EOS_LOADING_SPINNER_MAX_BARS;
    }

    /* Clamp phase count: 0 = default, then 1..max */
    if (phase_count == 0)
    {
        phase_count = EOS_LOADING_SPINNER_DEFAULT_PHASES;
    }
    if (phase_count < 1)
    {
        phase_count = 1;
    }
    if (phase_count > EOS_LOADING_SPINNER_MAX_PHASES)
    {
        phase_count = EOS_LOADING_SPINNER_MAX_PHASES;
    }

    /* Allocate context zeroed */
    eos_loading_spinner_t *spinner = (eos_loading_spinner_t *)eos_malloc_zeroed(sizeof(eos_loading_spinner_t));
    if (!spinner)
    {
        EOS_LOG_E("Failed to allocate spinner context");
        return NULL;
    }

    /* Allocate phase progress array */
    spinner->phase_progress = (uint16_t *)eos_malloc_zeroed(sizeof(uint16_t) * phase_count);
    if (!spinner->phase_progress)
    {
        EOS_LOG_E("Failed to allocate phase_progress array");
        eos_free(spinner);
        return NULL;
    }

    spinner->bar_count = bar_count;
    spinner->inner_ratio = EOS_LOADING_SPINNER_DEFAULT_INNER_RATIO;
    spinner->outer_ratio = EOS_LOADING_SPINNER_DEFAULT_OUTER_RATIO;
    spinner->bar_width = EOS_LOADING_SPINNER_DEFAULT_BAR_WIDTH;
    spinner->phase_count = phase_count;
    spinner->display_pct = 0;
    spinner->anim_duration_ms = 0;
    spinner->dim_opa = LV_OPA_COVER;

    /* Create a plain transparent LVGL object as the draw target */
    spinner->root = lv_obj_create(parent);
    if (!spinner->root)
    {
        EOS_LOG_E("Failed to create root object");
        eos_free(spinner->phase_progress);
        eos_free(spinner);
        return NULL;
    }

    /* Size first so subsequent layout sees the correct dimensions */
    lv_obj_set_size(spinner->root, _SPINNER_DEFAULT_SIZE, _SPINNER_DEFAULT_SIZE);

    /* Near-invisible background (1/255 opacity) — required so LVGL does not
     * skip this widget during the draw cycle. Radial bars are drawn via
     * LV_EVENT_DRAW_POST on top, so the 1-opa bg is never seen. */
    lv_obj_set_style_bg_opa(spinner->root, LV_OPA_MIN, 0);
    lv_obj_set_style_bg_color(spinner->root, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(spinner->root, 0, 0);
    lv_obj_set_style_pad_all(spinner->root, 0, 0);
    lv_obj_clear_flag(spinner->root, LV_OBJ_FLAG_SCROLLABLE);

    /* Let LVGL know the bars extend beyond the widget rect (avoid clipping) */
    lv_obj_add_event_cb(spinner->root, _spinner_refr_ext_draw_size_cb, LV_EVENT_REFR_EXT_DRAW_SIZE, spinner);

    /* Draw radial bars via DRAW_MAIN_END — fires after default bg/border
     * rendering, so the white bars render on top. */
    lv_obj_add_event_cb(spinner->root, _spinner_draw_cb, LV_EVENT_DRAW_MAIN_END, spinner);
    lv_obj_add_event_cb(spinner->root, _spinner_delete_cb, LV_EVENT_DELETE, spinner);

    /* Force an initial redraw */
    lv_obj_invalidate(spinner->root);

    return spinner;
}

void eos_loading_spinner_set_phase_count(eos_loading_spinner_t *spinner, uint16_t phase_count)
{
    if (!spinner)
    {
        return;
    }

    if (phase_count < 1)
    {
        phase_count = 1;
    }
    if (phase_count > EOS_LOADING_SPINNER_MAX_PHASES)
    {
        phase_count = EOS_LOADING_SPINNER_MAX_PHASES;
    }

    if (phase_count == spinner->phase_count)
    {
        return;
    }

    uint16_t *new_array = (uint16_t *)eos_malloc_zeroed(sizeof(uint16_t) * phase_count);
    if (!new_array)
    {
        EOS_LOG_E("Failed to reallocate phase_progress array");
        return;
    }

    /* Copy existing values up to the smaller count; excess truncated,
     * new entries remain zero-initialised */
    uint16_t copy_count = phase_count < spinner->phase_count ? phase_count : spinner->phase_count;
    if (spinner->phase_progress && copy_count > 0)
    {
        memcpy(new_array, spinner->phase_progress, sizeof(uint16_t) * copy_count);
    }

    eos_free(spinner->phase_progress);
    spinner->phase_progress = new_array;
    spinner->phase_count = phase_count;

    _spinner_recompute_and_redraw(spinner);
}

void eos_loading_spinner_set_phase_progress(eos_loading_spinner_t *spinner, uint16_t index, uint16_t pct)
{
    if (!spinner || !spinner->phase_progress)
    {
        return;
    }

    if (index >= spinner->phase_count)
    {
        index = spinner->phase_count > 0 ? spinner->phase_count - 1 : 0;
    }

    if (pct > 100)
    {
        pct = 100;
    }

    if (spinner->phase_progress[index] == pct)
    {
        return;
    }

    spinner->phase_progress[index] = pct;
    _spinner_recompute_and_redraw(spinner);
}

void eos_loading_spinner_set_all_phases(eos_loading_spinner_t *spinner, const uint16_t *values)
{
    if (!spinner || !spinner->phase_progress)
    {
        return;
    }

    if (values)
    {
        for (uint16_t i = 0; i < spinner->phase_count; i++)
        {
            uint16_t pct = values[i] > 100 ? 100 : values[i];
            spinner->phase_progress[i] = pct;
        }
    }
    else
    {
        memset(spinner->phase_progress, 0, sizeof(uint16_t) * spinner->phase_count);
    }

    _spinner_recompute_and_redraw(spinner);
}

uint16_t eos_loading_spinner_get_progress(const eos_loading_spinner_t *spinner)
{
    if (!spinner)
    {
        return 0;
    }

    return _compute_overall_pct(spinner);
}

void eos_loading_spinner_set_radii(eos_loading_spinner_t *spinner, uint16_t inner_ratio, uint16_t outer_ratio)
{
    if (!spinner)
    {
        return;
    }

    /* Clamp: inner >= 0, outer <= 200, inner <= outer */
    if (outer_ratio > 200)
    {
        outer_ratio = 200;
    }
    if (inner_ratio > outer_ratio)
    {
        inner_ratio = outer_ratio;
    }

    spinner->inner_ratio = inner_ratio;
    spinner->outer_ratio = outer_ratio;

    /* Redraw with new radii */
    if (spinner->root && lv_obj_is_valid(spinner->root))
    {
        lv_obj_invalidate(spinner->root);
    }
}

void eos_loading_spinner_set_bar_width(eos_loading_spinner_t *spinner, uint16_t width)
{
    if (!spinner)
    {
        return;
    }

    if (width < 1)
    {
        width = 1;
    }
    if (width > 16)
    {
        width = 16;
    }

    spinner->bar_width = width;

    if (spinner->root && lv_obj_is_valid(spinner->root))
    {
        lv_obj_invalidate(spinner->root);
    }
}

void eos_loading_spinner_set_anim_duration(eos_loading_spinner_t *spinner, uint32_t duration_ms)
{
    if (!spinner)
    {
        return;
    }

    spinner->anim_duration_ms = duration_ms;
}

void eos_loading_spinner_delete(eos_loading_spinner_t *spinner)
{
    if (!spinner)
    {
        return;
    }

    if (spinner->root && lv_obj_is_valid(spinner->root))
    {
        /* Deleting the root cascades to _spinner_delete_cb which
         * calls _spinner_free_internal */
        lv_obj_del(spinner->root);
    }
    else
    {
        _spinner_free_internal(spinner);
    }
}

/* Infinite mode ----------------------------------------------*/

/**
 * @brief Emerge animation execution callback — advances the sequential
 *        dot fade-in progress and invalidates for redraw.
 */
static void _spinner_emerge_anim_cb(void *var, int32_t v)
{
    eos_loading_spinner_t *spinner = (eos_loading_spinner_t *)var;

    spinner->emerge_progress = (uint16_t)v;
    if (spinner->root && lv_obj_is_valid(spinner->root))
    {
        lv_obj_invalidate(spinner->root);
    }
}

/**
 * @brief Completion dim animation callback — updates dim_opa and
 *        invalidates for redraw.
 */
static void _spinner_dim_anim_cb(void *var, int32_t v)
{
    eos_loading_spinner_t *spinner = (eos_loading_spinner_t *)var;

    spinner->dim_opa = (lv_opa_t)v;
    if (spinner->root && lv_obj_is_valid(spinner->root))
    {
        lv_obj_invalidate(spinner->root);
    }
}

/**
 * @brief Completion dim animation finished — fires the user callback.
 */
static void _spinner_dim_ready_cb(lv_anim_t *a)
{
    eos_loading_spinner_t *spinner = (eos_loading_spinner_t *)lv_anim_get_user_data(a);
    if (spinner && spinner->completed_cb)
    {
        spinner->completed_cb(spinner->completed_cb_data);
    }
}

/**
 * @brief Rotation animation execution callback — advances rotation
 *        angle and invalidates for redraw.
 */
static void _spinner_rotate_anim_cb(void *var, int32_t v)
{
    eos_loading_spinner_t *spinner = (eos_loading_spinner_t *)var;

    spinner->rotation = v;
    if (spinner->root && lv_obj_is_valid(spinner->root))
    {
        lv_obj_invalidate(spinner->root);
    }
}

void eos_loading_spinner_set_infinite(eos_loading_spinner_t *spinner, bool infinite)
{
    if (!spinner)
    {
        return;
    }

    if (spinner->infinite == infinite)
    {
        return;
    }

    spinner->infinite = infinite;

    if (infinite)
    {
        /* Kill any determinate-mode progress animation */
        lv_anim_delete(spinner, NULL);

        /* Reset rotation and start continuous spin */
        spinner->rotation = 0;

        lv_anim_init(&spinner->rotate_anim);
        lv_anim_set_var(&spinner->rotate_anim, spinner);
        lv_anim_set_exec_cb(&spinner->rotate_anim, _spinner_rotate_anim_cb);
        lv_anim_set_values(&spinner->rotate_anim, 0, 3600);
        lv_anim_set_duration(&spinner->rotate_anim, 2000);
        lv_anim_set_repeat_count(&spinner->rotate_anim, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&spinner->rotate_anim, lv_anim_path_linear);
        lv_anim_start(&spinner->rotate_anim);

        /* Sequential dot emerge intro with overlapping window.
         * _EMERGE_UNITS=384 sub-units per dot, _EMERGE_WINDOW_DOTS=5 extra
         * so the last dots fully emerge. ~100ms per dot gives a smooth sweep. */
#define _EMERGE_UNITS 384
#define _EMERGE_WINDOW_DOTS 5
        spinner->emerge_progress = 0;
        lv_anim_init(&spinner->emerge_anim);
        lv_anim_set_var(&spinner->emerge_anim, spinner);
        lv_anim_set_exec_cb(&spinner->emerge_anim, _spinner_emerge_anim_cb);
        lv_anim_set_values(&spinner->emerge_anim,
                           0,
                           (int32_t)(spinner->bar_count + _EMERGE_WINDOW_DOTS) * _EMERGE_UNITS);
        lv_anim_set_duration(&spinner->emerge_anim, (uint32_t)(spinner->bar_count + _EMERGE_WINDOW_DOTS) * 100);
        lv_anim_set_path_cb(&spinner->emerge_anim, lv_anim_path_linear);
        lv_anim_start(&spinner->emerge_anim);
#undef _EMERGE_UNITS
#undef _EMERGE_WINDOW_DOTS
    }
    else
    {
        /* Kill rotation animation, return to determinate rendering */
        lv_anim_delete(spinner, NULL);
        if (spinner->root && lv_obj_is_valid(spinner->root))
        {
            lv_obj_invalidate(spinner->root);
        }
    }
}

void eos_loading_spinner_set_completed(eos_loading_spinner_t *spinner,
                                       eos_loading_spinner_completed_cb_t cb,
                                       void *user_data)
{
    if (!spinner)
    {
        return;
    }

    spinner->completed = true;
    spinner->completed_cb = cb;
    spinner->completed_cb_data = user_data;

    /* Animate dim_opa from full (255) to transparent (0) over 400ms.
     * The draw callback multiplies all dot opacity by dim_opa/255,
     * so the spinner smoothly fades out while continuing to rotate. */
    lv_anim_init(&spinner->dim_anim);
    lv_anim_set_var(&spinner->dim_anim, spinner);
    lv_anim_set_exec_cb(&spinner->dim_anim, _spinner_dim_anim_cb);
    lv_anim_set_values(&spinner->dim_anim, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_duration(&spinner->dim_anim, 400);
    lv_anim_set_path_cb(&spinner->dim_anim, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&spinner->dim_anim, _spinner_dim_ready_cb);
    lv_anim_set_user_data(&spinner->dim_anim, spinner);
    lv_anim_start(&spinner->dim_anim);
}
