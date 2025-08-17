/**
 * @file elena_os_anim.h
 * @brief 动画库
 * @author Sab1e
 * @date 2025-08-14
 */

#ifndef ELENA_OS_ANIM_H
#define ELENA_OS_ANIM_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/
/**
 * @brief 所有动画类型，可以继续添加。
 */
typedef enum{
    EOS_ANIM_SCALE,
    // 此处可以添加其他动画类型
}eos_anim;
typedef struct eos_anim_t eos_anim_t;   // 预定义
/**
 * @brief 回调函数的类型定义
 */
typedef void (*eos_anim_cb_t)(eos_anim_t* a);
/**
 * @brief ElenaOS 动画对象的结构体
 */
struct eos_anim_t{
    lv_anim_timeline_t* anim_timeline;  /**< 动画的时间线指针 */
    eos_anim type;                      /**< 动画类型 */
    uint8_t anim_count;                 /**< 该类型总的动画数量 */
    uint8_t anim_completed_count;       /**< 当前动画已经播放完毕的数量（用于判断动画是不是全部播放完毕）*/
    eos_anim_cb_t user_cb;              /**< 用户设定的回调函数 */
    void* user_data;                    /**< 用户数据 */
    union {                             /**< 用于存储动画对象的共用体 */
        struct {
            lv_anim_t a_width;          /**< 缩放动画的宽度动画对象 */
            lv_anim_t a_height;         /**< 缩放动画的高度动画对象 */
        } scale;
        // 此处可以添加其他动画类型的结构
    };
};
/* Public function prototypes --------------------------------*/
 /**
 * @brief 创建缩放动画对象
 * @param tar_obj 目标对象
 * @param w_start 起始宽度
 * @param w_end 结束宽度
 * @param h_start 起始高度
 * @param h_end 结束高度
 * @param duration 持续时间(ms)
 * @return 创建的动画对象指针，失败返回NULL
 */
eos_anim_t* eos_anim_scale_create(lv_obj_t* tar_obj,
                                int32_t w_start, int32_t w_end,
                                int32_t h_start, int32_t h_end,
                                uint32_t duration);
/**
 * @brief 开始播放动画
 * @param anim 由create函数创建的动画对象
 * @return 成功返回true，失败返回false
 */
bool eos_anim_start(eos_anim_t* anim);
/**
 * @brief 创建并立即播放缩放动画，无法设置回调
 * @note 动画完成后会自动删除
 */
void eos_anim_scale_start(lv_obj_t* tar_obj,
                                int32_t w_start, int32_t w_end,
                                int32_t h_start, int32_t h_end,
                                uint32_t duration);
/**
 * @brief 给动画设置播放完毕时的回调
 * @param anim 要设置的动画
 * @param user_cb 回调函数
 * @param user_data 用户数据指针（需要用户自行管理生命周期）
 */
void eos_anim_set_cb(
    eos_anim_t *anim,
    eos_anim_cb_t user_cb,
    void *user_data);
/**
 * @brief 获取动画对象的用户数据
 */
void* eos_anim_get_user_data(eos_anim_t *anim);
/**
 * @brief 删除动画对象
 * @param anim 动画对象指针
 * @note 如果动画正在运行会自动停止
 */
void eos_anim_del(eos_anim_t* anim);

void eos_anim_blocker_show(void);
void eos_anim_blocker_hide(void);

#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_ANIM_H */
