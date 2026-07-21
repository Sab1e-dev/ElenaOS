/**
 * @file eos_widget_data.c
 * @brief Typed per-widget framework data registry implementation
 */
#include "eos_widget_data.h"

/* Includes ---------------------------------------------------*/
#include <string.h>
#include "eos_mem.h"
#define EOS_LOG_TAG "WData"
#include "eos_log.h"

/* Internal types ---------------------------------------------*/

#define EOS_WDATA_MAGIC 0x57445441U /* "WDTA" */

typedef struct eos_wdata_entry_t
{
    eos_widget_data_type_t type;
    void *data;
    eos_wdata_dtor_t dtor;
    struct eos_wdata_entry_t *next;
} eos_wdata_entry_t;

typedef struct
{
    uint32_t magic;
    eos_wdata_entry_t *head;
    bool registered;
} eos_wdata_container_t;

/* Internal helpers -------------------------------------------*/

static void _wdata_delete_cb(lv_event_t *e);

static eos_wdata_container_t *_wdata_get_container(lv_obj_t *obj)
{
    if (!obj || !lv_obj_is_valid(obj))
        return NULL;

    void *raw = lv_obj_get_user_data(obj);
    if (!raw)
        return NULL;

    eos_wdata_container_t *ctr = (eos_wdata_container_t *)raw;
    if (ctr->magic != EOS_WDATA_MAGIC)
        return NULL;

    return ctr;
}

static eos_wdata_container_t *_wdata_ensure_container(lv_obj_t *obj)
{
    eos_wdata_container_t *ctr = _wdata_get_container(obj);
    if (ctr)
        return ctr;

    ctr = (eos_wdata_container_t *)eos_malloc_zeroed(sizeof(eos_wdata_container_t));
    if (!ctr)
    {
        EOS_LOG_E("Failed to allocate widget data container for %p", (void *)obj);
        return NULL;
    }

    ctr->magic = EOS_WDATA_MAGIC;
    lv_obj_set_user_data(obj, ctr);
    lv_obj_add_event_cb(obj, _wdata_delete_cb, LV_EVENT_DELETE, NULL);
    ctr->registered = true;

    return ctr;
}

static void _wdata_delete_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    if (!obj)
        return;

    void *raw = lv_obj_get_user_data(obj);
    if (!raw)
        return;

    eos_wdata_container_t *ctr = (eos_wdata_container_t *)raw;
    if (ctr->magic != EOS_WDATA_MAGIC)
        return;

    eos_wdata_entry_t *entry = ctr->head;
    while (entry)
    {
        eos_wdata_entry_t *next = entry->next;
        if (entry->dtor && entry->data)
        {
            entry->dtor(entry->data);
        }
        eos_free(entry);
        entry = next;
    }

    lv_obj_set_user_data(obj, NULL);
    eos_free(ctr);
}

/* Public API -------------------------------------------------*/

void eos_wdata_set(lv_obj_t *obj, eos_widget_data_type_t type, void *data, eos_wdata_dtor_t dtor)
{
    if (!obj || !lv_obj_is_valid(obj))
        return;

    eos_wdata_container_t *ctr = _wdata_ensure_container(obj);
    if (!ctr)
        return;

    eos_wdata_entry_t *entry = ctr->head;
    while (entry)
    {
        if (entry->type == type)
        {
            entry->data = data;
            entry->dtor = dtor;
            return;
        }
        entry = entry->next;
    }

    entry = (eos_wdata_entry_t *)eos_malloc_zeroed(sizeof(eos_wdata_entry_t));
    if (!entry)
    {
        EOS_LOG_E("Failed to allocate widget data entry for %p type %d", (void *)obj, (int)type);
        return;
    }

    entry->type = type;
    entry->data = data;
    entry->dtor = dtor;

    if (ctr->head)
    {
        entry->next = ctr->head;
    }
    ctr->head = entry;
}

void *eos_wdata_get(lv_obj_t *obj, eos_widget_data_type_t type)
{
    eos_wdata_container_t *ctr = _wdata_get_container(obj);
    if (!ctr)
        return NULL;

    eos_wdata_entry_t *entry = ctr->head;
    while (entry)
    {
        if (entry->type == type)
            return entry->data;
        entry = entry->next;
    }

    return NULL;
}

void eos_wdata_remove(lv_obj_t *obj, eos_widget_data_type_t type)
{
    eos_wdata_container_t *ctr = _wdata_get_container(obj);
    if (!ctr)
        return;

    eos_wdata_entry_t *prev = NULL;
    eos_wdata_entry_t *entry = ctr->head;
    while (entry)
    {
        if (entry->type == type)
        {
            if (prev)
                prev->next = entry->next;
            else
                ctr->head = entry->next;

            if (entry->dtor && entry->data)
                entry->dtor(entry->data);

            eos_free(entry);

            if (!ctr->head)
            {
                lv_obj_remove_event_cb(obj, _wdata_delete_cb);
                lv_obj_set_user_data(obj, NULL);
                eos_free(ctr);
            }

            return;
        }
        prev = entry;
        entry = entry->next;
    }
}

void eos_wdata_clear(lv_obj_t *obj)
{
    eos_wdata_container_t *ctr = _wdata_get_container(obj);
    if (!ctr)
        return;

    eos_wdata_entry_t *entry = ctr->head;
    while (entry)
    {
        eos_wdata_entry_t *next = entry->next;
        if (entry->dtor && entry->data)
            entry->dtor(entry->data);
        eos_free(entry);
        entry = next;
    }

    ctr->head = NULL;
    lv_obj_remove_event_cb(obj, _wdata_delete_cb);
    lv_obj_set_user_data(obj, NULL);
    eos_free(ctr);
}
