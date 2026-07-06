/**
 * @file sni_api_lv_image.c
 * @brief LVGL image SNI special wrappers
 */

#include "sni_api_lv_special.h"

/* Includes ---------------------------------------------------*/
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include "eos_image.h"
#include "eos_mem.h"
#include "eos_port.h"
#include "lvgl.h"
#include "script_engine_core.h"
#include "sni_api_export.h"
#include "sni_type_bridge.h"
#include "sni_types.h"
#include "eos_log.h"
#include "eos_watchface.h"
#include "eos_app.h"
#include "eos_service_storage.h"

/* Function Implementations -----------------------------------*/

static bool sni_image_get_script_root(char *out_path, size_t out_size)
{
    const char *script_id = script_engine_get_current_script_id();

    if (!out_path || out_size == 0 || !script_id || script_id[0] == '\0')
    {
        return false;
    }

    if (script_engine_get_current_script_type() == SCRIPT_TYPE_APPLICATION)
    {
        snprintf(out_path, out_size, EOS_APP_INSTALLED_DIR "%s", script_id);
        return true;
    }

    if (script_engine_get_current_script_type() == SCRIPT_TYPE_WATCHFACE)
    {
        snprintf(out_path, out_size, EOS_WATCHFACE_INSTALLED_DIR "%s", script_id);
        return true;
    }

    return false;
}

static bool sni_image_is_within_root(const char *root_path, const char *candidate_path)
{
    size_t root_len;

    if (!root_path || !candidate_path)
    {
        return false;
    }

    root_len = strlen(root_path);
    if (strncmp(root_path, candidate_path, root_len) != 0)
    {
        return false;
    }

    return candidate_path[root_len] == '\0' || candidate_path[root_len] == '/';
}

static bool sni_image_has_parent_ref(const char *src)
{
    const char *cursor = src;

    while (cursor && *cursor)
    {
        const char *slash = strchr(cursor, '/');
        size_t len = slash ? (size_t)(slash - cursor) : strlen(cursor);

        if (len == 2 && cursor[0] == '.' && cursor[1] == '.')
        {
            return true;
        }

        if (!slash)
        {
            break;
        }
        cursor = slash + 1;
    }

    return false;
}

static char *sni_image_dup_string(const char *src)
{
    char *ret;

    if (!src)
    {
        return NULL;
    }

    ret = eos_malloc(strlen(src) + 1);
    if (!ret)
    {
        return NULL;
    }

    strcpy(ret, src);
    return ret;
}

static bool sni_image_normalize_absolute_path(const char *input, char *output, size_t output_size)
{
    size_t in_idx = 0;
    size_t out_len = 0;

    if (!input || !output || output_size < 2)
    {
        return false;
    }

    if (input[0] != '/')
    {
        return false;
    }

    output[0] = '/';
    out_len = 1;

    while (input[in_idx] != '\0')
    {
        size_t seg_start;
        size_t seg_len;

        while (input[in_idx] == '/')
        {
            in_idx++;
        }

        if (input[in_idx] == '\0')
        {
            break;
        }

        seg_start = in_idx;
        while (input[in_idx] != '\0' && input[in_idx] != '/')
        {
            in_idx++;
        }

        seg_len = in_idx - seg_start;
        if (seg_len == 0 || (seg_len == 1 && input[seg_start] == '.'))
        {
            continue;
        }

        if (seg_len == 2 && input[seg_start] == '.' && input[seg_start + 1] == '.')
        {
            if (out_len > 1)
            {
                out_len--;
                while (out_len > 0 && output[out_len - 1] != '/')
                {
                    out_len--;
                }
            }
            continue;
        }

        if (out_len > 1)
        {
            if (out_len + 1 >= output_size)
            {
                return false;
            }
            output[out_len++] = '/';
        }

        if (out_len + seg_len >= output_size)
        {
            return false;
        }

        memcpy(output + out_len, input + seg_start, seg_len);
        out_len += seg_len;
    }

    output[out_len] = '\0';
    return true;
}

