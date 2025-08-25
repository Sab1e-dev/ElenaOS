/**
 * @file elena_os_pkg_mgr.h
 * @brief 包管理器
 * @author Sab1e
 * @date 2025-08-22
 */

#ifndef ELENA_OS_PKG_MGR_H
#define ELENA_OS_PKG_MGR_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "elena_os_core.h"
#include "elena_os_app.h"
#include "elena_os_watchface.h"
#include "script_engine_core.h"
/* Public macros ----------------------------------------------*/
#define EOS_PKG_APP_MAGIC           "EAPK"
#define EOS_PKG_WATCHFACE_MAGIC     "EWPK"
#define EOS_PKG_READ_BLOCK          512
#define EOS_PKG_NAME_LEN_MAX        256     // 最后一个字节强制为"\0"，因此名称长度最大255字节
#define EOS_PKG_ID_LEN_MAX          256     // 同上
#define EOS_PKG_VERSION_LEN_MAX     256     // 同上


#define EOS_PKG_MAGIC_OFFSET        0
#define EOS_PKG_NAME_OFFSET         EOS_PKG_MAGIC_OFFSET + 4
#define EOS_PKG_ID_OFFSET           EOS_PKG_NAME_OFFSET + EOS_PKG_NAME_LEN_MAX
#define EOS_PKG_VERSION_OFFSET      EOS_PKG_ID_OFFSET + EOS_PKG_ID_LEN_MAX
#define EOS_PKG_FILE_COUNT_OFFSET   EOS_PKG_VERSION_OFFSET + EOS_PKG_VERSION_LEN_MAX
#define EOS_PKG_RESERVED_OFFSET     EOS_PKG_FILE_COUNT_OFFSET + 4
#define EOS_PKG_TABLE_OFFSET        EOS_PKG_RESERVED_OFFSET + 4

/**
 * @brief 定义软件包的文件头
 */
typedef struct
{
    char magic[4];                              // Magic Number
    char pkg_name[EOS_PKG_NAME_LEN_MAX];        // 软件包名
    char pkg_id[EOS_PKG_ID_LEN_MAX];            // 软件 ID
    char pkg_version[EOS_PKG_VERSION_LEN_MAX];  // 软件版本
    uint32_t file_count;   // 文件数量
    uint32_t reserved;     // 保留字段，方便将来扩展
} eos_pkg_header_t;

/**
 * @brief 没有使用此结构体，但是 eos_pkg_mgr_unpack 是按照此结构体解析的
 ***********************************
    typedef struct
    {
        uint32_t name_len;  // 文件名长度
        char name[];        // 文件名（不定长）
        uint32_t is_dir;    // 是否目录 (0=文件,1=目录)
        uint32_t offset;    // 数据在包中的偏移
        uint32_t size;      // 文件大小
    } eos_pkg_entry_t;
 ************************************/
/* Public typedefs --------------------------------------------*/

/* Public function prototypes --------------------------------*/
/**
 * @brief 读取文件包头
 * @param pkg_path 软件包路径
 * @param header 软件包头结构体指针
 * @return eos_result_t 执行结果
 */
eos_result_t eos_pkg_read_header(const char *pkg_path, eos_pkg_header_t *header);
/**
 * @brief 解包 EAPK/EWPK 文件（例如：app.eapk, watchface.ewpk）
 * @param pkg_path 包文件路径
 * @param output_path 输出目录
 * @param pkg_type 包类型(SCRIPT_TYPE_APPLICATION/SCRIPT_TYPE_WATCHFACE)
 * @return eos_result_t 执行结果
 */
eos_result_t eos_pkg_mgr_unpack(const char *pkg_path, const char *output_path, const script_pkg_type_t pkg_type);
#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_PKG_MGR_H */
