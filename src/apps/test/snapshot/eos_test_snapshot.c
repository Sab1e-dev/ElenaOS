/**
 * @file eos_test_snapshot.c
 * @brief LVGL snapshot & animation performance test
 *
 * Part 1: Measures tick cost of lv_snapshot_take_to_draw_buf at multiple sizes using
 *         two independent allocation paths:
 *           A) Direct heap (eos_malloc -> C stdlib heap)
 *           B) PSRAM/cache (eos_cache_buf_alloc -> mem_mgr_alloc -> PSRAM heap)
 *
 * Part 2: Measures per-frame rendering cost of 4 animation types (translate, scale,
 *         opacity, combined) on 2 target types:
 *           A) Snapshot image (static lv_image)
 *           B) Direct component (live 3-button UI)
 */

#include "eos_test_snapshot.h"
#if EOS_ENABLE_TEST_APP

/* Includes ---------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "eos_test_framework.h"
#include "eos_basic_widgets.h"
#include "eos_mem.h"
#include "eos_log.h"
#include "lvgl.h"

/* Macros and Definitions -------------------------------------*/
#define EOS_LOG_TAG "SnapshotTest"
#define SNAPSHOT_CF LV_COLOR_FORMAT_RGB565
#define MSG_BUF_SZ 96

/* ---------------------------------------------------------------------------
 * Draw-buffer factories: two independent allocation paths
 * ------------------------------------------------------------------------- */

static lv_draw_buf_t *_alloc_draw_buf_cache(uint32_t w, uint32_t h)
{
    return eos_draw_buf_create(w, h, SNAPSHOT_CF, 0);
}

static void _free_draw_buf_cache(lv_draw_buf_t *buf)
{
    eos_draw_buf_destroy(buf);
}

static lv_draw_buf_t *_alloc_draw_buf_direct(uint32_t w, uint32_t h)
{
    uint32_t data_size = w * h * lv_color_format_get_size(SNAPSHOT_CF);
    if (data_size == 0)
        return NULL;

    void *data = eos_malloc(data_size);
    if (!data)
        return NULL;
    memset(data, 0, data_size);

    lv_draw_buf_t *draw_buf = eos_malloc_zeroed(sizeof(lv_draw_buf_t));
    if (!draw_buf)
    {
        eos_free(data);
        return NULL;
    }

    uint32_t stride = w * lv_color_format_get_size(SNAPSHOT_CF);
    lv_result_t res = lv_draw_buf_init(draw_buf, w, h, SNAPSHOT_CF, stride, data, data_size);
    if (res != LV_RESULT_OK)
    {
        eos_free(draw_buf);
        eos_free(data);
        return NULL;
    }

    return draw_buf;
}

static void _free_draw_buf_direct(lv_draw_buf_t *buf)
{
    if (!buf)
        return;
    if (buf->data)
        eos_free(buf->data);
    eos_free(buf);
}

/* ---------------------------------------------------------------------------
 * Test runner: one size × one allocator
 * ------------------------------------------------------------------------- */

typedef lv_draw_buf_t *(*alloc_fn_t)(uint32_t w, uint32_t h);
typedef void (*free_fn_t)(lv_draw_buf_t *buf);