static char *sni_image_resolve_under_root(const char *root_dir, const char *candidate)
{
    char root_real[EOS_FS_PATH_MAX];
    char candidate_real[EOS_FS_PATH_MAX];

    if (!root_dir || !candidate)
    {
        return NULL;
    }

    if (!sni_image_normalize_absolute_path(root_dir, root_real, sizeof(root_real))
        || !sni_image_normalize_absolute_path(candidate, candidate_real, sizeof(candidate_real)))
    {
        return NULL;
    }

    if (!eos_storage_is_file(candidate_real))
    {
        return NULL;
    }

    if (!sni_image_is_within_root(root_real, candidate_real))
    {
        EOS_LOG_W("Reject image src outside script root: %s", candidate_real);
        return NULL;
    }

    return sni_image_dup_string(candidate_real);
}

static char *sni_image_resolve_asset_path(const char *src)
{
    char root_dir[EOS_FS_PATH_MAX];
    char candidate[EOS_FS_PATH_MAX];

    if (!src || src[0] == '\0')
    {
        return NULL;
    }

    if (!sni_image_get_script_root(root_dir, sizeof(root_dir)))
    {
        return NULL;
    }

    if (sni_image_has_parent_ref(src))
    {
        EOS_LOG_W("Reject image src with parent traversal: %s", src);
        return NULL;
    }

    if (src[0] == '/')
    {
        return sni_image_resolve_under_root(root_dir, src);
    }

    if (strchr(src, '/'))
    {
        snprintf(candidate, sizeof(candidate), "%s/%s", root_dir, src);
    }
    else
    {
        snprintf(candidate, sizeof(candidate), "%s/assets/%s", root_dir, src);
    }

    return sni_image_resolve_under_root(root_dir, candidate);
}

typedef enum
{
    SNI_IMAGEBUTTON_SRC_LEFT = 0,
    SNI_IMAGEBUTTON_SRC_MIDDLE = 1,
    SNI_IMAGEBUTTON_SRC_RIGHT = 2,
} sni_imagebutton_src_slot_t;

typedef struct sni_imagebutton_src_store
{
    lv_obj_t *obj;
    char *paths[LV_IMAGEBUTTON_STATE_NUM][3];
    struct sni_imagebutton_src_store *next;
} sni_imagebutton_src_store_t;

static sni_imagebutton_src_store_t *sni_imagebutton_src_store_head = NULL;

static sni_imagebutton_src_store_t *sni_imagebutton_find_store(lv_obj_t *obj)
{
    sni_imagebutton_src_store_t *cursor = sni_imagebutton_src_store_head;

    while (cursor)
    {
        if (cursor->obj == obj)
            return cursor;
        cursor = cursor->next;
    }

    return NULL;
}

static void sni_imagebutton_free_store(sni_imagebutton_src_store_t *store)
{
    int state;
    int slot;

    if (!store)
        return;

    for (state = 0; state < LV_IMAGEBUTTON_STATE_NUM; state++)
    {
        for (slot = 0; slot < 3; slot++)
        {
            if (store->paths[state][slot])
            {
                eos_free(store->paths[state][slot]);
                store->paths[state][slot] = NULL;
            }
        }
    }

    eos_free(store);
}

static void sni_imagebutton_detach_store(lv_obj_t *obj)
{
    sni_imagebutton_src_store_t *cursor = sni_imagebutton_src_store_head;
    sni_imagebutton_src_store_t *prev = NULL;

    while (cursor)
    {
        if (cursor->obj == obj)
        {
            if (prev)
                prev->next = cursor->next;
            else
                sni_imagebutton_src_store_head = cursor->next;

            sni_imagebutton_free_store(cursor);
            return;
        }
        prev = cursor;
        cursor = cursor->next;
    }
}

static void sni_imagebutton_delete_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    sni_imagebutton_detach_store(obj);
}

static sni_imagebutton_src_store_t *sni_imagebutton_ensure_store(lv_obj_t *obj)
{
    sni_imagebutton_src_store_t *store = sni_imagebutton_find_store(obj);

    if (store)
        return store;

    store = eos_malloc_zeroed(sizeof(*store));
    if (!store)
        return NULL;

    store->obj = obj;
    store->next = sni_imagebutton_src_store_head;
    sni_imagebutton_src_store_head = store;
    lv_obj_add_event_cb(obj, sni_imagebutton_delete_cb, LV_EVENT_DELETE, NULL);
    return store;
}

