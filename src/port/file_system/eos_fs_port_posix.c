/**
 * @file eos_fs_port.c
 * @brief File system porting
 */

#include "eos_config.h"

#if EOS_FS_TYPE == EOS_FS_POSIX

#include "eos_fs_port.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include "eos_port.h"
#include "eos_log.h"
/* Macros and Definitions -------------------------------------*/
#define FS_PATH_BUF_SIZE 512

/* Variables --------------------------------------------------*/

/** Runtime root directory.  Defaults to "/" → pass-through.
 *  Set via eos_fs_set_root() once at startup on simulator. */
static char _s_fs_root[FS_PATH_BUF_SIZE] = "/";

void eos_fs_set_root(const char *root)
{
    if (!root || root[0] == '\0') {
        _s_fs_root[0] = '/';
        _s_fs_root[1] = '\0';
        return;
    }
    size_t len = strlen(root);
    while (len > 0 && root[len - 1] == '/') len--;
    if (len == 0) {
        _s_fs_root[0] = '/';
        _s_fs_root[1] = '\0';
        return;
    }
    snprintf(_s_fs_root, sizeof(_s_fs_root), "%.*s", (int)len, root);
}

const char *eos_fs_realpath(const char *path, char *buf, size_t bufsz)
{
    if (!path || !buf || bufsz == 0) return NULL;
    if (path[0] != '/') {
        snprintf(buf, bufsz, "%s", path);
        return buf;
    }
    size_t root_len = strlen(_s_fs_root);
    while (root_len > 0 && _s_fs_root[root_len - 1] == '/') root_len--;
    if (strncmp(path, _s_fs_root, root_len) == 0) {
        snprintf(buf, bufsz, "%s", path);
        return buf;
    }
    snprintf(buf, bufsz, "%.*s%s", (int)root_len, _s_fs_root, path);
    return buf;
}

/* Function Implementations -----------------------------------*/

/* ---------------- POSIX FS implementation ------------------ */

/* Open file read-only */
eos_file_t eos_fs_open_read(const char *path)
{
    if (!path)
        return NULL;
    char resolved[FS_PATH_BUF_SIZE];
    return fopen(eos_fs_realpath(path, resolved, sizeof(resolved)), "rb");
}

/* Open file write-only (create if not exist, overwrite if exist) */
eos_file_t eos_fs_open_write(const char *path)
{
    if (!path)
        return NULL;
    char resolved[FS_PATH_BUF_SIZE];
    return fopen(eos_fs_realpath(path, resolved, sizeof(resolved)), "wb");
}

/* Read file data */
int eos_fs_read(eos_file_t fp, void *buf, size_t len)
{
    if (!fp || !buf)
        return -1;
    size_t n = fread(buf, 1, len, (FILE *)fp);
    if (n == 0 && ferror((FILE *)fp))
        return -1;
    return (int)n;
}

/* Write file data */
int eos_fs_write(eos_file_t fp, const void *buf, size_t len)
{
    if (!fp || !buf)
        return -1;
    size_t n = fwrite(buf, 1, len, (FILE *)fp);
    if (n < len)
        return -1;
    return (int)n;
}

/* File positioning */
eos_result_t eos_fs_seek(eos_file_t fp, uint32_t pos)
{
    if (!fp)
        return EOS_ERR_IO;
    return fseek((FILE *)fp, (long)pos, SEEK_SET) == 0 ? EOS_OK : EOS_ERR_IO;
}

/* Get file size */
eos_result_t eos_fs_size(eos_file_t fp, uint32_t *size)
{
    if (!fp || !size)
        return EOS_ERR_IO;
    long cur = ftell((FILE *)fp);
    if (cur < 0)
        return EOS_ERR_IO;
    if (fseek((FILE *)fp, 0, SEEK_END) != 0)
        return EOS_ERR_IO;
    long end = ftell((FILE *)fp);
    if (end < 0)
        return EOS_ERR_IO;
    *size = (uint32_t)end;
    fseek((FILE *)fp, cur, SEEK_SET);
    return EOS_OK;
}

/* Get current file position */
eos_result_t eos_fs_tell(eos_file_t fp, uint32_t *pos)
{
    if (!fp || !pos)
        return EOS_ERR_IO;
    long cur = ftell((FILE *)fp);
    if (cur < 0)
        return EOS_ERR_IO;
    *pos = (uint32_t)cur;
    return EOS_OK;
}

