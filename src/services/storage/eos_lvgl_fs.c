/**
 * @file eos_lvgl_fs.c
 * @brief LVGL file system interface implementation using ElenixOS storage service
 */

#include "eos_lvgl_fs.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "eos_config.h"
#include "eos_log.h"
#include "eos_service_storage.h"
#include "eos_mem.h"

/* Macros and Definitions -------------------------------------*/

#if !defined(EOS_LVGL_FS_LETTER)
    #error "EOS_LVGL_FS_LETTER is not defined in eos_config.h"
#endif

/**
 * LVGL FS must route to ElenixOS storage backend.
 * Ensure the default drive letter matches EOS driver.
 *
 * Note:
 *   Applications should always use POSIX-style paths ("/xxx").
 */
#if LV_FS_DEFAULT_DRIVE_LETTER != EOS_LVGL_FS_LETTER
#error "LV_FS_DEFAULT_DRIVE_LETTER must match EOS_LVGL_FS_LETTER"
#endif

#define LVGL_FS_MAX_PATH EOS_FS_PATH_MAX

typedef struct
{
    eos_file_t file_handle;
    eos_dir_t dir_handle;
    uint8_t type; // 0: file, 1: directory
} eos_lvgl_fs_handle_t;

/* Variables --------------------------------------------------*/
static lv_fs_drv_t fs_drv = {0};

/* Function Implementations -----------------------------------*/

static void *_drv_open_cb(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    (void)drv; // unused

    if (!path)
        return NULL;

    eos_lvgl_fs_handle_t *handle = (eos_lvgl_fs_handle_t *)eos_malloc(sizeof(eos_lvgl_fs_handle_t));
    if (!handle)
        return NULL;

    memset(handle, 0, sizeof(eos_lvgl_fs_handle_t));
    handle->type = 0; // file

    eos_file_t fp = EOS_FILE_INVALID;

    if (mode == (LV_FS_MODE_RD | LV_FS_MODE_WR))
    {
        // Read-write mode not supported, fallback to read
        fp = eos_storage_file_open_read(path);
    }
    else if (mode & LV_FS_MODE_WR)
    {
        // Write mode
        fp = eos_storage_file_open_write(path);
    }
    else if (mode & LV_FS_MODE_RD)
    {
        // Read mode
        fp = eos_storage_file_open_read(path);
    }

    if (fp == EOS_FILE_INVALID)
    {
        EOS_LOG_E("Failed to open file: %s", path);
        eos_free(handle);
        return NULL;
    }

    handle->file_handle = fp;
    return (void *)handle;
}

static lv_fs_res_t _drv_close_cb(lv_fs_drv_t *drv, void *file_p)
{
    (void)drv; // unused

    if (!file_p)
        return LV_FS_RES_INV_PARAM;

    eos_lvgl_fs_handle_t *handle = (eos_lvgl_fs_handle_t *)file_p;

    if (handle->type == 0) // file
    {
        eos_storage_file_close(handle->file_handle);
    }
    else if (handle->type == 1) // directory
    {
        eos_storage_dir_close(handle->dir_handle);
    }

    eos_free(handle);
    return LV_FS_RES_OK;
}

static lv_fs_res_t _drv_read_cb(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br)
{
    (void)drv; // unused

    if (!file_p || !buf || !br)
        return LV_FS_RES_INV_PARAM;

    eos_lvgl_fs_handle_t *handle = (eos_lvgl_fs_handle_t *)file_p;

    if (handle->type != 0) // not a file
        return LV_FS_RES_FS_ERR;

    ssize_t bytes_read = eos_storage_file_read(handle->file_handle, buf, btr);

    if (bytes_read < 0)
    {
        EOS_LOG_E("Failed to read file");
        return LV_FS_RES_FS_ERR;
    }

    *br = (uint32_t)bytes_read;
    return LV_FS_RES_OK;
}

static lv_fs_res_t _drv_write_cb(lv_fs_drv_t *drv, void *file_p, const void *buf, uint32_t btw, uint32_t *bw)
{
    (void)drv; // unused

    if (!file_p || !buf || !bw)
        return LV_FS_RES_INV_PARAM;

    eos_lvgl_fs_handle_t *handle = (eos_lvgl_fs_handle_t *)file_p;

    if (handle->type != 0) // not a file
        return LV_FS_RES_FS_ERR;

    ssize_t bytes_written = eos_storage_file_write(handle->file_handle, buf, btw);

    if (bytes_written < 0)
    {
        EOS_LOG_E("Failed to write file");
        return LV_FS_RES_FS_ERR;
    }

    *bw = (uint32_t)bytes_written;
    return LV_FS_RES_OK;
}

