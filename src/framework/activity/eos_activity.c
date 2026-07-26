/**
 * @file eos_activity.c
 * @brief Activity controller
 */

#include "eos_activity.h"

/* Includes ---------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "eos_stack.h"
#include "eos_mem.h"
#define EOS_LOG_TAG "Activity"
#include "eos_log.h"
#include "eos_core.h"
#include "eos_config.h"
#include "eos_dispatcher.h"
#include "eos_theme.h"
#include "eos_lang.h"
#include "eos_basic_widgets.h"
#include "eos_app_header.h"
#include "eos_chrome_manager.h"
#include "eos_overlay_layer.h"
#include "eos_event.h"
#include "eos_image.h"
#include "eos_widget_data.h"
/* Macros and Definitions -------------------------------------*/
#define _ACTIVITY_STACK_INIT_CAPACITY 8
#define _DEFAULT_TITLE_COLOR EOS_COLOR_BLUE
#define _SNAPSHOT_COLOR_FORMAT LV_COLOR_FORMAT_RGB565
#define _DEBUG_SNAPSHOT 0

typedef enum
{
    _TITLE_TYPE_INVALID = 0,
    _TITLE_TYPE_STRING,
    _TITLE_TYPE_ID
} eos_activity_title_type_t;

typedef struct eos_activity_snapshot_node_t
{
    lv_obj_t *snapshot_obj;
    lv_draw_buf_t *draw_buf;
    eos_activity_t *owner;
    struct eos_activity_snapshot_node_t *next;
} eos_activity_snapshot_node_t;

typedef struct
{
    eos_anim_group_t *group;

    eos_activity_t *from;
    eos_activity_t *to;
    bool destroy_from;
    bool cleanup_scheduled;
    eos_activity_snapshot_node_t *snapshots;
} eos_activity_anim_ctx_t;

struct eos_activity_t
{
    lv_obj_t *view;
    eos_activity_type_t type;
    uint8_t snapshot_ref_count;
    bool is_app_header_visible;
    bool is_app_header_time_only;
    lv_color_t app_header_time_only_text_color;
    bool destroy_on_exit;
    bool has_started;
    struct
    {
        lv_color_t color;
        union
        {
            char *string;
            uint32_t id;
        };
        eos_activity_title_type_t type;
    } title;

    eos_activity_lifecycle_t lifecycle;

    void *user_data;

    void *fault_panel;

    lv_obj_t *snap_container;
};

typedef struct
{
    eos_activity_t *root_activity; /**< Root Activity (e.g., watchface), not in stack */
    eos_activity_t *current_activity;
    eos_activity_t *visible_activity;
    eos_activity_t *previous_activity;
    eos_stack_t *activity_stack;
    lv_obj_t *root_screen;
    bool transition_in_progress;
    bool snapshot_capture_window;
    eos_activity_anim_ctx_t *active_anim_ctx;
} eos_activity_ctx_t;

/* Variables --------------------------------------------------*/
static eos_activity_ctx_t _activity_ctx = {
    .root_activity = NULL,
    .current_activity = NULL,
    .visible_activity = NULL,
    .previous_activity = NULL,
    .activity_stack = NULL,
    .root_screen = NULL,
    .transition_in_progress = false,
    .snapshot_capture_window = false,
    .active_anim_ctx = NULL,
};

static eos_activity_anim_cb_t _anim_callback_routes[EOS_ACTIVITY_TYPE_COUNT][EOS_ACTIVITY_TYPE_COUNT] = {0};

/* Function Implementations -----------------------------------*/
static const char *_activity_type_to_str(eos_activity_type_t type)
{
    switch (type)
    {
        case EOS_ACTIVITY_TYPE_NULL:
            return "NULL";
        case EOS_ACTIVITY_TYPE_APP:
            return "APP";
        case EOS_ACTIVITY_TYPE_INPUT_PAGE:
            return "INPUT_PAGE";
        case EOS_ACTIVITY_TYPE_APP_LIST:
            return "APP_LIST";
        case EOS_ACTIVITY_TYPE_WATCHFACE:
            return "WATCHFACE";
        case EOS_ACTIVITY_TYPE_WATCHFACE_LIST:
            return "WATCHFACE_LIST";
        case EOS_ACTIVITY_TYPE_LOCK_SCREEN:
            return "LOCK_SCREEN";
        default:
            return "UNKNOWN";
    }
}

static void _update_app_header_if_needed(eos_activity_t *activity);
static void _anim_group_start(eos_activity_t *from, eos_activity_t *to, eos_activity_anim_ctx_t *anim_ctx);
static void _anim_clean_up_activity(void *user_data);
static void _anim_clean_up_activity_deferred(void *user_data);
static void _activity_mark_visible(eos_activity_t *activity);
static void _snapshot_img_delete_cb(lv_event_t *e);
static void _activity_snapshot_hold(eos_activity_t *activity);
static void _activity_snapshot_release(eos_activity_t *activity);

static bool _controller_initialized(void)
{
    return _activity_ctx.activity_stack != NULL && _activity_ctx.root_screen != NULL;
}

static void _activity_run_destroy(eos_activity_t *activity)
{
    EOS_CHECK_PTR_RETURN(activity);

    EOS_LOG_I("Activity destroy begin: activity=%p type=%s destroy_on_exit=%d started=%d view=%p valid=%d "
              "snapshot_ref=%u header_visible=%d header_time_only=%d user_data=%p",
              (void *)activity,
              _activity_type_to_str(activity->type),
              activity->destroy_on_exit,
              activity->has_started,
              (void *)activity->view,
              (activity->view && lv_obj_is_valid(activity->view)),
              activity->snapshot_ref_count,
              activity->is_app_header_visible,
              activity->is_app_header_time_only,
              activity->user_data);

    // Call on_destroy callback before destroying resources
    if (activity->lifecycle.on_destroy)
    {
        activity->lifecycle.on_destroy(activity);
    }

    activity->user_data = NULL;

    if (activity->view && lv_obj_is_valid(activity->view))
    {
        lv_obj_delete(activity->view);
        activity->view = NULL;
    }

    if (activity->snap_container && lv_obj_is_valid(activity->snap_container))
    {
        lv_obj_delete(activity->snap_container);
        activity->snap_container = NULL;
    }

    if (activity->title.type == _TITLE_TYPE_STRING)
    {
        if (activity->title.string)
        {
            eos_free(activity->title.string);
            activity->title.string = NULL;
        }
    }

    /* Clear any dangling context pointers that still reference
       this activity so subsequent code does not read freed memory. */
    if (_activity_ctx.previous_activity == activity)
        _activity_ctx.previous_activity = NULL;
    if (_activity_ctx.visible_activity == activity)
        _activity_ctx.visible_activity = NULL;
    if (_activity_ctx.current_activity == activity)
        _activity_ctx.current_activity = NULL;
    /* NOTE: root_activity is intentionally NOT cleared here.
       It is managed explicitly by eos_activity_replace_root. */

    eos_free(activity);

    EOS_LOG_I("Activity destroy end");
}

bool eos_activity_has_started(eos_activity_t *activity)
{
    return activity ? activity->has_started : false;
}