static bool _run_snapshot_test(const char *test_name,
                               int32_t w, int32_t h,
                               alloc_fn_t alloc_fn, free_fn_t free_fn,
                               bool multi_pass)
{
    lv_obj_t *scr = lv_screen_active();
    if (!scr)
    {
        EOS_EXPECT_TRUE(false, test_name, "no active screen");
        return false;
    }

    lv_obj_t *cont = lv_obj_create(scr);
    lv_obj_set_size(cont, w, h);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);

    int cols = (w > 40) ? (w / 40) : 1;
    int rows = (h > 40) ? (h / 40) : 1;
    int total = cols * rows;
    if (total > 50)
        total = 50;
    for (int i = 0; i < total; i++)
    {
        lv_obj_t *rect = lv_obj_create(cont);
        lv_obj_set_size(rect, w / cols, h / rows);
        lv_obj_set_pos(rect, (i % cols) * (w / cols), (i / cols) * (h / rows));
        lv_obj_set_style_bg_color(rect,
                                  lv_color_hex(((i * 37) % 256) << 16 | ((i * 73) % 256) << 8 | ((i * 119) % 256)), 0);
        lv_obj_set_style_border_width(rect, 0, 0);
        lv_obj_set_style_radius(rect, 4, 0);
    }

    lv_obj_update_layout(cont);
    lv_refr_now(lv_display_get_default());

    lv_draw_buf_t *snap_buf = alloc_fn((uint32_t)w, (uint32_t)h);
    if (!snap_buf)
    {
        lv_obj_delete(cont);
        EOS_EXPECT_TRUE(false, test_name, "alloc failed");
        return false;
    }

    if (multi_pass)
    {
        uint32_t total_ticks = 0;
        bool ok = true;
        for (int run = 0; run < 5; run++)
        {
            uint32_t t0 = lv_tick_get();
            lv_result_t res = lv_snapshot_take_to_draw_buf(cont, SNAPSHOT_CF, snap_buf);
            uint32_t t1 = lv_tick_get();
            if (res != LV_RESULT_OK)
                ok = false;
            else
                total_ticks += (t1 - t0);
        }
        char msg[MSG_BUF_SZ];
        snprintf(msg, sizeof(msg), "%" PRId32 "x%" PRId32 " avg %" PRIu32 " ticks (5 runs)",
                 w, h, total_ticks / 5);
        EOS_EXPECT_TRUE(ok, test_name, msg);
        free_fn(snap_buf);
        lv_obj_delete(cont);
        return ok;
    }
    else
    {
        uint32_t t0 = lv_tick_get();
        lv_result_t res = lv_snapshot_take_to_draw_buf(cont, SNAPSHOT_CF, snap_buf);
        uint32_t t1 = lv_tick_get();
        free_fn(snap_buf);
        lv_obj_delete(cont);

        char msg[MSG_BUF_SZ];
        snprintf(msg, sizeof(msg), "%" PRId32 "x%" PRId32 " %" PRIu32 " ticks", w, h, t1 - t0);
        EOS_EXPECT_TRUE(res == LV_RESULT_OK, test_name, msg);
        return (res == LV_RESULT_OK);
    }
}

/* ---------------------------------------------------------------------------
 * Realistic component factory (full-screen, 3 buttons + labels)
 * ------------------------------------------------------------------------- */

static lv_obj_t *_create_realistic_component(lv_obj_t *parent)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, EOS_DISPLAY_WIDTH, EOS_DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);

    static const char *labels[] = {"Settings", "Messages", "Profile"};
    int32_t btn_w = 220;
    int32_t btn_h = 64;
    int32_t gap = 20;
    int32_t start_y = (EOS_DISPLAY_HEIGHT - (3 * btn_h + 2 * gap)) / 2;

    for (int i = 0; i < 3; i++)
    {
        lv_obj_t *btn = lv_button_create(cont);
        lv_obj_set_size(btn, btn_w, btn_h);
        lv_obj_set_pos(btn, (EOS_DISPLAY_WIDTH - btn_w) / 2, start_y + i * (btn_h + gap));
        lv_obj_set_style_bg_color(btn,
                                  lv_color_hex((i == 0) ? 0x0F3460 : (i == 1) ? 0x16213E
                                                                              : 0x533483),
                                  0);
        lv_obj_set_style_radius(btn, 12, 0);
        lv_obj_set_style_shadow_width(btn, 8, 0);
        lv_obj_set_style_shadow_opa(btn, LV_OPA_30, 0);
        lv_obj_set_style_border_width(btn, 0, 0);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, labels[i]);
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        lv_obj_center(lbl);
    }

    return cont;
}

/* ---------------------------------------------------------------------------
 * Animation frame measurement helpers
 * ------------------------------------------------------------------------- */

typedef void (*anim_apply_fn_t)(lv_obj_t *obj, int32_t val);

static void _anim_apply_translate_x(lv_obj_t *obj, int32_t val)
{
    lv_obj_set_x(obj, val);
}

