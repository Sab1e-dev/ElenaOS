/**
 * @file eos_event.c
 * @brief Event broadcast system - global broadcast using event ID as index
 */

#include "eos_event.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include "lvgl.h"
#define EOS_LOG_DISABLE
#define EOS_LOG_TAG "EventBus"
#include "eos_log.h"
#include "eos_port.h"
#include "eos_config.h"
#include "eos_mem.h"

/* Macros and Definitions -------------------------------------*/

/**
 * @brief Event structure (private implementation)
 */
struct _eos_event_t
{
    void *user_data;
    void *param;
    lv_obj_t *obj;
};

/**
 * @brief Event callback node structure
 */
typedef struct _event_node_t
{
    eos_event_code_t event_id;
    eos_event_cb_t cb;
    void *user_data;
    lv_obj_t *obj;
    struct _event_node_t *next;
    bool marked_for_delete;
} event_node_t;

/* Variables --------------------------------------------------*/
static event_node_t *_event_list_head = NULL;
static int _broadcast_depth = 0;
static bool _event_list_modified = false;
static eos_event_code_t _next_event_id = EOS_EVENT_LAST;

/* Function Implementations -----------------------------------*/

#if EOS_COMPIPILE_MODE == EOS_DEBUG
void _event_list_show(void)
{
    event_node_t *curr = _event_list_head;
    EOS_LOG_I("Event bus list:");
    while (curr)
    {
        EOS_LOG_I(" current: [%p] event[%d] cb[%p] marked[%d]",
                  curr,
                  (int)curr->event_id,
                  (void *)curr->cb,
                  curr->marked_for_delete ? 1 : 0);
        curr = curr->next;
    }
}
#endif

/**
 * @brief Mark matching nodes for deletion, deferred cleanup
 */
static void _mark_node_deleted_by_predicate(bool (*pred)(event_node_t *, void *), void *ctx)
{
    for (event_node_t *n = _event_list_head; n; n = n->next)
    {
        if (pred(n, ctx))
        {
            if (!n->marked_for_delete)
            {
                n->marked_for_delete = true;
                _event_list_modified = true;
                EOS_LOG_D("Marked node [%p] event[%d] cb[%p] for deletion", n, (int)n->event_id, (void *)n->cb);
            }
        }
    }
}

/**
 * @brief Immediately cleanup marked nodes; can also be called during non-broadcast to release immediately
 */
static void _cleanup_deleted_nodes(void)
{
    event_node_t **curr = &_event_list_head;
    while (*curr)
    {
        if ((*curr)->marked_for_delete)
        {
            event_node_t *tmp = *curr;
            *curr = (*curr)->next;

            EOS_LOG_D("Freeing node [%p] event[%d] cb[%p]", tmp, (int)tmp->event_id, (void *)tmp->cb);

            eos_free(tmp);
            _event_list_modified = true;
        }
        else
        {
            curr = &(*curr)->next;
        }
    }
}

static bool _pred_match_event_cb(event_node_t *n, void *ctx)
{
    struct
    {
        uint32_t event_id;
        eos_event_cb_t cb;
    } *params = ctx;

    return (n->event_id == params->event_id && n->cb == params->cb);
}

static bool _pred_match_cb(event_node_t *n, void *ctx)
{
    eos_event_cb_t cb = ctx;
    return (n->cb == cb);
}

static bool _pred_match_event_cb_user_data(event_node_t *n, void *ctx)
{
    struct
    {
        uint32_t event_id;
        eos_event_cb_t cb;
        void *user_data;
    } *params = ctx;

    return (n->event_id == params->event_id && n->cb == params->cb && n->user_data == params->user_data);
}

eos_event_code_t eos_event_register_id(void)
{
    if (_next_event_id >= EOS_EVENT_MAX)
    {
        EOS_LOG_E("Failed to register event ID - no more IDs available");
        return EOS_EVENT_UNKNOWN;
    }

    uint32_t new_id = _next_event_id++;
    EOS_LOG_I("Registered new event ID: %d", new_id);
    return new_id;
}

void eos_event_subscribe(eos_event_code_t event_id, eos_event_cb_t cb, void *user_data)
{
    eos_event_subscribe_ex(event_id, cb, user_data, NULL);
}

void eos_event_subscribe_ex(eos_event_code_t event_id, eos_event_cb_t cb, void *user_data, lv_obj_t *obj)
{
    EOS_CHECK_PTR_RETURN(cb);
    EOS_LOG_I("Subscribe event %d, callback: [%p], obj: [%p]", (int)event_id, (void *)cb, (void *)obj);

    event_node_t *new_node = eos_malloc(sizeof(event_node_t));
    if (!new_node)
    {
        EOS_LOG_E("Failed to allocate event node");
        return;
    }

    new_node->event_id = event_id;
    new_node->cb = cb;
    new_node->user_data = user_data;
    new_node->obj = obj;
    new_node->next = NULL;
    new_node->marked_for_delete = false;

    new_node->next = _event_list_head;
    _event_list_head = new_node;

    EOS_LOG_D("Event callback subscribed successfully");
}