void eos_activity_destroy(eos_activity_t *activity)
{
    _activity_run_destroy(activity);
}

static void _activity_reset_context(void)
{
    _activity_ctx.root_activity = NULL;
    _activity_ctx.current_activity = NULL;
    _activity_ctx.visible_activity = NULL;
    _activity_ctx.previous_activity = NULL;
    _activity_ctx.activity_stack = NULL;
    _activity_ctx.root_screen = NULL;
    _activity_ctx.transition_in_progress = false;
    _activity_ctx.snapshot_capture_window = false;
    _activity_ctx.active_anim_ctx = NULL;
    memset(_anim_callback_routes, 0, sizeof(_anim_callback_routes));
}

static void _activity_controller_init_failed(eos_activity_t *root_activity)
{
    if (_activity_ctx.activity_stack)
    {
        eos_stack_destroy(_activity_ctx.activity_stack);
        _activity_ctx.activity_stack = NULL;
    }

    if (_activity_ctx.root_screen && lv_obj_is_valid(_activity_ctx.root_screen))
    {
        lv_obj_delete(_activity_ctx.root_screen);
        if (root_activity && root_activity->view && !lv_obj_is_valid(root_activity->view))
        {
            root_activity->view = NULL;
        }
    }

    _activity_reset_context();
}