static void sni_imagebutton_replace_owned_src(lv_obj_t *obj,
                                              lv_imagebutton_state_t state,
                                              sni_imagebutton_src_slot_t slot,
                                              char *owned_path)
{
    sni_imagebutton_src_store_t *store;

    if (state < 0 || state >= LV_IMAGEBUTTON_STATE_NUM)
    {
        if (owned_path)
            eos_free(owned_path);
        return;
    }

    store = sni_imagebutton_find_store(obj);
    if (!store && !owned_path)
        return;

    if (!store)
        store = sni_imagebutton_ensure_store(obj);

    if (!store)
    {
        if (owned_path)
            eos_free(owned_path);
        return;
    }

    if (store->paths[state][slot])
    {
        eos_free(store->paths[state][slot]);
        store->paths[state][slot] = NULL;
    }

    store->paths[state][slot] = owned_path;
}

static bool sni_image_js_to_src(const jerry_value_t value, const void **out_src, char **out_owned_path)
{
    if (!out_src || !out_owned_path)
        return false;

    *out_src = NULL;
    *out_owned_path = NULL;

    if (jerry_value_is_null(value))
        return true;

    if (jerry_value_is_string(value))
    {
        const char *raw_src = sni_tb_js2c_string(value);

        if (!raw_src)
            return false;

        *out_owned_path = sni_image_resolve_asset_path(raw_src);
        eos_free((void *)raw_src);

        if (!*out_owned_path)
            return false;

        *out_src = *out_owned_path;
        return true;
    }

    if (jerry_value_is_object(value))
    {
        lv_image_dsc_t *arg_src_dsc = NULL;
        if (sni_tb_js2c(value, SNI_H_LV_IMAGE_DSC, &arg_src_dsc))
        {
            *out_src = arg_src_dsc;
            return true;
        }

        const void *arg_src_ptr = NULL;
        sni_type_t actual_type = SNI_T_UNKNOWN;
        if (sni_tb_js2c_any_handle(value, &arg_src_ptr, &actual_type))
        {
            *out_src = arg_src_ptr;
            return true;
        }
    }

    return false;
}

jerry_value_t sni_api_lv_image_set_src(const jerry_call_info_t *call_info_p,
                                       const jerry_value_t args_p[],
                                       const jerry_length_t args_count)
{
    lv_obj_t *self_obj = NULL;

    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    if (!sni_tb_js2c(call_info_p->this_value, SNI_H_LV_OBJ, &self_obj))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (jerry_value_is_null(args_p[0]))
    {
        lv_image_set_src(self_obj, NULL);
        return jerry_undefined();
    }

    if (jerry_value_is_string(args_p[0]))
    {
        const char *raw_src = sni_tb_js2c_string(args_p[0]);
        char *resolved_path = NULL;

        if (!raw_src)
        {
            return sni_api_throw_error("Out of memory");
        }

        resolved_path = sni_image_resolve_asset_path(raw_src);
        if (resolved_path)
        {
            lv_image_set_src(self_obj, resolved_path);
            eos_free(resolved_path);
        }
        else
        {
            eos_free((void *)raw_src);
            return sni_api_throw_error("Image src must stay within current script directory");
        }

        eos_free((void *)raw_src);
        return jerry_undefined();
    }

    if (jerry_value_is_object(args_p[0]))
    {
        lv_image_dsc_t *arg_src_dsc = NULL;
        if (sni_tb_js2c(args_p[0], SNI_H_LV_IMAGE_DSC, &arg_src_dsc))
        {
            lv_image_set_src(self_obj, arg_src_dsc);
            return jerry_undefined();
        }

        /* Fallback for wrappers that expose descriptors as a different handle type. */
        const void *arg_src_ptr = NULL;
        sni_type_t actual_type = SNI_T_UNKNOWN;
        if (sni_tb_js2c_any_handle(args_p[0], &arg_src_ptr, &actual_type))
        {
            EOS_LOG_W("lv_image_set_src fallback handle type=%d ptr=%p", actual_type, arg_src_ptr);
            lv_image_set_src(self_obj, arg_src_ptr);
            return jerry_undefined();
        }

        return sni_api_throw_error("Failed to convert argument");
    }

    return sni_api_throw_error("Invalid argument type");
}

jerry_value_t sni_api_prop_set_image_src(const jerry_call_info_t *call_info_p,
                                         const jerry_value_t args_p[],
                                         const jerry_length_t args_count)
{
    return sni_api_lv_image_set_src(call_info_p, args_p, args_count);
}