static void eos_event_unsubscribe_ex(eos_event_code_t event_id, eos_event_cb_t cb, void *user_data)
{
    EOS_LOG_I("Unsubscribe: event[%d] cb[%p] user_data[%p] broadcasting=%d",
              (int)event_id,
              (void *)cb,
              user_data,
              _broadcast_depth);

    event_node_t **curr = &_event_list_head;
    bool removed = false;

    while (*curr)
    {
        event_node_t *n = *curr;
        bool match = (n->event_id == event_id && n->cb == cb);

        if (match && user_data != NULL && n->user_data != user_data)
            match = false;

        if (match)
        {
            removed = true;
            if (_broadcast_depth > 0)
            {
                n->marked_for_delete = true;
                _event_list_modified = true;
                curr = &(*curr)->next;
            }
            else
            {
                *curr = n->next;
                eos_free(n);
                continue;
            }
        }
        else
        {
            curr = &(*curr)->next;
        }
    }

    if (!removed)
    {
        EOS_LOG_W("Callback not found for removal");
    }
}

void eos_event_unsubscribe(eos_event_code_t event_id, eos_event_cb_t cb)
{
    eos_event_unsubscribe_ex(event_id, cb, NULL);
}

void eos_event_unsubscribe_with_user_data(eos_event_code_t event_id, eos_event_cb_t cb, void *user_data)
{
    eos_event_unsubscribe_ex(event_id, cb, user_data);
}

void eos_event_unsubscribe_all(eos_event_cb_t cb)
{
    EOS_CHECK_PTR_RETURN(cb);
    EOS_LOG_I("Unsubscribe all events for cb[%p]", (void *)cb);

    if (_broadcast_depth > 0)
    {
        _mark_node_deleted_by_predicate(_pred_match_cb, cb);
    }
    else
    {
        _mark_node_deleted_by_predicate(_pred_match_cb, cb);
        _cleanup_deleted_nodes();
    }
}

void eos_event_unsubscribe_with_obj(eos_event_code_t event_id, eos_event_cb_t cb, lv_obj_t *obj)
{
    EOS_LOG_I("Unsubscribe with obj: event[%d] cb[%p] obj[%p] broadcasting=%d",
              (int)event_id,
              (void *)cb,
              (void *)obj,
              _broadcast_depth);

    event_node_t **curr = &_event_list_head;
    bool removed = false;

    while (*curr)
    {
        event_node_t *n = *curr;
        bool match = (n->event_id == event_id && n->cb == cb && n->obj == obj);

        if (match)
        {
            removed = true;
            if (_broadcast_depth > 0)
            {
                n->marked_for_delete = true;
                _event_list_modified = true;
                curr = &(*curr)->next;
            }
            else
            {
                *curr = n->next;
                eos_free(n);
                continue;
            }
        }
        else
        {
            curr = &(*curr)->next;
        }
    }

    if (!removed)
    {
        EOS_LOG_W("Callback not found for removal with obj");
    }
}

void eos_event_post(eos_event_code_t event_id, void *param, lv_obj_t *obj)
{
    EOS_LOG_I("Post event: [%d] (begin) depth=%d", (int)event_id, _broadcast_depth + 1);
    _broadcast_depth++;
    bool local_list_was_modified = false;
    event_node_t *curr = _event_list_head;

    while (curr)
    {
        event_node_t *next = curr->next;

        EOS_LOG_D("Post visiting node [%p] event[%d] marked[%d]",
                  curr,
                  (int)curr->event_id,
                  curr->marked_for_delete ? 1 : 0);

        if (!curr->marked_for_delete && curr->event_id == event_id)
        {
            EOS_LOG_D("Calling callback [%p] for event [%d]", (void *)curr->cb, (int)event_id);

            eos_event_t e;
            e.user_data = curr->user_data;
            e.param = param;
            e.obj = curr->obj;

            curr->cb(&e);

            if (_event_list_modified)
                local_list_was_modified = true;
        }

        curr = next;
    }

    _broadcast_depth--;

    if (_broadcast_depth == 0)
    {
        if (_event_list_modified || local_list_was_modified)
        {
            EOS_LOG_D("Cleaning up marked nodes after post");
            _cleanup_deleted_nodes();
            _event_list_modified = false;
        }
    }

    EOS_LOG_I("Post event: [%d] (end) depth=%d", (int)event_id, _broadcast_depth);
}

void eos_event_cleanup_now(void)
{
    if (_broadcast_depth > 0)
    {
        EOS_LOG_W("Cleanup requested during post; marking only.");
        _event_list_modified = true;
        return;
    }
    _cleanup_deleted_nodes();
    _event_list_modified = false;
}

void *eos_event_get_user_data(eos_event_t *e)
{
    if (!e)
        return NULL;
    return e->user_data;
}

void *eos_event_get_param(eos_event_t *e)
{
    if (!e)
        return NULL;
    return e->param;
}

lv_obj_t *eos_event_get_obj(eos_event_t *e)
{
    if (!e)
        return NULL;
    return e->obj;
}
