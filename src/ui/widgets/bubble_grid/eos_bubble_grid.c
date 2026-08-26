/**
 * @file eos_bubble_grid.c
 * @brief Bubble grid
 */

#include "eos_bubble_grid.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include "lvgl.h"
#include "lvgl/src/misc/lv_ll.h"
#include "eos_image.h"
#include "eos_widget_data.h"
#include "eos_theme.h"
/* Macros and Definitions -------------------------------------*/

#define FX_SHIFT 8 /* Fixed-point fractional bits. */
#define FX_ONE (1 << FX_SHIFT) /* Fixed-point representation of 1.0. */
#define FX_HALF (FX_ONE >> 1) /* Half of one fixed-point unit, used for rounding. */
#define FX_FROM_INT(v) ((int32_t)(v) << FX_SHIFT) /* Convert an integer value to fixed-point. */
#define FX_FROM_PERMILLE(p) ((int32_t)((((int64_t)(p)) * FX_ONE + 500) / 1000)) /* Convert permille to fixed-point. */

#ifndef WATCH_BUBBLE_DEMO_SHOW_CORE_MASK
#define WATCH_BUBBLE_DEMO_SHOW_CORE_MASK 0 /* Show the central max-scale area with a red mask. */
#endif

#ifndef WATCH_BUBBLE_DEMO_SHOW_FRINGE_MASK
#define WATCH_BUBBLE_DEMO_SHOW_FRINGE_MASK 0 /* Show the fringe transition area with a blue mask. */
#endif

#ifndef WATCH_BUBBLE_DEMO_SHOW_RADIUS_MASK
#define WATCH_BUBBLE_DEMO_SHOW_RADIUS_MASK 0 /* Show translucent guides for X_RADIUS and Y_RADIUS extents. */
#endif

#ifndef WATCH_BUBBLE_DISABLE_EDGE_COMPACTION
#define WATCH_BUBBLE_DISABLE_EDGE_COMPACTION 0 /* Disable edge crowding compaction for comparison. */
#endif

#define FX_EPSILON 1 /* Small epsilon for fixed-point comparisons. */

typedef struct
{
    uint32_t index;
    int32_t q;
    int32_t r;
    lv_color_t bubble_color;
    const void *src;
    int32_t src_w;
    int32_t src_h;
    bool active;
    void *user_data;
    lv_obj_t *bubble_obj;
    lv_obj_t *image_obj;
    bool visual_valid;
    bool visual_pressed;
    int32_t visual_x;
    int32_t visual_y;
    int32_t visual_diameter;
} icon_node_t;

typedef struct
{
    lv_obj_t *container;
    lv_ll_t icon_ll;
    bool icon_ll_ready;

    int32_t offset_x;
    int32_t offset_y;
    int32_t velocity_x;
    int32_t velocity_y;
    int32_t row_center;
    int32_t default_row_center;
    int pressed_icon_index;
    int press_candidate_index;
    int32_t press_anim_progress;
    int32_t press_anim_target;
    lv_point_t press_start_point;
    bool press_moved;
    bool pointer_is_down;
    bool dispatching_custom_click;
    bool pending_click_valid;
    eos_bubble_click_event_t pending_click;
    eos_bubble_config_t config;
    bool needs_refresh;

#if WATCH_BUBBLE_DEMO_SHOW_CORE_MASK || WATCH_BUBBLE_DEMO_SHOW_FRINGE_MASK || WATCH_BUBBLE_DEMO_SHOW_RADIUS_MASK
    lv_obj_t *demo_fringe_mask;
    lv_obj_t *demo_core_mask;
    lv_obj_t *demo_x_radius_mask;
    lv_obj_t *demo_y_radius_mask;
#endif

    lv_timer_t *bubble_tick_timer;
} eos_bubble_grid_t;

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

static void refresh_icon_objects(eos_bubble_grid_t *wb);
static void update_demo_overlays(eos_bubble_grid_t *wb);
static void apply_image_cover_scale(icon_node_t *node, lv_obj_t *image_obj, int32_t target_w, int32_t target_h);
static void create_icon_object(eos_bubble_grid_t *wb, icon_node_t *node);