static void _reset_view_visual_state(eos_activity_t *activity)
{
    if (!activity || !activity->view || !lv_obj_is_valid(activity->view))
    {
        return;
    }

    lv_obj_t *view = activity->view;

    lv_coord_t w = lv_obj_get_width(view);
    lv_coord_t h = lv_obj_get_height(view);

    lv_obj_set_style_opa(view, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_x(view, 256, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_y(view, 256, LV_PART_MAIN);
    lv_obj_set_style_translate_x(view, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(view, 0, LV_PART_MAIN);
    lv_obj_set_style_transform_rotation(view, 0, LV_PART_MAIN);
    lv_obj_set_pos(view, 0, 0);
    lv_obj_set_width(view, w);
    lv_obj_set_height(view, h);
}

static void _activity_show(eos_activity_t *activity)
{
    EOS_CHECK_PTR_RETURN(activity);
    if (!activity->view || !lv_obj_is_valid(activity->view))
    {
        EOS_LOG_W("_activity_show: Invalid or NULL view for activity, skipping");
        return;
    }

    lv_obj_remove_flag(activity->view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(activity->view);

    if (activity->snap_container && lv_obj_is_valid(activity->snap_container))
    {
        lv_obj_move_foreground(activity->snap_container);
    }
}

static void _activity_mark_visible(eos_activity_t *activity)
{
    _activity_ctx.visible_activity = activity;
    _activity_ctx.transition_in_progress = false;
    EOS_LOG_I("Activity visible updated: visible=%p[%s] current=%p[%s] trans=%d",
              (void *)_activity_ctx.visible_activity,
              _activity_type_to_str(_activity_ctx.visible_activity ? _activity_ctx.visible_activity->type
                                                                   : EOS_ACTIVITY_TYPE_NULL),
              (void *)_activity_ctx.current_activity,
              _activity_type_to_str(_activity_ctx.current_activity ? _activity_ctx.current_activity->type
                                                                   : EOS_ACTIVITY_TYPE_NULL),
              _activity_ctx.transition_in_progress);
    eos_event_post(EOS_EVENT_ACTIVITY_SCREEN_SWITCHED,
                   activity ? activity->view : NULL,
                   activity ? activity->view : NULL);
}

static void _snapshot_img_delete_cb(lv_event_t *e)
{
    eos_activity_snapshot_node_t *snapshot_node = lv_event_get_user_data(e);
    if (!snapshot_node)
    {
        return;
    }

    if (snapshot_node->draw_buf)
    {
        eos_draw_buf_destroy(snapshot_node->draw_buf);
        snapshot_node->draw_buf = NULL;
    }

    if (snapshot_node->owner)
    {
        _activity_snapshot_release(snapshot_node->owner);
        snapshot_node->owner = NULL;
    }
}

static void _activity_snapshot_hold(eos_activity_t *activity)
{
    EOS_CHECK_PTR_RETURN(activity);
    if (activity->snapshot_ref_count < UINT8_MAX)
    {
        activity->snapshot_ref_count++;
    }

    if (activity->view)
    {
        lv_obj_add_flag(activity->view, LV_OBJ_FLAG_HIDDEN);
    }
}

static void _activity_snapshot_release(eos_activity_t *activity)
{
    EOS_CHECK_PTR_RETURN(activity);
    if (activity->snapshot_ref_count > 0)
    {
        activity->snapshot_ref_count--;
    }
}

static void _anim_clean_up_activity_deferred(void *user_data)
{
    eos_activity_anim_ctx_t *anim_ctx = (eos_activity_anim_ctx_t *)user_data;
    if (!anim_ctx)
    {
        return;
    }

    EOS_LOG_I("Anim cleanup begin: from=%p[%s destroy=%d view=%p valid=%d] to=%p[%s destroy=%d view=%p valid=%d] "
              "snapshots=%p",
              (void *)anim_ctx->from,
              _activity_type_to_str(anim_ctx->from ? anim_ctx->from->type : EOS_ACTIVITY_TYPE_NULL),
              anim_ctx->from ? anim_ctx->from->destroy_on_exit : false,
              anim_ctx->from ? (void *)anim_ctx->from->view : NULL,
              (anim_ctx->from && anim_ctx->from->view) ? lv_obj_is_valid(anim_ctx->from->view) : false,
              (void *)anim_ctx->to,
              _activity_type_to_str(anim_ctx->to ? anim_ctx->to->type : EOS_ACTIVITY_TYPE_NULL),
              anim_ctx->to ? anim_ctx->to->destroy_on_exit : false,
              anim_ctx->to ? (void *)anim_ctx->to->view : NULL,
              (anim_ctx->to && anim_ctx->to->view) ? lv_obj_is_valid(anim_ctx->to->view) : false,
              (void *)anim_ctx->snapshots);

    eos_activity_snapshot_node_t *node = anim_ctx->snapshots;
    uint32_t snapshot_count = 0;
    while (node)
    {
        snapshot_count++;
        node = node->next;
    }
    EOS_LOG_E("DEFERRED cleanup: snapshot_count=%d destroy_from=%d", snapshot_count, anim_ctx->destroy_from);
    node = anim_ctx->snapshots;
    while (node)
    {
        eos_activity_snapshot_node_t *next = node->next;
        if (node->snapshot_obj && lv_obj_is_valid(node->snapshot_obj))
        {
            EOS_LOG_E("Deleting snapshot_obj[%p] draw_buf[%p]", node->snapshot_obj, node->draw_buf);
            lv_obj_delete(node->snapshot_obj);
        }
        else
        {
            EOS_LOG_E("Skipping invalid snapshot_obj[%p] draw_buf[%p]", node->snapshot_obj, node->draw_buf);
        }
        eos_free(node);
        node = next;
    }
    anim_ctx->snapshots = NULL;
    EOS_LOG_E("DEFERRED cleanup: snapshots freed");

    if (anim_ctx->destroy_from && anim_ctx->from)
    {
        if (!eos_activity_is_app_header_visible(anim_ctx->to))
        {
            eos_app_header_hide();
        }
        _activity_run_destroy(anim_ctx->from);
        anim_ctx->from = NULL;
    }
    else if (!eos_activity_is_app_header_visible(anim_ctx->to) && eos_activity_is_app_header_visible(anim_ctx->from))
    {
        if (!eos_app_header_is_attached_to_view())
        {
            eos_app_header_hide();
        }
    }

    if (anim_ctx->to && anim_ctx->to->snapshot_ref_count == 0)
    {
        _reset_view_visual_state(anim_ctx->to);
        _activity_show(anim_ctx->to);
    }
    else if (anim_ctx->to)
    {
        EOS_LOG_E("STALLED: to activity[%p] snapshot_ref_count=%d > 0, view NOT restored!",
                  anim_ctx->to,
                  anim_ctx->to->snapshot_ref_count);
    }
    if (anim_ctx->to && eos_activity_is_app_header_visible(anim_ctx->to))
    {
        eos_app_header_show(anim_ctx->to);
    }

    if (anim_ctx->group)
    {
        eos_anim_group_del(anim_ctx->group);
        anim_ctx->group = NULL;
    }

    eos_event_post(EOS_EVENT_ACTIVITY_SCREEN_SWITCHED,
                   anim_ctx->to ? anim_ctx->to->view : NULL,
                   anim_ctx->to ? anim_ctx->to->view : NULL);
    eos_free(anim_ctx);

    _activity_ctx.transition_in_progress = false;

    /* Transition fully complete — re-enable input. */
    eos_anim_blocker_hide();

    EOS_LOG_I("Anim cleanup end");
}

static void _activity_switch_to(eos_activity_t *next_activity, bool is_returning)
{
    EOS_CHECK_PTR_RETURN(next_activity);
    eos_activity_t *cur_activity = _activity_ctx.current_activity;
    EOS_LOG_I("Activity switch request: next=%p[%s] returning=%d current=%p[%s] visible=%p[%s] transition=%d",
              (void *)next_activity,
              _activity_type_to_str(next_activity->type),
              is_returning,
              (void *)cur_activity,
              _activity_type_to_str(cur_activity ? cur_activity->type : EOS_ACTIVITY_TYPE_NULL),
              (void *)_activity_ctx.visible_activity,
              _activity_type_to_str(_activity_ctx.visible_activity ? _activity_ctx.visible_activity->type
                                                                   : EOS_ACTIVITY_TYPE_NULL),
              _activity_ctx.transition_in_progress);
    if (cur_activity == next_activity)
    {
        _activity_mark_visible(next_activity);
        _reset_view_visual_state(next_activity);
        _activity_show(next_activity);
        if (next_activity->is_app_header_visible)
        {
            eos_app_header_show(next_activity);
        }
        else
        {
            eos_app_header_hide();
        }
        return;
    }

    _activity_ctx.previous_activity = cur_activity;
    _activity_ctx.current_activity = next_activity;

    /* Keep target view hidden during lifecycle work to avoid transient one-frame flashes. */
    if (next_activity->view && lv_obj_is_valid(next_activity->view))
    {
        lv_obj_add_flag(next_activity->view, LV_OBJ_FLAG_HIDDEN);
    }

    eos_chrome_manager_handle_activity_switch();

    if (!is_returning && cur_activity && cur_activity->lifecycle.on_pause)
    {
        cur_activity->lifecycle.on_pause(cur_activity);
    }

    if (!next_activity->has_started && next_activity->lifecycle.on_enter)
    {
        next_activity->lifecycle.on_enter(next_activity);
        next_activity->has_started = true;
    }
    else if (is_returning && next_activity->has_started && next_activity->lifecycle.on_resume)
    {
        next_activity->lifecycle.on_resume(next_activity);
    }

    bool header_need_anim = false;
    bool header_reverse_anim = false;
    if (eos_activity_is_app_header_visible(next_activity))
    {
        if (cur_activity)
        {
            if (eos_activity_is_app_header_visible(cur_activity))
            {
                header_need_anim = true;

                if (cur_activity->destroy_on_exit)
                {
                    header_reverse_anim = true;
                }
            }
        }
    }
    else
    {
        if (cur_activity && cur_activity->view && lv_obj_is_valid(cur_activity->view)
            && eos_activity_is_app_header_visible(cur_activity))
        {
            eos_app_header_attach_to_view(cur_activity->view);
        }
        else
        {
            eos_app_header_hide();
        }
    }

    eos_activity_anim_cb_t anim_cb = NULL;
    bool list_anim_available = false;
    if (cur_activity)
    {
        anim_cb = eos_activity_get_anim_route(cur_activity->type, next_activity->type);
        if (!anim_cb)
        {
            list_anim_available =
                eos_list_transition_should_animate(cur_activity, next_activity, cur_activity->destroy_on_exit);
        }
    }

    bool transition_started = false;
    if (cur_activity && (anim_cb || list_anim_available))
    {
        eos_activity_anim_ctx_t *anim_ctx = eos_malloc_zeroed(sizeof(eos_activity_anim_ctx_t));
        if (anim_ctx)
        {
            anim_ctx->group = eos_anim_group_create(_anim_clean_up_activity, anim_ctx);
            if (!anim_ctx->group)
            {
                eos_free(anim_ctx);
                anim_ctx = NULL;
            }
        }

        if (anim_ctx)
        {
            anim_ctx->from = cur_activity;
            anim_ctx->to = next_activity;
            anim_ctx->destroy_from = cur_activity ? cur_activity->destroy_on_exit : false;

            EOS_LOG_I("Activity transition start: from=%p[%s destroy=%d] to=%p[%s destroy=%d] anim_cb=%p list_anim=%d",
                      (void *)cur_activity,
                      _activity_type_to_str(cur_activity ? cur_activity->type : EOS_ACTIVITY_TYPE_NULL),
                      cur_activity ? cur_activity->destroy_on_exit : false,
                      (void *)next_activity,
                      _activity_type_to_str(next_activity->type),
                      next_activity->destroy_on_exit,
                      (void *)anim_cb,
                      list_anim_available);

            _activity_ctx.transition_in_progress = true;
            transition_started = true;

            /* Block all input during the animated transition.  Snapshot-
             * backend animations use preserve_layout (opa=0) to keep real
             * widgets in place — without the blocker those invisible-but-
             * clickable widgets could fire events and corrupt state. */
            eos_anim_blocker_show();

            _activity_ctx.active_anim_ctx = anim_ctx;
            _activity_ctx.snapshot_capture_window = true;
            if (anim_cb)
            {
                anim_cb(anim_ctx->group, cur_activity, next_activity);
            }
            else
            {
                eos_list_transition_play(anim_ctx->group, cur_activity, next_activity, cur_activity->destroy_on_exit);
            }
            _activity_ctx.snapshot_capture_window = false;
            _activity_ctx.active_anim_ctx = NULL;
            if (cur_activity)
            {
                _play_title_changed_anim(cur_activity,
                                         next_activity,
                                         header_need_anim,
                                         header_reverse_anim,
                                         anim_ctx->group);
            }
            _anim_group_start(cur_activity, next_activity, anim_ctx);
        }
    }

    if (!transition_started)
    {
        EOS_LOG_I("Activity switch no-transition path: cur=%p[%s destroy=%d] next=%p[%s]",
                  (void *)cur_activity,
                  _activity_type_to_str(cur_activity ? cur_activity->type : EOS_ACTIVITY_TYPE_NULL),
                  cur_activity ? cur_activity->destroy_on_exit : false,
                  (void *)next_activity,
                  _activity_type_to_str(next_activity->type));
        if (cur_activity && cur_activity->destroy_on_exit)
        {
            if (!eos_activity_is_app_header_visible(next_activity))
            {
                eos_app_header_hide();
            }
            _activity_run_destroy(cur_activity);
        }
        else if (!eos_activity_is_app_header_visible(next_activity) && cur_activity
                 && eos_activity_is_app_header_visible(cur_activity))
        {
            eos_app_header_hide();
        }

        _activity_show(next_activity);
        if (eos_activity_is_app_header_visible(next_activity))
        {
            if (cur_activity)
            {
                _play_title_changed_anim(cur_activity, next_activity, header_need_anim, header_reverse_anim, NULL);
            }
            eos_app_header_show(next_activity);
        }
        _activity_mark_visible(next_activity);
    }
}

static lv_obj_t *_view_create(lv_obj_t *parent)
{
    if (!parent)
    {
        parent = _activity_ctx.root_screen;
    }

    lv_obj_t *view = lv_obj_create(parent);
    if (!view)
    {
        return NULL;
    }

    lv_obj_remove_style_all(view);
    lv_obj_add_style(view, eos_theme_get_view_style(), 0);
    lv_obj_set_style_radius(view, EOS_DISPLAY_RADIUS, 0);
    lv_obj_update_layout(view);

    return view;
}

static lv_obj_t *_snap_container_create(void)
{
    lv_obj_t *container = lv_obj_create(eos_overlay_get_snapshot_layer());
    if (!container)
        return NULL;
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 0, 0);
    lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    return container;
}

static void _anim_clean_up_activity(void *user_data)
{
    if (!user_data)
        return;

    eos_activity_anim_ctx_t *anim_ctx = (eos_activity_anim_ctx_t *)user_data;
    if (!anim_ctx)
        return;

    if (anim_ctx->cleanup_scheduled)
    {
        return;
    }

    anim_ctx->cleanup_scheduled = true;

    EOS_LOG_I("Anim completed PRE-SET: visible=%p[%s] current=%p[%s] to=%p[%s] from=%p[%s] trans=%d",
              (void *)_activity_ctx.visible_activity,
              _activity_type_to_str(_activity_ctx.visible_activity ? _activity_ctx.visible_activity->type
                                                                   : EOS_ACTIVITY_TYPE_NULL),
              (void *)_activity_ctx.current_activity,
              _activity_type_to_str(_activity_ctx.current_activity ? _activity_ctx.current_activity->type
                                                                   : EOS_ACTIVITY_TYPE_NULL),
              (void *)anim_ctx->to,
              _activity_type_to_str(anim_ctx->to ? anim_ctx->to->type : EOS_ACTIVITY_TYPE_NULL),
              (void *)anim_ctx->from,
              _activity_type_to_str(anim_ctx->from ? anim_ctx->from->type : EOS_ACTIVITY_TYPE_NULL),
              _activity_ctx.transition_in_progress);

    _activity_ctx.visible_activity = anim_ctx->to;
    /* NOTE: transition_in_progress stays TRUE until deferred cleanup completes.
     * Clearing it here would allow a new transition to start before snapshots
     * and old views are cleaned up, causing a race where the new transition's
     * animations can be interfered with by the deferred cleanup of the old one. */
    EOS_LOG_I("Anim completed gate open: visible=%p[%s] (deferred cleanup pending)",
              (void *)anim_ctx->to,
              _activity_type_to_str(anim_ctx->to ? anim_ctx->to->type : EOS_ACTIVITY_TYPE_NULL));

    EOS_LOG_E("DISPATCHING deferred cleanup for anim_ctx[%p]", anim_ctx);
    eos_dispatcher_call(_anim_clean_up_activity_deferred, anim_ctx);
}

static void _anim_group_start(eos_activity_t *from, eos_activity_t *to, eos_activity_anim_ctx_t *anim_ctx)
{
    LV_UNUSED(from);
    LV_UNUSED(to);

    if (!(anim_ctx && anim_ctx->group))
    {
        return;
    }

    if (anim_ctx->group->expected == 0)
    {
        _anim_clean_up_activity(anim_ctx);
        return;
    }
}

void eos_activity_set_type(eos_activity_t *activity, eos_activity_type_t type)
{
    EOS_CHECK_PTR_RETURN(activity);
    if (type <= EOS_ACTIVITY_TYPE_NULL || type >= EOS_ACTIVITY_TYPE_COUNT)
    {
        EOS_LOG_W("Invalid activity type: %d", type);
        activity->type = EOS_ACTIVITY_TYPE_NULL;
        return;
    }

    activity->type = type;
}

eos_activity_type_t eos_activity_get_type(eos_activity_t *activity)
{
    EOS_CHECK_PTR_RETURN_VAL(activity, EOS_ACTIVITY_TYPE_NULL);
    return activity->type;
}

eos_result_t eos_activity_register_anim_route(eos_activity_type_t from_type,
                                              eos_activity_type_t to_type,
                                              eos_activity_anim_cb_t cb)
{
    if (from_type <= EOS_ACTIVITY_TYPE_NULL || from_type >= EOS_ACTIVITY_TYPE_COUNT || to_type <= EOS_ACTIVITY_TYPE_NULL
        || to_type >= EOS_ACTIVITY_TYPE_COUNT || cb == NULL)
    {
        EOS_LOG_W("Invalid route register request: from=%d to=%d cb=%p", from_type, to_type, cb);
        return EOS_FAILED;
    }

    if (_anim_callback_routes[from_type][to_type] != NULL)
    {
        EOS_LOG_W("Animation route duplicated: from=%d to=%d, overwrite", from_type, to_type);
    }

    _anim_callback_routes[from_type][to_type] = cb;
    return EOS_OK;
}

eos_activity_anim_cb_t eos_activity_get_anim_route(eos_activity_type_t from_type, eos_activity_type_t to_type)
{
    if (from_type <= EOS_ACTIVITY_TYPE_NULL || from_type >= EOS_ACTIVITY_TYPE_COUNT || to_type <= EOS_ACTIVITY_TYPE_NULL
        || to_type >= EOS_ACTIVITY_TYPE_COUNT)
    {
        return NULL;
    }

    return _anim_callback_routes[from_type][to_type];
}

void *eos_activity_get_user_data(eos_activity_t *activity)
{
    EOS_CHECK_PTR_RETURN_VAL(activity, NULL);
    return activity->user_data;
}

void eos_activity_set_user_data(eos_activity_t *activity, void *user_data)
{
    EOS_CHECK_PTR_RETURN(activity);
    activity->user_data = user_data;
}

void eos_activity_set_fault_panel(eos_activity_t *activity, void *fault_panel)
{
    EOS_CHECK_PTR_RETURN(activity);
    activity->fault_panel = fault_panel;
}

void *eos_activity_get_fault_panel(eos_activity_t *activity)
{
    EOS_CHECK_PTR_RETURN_VAL(activity, NULL);
    return activity->fault_panel;
}

lv_obj_t *eos_activity_get_view(eos_activity_t *activity)
{
    EOS_CHECK_PTR_RETURN_VAL(activity, NULL);
    return activity->view;
}

void eos_activity_set_view(eos_activity_t *activity, lv_obj_t *view)
{
    EOS_CHECK_PTR_RETURN(activity);
    if (activity->view && activity->view != view && lv_obj_is_valid(activity->view))
    {
        eos_wdata_remove(activity->view, EOS_WDATA_ACTIVITY);
        lv_obj_delete(activity->view);
    }
    activity->view = view;
    if (view && lv_obj_is_valid(view))
    {
        eos_wdata_set(view, EOS_WDATA_ACTIVITY, activity, NULL);
    }
    if (!activity->snap_container || !lv_obj_is_valid(activity->snap_container))
    {
        activity->snap_container = _snap_container_create();
    }
}

eos_activity_t *eos_activity_from_widget(lv_obj_t *obj)
{
    if (!obj)
        return NULL;

    lv_obj_t *current = obj;
    while (current)
    {
        eos_activity_t *activity = (eos_activity_t *)eos_wdata_get(current, EOS_WDATA_ACTIVITY);
        if (activity)
            return activity;
        current = lv_obj_get_parent(current);
    }

    return NULL;
}

lv_obj_t *eos_activity_get_snap_container(eos_activity_t *activity)
{
    EOS_CHECK_PTR_RETURN_VAL(activity, NULL);
    return (activity->snap_container && lv_obj_is_valid(activity->snap_container)) ? activity->snap_container : NULL;
}

lv_obj_t *eos_activity_get_root_screen(void)
{
    return _activity_ctx.root_screen;
}

lv_obj_t *eos_activity_take_snapshot(eos_activity_t *activity, bool include_header)
{
#if LV_USE_SNAPSHOT
    EOS_CHECK_PTR_RETURN_VAL(activity, NULL);

    if (!(_activity_ctx.snapshot_capture_window && _activity_ctx.active_anim_ctx))
    {
        EOS_LOG_W("eos_activity_take_snapshot is only allowed in animation callback");
        return NULL;
    }

    lv_obj_t *view = activity->view;
    if (!(view && lv_obj_is_valid(view)))
    {
        return NULL;
    }

    eos_activity_snapshot_node_t *snapshot_node = eos_malloc_zeroed(sizeof(eos_activity_snapshot_node_t));
    if (!snapshot_node)
    {
        return NULL;
    }

    lv_obj_t *snapshot_obj =
        lv_image_create(activity->snap_container ? activity->snap_container : eos_overlay_get_snapshot_layer());
    if (!snapshot_obj)
    {
        eos_free(snapshot_node);
        return NULL;
    }

#if _DEBUG_SNAPSHOT
    lv_obj_set_style_image_recolor(snapshot_obj, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_image_recolor_opa(snapshot_obj, LV_OPA_20, 0);
#endif /* _DEBUG_SNAPSHOT */

    lv_draw_buf_t *snapshot = eos_draw_buf_create((uint32_t)lv_obj_get_width(view),
                                                  (uint32_t)lv_obj_get_height(view),
                                                  _SNAPSHOT_COLOR_FORMAT,
                                                  0);
    if (!snapshot)
    {
        lv_obj_delete(snapshot_obj);
        eos_free(snapshot_node);
        return NULL;
    }

    bool prev_header_visible = eos_app_header_is_visible();
    eos_activity_t *prev_visible_activity = eos_activity_get_visible();
    bool need_attach_header = include_header && activity->is_app_header_visible;
    if (need_attach_header)
    {
        eos_app_header_show(activity);
        eos_app_header_attach_to_view(view);
    }

    bool was_hidden = lv_obj_has_flag(view, LV_OBJ_FLAG_HIDDEN);
    if (was_hidden)
    {
        lv_obj_remove_flag(view, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_update_layout(view);

    {
        lv_display_t *disp = lv_display_get_default();
        if (disp)
        {
            /* Let pending animations (e.g. lv_slider with LV_ANIM_ON)
             * complete before the snapshot is taken. */
            lv_obj_invalidate(view);
            lv_refr_now(disp);
        }
    }

    lv_result_t snapshot_result = lv_snapshot_take_to_draw_buf(view, _SNAPSHOT_COLOR_FORMAT, snapshot);

    if (was_hidden)
    {
        lv_obj_add_flag(view, LV_OBJ_FLAG_HIDDEN);
    }

    if (need_attach_header)
    {
        eos_app_header_detach_from_view();

        if (prev_header_visible)
        {
            if (prev_visible_activity)
            {
                eos_app_header_show(prev_visible_activity);
            }
            else
            {
                eos_app_header_show(eos_activity_get_current());
            }
        }
        else
        {
            eos_app_header_hide();
        }
    }

    if (snapshot_result != LV_RESULT_OK)
    {
        eos_draw_buf_destroy(snapshot);
        lv_obj_delete(snapshot_obj);
        eos_free(snapshot_node);
        return NULL;
    }

    snapshot_node->snapshot_obj = snapshot_obj;
    snapshot_node->draw_buf = snapshot;
    snapshot_node->owner = activity;
    snapshot_node->next = _activity_ctx.active_anim_ctx->snapshots;
    _activity_ctx.active_anim_ctx->snapshots = snapshot_node;

    lv_image_set_src(snapshot_obj, snapshot);
    lv_obj_set_size(snapshot_obj, lv_obj_get_width(view), lv_obj_get_height(view));
    lv_obj_set_pos(snapshot_obj, lv_obj_get_x(view), lv_obj_get_y(view));
    lv_obj_add_event_cb(snapshot_obj, _snapshot_img_delete_cb, LV_EVENT_DELETE, snapshot_node);
    _activity_snapshot_hold(activity);
    return snapshot_obj;
#else
    LV_UNUSED(activity);
    LV_UNUSED(include_header);
    EOS_LOG_W("LV_USE_SNAPSHOT disabled");
    return NULL;
#endif
}

eos_activity_t *eos_activity_get_watchface(void)
{
    return _activity_ctx.root_activity;
}

eos_result_t eos_activity_replace_root(eos_activity_t *new_root)
{
    EOS_CHECK_PTR_RETURN_VAL(new_root, EOS_FAILED);

    if (!_controller_initialized())
    {
        EOS_LOG_E("Activity controller not initialized");
        return EOS_FAILED;
    }

    if (_activity_ctx.transition_in_progress)
    {
        EOS_LOG_W("Activity transition in progress");
        return EOS_FAILED;
    }

    EOS_CHECK_PTR_RETURN_VAL(new_root->view, EOS_FAILED);

    // Destroy old root completely
    if (_activity_ctx.root_activity)
    {
        eos_activity_t *old_root = _activity_ctx.root_activity;
        _activity_ctx.root_activity = NULL;
        _activity_run_destroy(old_root);
    }

    // Set new root
    _activity_ctx.root_activity = new_root;

    // Ensure new root's view is parented to root screen
    if (lv_obj_get_parent(new_root->view) != _activity_ctx.root_screen)
    {
        lv_obj_set_parent(new_root->view, _activity_ctx.root_screen);
    }

    _activity_ctx.current_activity = new_root;
    _activity_ctx.visible_activity = new_root;

    // Enter new root (call on_enter) - now eos_view_active() will return correct view
    if (new_root->lifecycle.on_enter)
    {
        new_root->lifecycle.on_enter(new_root);
        new_root->has_started = true;
    }

    // Display new root
    _activity_show(new_root);

    if (new_root->is_app_header_visible)
    {
        eos_app_header_show(new_root);
    }
    else
    {
        eos_app_header_hide();
    }

    return EOS_OK;
}

eos_activity_t *eos_activity_get_root(void)
{
    return _activity_ctx.root_activity;
}

const char *eos_activity_get_title(eos_activity_t *activity)
{
    EOS_CHECK_PTR_RETURN_VAL(activity, NULL);
    if (activity->title.type == _TITLE_TYPE_STRING)
    {
        return activity->title.string;
    }
    else if (activity->title.type == _TITLE_TYPE_ID)
    {
        return eos_lang_get_text(activity->title.id);
    }
    return NULL;
}

void eos_activity_set_title(eos_activity_t *activity, const char *title)
{
    EOS_CHECK_PTR_RETURN(activity);
    if (activity->title.type == _TITLE_TYPE_STRING && activity->title.string)
    {
        eos_free(activity->title.string);
        activity->title.string = NULL;
    }
    if (title)
    {
        activity->title.string = (char *)eos_strdup(title);
        activity->title.type = _TITLE_TYPE_STRING;
    }
    else
    {
        activity->title.type = _TITLE_TYPE_INVALID;
    }

    _update_app_header_if_needed(activity);
}

void eos_activity_set_title_id(eos_activity_t *activity, lang_string_id_t id)
{
    EOS_CHECK_PTR_RETURN(activity);
    if (activity->title.type == _TITLE_TYPE_STRING && activity->title.string)
    {
        eos_free(activity->title.string);
        activity->title.string = NULL;
    }
    activity->title.id = id;
    activity->title.type = _TITLE_TYPE_ID;

    _update_app_header_if_needed(activity);
}

void eos_activity_set_app_header_visible(eos_activity_t *activity, bool visible)
{
    EOS_CHECK_PTR_RETURN(activity);

    if (visible && activity == eos_activity_get_watchface())
    {
        EOS_LOG_D("Cannot set app header visible for watchface activity");
        return;
    }

    activity->is_app_header_visible = visible;

    if (visible)
    {
        _update_app_header_if_needed(activity);
    }
    else
    {
        eos_activity_t *current = eos_activity_get_current();
        if (current == activity)
        {
            eos_app_header_hide();
        }
    }
}

void eos_activity_set_app_header_visible_animated(eos_activity_t *activity, bool visible, uint32_t duration_ms)
{
    EOS_CHECK_PTR_RETURN(activity);

    if (visible && activity == eos_activity_get_watchface())
    {
        EOS_LOG_D("Cannot set app header visible for watchface activity");
        return;
    }

    activity->is_app_header_visible = visible;

    eos_activity_t *current = eos_activity_get_current();
    if (current != activity)
        return;

    if (visible)
    {
        eos_app_header_set_visible_animated(activity, true, duration_ms);
    }
    else
    {
        eos_app_header_set_visible_animated(NULL, false, duration_ms);
    }
}

static void _update_app_header_if_needed(eos_activity_t *activity)
{
    eos_activity_t *visible = eos_activity_get_visible();
    if (visible != activity)
        return;

    if (!activity->is_app_header_visible)
        return;

    eos_app_header_show(activity);
}

bool eos_activity_is_app_header_visible(eos_activity_t *activity)
{
    EOS_CHECK_PTR_RETURN_VAL(activity, false);
    return activity->is_app_header_visible;
}

void eos_activity_set_app_header_time_only(eos_activity_t *activity, bool time_only)
{
    EOS_CHECK_PTR_RETURN(activity);

    activity->is_app_header_time_only = time_only;

    _update_app_header_if_needed(activity);
}

bool eos_activity_is_app_header_time_only(eos_activity_t *activity)
{
    EOS_CHECK_PTR_RETURN_VAL(activity, false);
    return activity->is_app_header_time_only;
}

void eos_activity_set_app_header_time_only_text_color(eos_activity_t *activity, lv_color_t color)
{
    EOS_CHECK_PTR_RETURN(activity);

    activity->app_header_time_only_text_color = color;

    _update_app_header_if_needed(activity);
}

lv_color_t eos_activity_get_app_header_time_only_text_color(eos_activity_t *activity)
{
    EOS_CHECK_PTR_RETURN_VAL(activity, EOS_COLOR_WHITE);
    return activity->app_header_time_only_text_color;
}

lv_color_t eos_activity_get_title_color(eos_activity_t *activity)
{
    EOS_CHECK_PTR_RETURN_VAL(activity, _DEFAULT_TITLE_COLOR);
    return activity->title.color;
}

void eos_activity_set_title_color(eos_activity_t *activity, lv_color_t color)
{
    EOS_CHECK_PTR_RETURN(activity);
    activity->title.color = color;

    _update_app_header_if_needed(activity);
}

lv_obj_t *eos_view_active(void)
{
    eos_activity_t *current = eos_activity_get_current();
    if (!current || !current->view || !lv_obj_is_valid(current->view))
    {
        return NULL;
    }
    return current->view;
}

eos_result_t eos_activity_controller_init(eos_activity_t *root_activity)
{
    EOS_CHECK_PTR_RETURN_VAL(root_activity, EOS_FAILED);

    if (_controller_initialized())
    {
        EOS_LOG_E("Activity controller already initialized, cannot reinitialize");
        return EOS_FAILED;
    }

    // Lazy initialization: create root_screen and stack if not exist
    // This allows eos_activity_create_root() to be called before controller_init()
    if (!_activity_ctx.root_screen)
    {
        if (lv_screen_active())
        {
            lv_obj_delete(lv_screen_active());
        }
        _activity_ctx.root_screen = lv_obj_create(NULL);
        lv_obj_set_scrollbar_mode(_activity_ctx.root_screen, LV_SCROLLBAR_MODE_OFF);
        lv_obj_remove_flag(_activity_ctx.root_screen, LV_OBJ_FLAG_SCROLLABLE);
        lv_screen_load(_activity_ctx.root_screen);
    }

    if (!_activity_ctx.activity_stack)
    {
        _activity_ctx.activity_stack =
            eos_stack_create_with_mode(_ACTIVITY_STACK_INIT_CAPACITY, EOS_STACK_CAPACITY_FIXED);
        if (!_activity_ctx.activity_stack)
        {
            if (_activity_ctx.root_screen)
            {
                lv_obj_delete(_activity_ctx.root_screen);
                _activity_ctx.root_screen = NULL;
            }
            return EOS_FAILED;
        }
    }

    // Set root Activity
    _activity_ctx.root_activity = root_activity;

    // Root activity must have view created by eos_activity_create_root()
    if (!root_activity->view)
    {
        EOS_LOG_E("controller_init: Root activity has no view (must use eos_activity_create_root)");
        _activity_controller_init_failed(root_activity);
        return EOS_FAILED;
    }

    if (root_activity->view)
    {
        if (lv_obj_is_valid(root_activity->view))
        {
            lv_obj_set_parent(root_activity->view, _activity_ctx.root_screen);
        }
        else
        {
            // View is invalid (dangling pointer), reset to NULL so on_enter can recreate
            EOS_LOG_W("controller_init: Detected invalid/dangling view pointer");
            root_activity->view = NULL;
        }
    }

    // Enter root Activity (this will call on_enter which creates the view)
    if (root_activity->lifecycle.on_enter)
    {
        root_activity->lifecycle.on_enter(root_activity);
        root_activity->has_started = true;
    }
    else
    {
        EOS_LOG_E("controller_init: Root activity has no on_enter callback!");
    }

    // Verify view was created by on_enter
    if (!root_activity->view || !lv_obj_is_valid(root_activity->view))
    {
        EOS_LOG_E("Root activity's on_enter failed to create a valid view (view=%p)", (void *)root_activity->view);
        _activity_controller_init_failed(root_activity);
        return EOS_FAILED;
    }
    _activity_show(root_activity);
    _activity_ctx.current_activity = root_activity;
    _activity_ctx.visible_activity = root_activity;
    _activity_ctx.transition_in_progress = false;

    if (root_activity->is_app_header_visible)
    {
        eos_app_header_show(root_activity);
    }
    else
    {
        eos_app_header_hide();
    }

    return EOS_OK;
}

eos_activity_t *eos_activity_create(const eos_activity_lifecycle_t *lifecycle)
{
    eos_activity_t *activity = eos_malloc_zeroed(sizeof(eos_activity_t));
    if (!activity)
    {
        return NULL;
    }

    if (_activity_ctx.root_screen)
    {
        activity->view = _view_create(_activity_ctx.root_screen);
        if (!activity->view)
        {
            eos_free(activity);
            return NULL;
        }
        eos_wdata_set(activity->view, EOS_WDATA_ACTIVITY, activity, NULL);

        activity->snap_container = _snap_container_create();
    }
    else
    {
        activity->view = NULL;
    }
    if (lifecycle)
    {
        activity->lifecycle = *lifecycle;
    }
    else
    {
        memset(&activity->lifecycle, 0, sizeof(activity->lifecycle));
    }
    activity->type = EOS_ACTIVITY_TYPE_APP;
    activity->snapshot_ref_count = 0;
    activity->is_app_header_visible = false;
    activity->is_app_header_time_only = false;
    activity->app_header_time_only_text_color = EOS_COLOR_WHITE;
    activity->destroy_on_exit = false;
    activity->has_started = false;
    activity->title.color = _DEFAULT_TITLE_COLOR;
    activity->title.type = _TITLE_TYPE_INVALID;
    activity->title.string = NULL;
    activity->user_data = NULL;

    return activity;
}

eos_activity_t *eos_activity_create_root(const eos_activity_lifecycle_t *lifecycle)
{
    // Lazy initialization: auto-create root_screen if not exists
    // This allows create_root() to be called before controller_init()
    if (!_activity_ctx.root_screen)
    {
        if (lv_screen_active())
        {
            lv_obj_delete(lv_screen_active());
        }
        _activity_ctx.root_screen = lv_obj_create(NULL);
        lv_obj_set_scrollbar_mode(_activity_ctx.root_screen, LV_SCROLLBAR_MODE_OFF);
        lv_obj_remove_flag(_activity_ctx.root_screen, LV_OBJ_FLAG_SCROLLABLE);
        lv_screen_load(_activity_ctx.root_screen);
    }

    eos_activity_t *activity = eos_malloc_zeroed(sizeof(eos_activity_t));
    if (!activity)
    {
        return NULL;
    }

    // Always create view immediately with standard style
    activity->view = _view_create(_activity_ctx.root_screen);
    if (!activity->view)
    {
        eos_free(activity);
        return NULL;
    }
    eos_wdata_set(activity->view, EOS_WDATA_ACTIVITY, activity, NULL);

    activity->snap_container = _snap_container_create();

    if (lifecycle)
    {
        activity->lifecycle = *lifecycle;
    }
    else
    {
        memset(&activity->lifecycle, 0, sizeof(activity->lifecycle));
    }
    activity->type = EOS_ACTIVITY_TYPE_WATCHFACE;
    activity->snapshot_ref_count = 0;
    activity->is_app_header_visible = false;
    activity->is_app_header_time_only = false;
    activity->app_header_time_only_text_color = EOS_COLOR_WHITE;
    activity->destroy_on_exit = false;
    activity->has_started = false;
    activity->title.color = _DEFAULT_TITLE_COLOR;
    activity->title.type = _TITLE_TYPE_INVALID;
    activity->title.string = NULL;
    activity->user_data = NULL;

    return activity;
}

void eos_activity_enter(eos_activity_t *activity)
{
    EOS_CHECK_PTR_RETURN(activity);
    EOS_LOG_I("Activity enter request: activity=%p[%s] current=%p[%s] visible=%p[%s] transition=%d",
              (void *)activity,
              _activity_type_to_str(activity->type),
              (void *)_activity_ctx.current_activity,
              _activity_type_to_str(_activity_ctx.current_activity ? _activity_ctx.current_activity->type
                                                                   : EOS_ACTIVITY_TYPE_NULL),
              (void *)_activity_ctx.visible_activity,
              _activity_type_to_str(_activity_ctx.visible_activity ? _activity_ctx.visible_activity->type
                                                                   : EOS_ACTIVITY_TYPE_NULL),
              _activity_ctx.transition_in_progress);
    if (!_controller_initialized())
    {
        return;
    }

    if (_activity_ctx.transition_in_progress)
    {
        EOS_LOG_W("Activity transition in progress");
        return;
    }

    if (_activity_ctx.current_activity == activity)
    {
        EOS_LOG_I("Activity enter is no-op because activity is already current");
        _activity_show(activity);
        return;
    }

    // Prevent entering root Activity through normal enter (use replace_root instead)
    if (activity == _activity_ctx.root_activity)
    {
        EOS_LOG_W("Use eos_activity_replace_root() to switch root Activity");
        return;
    }

    if (!eos_stack_push(_activity_ctx.activity_stack, activity))
    {
        return;
    }

    EOS_LOG_I("Activity enter pushed to stack: activity=%p[%s] stack_size=%zu",
              (void *)activity,
              _activity_type_to_str(activity->type),
              eos_stack_get_size(_activity_ctx.activity_stack));

    _activity_switch_to(activity, false);
}

eos_result_t eos_activity_back(void)
{
    if (!_controller_initialized())
    {
        return EOS_FAILED;
    }

    EOS_LOG_I(
        "Activity back request: current=%p[%s] visible=%p[%s] root=%p[%s] transition=%d stack_size=%zu",
        (void *)_activity_ctx.current_activity,
        _activity_type_to_str(_activity_ctx.current_activity ? _activity_ctx.current_activity->type
                                                             : EOS_ACTIVITY_TYPE_NULL),
        (void *)_activity_ctx.visible_activity,
        _activity_type_to_str(_activity_ctx.visible_activity ? _activity_ctx.visible_activity->type
                                                             : EOS_ACTIVITY_TYPE_NULL),
        (void *)_activity_ctx.root_activity,
        _activity_type_to_str(_activity_ctx.root_activity ? _activity_ctx.root_activity->type : EOS_ACTIVITY_TYPE_NULL),
        _activity_ctx.transition_in_progress,
        eos_stack_get_size(_activity_ctx.activity_stack));

    if (_activity_ctx.transition_in_progress)
    {
        EOS_LOG_W("Activity transition in progress");
        return EOS_FAILED;
    }

    if (_activity_ctx.current_activity != _activity_ctx.visible_activity)
    {
        EOS_LOG_W("Activity state mismatch");
        return EOS_FAILED;
    }

    // If stack is empty, we're at root - cannot go back further
    if (eos_stack_get_size(_activity_ctx.activity_stack) == 0)
    {
        if (_activity_ctx.current_activity == _activity_ctx.root_activity)
        {
            // Already at root, cannot back
            EOS_LOG_I("Activity back no-op: already at root");
            return EOS_FAILED;
        }
        // This shouldn't happen, but if current is not root and stack is empty, go to root
        _activity_ctx.current_activity->destroy_on_exit = true;
        EOS_LOG_W("Activity back fallback to root: current marked destroy_on_exit");
        _activity_switch_to(_activity_ctx.root_activity, true);
        return EOS_OK;
    }

    eos_activity_t *current = eos_stack_pop(_activity_ctx.activity_stack);
    EOS_CHECK_PTR_RETURN_VAL(current, EOS_FAILED);

    eos_activity_t *cur_activity = _activity_ctx.current_activity;
    cur_activity->destroy_on_exit = true;

    eos_activity_t *prev = NULL;
    if (eos_stack_get_size(_activity_ctx.activity_stack) == 0)
    {
        // Stack empty, return to root
        prev = _activity_ctx.root_activity;
    }
    else
    {
        prev = eos_stack_peek(_activity_ctx.activity_stack);
    }

    EOS_CHECK_PTR_RETURN_VAL(prev, EOS_FAILED);

    EOS_LOG_I("Activity back will destroy current after switch: current=%p[%s] prev=%p[%s]",
              (void *)cur_activity,
              _activity_type_to_str(cur_activity ? cur_activity->type : EOS_ACTIVITY_TYPE_NULL),
              (void *)prev,
              _activity_type_to_str(prev ? prev->type : EOS_ACTIVITY_TYPE_NULL));

    _activity_switch_to(prev, true);

    return EOS_OK;
}

eos_result_t eos_activity_back_to_watchface(void)
{
    if (!_controller_initialized())
    {
        return EOS_FAILED;
    }

    EOS_LOG_I(
        "Back-to-watchface request: current=%p[%s] visible=%p[%s] root=%p[%s] transition=%d stack_size=%zu",
        (void *)_activity_ctx.current_activity,
        _activity_type_to_str(_activity_ctx.current_activity ? _activity_ctx.current_activity->type
                                                             : EOS_ACTIVITY_TYPE_NULL),
        (void *)_activity_ctx.visible_activity,
        _activity_type_to_str(_activity_ctx.visible_activity ? _activity_ctx.visible_activity->type
                                                             : EOS_ACTIVITY_TYPE_NULL),
        (void *)_activity_ctx.root_activity,
        _activity_type_to_str(_activity_ctx.root_activity ? _activity_ctx.root_activity->type : EOS_ACTIVITY_TYPE_NULL),
        _activity_ctx.transition_in_progress,
        eos_stack_get_size(_activity_ctx.activity_stack));

    if (_activity_ctx.transition_in_progress)
    {
        EOS_LOG_W("Activity transition in progress");
        return EOS_FAILED;
    }

    eos_activity_t *root = _activity_ctx.root_activity;
    eos_activity_t *current = _activity_ctx.current_activity;
    if (!root || !current)
    {
        return EOS_FAILED;
    }

    if (current == root)
    {
        // Already at root
        return EOS_OK;
    }

    // Destroy stacked activities, but keep current alive until _activity_switch_to()
    // finishes any transition and destroys it through the normal path.
    while (eos_stack_get_size(_activity_ctx.activity_stack) > 0)
    {
        eos_activity_t *activity = eos_stack_pop(_activity_ctx.activity_stack);
        if (!activity)
        {
            continue;
        }

        EOS_LOG_I("Back-to-watchface popped stack activity=%p[%s] current=%p[%s]",
                  (void *)activity,
                  _activity_type_to_str(activity->type),
                  (void *)current,
                  _activity_type_to_str(current->type));

        if (activity != current)
        {
            _activity_run_destroy(activity);
        }
    }

    // Mark current activity for destruction
    current->destroy_on_exit = true;
    EOS_LOG_I("Back-to-watchface marked current for destroy: current=%p[%s]",
              (void *)current,
              _activity_type_to_str(current->type));

    // Switch to root (will call on_resume)
    _activity_switch_to(root, true);
    return EOS_OK;
}

void eos_activity_back_cb(lv_event_t *e)
{
    eos_activity_back();
}

eos_activity_t *eos_activity_get_current(void)
{
    if (!_controller_initialized())
    {
        return NULL;
    }
    if (_activity_ctx.current_activity)
        return _activity_ctx.current_activity;
    if (_activity_ctx.root_activity)
        return _activity_ctx.root_activity;
    return NULL;
}

eos_activity_t *eos_activity_get_visible(void)
{
    if (!_controller_initialized())
    {
        return NULL;
    }
    if (_activity_ctx.visible_activity)
        return _activity_ctx.visible_activity;
    return eos_activity_get_current();
}

eos_activity_t *eos_activity_get_previous(void)
{
    if (!_controller_initialized())
    {
        return NULL;
    }
    return _activity_ctx.previous_activity;
}

bool eos_activity_is_transition_in_progress(void)
{
    return _activity_ctx.transition_in_progress;
}

eos_activity_t *eos_activity_get_bottom(void)
{
    if (!_controller_initialized())
    {
        return NULL;
    }

    // If stack has items, return bottom of stack
    if (eos_stack_get_size(_activity_ctx.activity_stack) > 0)
    {
        return eos_stack_peek(_activity_ctx.activity_stack);
    }

    // Stack empty, return root activity
    return _activity_ctx.root_activity;
}
