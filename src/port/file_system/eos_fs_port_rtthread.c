/**
 * @file eos_fs_port_rtthread.c
 * @brief File system porting to RTThread OS
 */

#include "eos_config.h"

#if EOS_FS_TYPE == EOS_FS_RTTHREAD

#include "eos_fs_port.h"

/* Includes ---------------------------------------------------*/
#include <string.h>
#include "eos_log.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

void eos_fs_set_root(const char *root)
{
    (void)root;
}

const char *eos_fs_realpath(const char *path, char *buf, size_t bufsz)
{
    if (!path || !buf || bufsz == 0) return NULL;
    snprintf(buf, bufsz, "%s", path);
    return buf;
}

eos_file_t eos_fs_open_read(const char *path)
{
    if (!path)
        return -1;
    return open(path, O_RDONLY, 0);
}

eos_file_t eos_fs_open_write(const char *path)
{
    if (!path)
        return -1;
    return open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
}

int eos_fs_read(eos_file_t fp, void *buf, size_t len)
{
    if (fp < 0 || !buf)
        return -1;
    int n = read(fp, buf, len);
    return n >= 0 ? n : -1;
}

int eos_fs_write(eos_file_t fp, const void *buf, size_t len)
{
    if (fp < 0 || !buf)
        return -1;
    int n = write(fp, buf, len);
    return n == (int)len ? n : -1;
}

int eos_fs_seek(eos_file_t fp, uint32_t pos)
{
    if (fp < 0)
        return -1;
    int r = lseek(fp, pos, SEEK_SET);
    return r >= 0 ? 0 : -1;
}

int eos_fs_size(eos_file_t fp, uint32_t *size)
{
    if (fp < 0 || !size)
        return -1;
    struct stat st;
    if (fstat(fp, &st) != 0)
        return -1;
    *size = st.st_size;
    return 0;
}

int eos_fs_tell(eos_file_t fp, uint32_t *pos)
{
    if (fp < 0 || !pos)
        return -1;
    off_t cur = lseek(fp, 0, SEEK_CUR);
    if (cur < 0)
        return -1;
    *pos = (uint32_t)cur;
    return 0;
}

void eos_fs_close(eos_file_t fp)
{
    if (fp >= 0)
        close(fp);
}

int eos_fs_mkdir(const char *path)
{
    if (!path)
        return -1;
    return mkdir(path, 0755) == 0 ? 0 : -1;
}

int eos_fs_rmdir(const char *path)
{
    if (!path)
        return -1;
    return rmdir(path) == 0 ? 0 : -1;
}

int eos_fs_remove(const char *path)
{
    if (!path)
        return -1;
    return unlink(path) == 0 ? 0 : -1;
}

int eos_fs_exists(const char *path)
{
    if (!path)
        return 0;
    struct stat st;
    return stat(path, &st) == 0 ? 1 : 0;
}

int eos_fs_type(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return EOS_FS_TYPE_NOT_EXIST;
    if (S_ISDIR(st.st_mode))
        return EOS_FS_TYPE_DIR;
    return EOS_FS_TYPE_FILE;
}

eos_dir_t eos_fs_opendir(const char *path)
{
    return opendir(path);
}

int eos_fs_readdir(eos_dir_t dir, char *name, size_t max_len)
{
    if (!dir)
        return -1;
    struct dirent *entry = readdir(dir);
    if (!entry)
        return -1;
    strncpy(name, entry->d_name, max_len - 1);
    name[max_len - 1] = '\0';
    return 0;
}

void eos_fs_closedir(eos_dir_t dir)
{
    if (dir)
        closedir(dir);
}

int eos_fs_mv(const char *old_path, const char *new_path)
{
    return rename(old_path, new_path) == 0 ? 0 : -1;
}

int eos_fs_sync(eos_file_t fp)
{
    if (fp < 0)
        return -1;
    return fsync(fp);
}

#endif /* EOS_FS_TYPE */