static void _anim_apply_scale(lv_obj_t *obj, int32_t val)
{
    lv_obj_set_style_transform_scale(obj, val, 0);
}

static void _anim_apply_opa(lv_obj_t *obj, int32_t val)
{
    lv_obj_set_style_opa(obj, (lv_opa_t)val, 0);
}

static void _anim_apply_combined(lv_obj_t *obj, int32_t step)
{
    int32_t tx = step * 40 / 100;
    int32_t sc = 256 + step * 128 / 100;
    lv_opa_t op = (lv_opa_t)(255 - step * 127 / 100);
    lv_obj_set_x(obj, tx);
    lv_obj_set_style_transform_scale(obj, sc, 0);
    lv_obj_set_style_opa(obj, op, 0);
}

static void _measure_anim(const char *test_name, lv_obj_t *target,
                          anim_apply_fn_t apply_fn, int32_t val_start, int32_t val_end)
{
    uint32_t max_ticks = 0;
    uint32_t min_ticks = UINT32_MAX;
    uint32_t sum_ticks = 0;
    int steps = 5;

    for (int i = 0; i <= steps; i++)
    {
        int32_t val = val_start + (val_end - val_start) * i / steps;
        apply_fn(target, val);
        uint32_t t0 = lv_tick_get();
        lv_refr_now(lv_display_get_default());
        uint32_t t1 = lv_tick_get();
        uint32_t dt = t1 - t0;
        sum_ticks += dt;
        if (dt > max_ticks)
            max_ticks = dt;
        if (dt < min_ticks)
            min_ticks = dt;
    }

    char msg[MSG_BUF_SZ];
    snprintf(msg, sizeof(msg), "%df avg %" PRIu32 " max %" PRIu32 " min %" PRIu32 " ticks",
             steps + 1, sum_ticks / (uint32_t)(steps + 1), max_ticks, min_ticks);
    EOS_EXPECT_TRUE(true, test_name, msg);
}

/* ---------------------------------------------------------------------------
 * Animation tests: snapshot image path
 * ------------------------------------------------------------------------- */

static bool _test_anim_trans_image(void)
{
    lv_obj_t *scr = lv_screen_active();
    if (!scr)
    {
        EOS_EXPECT_TRUE(false, "Snapshot: anim trans image", "no active screen");
        return false;
    }

    lv_obj_t *comp = _create_realistic_component(scr);
    lv_obj_update_layout(comp);
    lv_refr_now(lv_display_get_default());

    lv_draw_buf_t *snap = eos_draw_buf_create(EOS_DISPLAY_WIDTH, EOS_DISPLAY_HEIGHT, SNAPSHOT_CF, 0);
    if (!snap)
    {
        lv_obj_delete(comp);
        EOS_EXPECT_TRUE(false, "Snapshot: anim trans image", "snap alloc");
        return false;
    }

    lv_result_t r = lv_snapshot_take_to_draw_buf(comp, SNAPSHOT_CF, snap);
    lv_obj_delete(comp);

    if (r != LV_RESULT_OK)
    {
        eos_draw_buf_destroy(snap);
        EOS_EXPECT_TRUE(false, "Snapshot: anim trans image", "snap failed");
        return false;
    }

    lv_obj_t *img = lv_image_create(scr);
    lv_image_set_src(img, snap);
    lv_obj_set_size(img, EOS_DISPLAY_WIDTH, EOS_DISPLAY_HEIGHT);
    lv_obj_set_pos(img, 0, 0);

    _measure_anim("Snapshot: anim trans image", img, _anim_apply_translate_x, 0, 40);
    eos_draw_buf_destroy(snap);
    lv_obj_delete(img);
    return true;
}