static lv_fs_res_t _drv_seek_cb(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence)
{
    (void)drv; // unused

    if (!file_p)
        return LV_FS_RES_INV_PARAM;

    eos_lvgl_fs_handle_t *handle = (eos_lvgl_fs_handle_t *)file_p;

    if (handle->type != 0) // not a file
        return LV_FS_RES_FS_ERR;

    // Note: Current implementation only supports SEEK_SET (absolute positioning)
    if (whence != LV_FS_SEEK_SET)
    {
        // For SEEK_CUR and SEEK_END, we would need to handle them separately
        // For now, we only support absolute positioning
        EOS_LOG_W("Only absolute seek (SEEK_SET) is supported");
        return LV_FS_RES_NOT_IMP;
    }

    if (eos_storage_file_seek(handle->file_handle, pos) != EOS_OK)
    {
        EOS_LOG_E("Failed to seek in file");
        return LV_FS_RES_FS_ERR;
    }

    return LV_FS_RES_OK;
}

static lv_fs_res_t _drv_tell_cb(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    (void)drv; // unused

    if (!file_p || !pos_p)
        return LV_FS_RES_INV_PARAM;

    eos_lvgl_fs_handle_t *handle = (eos_lvgl_fs_handle_t *)file_p;

    if (handle->type != 0) // not a file
        return LV_FS_RES_FS_ERR;

    if (eos_storage_file_tell(handle->file_handle, pos_p) != EOS_OK)
    {
        EOS_LOG_E("Failed to get file position");
        return LV_FS_RES_FS_ERR;
    }

    return LV_FS_RES_OK;
}


static void *_drv_dir_open_cb(lv_fs_drv_t *drv, const char *path)
{
    (void)drv; // unused

    if (!path)
        return NULL;

    eos_lvgl_fs_handle_t *handle = (eos_lvgl_fs_handle_t *)eos_malloc(sizeof(eos_lvgl_fs_handle_t));
    if (!handle)
        return NULL;

    memset(handle, 0, sizeof(eos_lvgl_fs_handle_t));
    handle->type = 1; // directory

    eos_dir_t dir = eos_storage_dir_open(path);
    if (dir == NULL)
    {
        EOS_LOG_E("Failed to open directory: %s", path);
        eos_free(handle);
        return NULL;
    }

    handle->dir_handle = dir;
    return (void *)handle;
}

static lv_fs_res_t _drv_dir_read_cb(lv_fs_drv_t *drv, void *rddir_p, char *fn, uint32_t fn_len)
{
    (void)drv; // unused

    if (!rddir_p || !fn || fn_len == 0)
        return LV_FS_RES_INV_PARAM;

    eos_lvgl_fs_handle_t *handle = (eos_lvgl_fs_handle_t *)rddir_p;

    if (handle->type != 1) // not a directory
        return LV_FS_RES_FS_ERR;

    if (eos_storage_dir_read(handle->dir_handle, fn, fn_len) != EOS_OK)
    {
        // End of directory or error
        return LV_FS_RES_FS_ERR;
    }

    return LV_FS_RES_OK;
}

static lv_fs_res_t _drv_dir_close_cb(lv_fs_drv_t *drv, void *rddir_p)
{
    (void)drv; // unused

    if (!rddir_p)
        return LV_FS_RES_INV_PARAM;

    eos_lvgl_fs_handle_t *handle = (eos_lvgl_fs_handle_t *)rddir_p;

    if (handle->type != 1) // not a directory
        return LV_FS_RES_FS_ERR;

    eos_storage_dir_close(handle->dir_handle);
    eos_free(handle);

    return LV_FS_RES_OK;
}

void eos_lvgl_fs_register(void)
{
    lv_fs_drv_init(&fs_drv); /*Basic initialization*/

    fs_drv.letter = EOS_LVGL_FS_LETTER;  /*An uppercase letter to identify the drive */
    fs_drv.cache_size = 0; /*Cache size for reading in bytes. 0 to not cache.*/

    fs_drv.ready_cb = NULL;          /*Callback to tell if the drive is ready to use */
    fs_drv.open_cb = _drv_open_cb;   /*Callback to open a file */
    fs_drv.close_cb = _drv_close_cb; /*Callback to close a file */
    fs_drv.read_cb = _drv_read_cb;   /*Callback to read a file */
    fs_drv.write_cb = _drv_write_cb; /*Callback to write a file */
    fs_drv.seek_cb = _drv_seek_cb;   /*Callback to seek in a file (Move cursor) */
    fs_drv.tell_cb = _drv_tell_cb;   /*Callback to tell the cursor position  */

    fs_drv.dir_open_cb = _drv_dir_open_cb;   /*Callback to open directory to read its content */
    fs_drv.dir_read_cb = _drv_dir_read_cb;   /*Callback to read a directory's content */
    fs_drv.dir_close_cb = _drv_dir_close_cb; /*Callback to close a directory */

    fs_drv.user_data = NULL; /*Any custom data if required*/

    lv_fs_drv_register(&fs_drv);

    EOS_LOG_I("LVGL file system driver registered");
}
