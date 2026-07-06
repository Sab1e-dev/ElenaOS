/**
 * @file eos_fs_port.h
 * @brief File system porting
 */

#ifndef EOS_FS_PORT_H
#define EOS_FS_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include "eos_config.h"
#include "eos_error.h"
/* Public macros ----------------------------------------------*/
#if EOS_FS_TYPE == EOS_FS_POSIX
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
typedef FILE *eos_file_t;
typedef DIR *eos_dir_t;
#define EOS_FILE_INVALID NULL
#define EOS_DIR_INVALID NULL

#elif EOS_FS_TYPE == EOS_FS_FATFS
#include "ff.h"
typedef FIL *eos_file_t;
typedef FF_DIR *eos_dir_t;
#define EOS_FILE_INVALID NULL
#define EOS_DIR_INVALID NULL

#elif EOS_FS_TYPE == EOS_FS_LITTLEFS
#include "lfs.h"
typedef lfs_file_t *eos_file_t;
typedef lfs_dir_t *eos_dir_t;
#define EOS_FILE_INVALID NULL
#define EOS_DIR_INVALID NULL

#elif EOS_FS_TYPE == EOS_FS_RTTHREAD
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
typedef int eos_file_t;
typedef DIR *eos_dir_t;
#define EOS_FILE_INVALID (-1)
#define EOS_DIR_INVALID NULL

#elif EOS_FS_TYPE == EOS_FS_CUSTOM
typedef EOS_FS_FILE_TYPE eos_file_t;
typedef EOS_FS_DIR_TYPE eos_dir_t;
#endif /* EOS_FS_TYPE */
/* Public typedefs --------------------------------------------*/
typedef enum
{
    EOS_FS_TYPE_NOT_EXIST = 0,
    EOS_FS_TYPE_FILE = 1,
    EOS_FS_TYPE_DIR = 2
} eos_fs_type_t;

typedef struct
{
    eos_file_t file;
    void *data;
    size_t size;
} eos_async_write_task_t;
/* Public function prototypes --------------------------------*/

/**
 * @brief Set the virtual filesystem root directory.
 *        On simulator, call once at startup with the sandbox path.
 *        On real hardware, skip (defaults to "/", i.e. pass-through).
 * @param root Absolute path to use as filesystem root, e.g. "/path/to/fs/"
 */
void eos_fs_set_root(const char *root);

/**
 * @brief Resolve a virtual path to a real filesystem path.
 *        If root is "/" (or NULL / empty), path passes through unchanged.
 *        Otherwise prepends root.  Already-prefixed paths pass through.
 * @param path  Virtual path (e.g. "/.sys/config/cfg.json")
 * @param buf   Output buffer for the resolved path
 * @param bufsz Buffer size
 * @return buf on success, NULL on invalid input
 */
const char *eos_fs_realpath(const char *path, char *buf, size_t bufsz);

/**
 * @brief Open a file in read-only mode
 * @param path File path
 * @return void* File handle, returns NULL on failure
 */
eos_file_t eos_fs_open_read(const char *path);
/**
 * @brief Open a file in write-only mode, creates the file if it does not exist
 * @param path File path
 * @return void* File handle, returns NULL on failure
 */
eos_file_t eos_fs_open_write(const char *path);
/**
 * @brief Read data from file
 * @param file File handle
 * @param buf Data buffer
 * @param len Number of bytes to read
 * @return int Actual number of bytes read, returns -1 on error
 */
int eos_fs_read(eos_file_t file, void *buf, size_t len);
/**
 * @brief Write data to file
 * @param file File handle
 * @param buf Data buffer
 * @param len Number of bytes to write
 * @return int Actual number of bytes written, returns -1 on error
 */
int eos_fs_write(eos_file_t file, const void *buf, size_t len);
/**
 * @brief Position file pointer
 * @param file File handle
 * @param pos File offset position (from beginning of file)
 * @return int Returns 0 on success, -1 on failure
 */
eos_result_t eos_fs_seek(eos_file_t file, uint32_t pos);
/**
 * @brief Get file size
 * @param file File handle
 * @param size Output file size (in bytes)
 * @return int Returns 0 on success, -1 on failure
 */
eos_result_t eos_fs_size(eos_file_t file, uint32_t *size);
/**
 * @brief Get current file position
 * @param file File handle
 * @param pos Output current file position
 * @return int Returns 0 on success, -1 on failure
 */
eos_result_t eos_fs_tell(eos_file_t file, uint32_t *pos);
/**
 * @brief Close file
 * @param file File handle
 */
void eos_fs_close(eos_file_t file);
/**
 * @brief Create directory (single level)
 * @param path Directory path
 * @return int Returns 0 on success, -1 on failure
 */
eos_result_t eos_fs_mkdir(const char *path);
/**
 * @brief Remove empty directory
 * @param path Directory path
 * @return int Returns 0 on success, -1 on failure
 */
eos_result_t eos_fs_rmdir(const char *path);
/**
 * @brief Remove file
 * @param path File path
 * @return int Returns 0 on success, -1 on failure
 */
eos_result_t eos_fs_remove(const char *path);
/**
 * @brief Check if file or directory exists
 * @param path File or directory path
 * @return int 1 if exists, 0 if not exists, <0 on error
 */
int eos_fs_exists(const char *path);
/**
 * @brief Get the type of specified path (file or directory)
 * @param path File or directory path
 * @return int See `eos_fs_type_t`
 */
int eos_fs_type(const char *path);
/**
 * @brief Open directory
 * @param path Directory path
 * @return eos_dir_t* Returns directory pointer on success, otherwise returns NULL
 */
eos_dir_t eos_fs_opendir(const char *path);
/**
 * @brief Read next filename in directory
 *
 * This function reads the name of the next file or subdirectory from the specified directory stream.
 * Each call returns the next entry in the directory until the end of the directory is reached.
 *
 * @param dir Pointer to opened directory stream (opened by eos_fs_opendir)
 * @param name Buffer to store the read filename
 * @param max_len Maximum length of name buffer (including trailing '\0')
 *
 * @return int
 * @retval 0 Read successful, filename stored in name
 * @retval -1 Read failed (e.g., dir is NULL or directory reading ended)
 *
 * @note If filename length exceeds max_len-1, it will be truncated and guaranteed to end with '\0'
 * @note This function only returns filename, without path
 */
eos_result_t eos_fs_readdir(eos_dir_t dir, char *name, size_t max_len);
/**
 * @brief Close directory
 * @param dir Target directory pointer
 */
void eos_fs_closedir(eos_dir_t dir);
/**
 * @brief Move file or rename
 * @param old_path
 * @param new_path
 * @return int Returns 0 on success, -1 on failure
 */
eos_result_t eos_fs_mv(const char *old_path, const char *new_path);
/**
 * @brief Synchronize file data
 * @param file File
 * @return int
 */
eos_result_t eos_fs_sync(eos_file_t file);

#ifdef __cplusplus
}
#endif

#endif /* EOS_FS_PORT_H */