/* Close file */
void eos_fs_close(eos_file_t fp)
{
    if (fp)
        fclose((FILE *)fp);
}

/* Create directory (single level directory) */
eos_result_t eos_fs_mkdir(const char *path)
{
    if (!path)
        return EOS_ERR_IO;
    char resolved[FS_PATH_BUF_SIZE];
    const char *rp = eos_fs_realpath(path, resolved, sizeof(resolved));
    if (!rp) return EOS_ERR_IO;
#ifdef _WIN32
    return mkdir(rp) == 0 ? EOS_OK : EOS_ERR_IO;
#else
    return mkdir(rp, 0755) == 0 ? EOS_OK : EOS_ERR_IO;
#endif
}

/* Remove empty directory */
eos_result_t eos_fs_rmdir(const char *path)
{
    if (!path)
        return EOS_ERR_IO;
    char resolved[FS_PATH_BUF_SIZE];
    const char *rp = eos_fs_realpath(path, resolved, sizeof(resolved));
    if (!rp) return EOS_ERR_IO;
    return rmdir(rp) == 0 ? EOS_OK : EOS_ERR_IO;
}

/* Remove file */
eos_result_t eos_fs_remove(const char *path)
{
    if (!path)
        return EOS_ERR_IO;
    char resolved[FS_PATH_BUF_SIZE];
    const char *rp = eos_fs_realpath(path, resolved, sizeof(resolved));
    if (!rp) return EOS_ERR_IO;
    return remove(rp) == 0 ? EOS_OK : EOS_ERR_IO;
}

/* Check if file or directory exists */
int eos_fs_exists(const char *path)
{
    if (!path)
        return EOS_OK;
    char resolved[FS_PATH_BUF_SIZE];
    const char *rp = eos_fs_realpath(path, resolved, sizeof(resolved));
    if (!rp) return EOS_OK;
    struct stat st;
    return stat(rp, &st) == 0 ? 1 : 0;
}

int eos_fs_type(const char *path)
{
    char resolved[FS_PATH_BUF_SIZE];
    const char *rp = eos_fs_realpath(path, resolved, sizeof(resolved));
    if (!rp) return EOS_FS_TYPE_NOT_EXIST;
    struct stat st;
    if (stat(rp, &st) != 0)
        return EOS_FS_TYPE_NOT_EXIST;
    if (S_ISDIR(st.st_mode))
        return EOS_FS_TYPE_DIR;
    return EOS_FS_TYPE_FILE;
}

eos_dir_t eos_fs_opendir(const char *path)
{
    if (!path) return NULL;
    char resolved[FS_PATH_BUF_SIZE];
    const char *rp = eos_fs_realpath(path, resolved, sizeof(resolved));
    if (!rp) return NULL;
    return opendir(rp);
}

eos_result_t eos_fs_readdir(eos_dir_t dir, char *name, size_t max_len)
{
    if (!dir)
        return EOS_ERR_IO;

    struct dirent *entry = readdir(dir);
    if (!entry)
        return EOS_ERR_IO;

    strncpy(name, entry->d_name, max_len - 1);
    name[max_len - 1] = '\0';
    return EOS_OK;
}

void eos_fs_closedir(eos_dir_t dir)
{
    if (dir)
    {
        closedir(dir);
    }
}

eos_result_t eos_fs_mv(const char *old_path, const char *new_path)
{
    if (!old_path || !new_path) return EOS_ERR_IO;
    char old_r[FS_PATH_BUF_SIZE], new_r[FS_PATH_BUF_SIZE];
    const char *op = eos_fs_realpath(old_path, old_r, sizeof(old_r));
    const char *np = eos_fs_realpath(new_path, new_r, sizeof(new_r));
    if (!op || !np) return EOS_ERR_IO;
    if (rename(op, np) != 0)
    {
        perror("rename failed");
        return EOS_ERR_IO;
    }
    return EOS_OK;
}

eos_result_t eos_fs_sync(eos_file_t fp)
{
    return fflush(fp) == 0 ? EOS_OK : EOS_ERR_IO;
}

#endif /* EOS_FS_TYPE */