static bool _test_anim_scale_image(void)
{
    lv_obj_t *scr = lv_screen_active();
    if (!scr)
    {
        EOS_EXPECT_TRUE(false, "Snapshot: anim scale image", "no active screen");
        return false;
    }

    lv_obj_t *comp = _create_realistic_component(scr);
    lv_obj_update_layout(comp);
    lv_refr_now(lv_display_get_default());

    lv_draw_buf_t *snap = eos_draw_buf_create(EOS_DISPLAY_WIDTH, EOS_DISPLAY_HEIGHT, SNAPSHOT_CF, 0);
    if (!snap)
    {
        lv_obj_delete(comp);
        EOS_EXPECT_TRUE(false, "Snapshot: anim scale image", "snap alloc");
        return false;
    }

    lv_result_t r = lv_snapshot_take_to_draw_buf(comp, SNAPSHOT_CF, snap);
    lv_obj_delete(comp);

    if (r != LV_RESULT_OK)
    {
        eos_draw_buf_destroy(snap);
        EOS_EXPECT_TRUE(false, "Snapshot: anim scale image", "snap failed");
        return false;
    }

    lv_obj_t *img = lv_image_create(scr);
    lv_image_set_src(img, snap);
    lv_obj_set_size(img, EOS_DISPLAY_WIDTH, EOS_DISPLAY_HEIGHT);
    lv_obj_set_pos(img, 0, 0);
    lv_obj_set_style_transform_pivot_x(img, EOS_DISPLAY_WIDTH / 2, 0);
    lv_obj_set_style_transform_pivot_y(img, EOS_DISPLAY_HEIGHT / 2, 0);

    _measure_anim("Snapshot: anim scale image", img, _anim_apply_scale, 256, 384);
    eos_draw_buf_destroy(snap);
    lv_obj_delete(img);
    return true;
}

static bool _test_anim_opa_image(void)
{
    lv_obj_t *scr = lv_screen_active();
    if (!scr)
    {
        EOS_EXPECT_TRUE(false, "Snapshot: anim opa image", "no active screen");
        return false;
    }

    lv_obj_t *comp = _create_realistic_component(scr);
    lv_obj_update_layout(comp);
    lv_refr_now(lv_display_get_default());

    lv_draw_buf_t *snap = eos_draw_buf_create(EOS_DISPLAY_WIDTH, EOS_DISPLAY_HEIGHT, SNAPSHOT_CF, 0);
    if (!snap)
    {
        lv_obj_delete(comp);
        EOS_EXPECT_TRUE(false, "Snapshot: anim opa image", "snap alloc");
        return false;
    }

    lv_result_t r = lv_snapshot_take_to_draw_buf(comp, SNAPSHOT_CF, snap);
    lv_obj_delete(comp);

    if (r != LV_RESULT_OK)
    {
        eos_draw_buf_destroy(snap);
        EOS_EXPECT_TRUE(false, "Snapshot: anim opa image", "snap failed");
        return false;
    }

    lv_obj_t *img = lv_image_create(scr);
    lv_image_set_src(img, snap);
    lv_obj_set_size(img, EOS_DISPLAY_WIDTH, EOS_DISPLAY_HEIGHT);
    lv_obj_set_pos(img, 0, 0);

    _measure_anim("Snapshot: anim opa image", img, _anim_apply_opa, LV_OPA_COVER, LV_OPA_20);
    eos_draw_buf_destroy(snap);
    lv_obj_delete(img);
    return true;
}

static bool _test_anim_combined_image(void)
{
    lv_obj_t *scr = lv_screen_active();
    if (!scr)
    {
        EOS_EXPECT_TRUE(false, "Snapshot: anim combined image", "no active screen");
        return false;
    }

    lv_obj_t *comp = _create_realistic_component(scr);
    lv_obj_update_layout(comp);
    lv_refr_now(lv_display_get_default());

    lv_draw_buf_t *snap = eos_draw_buf_create(EOS_DISPLAY_WIDTH, EOS_DISPLAY_HEIGHT, SNAPSHOT_CF, 0);
    if (!snap)
    {
        lv_obj_delete(comp);
        EOS_EXPECT_TRUE(false, "Snapshot: anim combined image", "snap alloc");
        return false;
    }

    lv_result_t r = lv_snapshot_take_to_draw_buf(comp, SNAPSHOT_CF, snap);
    lv_obj_delete(comp);

    if (r != LV_RESULT_OK)
    {
        eos_draw_buf_destroy(snap);
        EOS_EXPECT_TRUE(false, "Snapshot: anim combined image", "snap failed");
        return false;
    }

    lv_obj_t *img = lv_image_create(scr);
    lv_image_set_src(img, snap);
    lv_obj_set_size(img, EOS_DISPLAY_WIDTH, EOS_DISPLAY_HEIGHT);
    lv_obj_set_pos(img, 0, 0);
    lv_obj_set_style_transform_pivot_x(img, EOS_DISPLAY_WIDTH / 2, 0);
    lv_obj_set_style_transform_pivot_y(img, EOS_DISPLAY_HEIGHT / 2, 0);

    _measure_anim("Snapshot: anim combined image", img, _anim_apply_combined, 0, 100);
    eos_draw_buf_destroy(snap);
    lv_obj_delete(img);
    return true;
}