jerry_value_t sni_api_lv_imagebutton_set_src(const jerry_call_info_t *call_info_p,
                                             const jerry_value_t args_p[],
                                             const jerry_length_t args_count)
{
    lv_obj_t *self_obj = NULL;
    lv_imagebutton_state_t state;
    const void *src_left = NULL;
    const void *src_mid = NULL;
    const void *src_right = NULL;
    char *owned_left = NULL;
    char *owned_mid = NULL;
    char *owned_right = NULL;

    if (args_count != 4)
        return sni_api_throw_error("Invalid argument count");

    if (!sni_tb_js2c(call_info_p->this_value, SNI_H_LV_OBJ, &self_obj))
        return sni_api_throw_error("Failed to convert argument");

    if (!jerry_value_is_number(args_p[0]))
        return sni_api_throw_error("Invalid argument type");

    state = sni_tb_js2c_int32(args_p[0]);
    if (state < 0 || state >= LV_IMAGEBUTTON_STATE_NUM)
        return sni_api_throw_error("Invalid imagebutton state");

    if (!sni_image_js_to_src(args_p[1], &src_left, &owned_left) || !sni_image_js_to_src(args_p[2], &src_mid, &owned_mid)
        || !sni_image_js_to_src(args_p[3], &src_right, &owned_right))
    {
        if (owned_left)
            eos_free(owned_left);
        if (owned_mid)
            eos_free(owned_mid);
        if (owned_right)
            eos_free(owned_right);
        return sni_api_throw_error("Failed to convert argument");
    }

    if ((owned_left || owned_mid || owned_right) && !sni_imagebutton_ensure_store(self_obj))
    {
        if (owned_left)
            eos_free(owned_left);
        if (owned_mid)
            eos_free(owned_mid);
        if (owned_right)
            eos_free(owned_right);
        return sni_api_throw_error("Out of memory");
    }

    sni_imagebutton_replace_owned_src(self_obj, state, SNI_IMAGEBUTTON_SRC_LEFT, owned_left);
    sni_imagebutton_replace_owned_src(self_obj, state, SNI_IMAGEBUTTON_SRC_MIDDLE, owned_mid);
    sni_imagebutton_replace_owned_src(self_obj, state, SNI_IMAGEBUTTON_SRC_RIGHT, owned_right);

    lv_imagebutton_set_src(self_obj, state, src_left, src_mid, src_right);
    return jerry_undefined();
}

static jerry_value_t sni_api_lv_imagebutton_get_src_common(const jerry_call_info_t *call_info_p,
                                                           const jerry_value_t args_p[],
                                                           const jerry_length_t args_count,
                                                           const void *(*getter)(lv_obj_t *, lv_imagebutton_state_t))
{
    lv_obj_t *self_obj = NULL;
    lv_imagebutton_state_t state;
    const void *result;

    if (args_count != 1)
        return sni_api_throw_error("Invalid argument count");

    if (!sni_tb_js2c(call_info_p->this_value, SNI_H_LV_OBJ, &self_obj))
        return sni_api_throw_error("Failed to convert argument");

    if (!jerry_value_is_number(args_p[0]))
        return sni_api_throw_error("Invalid argument type");

    state = sni_tb_js2c_int32(args_p[0]);
    if (state < 0 || state >= LV_IMAGEBUTTON_STATE_NUM)
        return sni_api_throw_error("Invalid imagebutton state");

    result = getter(self_obj, state);
    return sni_tb_c2js(&result, SNI_T_PTR);
}

jerry_value_t sni_api_lv_imagebutton_get_src_left(const jerry_call_info_t *call_info_p,
                                                  const jerry_value_t args_p[],
                                                  const jerry_length_t args_count)
{
    return sni_api_lv_imagebutton_get_src_common(call_info_p, args_p, args_count, lv_imagebutton_get_src_left);
}

jerry_value_t sni_api_lv_imagebutton_get_src_middle(const jerry_call_info_t *call_info_p,
                                                    const jerry_value_t args_p[],
                                                    const jerry_length_t args_count)
{
    return sni_api_lv_imagebutton_get_src_common(call_info_p, args_p, args_count, lv_imagebutton_get_src_middle);
}

jerry_value_t sni_api_lv_imagebutton_get_src_right(const jerry_call_info_t *call_info_p,
                                                   const jerry_value_t args_p[],
                                                   const jerry_length_t args_count)
{
    return sni_api_lv_imagebutton_get_src_common(call_info_p, args_p, args_count, lv_imagebutton_get_src_right);
}
