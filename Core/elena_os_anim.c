/**
 * @file elena_os_anim.c
 * @brief 动画库
 * @author Sab1e
 * @date 2025-08-14
 */

#include "elena_os_anim.h"

// Includes
#include <stdio.h>
#include <stdlib.h>

// Macros and Definitions

// Variables

// Function Implementations
/**
 * @brief 动画播放时设置宽度的回调
 *
 */
static void _set_width_cb(lv_anim_t *var, int32_t v)
{
    lv_obj_set_width(var->var, v);
}
/**
 * @brief 动画播放时设置高度的回调
 *
 */
static void _set_height_cb(lv_anim_t *var, int32_t v)
{
    lv_obj_set_height(var->var, v);
}
/**
 * @brief 动画播放完毕回调，回调用户函数，自动清理资源
 *
 */
static void _eos_anim_ready_cb(lv_anim_t *a)
{
    eos_anim_t *anim = (eos_anim_t *)lv_anim_get_user_data(a);
    anim->anim_completed_count++;

    if (anim->anim_completed_count == anim->anim_count)
    {
        if (anim->user_cb)
        {
            anim->user_cb(anim);
        }
        // 自动清理资源
        lv_anim_timeline_delete(anim->anim_timeline);
        lv_free(anim);
    }
}
/**
 * @brief 内部函数：初始化宽度动画
 *
 */
static void _init_width_anim(lv_anim_t *a, lv_obj_t *obj,
                             int32_t start, int32_t end,
                             uint32_t duration, eos_anim_t *ctx)
{
    lv_anim_init(a);
    lv_anim_set_var(a, obj);
    lv_anim_set_values(a, start, end);
    lv_anim_set_custom_exec_cb(a, _set_width_cb);
    lv_anim_set_path_cb(a, lv_anim_path_ease_out);
    lv_anim_set_duration(a, duration);
    lv_anim_set_ready_cb(a, _eos_anim_ready_cb);
    lv_anim_set_user_data(a, ctx);
}
/**
 * @brief 内部函数：初始化高度动画
 *
 */
static void _init_height_anim(lv_anim_t *a, lv_obj_t *obj,
                              int32_t start, int32_t end,
                              uint32_t duration, eos_anim_t *ctx)
{
    lv_anim_init(a);
    lv_anim_set_var(a, obj);
    lv_anim_set_values(a, start, end);
    lv_anim_set_custom_exec_cb(a, _set_height_cb);
    lv_anim_set_path_cb(a, lv_anim_path_ease_out);
    lv_anim_set_duration(a, duration);
    lv_anim_set_ready_cb(a, _eos_anim_ready_cb);
    lv_anim_set_user_data(a, ctx);
}

eos_anim_t *eos_anim_scale_create(lv_obj_t *tar_obj,
                                  int32_t w_start, int32_t w_end,
                                  int32_t h_start, int32_t h_end,
                                  uint32_t duration)
{
    if (!tar_obj || duration == 0)
        return NULL;

    eos_anim_t *anim = lv_mem_alloc(sizeof(eos_anim_t));
    if (!anim)
        return NULL;

    // 基础初始化
    anim->type = EOS_ANIM_SCALE;
    anim->anim_count = 2;
    anim->anim_completed_count = 0;
    anim->user_cb = NULL;
    anim->user_data = NULL;
    anim->anim_timeline = lv_anim_timeline_create();
    if (!anim->anim_timeline)
    {
        lv_mem_free(anim);
        return NULL;
    }

    // 初始化宽度动画
    _init_width_anim(&anim->scale.a_width, tar_obj, w_start, w_end, duration, anim);

    // 初始化高度动画
    _init_height_anim(&anim->scale.a_height, tar_obj, h_start, h_end, duration, anim);

    return anim;
}

bool eos_anim_start(eos_anim_t *anim)
{
    if (!anim || !anim->anim_timeline)
        return false;

    // 添加所有子动画到时间线
    switch (anim->type)
    {
    case EOS_ANIM_SCALE:
        lv_anim_timeline_add(anim->anim_timeline, 0, &anim->scale.a_width);
        lv_anim_timeline_add(anim->anim_timeline, 0, &anim->scale.a_height);
        break;
    // 其他动画类型...
    default:
        return false;
    }

    lv_anim_timeline_start(anim->anim_timeline);
    return true;
}

void eos_anim_scale_start(lv_obj_t *tar_obj,
                          int32_t w_start, int32_t w_end,
                          int32_t h_start, int32_t h_end,
                          uint32_t duration)
{
    eos_anim_t *anim = eos_anim_scale_create(tar_obj, w_start, w_end, h_start, h_end, duration);
    if (!anim)
        return;

    if (!eos_anim_start(anim))
    {
        eos_anim_del(anim);
    }
}

void eos_anim_set_cb(
    eos_anim_t *anim,
    eos_anim_cb_t user_cb,
    void *user_data)
{
    if (!anim)
        return;
    anim->user_cb = user_cb;
    anim->user_data = user_data;
}

void *eos_anim_get_user_data(eos_anim_t *anim)
{
    return anim ? anim->user_data : NULL;
}