/* ---------------------------------------------------------------------------
 * Animation tests: direct component path (no snapshot)
 * ------------------------------------------------------------------------- */

static bool _test_anim_trans_direct(void)
{
    lv_obj_t *scr = lv_screen_active();
    if (!scr)
    {
        EOS_EXPECT_TRUE(false, "Snapshot: anim trans direct", "no active screen");
        return false;
    }

    lv_obj_t *comp = _create_realistic_component(scr);
    lv_obj_update_layout(comp);
    lv_refr_now(lv_display_get_default());

    _measure_anim("Snapshot: anim trans direct", comp, _anim_apply_translate_x, 0, 40);
    lv_obj_delete(comp);
    return true;
}

static bool _test_anim_scale_direct(void)
{
    lv_obj_t *scr = lv_screen_active();
    if (!scr)
    {
        EOS_EXPECT_TRUE(false, "Snapshot: anim scale direct", "no active screen");
        return false;
    }

    lv_obj_t *comp = _create_realistic_component(scr);
    lv_obj_set_style_transform_pivot_x(comp, EOS_DISPLAY_WIDTH / 2, 0);
    lv_obj_set_style_transform_pivot_y(comp, EOS_DISPLAY_HEIGHT / 2, 0);
    lv_obj_update_layout(comp);
    lv_refr_now(lv_display_get_default());

    _measure_anim("Snapshot: anim scale direct", comp, _anim_apply_scale, 256, 384);
    lv_obj_delete(comp);
    return true;
}

static bool _test_anim_opa_direct(void)
{
    lv_obj_t *scr = lv_screen_active();
    if (!scr)
    {
        EOS_EXPECT_TRUE(false, "Snapshot: anim opa direct", "no active screen");
        return false;
    }

    lv_obj_t *comp = _create_realistic_component(scr);
    lv_obj_update_layout(comp);
    lv_refr_now(lv_display_get_default());

    _measure_anim("Snapshot: anim opa direct", comp, _anim_apply_opa, LV_OPA_COVER, LV_OPA_20);
    lv_obj_delete(comp);
    return true;
}

static bool _test_anim_combined_direct(void)
{
    lv_obj_t *scr = lv_screen_active();
    if (!scr)
    {
        EOS_EXPECT_TRUE(false, "Snapshot: anim combined direct", "no active screen");
        return false;
    }

    lv_obj_t *comp = _create_realistic_component(scr);
    lv_obj_set_style_transform_pivot_x(comp, EOS_DISPLAY_WIDTH / 2, 0);
    lv_obj_set_style_transform_pivot_y(comp, EOS_DISPLAY_HEIGHT / 2, 0);
    lv_obj_update_layout(comp);
    lv_refr_now(lv_display_get_default());

    _measure_anim("Snapshot: anim combined direct", comp, _anim_apply_combined, 0, 100);
    lv_obj_delete(comp);
    return true;
}

/* ---------------------------------------------------------------------------
 * Individual test cases - Direct heap (eos_malloc) path
 * ------------------------------------------------------------------------- */
static bool _test_snap_direct_100(void)
{
    return _run_snapshot_test("Snapshot: direct heap 100x100", 100, 100, _alloc_draw_buf_direct, _free_draw_buf_direct, false);
}