#if WATCH_BUBBLE_DEMO_SHOW_CORE_MASK || WATCH_BUBBLE_DEMO_SHOW_FRINGE_MASK || WATCH_BUBBLE_DEMO_SHOW_RADIUS_MASK
static lv_obj_t *create_demo_mask(lv_obj_t *parent, lv_color_t color, lv_opa_t opa)
{
    lv_obj_t *mask = lv_obj_create(parent);
    if (mask == NULL)
        return NULL;

    lv_obj_remove_flag(mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(mask, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(mask, 0, 0);
    lv_obj_set_style_bg_color(mask, color, 0);
    lv_obj_set_style_bg_opa(mask, opa, 0);
    lv_obj_set_style_border_width(mask, 0, 0);
    lv_obj_set_style_pad_all(mask, 0, 0);
    lv_obj_set_style_outline_width(mask, 0, 0);
    lv_obj_set_style_shadow_width(mask, 0, 0);
    return mask;
}
#endif

static inline int32_t fx_abs(int32_t v)
{
    return v >= 0 ? v : -v;
}

static inline int32_t fx_clamp(int32_t v, int32_t min_v, int32_t max_v)
{
    if (v < min_v)
        return min_v;
    if (v > max_v)
        return max_v;
    return v;
}

static inline int32_t fx_mul(int32_t a, int32_t b)
{
    int64_t prod = (int64_t)a * (int64_t)b;
    if (prod >= 0)
        prod += FX_HALF;
    else
        prod -= FX_HALF;
    return (int32_t)(prod >> FX_SHIFT);
}

static inline int32_t fx_div(int32_t a, int32_t b)
{
    if (b == 0)
        return 0;

    int64_t num = (int64_t)a << FX_SHIFT;
    int64_t den = (int64_t)b;
    if ((num ^ den) >= 0)
        num += fx_abs((int32_t)den) / 2;
    else
        num -= fx_abs((int32_t)den) / 2;

    return (int32_t)(num / den);
}

static inline int32_t fx_to_int_round(int32_t v)
{
    if (v >= 0)
        return (v + FX_HALF) >> FX_SHIFT;
    return -(((-v) + FX_HALF) >> FX_SHIFT);
}

static inline int32_t fx_lerp(int32_t actual_min,
                              int32_t actual_max,
                              int32_t val,
                              int32_t target_min,
                              int32_t target_max)
{
    int32_t span = actual_max - actual_min;
    if (span == 0)
        return target_min;

    int64_t delta = (int64_t)(val - actual_min) * (int64_t)(target_max - target_min);
    return target_min + (int32_t)(delta / span);
}

static int32_t fx_round_to_step(int32_t value, int32_t step)
{
    if (step <= 0)
        return value;

    int32_t half = step / 2;
    if (value >= 0)
        return ((value + half) / step) * step;
    return -(((-value + half) / step) * step);
}

static int32_t fx_sqrt_u64(uint64_t value)
{
    if (value == 0)
        return 0;

    /* Integer Newton iteration used 64-bit divisions for every corner and
     * press-neighbor distance. The restoring form below uses only shifts,
     * compares and subtracts, which is considerably cheaper on Cortex-M33. */
    uint64_t bit = 1ULL << 62;
    while (bit > value)
        bit >>= 2;

    uint64_t result = 0;
    while (bit != 0)
    {
        if (value >= result + bit)
        {
            value -= result + bit;
            result = (result >> 1) + bit;
        }
        else
        {
            result >>= 1;
        }
        bit >>= 2;
    }

    return (int32_t)result;
}

static int32_t get_view_w_fx(const eos_bubble_grid_t *wb)
{
    int32_t w = (wb != NULL && wb->container != NULL) ? lv_obj_get_width(wb->container) : 0;
    if (w < 1)
        w = 1;
    return FX_FROM_INT(w);
}

static int32_t get_view_h_fx(const eos_bubble_grid_t *wb)
{
    int32_t h = (wb != NULL && wb->container != NULL) ? lv_obj_get_height(wb->container) : 0;
    if (h < 1)
        h = 1;
    return FX_FROM_INT(h);
}

static void init_default_config(eos_bubble_config_t *cfg)
{
    if (cfg == NULL)
        return;

    cfg->bubble_size_px = 40;
    cfg->row_pitch_x_px = 92;
    cfg->row_pitch_y_px = 80;
    cfg->x_radius_px = 120;
    cfg->y_radius_px = 150;
    cfg->corner_radius_px = 50;
    cfg->tap_move_tolerance_px = 12;
    cfg->press_scale_permille = 920;
    cfg->press_darken_lvl = LV_OPA_30;
    cfg->press_image_darken_lvl = LV_OPA_40;
    cfg->press_neighbor_pull_px = 8;
    cfg->press_neighbor_radius_px = 140;
    cfg->press_anim_in_speed_permille = 220;
    cfg->press_anim_out_speed_permille = 180;

    cfg->x_drag_factor_permille = 800;
    cfg->x_inertia_damp_permille = 720;
    cfg->max_x_offset_px = 80;
    cfg->max_scale_permille = 1000;
    cfg->min_scale_permille = 60;
    cfg->fringe_width_px = 120;
    cfg->y_overscroll_max_px = 20;
    cfg->y_spring_k_permille = 220;
    cfg->y_spring_damp_permille = 100;
    cfg->x_spring_k_permille = 300;
    cfg->x_spring_damp_permille = 720;
    cfg->y_ratchet_step_px = 78;
    cfg->y_ratchet_pull_permille = 20;
    cfg->y_ratchet_snap_permille = 40;
    cfg->y_ratchet_dead_px = 118;
}

static void sanitize_config(eos_bubble_config_t *cfg)
{
    if (cfg == NULL)
        return;

    cfg->bubble_size_px = LV_MAX(1, cfg->bubble_size_px);
    cfg->row_pitch_x_px = LV_MAX(1, cfg->row_pitch_x_px);
    cfg->row_pitch_y_px = LV_MAX(1, cfg->row_pitch_y_px);
    cfg->x_radius_px = LV_MAX(1, cfg->x_radius_px);
    cfg->y_radius_px = LV_MAX(1, cfg->y_radius_px);
    cfg->corner_radius_px = LV_MAX(0, cfg->corner_radius_px);
    cfg->corner_radius_px = LV_MIN(cfg->corner_radius_px, LV_MIN(cfg->x_radius_px, cfg->y_radius_px));
    cfg->tap_move_tolerance_px = LV_MAX(0, cfg->tap_move_tolerance_px);
    cfg->press_scale_permille = LV_MAX(1, cfg->press_scale_permille);
    cfg->press_darken_lvl = (lv_opa_t)LV_MIN((int32_t)cfg->press_darken_lvl, (int32_t)LV_OPA_COVER);
    cfg->press_image_darken_lvl = (lv_opa_t)LV_MIN((int32_t)cfg->press_image_darken_lvl, (int32_t)LV_OPA_COVER);
    cfg->press_neighbor_pull_px = LV_MAX(0, cfg->press_neighbor_pull_px);
    cfg->press_neighbor_radius_px = LV_MAX(1, cfg->press_neighbor_radius_px);
    cfg->press_anim_in_speed_permille = LV_MIN((uint16_t)1000, LV_MAX((uint16_t)1, cfg->press_anim_in_speed_permille));
    cfg->press_anim_out_speed_permille =
        LV_MIN((uint16_t)1000, LV_MAX((uint16_t)1, cfg->press_anim_out_speed_permille));

    cfg->x_drag_factor_permille = LV_MAX(1, cfg->x_drag_factor_permille);
    cfg->x_inertia_damp_permille = LV_MAX(1, cfg->x_inertia_damp_permille);
    cfg->max_x_offset_px = LV_MAX(0, cfg->max_x_offset_px);
    cfg->max_scale_permille = LV_MAX(1, cfg->max_scale_permille);
    cfg->min_scale_permille = LV_MAX(1, cfg->min_scale_permille);
    cfg->fringe_width_px = LV_MAX(1, cfg->fringe_width_px);
    cfg->y_overscroll_max_px = LV_MAX(1, cfg->y_overscroll_max_px);
    cfg->y_spring_k_permille = LV_MAX(1, cfg->y_spring_k_permille);
    cfg->y_spring_damp_permille = LV_MAX(1, cfg->y_spring_damp_permille);
    cfg->x_spring_k_permille = LV_MAX(1, cfg->x_spring_k_permille);
    cfg->x_spring_damp_permille = LV_MAX(1, cfg->x_spring_damp_permille);
    cfg->y_ratchet_step_px = LV_MAX(1, cfg->y_ratchet_step_px);
    cfg->y_ratchet_pull_permille = LV_MAX(1, cfg->y_ratchet_pull_permille);
    cfg->y_ratchet_snap_permille = LV_MAX(1, cfg->y_ratchet_snap_permille);
    cfg->y_ratchet_dead_px = LV_MAX(1, cfg->y_ratchet_dead_px);

    if (cfg->min_scale_permille > cfg->max_scale_permille)
    {
        uint16_t tmp = cfg->min_scale_permille;
        cfg->min_scale_permille = cfg->max_scale_permille;
        cfg->max_scale_permille = tmp;
    }
}

static inline int32_t cfg_permille(uint16_t v)
{
    return FX_FROM_PERMILLE(v);
}

static inline int32_t cfg_px(int16_t v)
{
    return FX_FROM_INT(v);
}

static inline int32_t cfg_bubble_size_fx(const eos_bubble_grid_t *wb)
{
    return cfg_px(wb->config.bubble_size_px);
}

static inline int32_t cfg_row_pitch_x_fx(const eos_bubble_grid_t *wb)
{
    return cfg_px(wb->config.row_pitch_x_px);
}

static inline int32_t cfg_row_pitch_y_fx(const eos_bubble_grid_t *wb)
{
    return cfg_px(wb->config.row_pitch_y_px);
}

static inline int32_t cfg_x_radius_fx(const eos_bubble_grid_t *wb)
{
    return cfg_px(wb->config.x_radius_px);
}

static inline int32_t cfg_y_radius_fx(const eos_bubble_grid_t *wb)
{
    return cfg_px(wb->config.y_radius_px);
}

static inline int32_t cfg_corner_radius_fx(const eos_bubble_grid_t *wb)
{
    return cfg_px(wb->config.corner_radius_px);
}

static inline int32_t cfg_press_neighbor_pull_fx(const eos_bubble_grid_t *wb)
{
    return cfg_px(wb->config.press_neighbor_pull_px);
}

static inline int32_t cfg_press_neighbor_radius_fx(const eos_bubble_grid_t *wb)
{
    return cfg_px(wb->config.press_neighbor_radius_px);
}

static inline int32_t cfg_press_scale_fx(const eos_bubble_grid_t *wb)
{
    return cfg_permille(wb->config.press_scale_permille);
}

static inline int32_t cfg_press_anim_in_speed_fx(const eos_bubble_grid_t *wb)
{
    return cfg_permille(wb->config.press_anim_in_speed_permille);
}

static inline int32_t cfg_press_anim_out_speed_fx(const eos_bubble_grid_t *wb)
{
    return cfg_permille(wb->config.press_anim_out_speed_permille);
}

static void mark_refresh(eos_bubble_grid_t *wb)
{
    if (wb != NULL)
        wb->needs_refresh = true;
}

static void refresh_if_needed(eos_bubble_grid_t *wb)
{
    if (wb == NULL || !wb->needs_refresh)
        return;
    refresh_icon_objects(wb);
    wb->needs_refresh = false;
}

static eos_bubble_grid_t *get_instance(lv_obj_t *obj)
{
    if (obj == NULL)
        return NULL;
    return (eos_bubble_grid_t *)eos_wdata_get(obj, EOS_WDATA_BUBBLE_GRID);
}

static bool is_component_obj(lv_obj_t *obj)
{
    eos_bubble_grid_t *wb = get_instance(obj);
    return wb != NULL && wb->container == obj;
}

static icon_node_t *get_icon_node_by_index(eos_bubble_grid_t *wb, uint32_t index)
{
    if (wb == NULL || !wb->icon_ll_ready)
        return NULL;

    icon_node_t *node;
    LV_LL_READ(&wb->icon_ll, node)
    {
        if (node->index == index)
            return node;
    }

    return NULL;
}

static void index_to_row_col(uint32_t index, int32_t *out_row, int32_t *out_col)
{
    uint32_t row = 0;
    uint32_t consumed = 0;

    while (1)
    {
        uint32_t bubble_count = (row % 2U == 0U) ? 3U : 4U;
        if (index < consumed + bubble_count)
        {
            *out_row = (int32_t)row;
            *out_col = (int32_t)(index - consumed);
            return;
        }

        consumed += bubble_count;
        row++;
    }
}

static icon_node_t *ensure_icon_node_by_index(eos_bubble_grid_t *wb, uint32_t index)
{
    if (wb == NULL || !wb->icon_ll_ready)
        return NULL;

    icon_node_t *found = get_icon_node_by_index(wb, index);
    if (found != NULL)
        return found;

    uint32_t next_index = 0;
    icon_node_t *tail = lv_ll_get_tail(&wb->icon_ll);
    if (tail != NULL)
    {
        next_index = tail->index + 1U;
    }

    lv_color_t default_color = EOS_COLOR_ICON_BG;

    for (uint32_t i = next_index; i <= index; i++)
    {
        icon_node_t *node = lv_ll_ins_tail(&wb->icon_ll);
        if (node == NULL)
        {
            return get_icon_node_by_index(wb, index);
        }

        int32_t row;
        int32_t col;
        index_to_row_col(i, &row, &col);

        node->index = i;
        node->q = row;
        node->r = col;
        node->bubble_color = default_color;
        node->src = NULL;
        node->active = false;
        node->user_data = NULL;
        node->bubble_obj = NULL;
        node->image_obj = NULL;
        node->visual_valid = false;
        node->visual_pressed = false;
        node->visual_x = 0;
        node->visual_y = 0;
        node->visual_diameter = 0;
    }

    if (index > 0)
    {
        int32_t last_row;
        int32_t last_col;
        index_to_row_col(index, &last_row, &last_col);
        LV_UNUSED(last_col);
        wb->default_row_center = (last_row * FX_ONE) / 2;
    }

    return get_icon_node_by_index(wb, index);
}

static void clear_icon_list(eos_bubble_grid_t *wb)
{
    if (wb == NULL || !wb->icon_ll_ready)
        return;

    icon_node_t *node = lv_ll_get_head(&wb->icon_ll);
    while (node != NULL)
    {
        icon_node_t *next = lv_ll_get_next(&wb->icon_ll, node);
        if (node->bubble_obj != NULL)
        {
            lv_obj_delete(node->bubble_obj);
            node->bubble_obj = NULL;
            node->image_obj = NULL;
        }
        lv_ll_remove(&wb->icon_ll, node);
        lv_free(node);
        node = next;
    }
}

static void update_active_row_center(eos_bubble_grid_t *wb)
{
    bool has_active = false;
    int32_t min_row = 0;
    int32_t max_row = 0;

    icon_node_t *node;
    LV_LL_READ(&wb->icon_ll, node)
    {
        if (!node->active)
            continue;

        int32_t row = node->q;
        if (!has_active)
        {
            min_row = row;
            max_row = row;
            has_active = true;
        }
        else
        {
            if (row < min_row)
                min_row = row;
            if (row > max_row)
                max_row = row;
        }
    }

    wb->row_center = has_active ? (((min_row + max_row) * FX_ONE) / 2) : wb->default_row_center;
}

static void row_layout_to_pixel(eos_bubble_grid_t *wb, int32_t row, int32_t col, int32_t *x, int32_t *y)
{
    int32_t bubble_count = (row % 2 == 0) ? 3 : 4;
    int32_t row_half_span = ((bubble_count - 1) * FX_ONE) / 2;

    *x = fx_mul((col * FX_ONE) - row_half_span, cfg_row_pitch_x_fx(wb));
    *y = fx_mul((row * FX_ONE) - wb->row_center, cfg_row_pitch_y_fx(wb));
}

static int32_t calc_distance_to_edge(const eos_bubble_grid_t *wb, int32_t x, int32_t y)
{
    int32_t fringe_width = cfg_px(wb->config.fringe_width_px);
    int32_t x_radius = cfg_x_radius_fx(wb);
    int32_t y_radius = cfg_y_radius_fx(wb);
    int32_t corner_radius = cfg_corner_radius_fx(wb);
    int32_t dx = fx_abs(x);
    int32_t dy = fx_abs(y);

    if (dx <= x_radius && dy <= y_radius)
    {
        return 0;
    }

    if (dx <= x_radius + fringe_width && dy <= y_radius + fringe_width)
    {
        if (dx > x_radius - corner_radius && dy > y_radius - corner_radius)
        {
            int32_t dx_to_corner = dx - (x_radius - corner_radius);
            int32_t dy_to_corner = dy - (y_radius - corner_radius);
            uint64_t dist_sq =
                (uint64_t)dx_to_corner * (uint64_t)dx_to_corner + (uint64_t)dy_to_corner * (uint64_t)dy_to_corner;
            return fx_sqrt_u64(dist_sq) - corner_radius;
        }

        return LV_MAX(dx - x_radius, dy - y_radius);
    }

    if (dx > x_radius - corner_radius && dy > y_radius - corner_radius)
    {
        int32_t dx_to_corner = dx - (x_radius - corner_radius);
        int32_t dy_to_corner = dy - (y_radius - corner_radius);
        uint64_t dist_sq =
            (uint64_t)dx_to_corner * (uint64_t)dx_to_corner + (uint64_t)dy_to_corner * (uint64_t)dy_to_corner;
        return fx_sqrt_u64(dist_sq) - corner_radius;
    }

    return LV_MAX(dx - x_radius, dy - y_radius);
}

static int32_t calc_scale(const eos_bubble_grid_t *wb, int32_t distance_from_edge)
{
    int32_t max_scale = cfg_permille(wb->config.max_scale_permille);
    int32_t min_scale = cfg_permille(wb->config.min_scale_permille);
    int32_t fringe_width = cfg_px(wb->config.fringe_width_px);

    if (distance_from_edge <= 0)
    {
        return max_scale;
    }
    else if (distance_from_edge <= fringe_width)
    {
        return fx_lerp(0, fringe_width, distance_from_edge, max_scale, min_scale);
    }
    else
    {
        return min_scale;
    }
}

static void apply_boundary_compaction(int32_t *coord, int32_t radius, int32_t fringe)
{
    int32_t sign = (*coord >= 0) ? 1 : -1;
    int32_t abs_coord = fx_abs(*coord);

    if (abs_coord <= radius)
        return;

    int32_t outer = radius + fringe;
    int32_t denom = LV_MAX(FX_ONE, outer - radius);
    int32_t t = fx_div(abs_coord - radius, denom);
    t = fx_clamp(t, 0, FX_ONE);

    int32_t keep_ratio = FX_ONE - fx_mul(FX_FROM_PERMILLE(550), t);
    int32_t compact_abs_coord = radius + fx_mul(abs_coord - radius, keep_ratio);

    *coord = sign * compact_abs_coord;
}

static void apply_compact_translation(const eos_bubble_grid_t *wb, int32_t *x, int32_t *y, int32_t distance_from_edge)
{
    const int32_t min_scale = cfg_permille(wb->config.min_scale_permille);
    const int32_t fringe_width = cfg_px(wb->config.fringe_width_px);
    const int32_t bubble_size_fx = cfg_bubble_size_fx(wb);
    const int32_t x_radius = cfg_x_radius_fx(wb);
    const int32_t y_radius = cfg_y_radius_fx(wb);
    const int32_t corner_radius = cfg_corner_radius_fx(wb);
    const int32_t compact_strength = LV_MIN((bubble_size_fx - fx_mul(bubble_size_fx, min_scale)) / 2,
                                            fx_mul(cfg_row_pitch_x_fx(wb), FX_FROM_PERMILLE(80)));
    const int32_t compact_limit = fx_mul(cfg_row_pitch_x_fx(wb), FX_FROM_PERMILLE(80));
    const int32_t gravitation = FX_FROM_INT(20);

    int32_t tx = 0;
    int32_t ty = 0;

    if (distance_from_edge > 0 && distance_from_edge <= fringe_width)
    {
        int32_t interpolated = fx_lerp(0, fringe_width, distance_from_edge, 0, compact_strength);
        tx = interpolated;
        ty = interpolated;
    }
    else if (distance_from_edge > fringe_width)
    {
        int32_t extra_base = LV_MAX(0, distance_from_edge - fringe_width - (bubble_size_fx / 2));
        int32_t extra = fx_div(fx_mul(extra_base, gravitation), FX_FROM_INT(10));
        tx = compact_strength + extra;
        ty = compact_strength + extra;
    }

    if (tx == 0 && ty == 0)
        return;

    int32_t dx = *x;
    int32_t dy = *y;
    bool is_corner = (fx_abs(dy) > (y_radius - corner_radius)) && (fx_abs(dx) > (x_radius - corner_radius));

    if (is_corner)
    {
        int32_t corner_dx = fx_abs(dx) - x_radius + corner_radius;
        int32_t corner_dy = fx_abs(dy) - y_radius + corner_radius;
        int32_t dist =
            fx_sqrt_u64((uint64_t)corner_dx * (uint64_t)corner_dx + (uint64_t)corner_dy * (uint64_t)corner_dy);
        if (dist > 0)
        {
            tx = fx_div(fx_mul(tx, corner_dx), dist);
            ty = fx_div(fx_mul(ty, corner_dy), dist);
            tx = (dx >= 0) ? -tx : tx;
            ty = (dy >= 0) ? -ty : ty;
        }
    }
    else if (fx_abs(dx) > x_radius || fx_abs(dy) > y_radius)
    {
        if (fx_abs(dx) > x_radius)
        {
            tx = (dx >= 0) ? -tx : tx;
            ty = 0;
        }
        else
        {
            ty = (dy >= 0) ? -ty : ty;
            tx = 0;
        }
    }

    tx = fx_clamp(tx, -compact_limit, compact_limit);
    ty = fx_clamp(ty, -compact_limit, compact_limit);

    *x += tx;
    *y += ty;
}

static void update_demo_overlays(eos_bubble_grid_t *wb)
{
    if (wb == NULL || wb->container == NULL)
        return;

#if WATCH_BUBBLE_DEMO_SHOW_CORE_MASK || WATCH_BUBBLE_DEMO_SHOW_FRINGE_MASK || WATCH_BUBBLE_DEMO_SHOW_RADIUS_MASK
    int32_t view_w_fx = get_view_w_fx(wb);
    int32_t view_h_fx = get_view_h_fx(wb);
    int32_t x_radius = cfg_x_radius_fx(wb);
    int32_t y_radius = cfg_y_radius_fx(wb);
    int32_t corner_radius_px = fx_to_int_round(cfg_corner_radius_fx(wb));

#if WATCH_BUBBLE_DEMO_SHOW_RADIUS_MASK
    if (wb->demo_x_radius_mask == NULL)
    {
        wb->demo_x_radius_mask = create_demo_mask(wb->container, lv_color_hex(0x8BC34A), LV_OPA_20);
    }
    if (wb->demo_x_radius_mask != NULL)
    {
        int32_t w_px = fx_to_int_round(x_radius * 2);
        int32_t h_px = fx_to_int_round(view_h_fx);
        if (w_px < 1)
            w_px = 1;
        if (h_px < 1)
            h_px = 1;
        lv_obj_set_style_radius(wb->demo_x_radius_mask, corner_radius_px, 0);
        lv_obj_set_size(wb->demo_x_radius_mask, w_px, h_px);
        lv_obj_set_pos(wb->demo_x_radius_mask, fx_to_int_round((view_w_fx - FX_FROM_INT(w_px)) / 2), 0);
    }

    if (wb->demo_y_radius_mask == NULL)
    {
        wb->demo_y_radius_mask = create_demo_mask(wb->container, lv_color_hex(0x00BCD4), LV_OPA_20);
    }
    if (wb->demo_y_radius_mask != NULL)
    {
        int32_t w_px = fx_to_int_round(view_w_fx);
        int32_t h_px = fx_to_int_round(y_radius * 2);
        if (w_px < 1)
            w_px = 1;
        if (h_px < 1)
            h_px = 1;
        lv_obj_set_style_radius(wb->demo_y_radius_mask, corner_radius_px, 0);
        lv_obj_set_size(wb->demo_y_radius_mask, w_px, h_px);
        lv_obj_set_pos(wb->demo_y_radius_mask, 0, fx_to_int_round((view_h_fx - FX_FROM_INT(h_px)) / 2));
    }
#endif

#if WATCH_BUBBLE_DEMO_SHOW_FRINGE_MASK
    if (wb->demo_fringe_mask == NULL)
    {
        wb->demo_fringe_mask = create_demo_mask(wb->container, lv_color_hex(0x3A78FF), LV_OPA_20);
    }
    if (wb->demo_fringe_mask != NULL)
    {
        int32_t fringe_px = cfg_px(wb->config.fringe_width_px);
        int32_t w_px = fx_to_int_round((x_radius + fringe_px) * 2);
        int32_t h_px = fx_to_int_round((y_radius + fringe_px) * 2);
        if (w_px < 1)
            w_px = 1;
        if (h_px < 1)
            h_px = 1;
        lv_obj_set_style_radius(wb->demo_fringe_mask, corner_radius_px, 0);
        lv_obj_set_size(wb->demo_fringe_mask, w_px, h_px);
        lv_obj_set_pos(wb->demo_fringe_mask,
                       fx_to_int_round((view_w_fx - FX_FROM_INT(w_px)) / 2),
                       fx_to_int_round((view_h_fx - FX_FROM_INT(h_px)) / 2));
    }
#endif

#if WATCH_BUBBLE_DEMO_SHOW_CORE_MASK
    if (wb->demo_core_mask == NULL)
    {
        wb->demo_core_mask = create_demo_mask(wb->container, lv_color_hex(0xFF4B4B), LV_OPA_30);
    }
    if (wb->demo_core_mask != NULL)
    {
        int32_t w_px = fx_to_int_round(x_radius * 2);
        int32_t h_px = fx_to_int_round(y_radius * 2);
        if (w_px < 1)
            w_px = 1;
        if (h_px < 1)
            h_px = 1;
        lv_obj_set_style_radius(wb->demo_core_mask, corner_radius_px, 0);
        lv_obj_set_size(wb->demo_core_mask, w_px, h_px);
        lv_obj_set_pos(wb->demo_core_mask,
                       fx_to_int_round((view_w_fx - FX_FROM_INT(w_px)) / 2),
                       fx_to_int_round((view_h_fx - FX_FROM_INT(h_px)) / 2));
    }
#endif
#else
    LV_UNUSED(wb);
#endif
}

static bool calc_icon_visual(eos_bubble_grid_t *wb,
                             const icon_node_t *icon,
                             int32_t *out_x,
                             int32_t *out_y,
                             int32_t *out_scale)
{
    if (wb == NULL || icon == NULL)
        return false;

    int32_t view_w = get_view_w_fx(wb);
    int32_t view_h = get_view_h_fx(wb);
    int32_t fringe_width = cfg_px(wb->config.fringe_width_px);
    int32_t quick_margin = cfg_bubble_size_fx(wb) * 2;

    int32_t x, y;
    row_layout_to_pixel(wb, icon->q, icon->r, &x, &y);

    x += wb->offset_x;
    y += wb->offset_y;

    if (x < -(view_w / 2) - quick_margin || x > (view_w / 2) + quick_margin || y < -(view_h / 2) - quick_margin
        || y > (view_h / 2) + quick_margin)
    {
        return false;
    }

    int32_t distance_from_edge = calc_distance_to_edge(wb, x, y);
    int32_t scale = calc_scale(wb, distance_from_edge);

#if !WATCH_BUBBLE_DISABLE_EDGE_COMPACTION
    apply_boundary_compaction(&x, cfg_x_radius_fx(wb), fringe_width);
    apply_boundary_compaction(&y, cfg_y_radius_fx(wb), fringe_width);
    apply_compact_translation(wb, &x, &y, distance_from_edge);
#endif

    if (scale < FX_FROM_PERMILLE(20))
        return false;

    x += view_w / 2;
    y += view_h / 2;

    int32_t bubble_r = fx_mul(cfg_bubble_size_fx(wb), scale);
    if (x + bubble_r < 0 || x - bubble_r > view_w || y + bubble_r < 0 || y - bubble_r > view_h)
    {
        return false;
    }

    *out_x = x;
    *out_y = y;
    *out_scale = scale;
    return true;
}

static int hit_test_icon_index(eos_bubble_grid_t *wb, int32_t px, int32_t py)
{
    if (wb == NULL || wb->container == NULL)
        return -1;

    update_active_row_center(wb);

    lv_area_t container_coords;
    lv_obj_get_coords(wb->container, &container_coords);
    int32_t local_px = FX_FROM_INT(px - container_coords.x1);
    int32_t local_py = FX_FROM_INT(py - container_coords.y1);

    int hit = -1;
    int32_t best_scale = -1;

    icon_node_t *node;
    LV_LL_READ(&wb->icon_ll, node)
    {
        if (!node->active)
            continue;

        int32_t x, y, scale;
        if (!calc_icon_visual(wb, node, &x, &y, &scale))
            continue;

        int32_t r = fx_mul(cfg_bubble_size_fx(wb), scale);
        int32_t dx = local_px - x;
        int32_t dy = local_py - y;
        int64_t dist_sq = (int64_t)dx * (int64_t)dx + (int64_t)dy * (int64_t)dy;
        int64_t radius_sq = (int64_t)r * (int64_t)r;
        if (dist_sq <= radius_sq)
        {
            if (scale > best_scale)
            {
                best_scale = scale;
                hit = (int)node->index;
            }
        }
    }

    return hit;
}

static bool get_offset_y_settle_limits(eos_bubble_grid_t *wb, int32_t *out_min_allowed, int32_t *out_max_allowed)
{
    if (wb == NULL)
        return false;

    update_active_row_center(wb);

    bool has_active = false;
    int32_t min_base_y = 0;
    int32_t max_base_y = 0;

    icon_node_t *node;
    LV_LL_READ(&wb->icon_ll, node)
    {
        if (!node->active)
            continue;

        int32_t base_x, base_y;
        row_layout_to_pixel(wb, node->q, node->r, &base_x, &base_y);
        LV_UNUSED(base_x);

        if (!has_active)
        {
            min_base_y = base_y;
            max_base_y = base_y;
            has_active = true;
        }
        else
        {
            if (base_y < min_base_y)
                min_base_y = base_y;
            if (base_y > max_base_y)
                max_base_y = base_y;
        }
    }

    if (!has_active)
        return false;

    int32_t min_allowed = -max_base_y;
    int32_t max_allowed = -min_base_y;

    if (min_allowed > max_allowed)
    {
        int32_t mid = (min_allowed + max_allowed) / 2;
        min_allowed = mid;
        max_allowed = mid;
    }

    *out_min_allowed = min_allowed;
    *out_max_allowed = max_allowed;
    return true;
}

static bool get_offset_y_drag_limits(eos_bubble_grid_t *wb, int32_t *out_min_allowed, int32_t *out_max_allowed)
{
    int32_t settle_min, settle_max;
    if (!get_offset_y_settle_limits(wb, &settle_min, &settle_max))
        return false;

    int32_t span = settle_max - settle_min;
    int32_t extra = LV_MAX(cfg_px(wb->config.y_overscroll_max_px),
                           LV_MIN(FX_FROM_INT(180), FX_FROM_INT(48) + fx_mul(span, FX_FROM_PERMILLE(350))));

    *out_min_allowed = settle_min - extra;
    *out_max_allowed = settle_max + extra;
    return true;
}

static int32_t clamp_to_settle_limits(eos_bubble_grid_t *wb, int32_t candidate_offset_y)
{
    int32_t min_allowed, max_allowed;
    if (!get_offset_y_settle_limits(wb, &min_allowed, &max_allowed))
        return candidate_offset_y;
    return fx_clamp(candidate_offset_y, min_allowed, max_allowed);
}

static int32_t snap_offset_y_to_ratch(eos_bubble_grid_t *wb, int32_t candidate_offset_y)
{
    int32_t snapped = fx_round_to_step(candidate_offset_y, cfg_px(wb->config.y_ratchet_step_px));
    snapped = clamp_to_settle_limits(wb, snapped);
    return snapped;
}

static int32_t apply_drag_resistance_y(eos_bubble_grid_t *wb, int32_t candidate_offset_y)
{
    int32_t min_allowed, max_allowed;
    if (!get_offset_y_drag_limits(wb, &min_allowed, &max_allowed))
    {
        return candidate_offset_y;
    }

    if (candidate_offset_y >= min_allowed && candidate_offset_y <= max_allowed)
    {
        int32_t snapped = snap_offset_y_to_ratch(wb, candidate_offset_y);
        int32_t delta = snapped - candidate_offset_y;
        if (fx_abs(delta) <= cfg_px(wb->config.y_ratchet_dead_px))
        {
            candidate_offset_y += fx_mul(delta, cfg_permille(wb->config.y_ratchet_pull_permille));
        }
        return candidate_offset_y;
    }

    if (candidate_offset_y < min_allowed)
    {
        int32_t overshoot = min_allowed - candidate_offset_y;
        int32_t compressed = fx_div(overshoot, FX_ONE + fx_div(overshoot, cfg_px(wb->config.y_overscroll_max_px)));
        return min_allowed - compressed;
    }

    int32_t overshoot = candidate_offset_y - max_allowed;
    int32_t compressed = fx_div(overshoot, FX_ONE + fx_div(overshoot, cfg_px(wb->config.y_overscroll_max_px)));
    return max_allowed + compressed;
}

static void refresh_icon_objects(eos_bubble_grid_t *wb)
{
    if (wb == NULL || wb->container == NULL)
        return;

    update_demo_overlays(wb);
    update_active_row_center(wb);

    bool has_pressed_center = false;
    int32_t pressed_x = 0;
    int32_t pressed_y = 0;
    icon_node_t *pressed_node = NULL;
    if (wb->press_anim_progress > FX_EPSILON && wb->pressed_icon_index >= 0)
    {
        pressed_node = get_icon_node_by_index(wb, (uint32_t)wb->pressed_icon_index);
    }

    if (pressed_node != NULL && pressed_node->active)
    {
        int32_t pressed_scale;
        has_pressed_center = calc_icon_visual(wb, pressed_node, &pressed_x, &pressed_y, &pressed_scale);
        LV_UNUSED(pressed_scale);
    }

    icon_node_t *node;
    LV_LL_READ(&wb->icon_ll, node)
    {
        lv_obj_t *bubble = node->bubble_obj;
        lv_obj_t *image = node->image_obj;
        if (bubble == NULL)
            continue;

        if (!node->active)
        {
            if (node->visual_valid)
            {
                lv_obj_add_flag(bubble, LV_OBJ_FLAG_HIDDEN);
                node->visual_valid = false;
            }
            continue;
        }

        int32_t x, y, scale;
        if (!calc_icon_visual(wb, node, &x, &y, &scale))
        {
            if (node->visual_valid)
            {
                lv_obj_add_flag(bubble, LV_OBJ_FLAG_HIDDEN);
                node->visual_valid = false;
            }
            continue;
        }

        if (has_pressed_center && (int)node->index != wb->pressed_icon_index)
        {
            int32_t vx = pressed_x - x;
            int32_t vy = pressed_y - y;
            int32_t dist = fx_sqrt_u64((uint64_t)vx * (uint64_t)vx + (uint64_t)vy * (uint64_t)vy);
            int32_t press_neighbor_radius = cfg_press_neighbor_radius_fx(wb);
            if (dist > FX_EPSILON && dist < press_neighbor_radius)
            {
                int32_t t = FX_ONE - fx_div(dist, press_neighbor_radius);
                int32_t pull = fx_mul(cfg_press_neighbor_pull_fx(wb), fx_mul(t, wb->press_anim_progress));
                x += fx_mul(fx_div(vx, dist), pull);
                y += fx_mul(fx_div(vy, dist), pull);
            }
        }

        lv_color_t bubble_color = node->bubble_color;
        bool is_pressed = ((int)node->index == wb->pressed_icon_index) && wb->press_anim_progress > FX_EPSILON;
        if (is_pressed)
        {
            int32_t press_scale = fx_lerp(0, FX_ONE, wb->press_anim_progress, FX_ONE, cfg_press_scale_fx(wb));
            scale = fx_mul(scale, press_scale);
            lv_opa_t darken_lvl =
                (lv_opa_t)(((int64_t)wb->config.press_darken_lvl * wb->press_anim_progress + FX_HALF) / FX_ONE);
            bubble_color = lv_color_darken(bubble_color, darken_lvl);
        }

        int32_t bubble_r = fx_mul(cfg_bubble_size_fx(wb), scale) >> FX_SHIFT;
        int32_t y_center_dist = fx_abs(y - (get_view_h_fx(wb) / 2));
        if (bubble_r < 2 || (bubble_r <= 2 && y_center_dist > cfg_y_radius_fx(wb)))
        {
            if (node->visual_valid)
            {
                lv_obj_add_flag(bubble, LV_OBJ_FLAG_HIDDEN);
                node->visual_valid = false;
            }
            continue;
        }

        int32_t diameter = bubble_r * 2;
        int32_t pos_x = fx_to_int_round(x) - bubble_r;
        int32_t pos_y = fx_to_int_round(y) - bubble_r;
        bool geometry_changed = !node->visual_valid || node->visual_x != pos_x || node->visual_y != pos_y
                                || node->visual_diameter != diameter;
        bool pressed_changed = !node->visual_valid || node->visual_pressed != is_pressed;

        if (!node->visual_valid)
            lv_obj_remove_flag(bubble, LV_OBJ_FLAG_HIDDEN);
        if (geometry_changed)
        {
            lv_obj_set_size(bubble, diameter, diameter);
            lv_obj_set_pos(bubble, pos_x, pos_y);
        }

        /* The normal color is assigned by eos_bubble_set_icon_color(). Only
         * update the style here while the press animation is changing it. */
        if (is_pressed || (pressed_changed && !is_pressed))
            lv_obj_set_style_bg_color(bubble, bubble_color, 0);

        if (node->src != NULL)
        {
            if (node->image_obj == NULL)
                create_icon_object(wb, node);

            image = node->image_obj;
            if (image != NULL)
            {
                lv_obj_remove_flag(image, LV_OBJ_FLAG_HIDDEN);
                if (geometry_changed)
                {
                    lv_obj_set_size(image, diameter, diameter);
                    apply_image_cover_scale(node, image, diameter, diameter);
                    lv_obj_center(image);
                }
                lv_obj_set_style_image_recolor(image, lv_color_black(), 0);
                lv_opa_t image_darken_opa =
                    is_pressed ? (lv_opa_t)(((int64_t)wb->config.press_image_darken_lvl * wb->press_anim_progress
                                              + FX_HALF)
                                             / FX_ONE)
                               : LV_OPA_TRANSP;
                if (geometry_changed || is_pressed || pressed_changed)
                    lv_obj_set_style_image_recolor_opa(image, image_darken_opa, 0);
            }
        }
        else if (image != NULL)
        {
            lv_obj_add_flag(image, LV_OBJ_FLAG_HIDDEN);
        }

        node->visual_valid = true;
        node->visual_pressed = is_pressed;
        node->visual_x = pos_x;
        node->visual_y = pos_y;
        node->visual_diameter = diameter;
    }

    wb->needs_refresh = false;
}

static void init_icon_slots(eos_bubble_grid_t *wb)
{
    if (wb == NULL)
        return;

    if (!wb->icon_ll_ready)
    {
        lv_ll_init(&wb->icon_ll, sizeof(icon_node_t));
        wb->icon_ll_ready = true;
    }

    clear_icon_list(wb);
    wb->row_center = 0;
    wb->default_row_center = 0;
}

static void create_icon_object(eos_bubble_grid_t *wb, icon_node_t *node)
{
    if (wb == NULL || node == NULL)
        return;
    if (wb->container == NULL)
        return;
    if (node->bubble_obj == NULL)
    {
        lv_obj_t *bubble = lv_obj_create(wb->container);
        node->bubble_obj = bubble;

        lv_obj_set_size(bubble, wb->config.bubble_size_px * 2, wb->config.bubble_size_px * 2);
        lv_obj_set_style_radius(bubble, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(bubble, node->bubble_color, 0);
        lv_obj_set_style_border_width(bubble, 0, 0);
        lv_obj_set_style_pad_all(bubble, 0, 0);
        lv_obj_set_style_clip_corner(bubble, true, 0);
        lv_obj_add_flag(bubble, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);
        /* Let the visible bubble be the input target and bubble pointer events
         * to the grid, whose handlers perform drag and custom hit-test logic. */
        lv_obj_add_flag(bubble, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
    }

    /* Color-only bubbles do not need a child image. Create it lazily when an
     * actual image source is assigned later. */
    if (node->src == NULL || node->image_obj != NULL)
        return;

    lv_obj_t *img = lv_image_create(node->bubble_obj);
    node->image_obj = img;

    lv_obj_set_size(img, wb->config.bubble_size_px * 2, wb->config.bubble_size_px * 2);
    lv_image_set_inner_align(img, LV_IMAGE_ALIGN_CENTER);
    lv_image_set_scale_x(img, LV_SCALE_NONE);
    lv_image_set_scale_y(img, LV_SCALE_NONE);
    lv_obj_center(img);
    lv_obj_set_style_bg_opa(img, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(img, 0, 0);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
}

static void apply_image_cover_scale(icon_node_t *node, lv_obj_t *image_obj, int32_t target_w, int32_t target_h)
{
    if (node == NULL || image_obj == NULL)
        return;

    if (node->src_w <= 0 || node->src_h <= 0 || target_w <= 0 || target_h <= 0)
    {
        lv_image_set_scale_x(image_obj, LV_SCALE_NONE);
        lv_image_set_scale_y(image_obj, LV_SCALE_NONE);
        return;
    }

    int32_t scale_x = (int32_t)(((int64_t)target_w * LV_SCALE_NONE) / node->src_w);
    int32_t scale_y = (int32_t)(((int64_t)target_h * LV_SCALE_NONE) / node->src_h);
    int32_t cover_scale = LV_MAX(scale_x, scale_y);
    if (cover_scale < 1)
        cover_scale = 1;

    lv_image_set_scale_x(image_obj, (uint32_t)cover_scale);
    lv_image_set_scale_y(image_obj, (uint32_t)cover_scale);
}

static void pressed_event(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_current_target(e);
    eos_bubble_grid_t *wb = get_instance(obj);
    if (wb == NULL)
        return;

    lv_indev_t *indev = lv_indev_get_act();
    if (!indev)
        return;

    lv_indev_get_point(indev, &wb->press_start_point);
    wb->pointer_is_down = true;
    wb->press_moved = false;
    wb->velocity_x = 0;
    wb->velocity_y = 0;
    wb->press_candidate_index = hit_test_icon_index(wb, wb->press_start_point.x, wb->press_start_point.y);
    wb->pressed_icon_index = wb->press_candidate_index;
    wb->press_anim_target = (wb->pressed_icon_index >= 0) ? FX_ONE : 0;
    mark_refresh(wb);

    LV_UNUSED(e);
}

static void drag_event(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_current_target(e);
    eos_bubble_grid_t *wb = get_instance(obj);
    if (wb == NULL)
        return;

    lv_indev_t *indev = lv_indev_get_act();
    if (!indev)
        return;

    lv_point_t vect;
    lv_indev_get_vect(indev, &vect);

    lv_point_t point;
    lv_indev_get_point(indev, &point);
    if (!wb->press_moved)
    {
        int32_t moved_x = LV_ABS(point.x - wb->press_start_point.x);
        int32_t moved_y = LV_ABS(point.y - wb->press_start_point.y);
        if (moved_x > wb->config.tap_move_tolerance_px || moved_y > wb->config.tap_move_tolerance_px)
        {
            wb->press_moved = true;
            wb->press_anim_target = 0;
        }
    }

    wb->offset_x += fx_mul(vect.x * FX_ONE, cfg_permille(wb->config.x_drag_factor_permille));
    wb->offset_y = apply_drag_resistance_y(wb, wb->offset_y + (vect.y * FX_ONE));
    wb->offset_x = fx_clamp(wb->offset_x, -cfg_px(wb->config.max_x_offset_px), cfg_px(wb->config.max_x_offset_px));

    wb->velocity_x = fx_mul(vect.x * FX_ONE, cfg_permille(wb->config.x_drag_factor_permille));
    wb->velocity_y = vect.y * FX_ONE;

    mark_refresh(wb);
    LV_UNUSED(e);
}

static void released_event(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_current_target(e);
    eos_bubble_grid_t *wb = get_instance(obj);
    if (wb == NULL)
        return;

    lv_indev_t *indev = lv_indev_get_act();
    wb->pointer_is_down = false;
    if (!indev)
    {
        wb->pressed_icon_index = -1;
        wb->press_candidate_index = -1;
        mark_refresh(wb);
        return;
    }

    lv_point_t release_point;
    lv_indev_get_point(indev, &release_point);

    int32_t moved_x = LV_ABS(release_point.x - wb->press_start_point.x);
    int32_t moved_y = LV_ABS(release_point.y - wb->press_start_point.y);
    bool is_click =
        !wb->press_moved && moved_x <= wb->config.tap_move_tolerance_px && moved_y <= wb->config.tap_move_tolerance_px;

    int release_index = hit_test_icon_index(wb, release_point.x, release_point.y);
    icon_node_t *release_node = (release_index >= 0) ? get_icon_node_by_index(wb, (uint32_t)release_index) : NULL;
    wb->pending_click_valid = false;
    if (is_click && release_index >= 0 && release_index == wb->press_candidate_index && release_node != NULL)
    {
        wb->pending_click.index = (uint32_t)release_index;
        wb->pending_click.icon_user_data = release_node->user_data;
        wb->pending_click_valid = true;
    }

    wb->press_anim_target = 0;
    wb->press_candidate_index = -1;
    mark_refresh(wb);

    LV_UNUSED(e);
}

static void clicked_event(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_current_target(e);
    eos_bubble_grid_t *wb = get_instance(obj);
    if (wb == NULL)
        return;

    if (wb->dispatching_custom_click)
    {
        return;
    }

    if (!wb->pending_click_valid)
    {
        lv_event_stop_processing(e);
        return;
    }

    eos_bubble_click_event_t event_data = wb->pending_click;
    wb->pending_click_valid = false;
    wb->dispatching_custom_click = true;

    /* Suppress the native CLICKED callback chain and forward one payloaded CLICKED event. */
    lv_event_stop_processing(e);
    lv_obj_send_event(wb->container, LV_EVENT_CLICKED, &event_data);

    wb->dispatching_custom_click = false;
}

static void bubble_tick_timer(lv_timer_t *t)
{
    eos_bubble_grid_t *wb = lv_timer_get_user_data(t);
    if (wb == NULL || wb->container == NULL)
        return;

    bool changed = false;

    int32_t old_press_anim = wb->press_anim_progress;
    int old_pressed_idx = wb->pressed_icon_index;
    int32_t old_velocity_x = wb->velocity_x;
    int32_t old_velocity_y = wb->velocity_y;
    int32_t old_offset_x = wb->offset_x;
    int32_t old_offset_y = wb->offset_y;

    int32_t anim_speed = (wb->press_anim_progress < wb->press_anim_target) ? cfg_press_anim_in_speed_fx(wb)
                                                                           : cfg_press_anim_out_speed_fx(wb);
    wb->press_anim_progress += fx_mul(wb->press_anim_target - wb->press_anim_progress, anim_speed);
    if (fx_abs(wb->press_anim_target - wb->press_anim_progress) < FX_EPSILON)
    {
        wb->press_anim_progress = wb->press_anim_target;
    }

    if (!wb->pointer_is_down && wb->press_anim_progress <= FX_EPSILON && wb->press_anim_target <= FX_EPSILON)
    {
        wb->press_anim_progress = 0;
        if (wb->pressed_icon_index != -1)
        {
            wb->pressed_icon_index = -1;
        }
    }

    if (wb->press_anim_progress != old_press_anim || wb->pressed_icon_index != old_pressed_idx)
    {
        changed = true;
    }

    if (wb->pointer_is_down)
    {
        if (changed)
            mark_refresh(wb);
        refresh_if_needed(wb);
        return;
    }

    wb->velocity_x = fx_mul(wb->velocity_x, cfg_permille(wb->config.x_inertia_damp_permille));
    wb->velocity_y = fx_mul(wb->velocity_y, FX_FROM_PERMILLE(920));

    wb->offset_x += wb->velocity_x;
    wb->offset_y += wb->velocity_y;

    wb->velocity_x = fx_mul(wb->velocity_x, cfg_permille(wb->config.x_spring_damp_permille))
                     + fx_mul(-wb->offset_x, cfg_permille(wb->config.x_spring_k_permille));
    wb->offset_x = fx_clamp(wb->offset_x, -cfg_px(wb->config.max_x_offset_px), cfg_px(wb->config.max_x_offset_px));

    int32_t min_allowed, max_allowed;
    if (get_offset_y_settle_limits(wb, &min_allowed, &max_allowed))
    {
        int32_t snap_target = snap_offset_y_to_ratch(wb, wb->offset_y);
        int32_t correction = 0;
        if (wb->offset_y < min_allowed)
        {
            correction = min_allowed - wb->offset_y;
        }
        else if (wb->offset_y > max_allowed)
        {
            correction = max_allowed - wb->offset_y;
        }
        else
        {
            correction = snap_target - wb->offset_y;
        }

        if (fx_abs(correction) > 0)
        {
            int32_t ratchet_dead = cfg_px(wb->config.y_ratchet_dead_px);
            int32_t spring_factor = (fx_abs(correction) > ratchet_dead)
                                        ? cfg_permille(wb->config.y_spring_k_permille)
                                        : cfg_permille(wb->config.y_ratchet_snap_permille);
            wb->velocity_y = fx_mul(wb->velocity_y, cfg_permille(wb->config.y_spring_damp_permille))
                             + fx_mul(correction, spring_factor);
            wb->offset_y +=
                fx_mul(correction,
                       (fx_abs(correction) > ratchet_dead) ? FX_FROM_PERMILLE(165)
                                                           : cfg_permille(wb->config.y_ratchet_snap_permille));

            if (fx_abs(correction) < FX_FROM_PERMILLE(600) && fx_abs(wb->velocity_y) < FX_FROM_PERMILLE(350))
            {
                wb->offset_y = (wb->offset_y < min_allowed) ? min_allowed
                                                            : (wb->offset_y > max_allowed ? max_allowed : snap_target);
                wb->velocity_y = 0;
            }
        }
        else if (fx_abs(wb->velocity_y) < FX_FROM_PERMILLE(50))
        {
            wb->velocity_y = 0;
        }
    }

    if (fx_abs(wb->offset_x) < FX_FROM_PERMILLE(20))
    {
        wb->offset_x = 0;
    }

    if (fx_abs(wb->velocity_x) < FX_FROM_PERMILLE(50) && fx_abs(wb->velocity_y) < FX_FROM_PERMILLE(50))
    {
        wb->velocity_x = 0;
        wb->velocity_y = 0;
    }

    if (wb->velocity_x != old_velocity_x || wb->velocity_y != old_velocity_y || wb->offset_x != old_offset_x
        || wb->offset_y != old_offset_y)
    {
        changed = true;
    }

    if (changed)
        mark_refresh(wb);
    refresh_if_needed(wb);
}

static void _bubble_grid_dtor(void *data)
{
    eos_bubble_grid_t *wb = (eos_bubble_grid_t *)data;
    if (wb->bubble_tick_timer != NULL)
    {
        lv_timer_delete(wb->bubble_tick_timer);
        wb->bubble_tick_timer = NULL;
    }
    clear_icon_list(wb);
    lv_free(wb);
}

static void size_changed_event(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_current_target(e);
    eos_bubble_grid_t *wb = get_instance(obj);
    if (wb == NULL)
        return;

    mark_refresh(wb);
    refresh_if_needed(wb);
}

lv_obj_t *eos_bubble_create(lv_obj_t *parent)
{
    lv_obj_t *container = lv_obj_create(parent);
    if (container == NULL)
        return NULL;

    eos_bubble_grid_t *wb = lv_malloc(sizeof(eos_bubble_grid_t));
    if (wb == NULL)
    {
        lv_obj_delete(container);
        return NULL;
    }

    lv_memzero(wb, sizeof(*wb));
    wb->container = container;
    wb->pressed_icon_index = -1;
    wb->press_candidate_index = -1;
    init_default_config(&wb->config);
    sanitize_config(&wb->config);
    wb->needs_refresh = true;

    eos_wdata_set(container, EOS_WDATA_BUBBLE_GRID, wb, _bubble_grid_dtor);

    lv_obj_set_size(container, 400, 400);
    lv_obj_center(container);

    lv_obj_set_style_radius(container, 0, 0);
    lv_obj_set_style_bg_color(container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 0, 0);

    lv_obj_add_flag(container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(container, LV_OBJ_FLAG_SCROLLABLE);

    init_icon_slots(wb);
    update_demo_overlays(wb);

    lv_obj_add_event_cb(container, pressed_event, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(container, drag_event, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(container, released_event, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(container, clicked_event, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(container, size_changed_event, LV_EVENT_SIZE_CHANGED, NULL);

    wb->bubble_tick_timer = lv_timer_create(bubble_tick_timer, 16, wb);

    return container;
}

void eos_bubble_set_icon_src(lv_obj_t *obj, uint32_t index, const void *src)
{
    if (!is_component_obj(obj))
        return;

    eos_bubble_grid_t *wb = get_instance(obj);
    if (wb == NULL)
        return;

    update_demo_overlays(wb);

    icon_node_t *node = ensure_icon_node_by_index(wb, index);
    if (node == NULL)
        return;

    node->src = src;
    node->active = src != NULL;
    node->visual_valid = false;
    node->src_w = 0;
    node->src_h = 0;

    if (src != NULL)
    {
        lv_image_header_t header;
        if (lv_image_decoder_get_info(src, &header) == LV_RESULT_OK)
        {
            node->src_w = (int32_t)header.w;
            node->src_h = (int32_t)header.h;
        }
    }

    if (src != NULL)
    {
        create_icon_object(wb, node);
    }

    if (node->image_obj != NULL && src != NULL)
    {
        lv_image_set_src(node->image_obj, src);
    }
    else if (node->bubble_obj != NULL && node->image_obj != NULL)
    {
        lv_image_set_src(node->image_obj, NULL);
        lv_obj_add_flag(node->bubble_obj, LV_OBJ_FLAG_HIDDEN);
    }

    {
        int32_t min_allowed, max_allowed;
        if (get_offset_y_settle_limits(wb, &min_allowed, &max_allowed))
        {
            wb->offset_y = fx_clamp(wb->offset_y, min_allowed, max_allowed);
        }
    }

    mark_refresh(wb);
    refresh_if_needed(wb);
}

void eos_bubble_set_icon_user_data(lv_obj_t *obj, uint32_t index, void *user_data)
{
    if (!is_component_obj(obj))
        return;

    eos_bubble_grid_t *wb = get_instance(obj);
    if (wb == NULL)
        return;

    update_demo_overlays(wb);

    icon_node_t *node = ensure_icon_node_by_index(wb, index);
    if (node == NULL)
        return;
    node->user_data = user_data;
}

void eos_bubble_init_config(eos_bubble_config_t *config)
{
    init_default_config(config);
}

void eos_bubble_set_config(lv_obj_t *obj, const eos_bubble_config_t *config)
{
    if (!is_component_obj(obj) || config == NULL)
        return;

    eos_bubble_grid_t *wb = get_instance(obj);
    if (wb == NULL)
        return;

    wb->config = *config;
    sanitize_config(&wb->config);

    wb->offset_x = fx_clamp(wb->offset_x, -cfg_px(wb->config.max_x_offset_px), cfg_px(wb->config.max_x_offset_px));

    update_demo_overlays(wb);

    {
        int32_t min_allowed, max_allowed;
        if (get_offset_y_settle_limits(wb, &min_allowed, &max_allowed))
        {
            wb->offset_y = fx_clamp(wb->offset_y, min_allowed, max_allowed);
        }
    }

    mark_refresh(wb);
    refresh_if_needed(wb);
}

void eos_bubble_set_icon_color(lv_obj_t *obj, uint32_t index, lv_color_t color)
{
    if (!is_component_obj(obj))
        return;

    eos_bubble_grid_t *wb = get_instance(obj);
    if (wb == NULL)
        return;

    icon_node_t *node = ensure_icon_node_by_index(wb, index);
    if (node == NULL)
        return;

    create_icon_object(wb, node);

    node->bubble_color = color;
    node->active = true;
    node->visual_valid = false;
    if (node->bubble_obj != NULL)
        lv_obj_set_style_bg_color(node->bubble_obj, color, 0);

    mark_refresh(wb);
    refresh_if_needed(wb);
}

void eos_bubble_get_config(lv_obj_t *obj, eos_bubble_config_t *config)
{
    if (!is_component_obj(obj) || config == NULL)
        return;

    eos_bubble_grid_t *wb = get_instance(obj);
    if (wb == NULL)
        return;

    *config = wb->config;
}

lv_obj_t *eos_bubble_get_icon_obj(lv_obj_t *obj, uint32_t index)
{
    if (!is_component_obj(obj))
        return NULL;

    eos_bubble_grid_t *wb = get_instance(obj);
    if (wb == NULL)
        return NULL;

    icon_node_t *node = get_icon_node_by_index(wb, index);
    if (node == NULL)
        return NULL;

    return node->bubble_obj;
}