static bool _test_snap_direct_200(void)
{
    return _run_snapshot_test("Snapshot: direct heap 200x200", 200, 200, _alloc_draw_buf_direct, _free_draw_buf_direct, false);
}

static bool _test_snap_direct_300(void)
{
    return _run_snapshot_test("Snapshot: direct heap 300x300", 300, 300, _alloc_draw_buf_direct, _free_draw_buf_direct, false);
}

static bool _test_snap_direct_full(void)
{
    return _run_snapshot_test("Snapshot: direct heap fullscreen", EOS_DISPLAY_WIDTH, EOS_DISPLAY_HEIGHT, _alloc_draw_buf_direct, _free_draw_buf_direct, false);
}

static bool _test_snap_direct_repeat_200(void)
{
    return _run_snapshot_test("Snapshot: direct heap repeat 200x200", 200, 200, _alloc_draw_buf_direct, _free_draw_buf_direct, true);
}

/* ---------------------------------------------------------------------------
 * Individual test cases - PSRAM cache (eos_cache_buf_alloc) path
 * ------------------------------------------------------------------------- */
static bool _test_snap_cache_100(void)
{
    return _run_snapshot_test("Snapshot: cache heap 100x100", 100, 100, _alloc_draw_buf_cache, _free_draw_buf_cache, false);
}

static bool _test_snap_cache_200(void)
{
    return _run_snapshot_test("Snapshot: cache heap 200x200", 200, 200, _alloc_draw_buf_cache, _free_draw_buf_cache, false);
}

static bool _test_snap_cache_300(void)
{
    return _run_snapshot_test("Snapshot: cache heap 300x300", 300, 300, _alloc_draw_buf_cache, _free_draw_buf_cache, false);
}

static bool _test_snap_cache_full(void)
{
    return _run_snapshot_test("Snapshot: cache heap fullscreen", EOS_DISPLAY_WIDTH, EOS_DISPLAY_HEIGHT, _alloc_draw_buf_cache, _free_draw_buf_cache, false);
}

static bool _test_snap_cache_repeat_200(void)
{
    return _run_snapshot_test("Snapshot: cache heap repeat 200x200", 200, 200, _alloc_draw_buf_cache, _free_draw_buf_cache, true);
}

/* ---------------------------------------------------------------------------
 * Registration
 * ------------------------------------------------------------------------- */

void eos_test_snapshot_register_tests(void)
{
    eos_test_register("Snapshot: direct heap 100x100", _test_snap_direct_100);
    eos_test_register("Snapshot: direct heap 200x200", _test_snap_direct_200);
    eos_test_register("Snapshot: direct heap 300x300", _test_snap_direct_300);
    eos_test_register("Snapshot: direct heap fullscreen", _test_snap_direct_full);
    eos_test_register("Snapshot: direct heap repeat 200x200", _test_snap_direct_repeat_200);
    eos_test_register("Snapshot: cache heap 100x100", _test_snap_cache_100);
    eos_test_register("Snapshot: cache heap 200x200", _test_snap_cache_200);
    eos_test_register("Snapshot: cache heap 300x300", _test_snap_cache_300);
    eos_test_register("Snapshot: cache heap fullscreen", _test_snap_cache_full);
    eos_test_register("Snapshot: cache heap repeat 200x200", _test_snap_cache_repeat_200);
    eos_test_register("Snapshot: anim trans image", _test_anim_trans_image);
    eos_test_register("Snapshot: anim scale image", _test_anim_scale_image);
    eos_test_register("Snapshot: anim opa image", _test_anim_opa_image);
    eos_test_register("Snapshot: anim combined image", _test_anim_combined_image);
    eos_test_register("Snapshot: anim trans direct", _test_anim_trans_direct);
    eos_test_register("Snapshot: anim scale direct", _test_anim_scale_direct);
    eos_test_register("Snapshot: anim opa direct", _test_anim_opa_direct);
    eos_test_register("Snapshot: anim combined direct", _test_anim_combined_direct);
}

#endif /* EOS_ENABLE_TEST_APP */
