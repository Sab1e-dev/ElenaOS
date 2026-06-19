/**
 * @file eos_config.h
 * @brief System configuration
 */

#if 1 /* Enable configuration */

#ifndef EOS_CONFIG_H
#define EOS_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ---------------------------------------------------*/
#include "eos_config_internal.h"
#include "eos_storage_paths.h"

/* Configuration start ----------------------------------------------------*/

#define ELENIX_WATCH_MARKETING_NAME "Elenix Watch"    /**< Model name */
#define ELENIX_WATCH_MODEL_NUMBER "S2508(0)"        /**< Model */

/**
 * Select compilation mode
 * Available modes:
 *  - DEBUG:        Debug mode, enable all initialization checks.
 *  - RELEASE:      Release mode, disable all initialization checks.
 */
#ifndef EOS_COMPILE_MODE
#define EOS_COMPILE_MODE DEBUG
#endif

/**
 * Select whether to enable animation
 */
#define EOS_ANIMATION_ENABLE 0

/**
 * Select whether to build icon strings into JerryScript
 */
#define EOS_ICON_STR_BUILD_IN 0

/************************** Modules **************************/

/**
 * Enable virtual display module
 */
#define EOS_USE_VIRTUAL_DISPLAY 1

/************************** Memory configuration **************************/

/**
 * Select memory allocation mode
 * Available options:
 *  - EOS_MEM_PROVIDER_STDLIB_CLIB:    Use standard library memory allocation functions
 *  - EOS_MEM_PROVIDER_AUTO:           Use automatic allocation method
 *  - EOS_MEM_PROVIDER_CUSTOM:         Use custom allocation functions (just override the weakly defined `eos_malloc_core()` and other functions)
 */
#ifndef EOS_MEM_ALLOC_PROVIDER
    #define EOS_MEM_ALLOC_PROVIDER EOS_MEM_PROVIDER_STDLIB_CLIB
#endif /* EOS_MEM_ALLOC_PROVIDER */

#if EOS_MEM_ALLOC_PROVIDER == EOS_MEM_PROVIDER_AUTO
    #define EOS_MEM_POOL_ALLOC_THRESHOLD 32/*KB*/ * 1024/*B*/    /**< Allocation threshold, unit: bytes */

    #define EOS_MEM_PROVIDER_AUTO_INCLUDE    "mem_mgr.h"

    #define EOS_MEM_ALLOC_FAST malloc
    #define EOS_MEM_ALLOC_LARGE mem_mgr_alloc

    #define EOS_MEM_FREE_FAST free
    #define EOS_MEM_FREE_LARGE mem_mgr_free

    #define EOS_MEM_REALLOC_FAST realloc
    #define EOS_MEM_REALLOC_LARGE mem_mgr_realloc

    #define EOS_MEM_CALLOC_FAST calloc
    #define EOS_MEM_CALLOC_LARGE mem_mgr_calloc
#endif /* EOS_MEM_ALLOC_PROVIDER */

/**
 * Whether to let ElenixOS override LVGL's standard library malloc
 *
 * - When this is disabled:
 *   When LVGL needs to allocate large memory for display-related purposes (such as images, screenshot buffers, etc.),
 *   it will eventually call ElenixOS's `eos_malloc()` to complete the allocation,
 *   so these display memories are actually still allocated and managed by ElenixOS.
 *
 * - When this is enabled:
 *   All memory requests from LVGL (including display-related and other internal data) will be completely handled by
 *   ElenixOS's allocation functions, with ElenixOS uniformly managing all memory.
 */
#define EOS_OVERRIDE_LVGL_STDLIB_MALLOC_ENABLE \
    (LV_USE_STDLIB_MALLOC == LV_STDLIB_CUSTOM)

#define EOS_MEM_TRACK_ENABLE 1 /**< Whether to enable memory tracking */

/************************** Image cache configuration **************************/

/**
 * Enable LVGL image decode cache.
 * When enabled, decoded image pixel data is cached to avoid
 * re-decoding.  The cache is initialised automatically during
 * system startup.
 */
#ifndef EOS_CACHE_ENABLE
    #define EOS_CACHE_ENABLE                1
#endif

/**
 * Max cache memory budget for decoded image pixel data (bytes).
 *
 * Default: 128 KB.  On platforms that define a dedicated image
 * cache memory pool, the value is derived from that pool automatically.
 */
#ifndef EOS_CACHE_SIZE
    #if defined(IMAGE_CACHE_IN_PSRAM_SIZE) && (IMAGE_CACHE_IN_PSRAM_SIZE > 0)
        #define EOS_CACHE_SIZE              ((uint32_t)IMAGE_CACHE_IN_PSRAM_SIZE)
    #elif defined(IMAGE_CACHE_IN_SRAM_SIZE) && (IMAGE_CACHE_IN_SRAM_SIZE > 0)
        #define EOS_CACHE_SIZE              ((uint32_t)IMAGE_CACHE_IN_SRAM_SIZE)
    #else
        #define EOS_CACHE_SIZE              (128U * 1024U)
    #endif
#endif

/**
 * Max number of cached image header entries.
 * Each entry stores image metadata (dimensions, format, etc.)
 * for quick lookup without re-reading the file header.
 */
#ifndef EOS_CACHE_HEADER_COUNT
    #define EOS_CACHE_HEADER_COUNT          16U
#endif

/**
 * Allocate cache pixel buffers from a dedicated memory pool
 * (e.g. PSRAM on SiFli platforms) instead of the default heap.
 *
 * 0 = use default heap (lv_malloc / lv_free).
 * 1 = use dedicated pool via eos_cache_buf_alloc / eos_cache_buf_free.
 *
 * When enabled, platform ports must override the weak default
 * of eos_cache_buf_alloc / eos_cache_buf_free.
 */
#ifndef EOS_CACHE_USE_DEDICATED_MEM
    #if defined(BSP_USING_PSRAM) || defined(IMAGE_CACHE_IN_PSRAM_SIZE)
        #define EOS_CACHE_USE_DEDICATED_MEM 1
    #else
        #define EOS_CACHE_USE_DEDICATED_MEM 0
    #endif
#endif

/************************** RTOS configuration **************************/

/**
 * Select RTOS type
 * Available options:
 *  - EOS_RTOS_BARE_METAL:  Bare metal, use interrupt disable/enable for critical sections
 *  - EOS_RTOS_FREERTOS:    FreeRTOS
 *  - EOS_RTOS_RTTHREAD:    RT-Thread
 *  - EOS_RTOS_POSIX:       POSIX threads (pthread/semaphore)
 *  - EOS_RTOS_CUSTOM:      Custom RTOS (provide your own implementation)
 */
#ifndef EOS_RTOS_TYPE
    #define EOS_RTOS_TYPE EOS_RTOS_BARE_METAL
#endif /* EOS_RTOS_TYPE */

/************************** Font configuration **************************/

/**
 * Select font source type
 *
 * `EOS_FONT_C_*` uses C font libraries generated by LVGL Font Converter, or LVGL built-in fonts
 *
 * Available formats:
 *  - EOS_FONT_C_MULTI:     Store multiple bitmap fonts, different font sizes use different fonts
 *  - EOS_FONT_TTF:         Use TTF font files from the file system
 */
#define EOS_FONT_TYPE EOS_FONT_TTF

/**
 * Configure font sizes, the following three configurations correspond to three fonts:
 * - EOS_FONT_SIZE_LARGE
 * - EOS_FONT_SIZE_MEDIUM
 * - EOS_FONT_SIZE_SMALL
 */
#define EOS_FONT_CFG_LARGE_SIZE 30
#define EOS_FONT_CFG_MEDIUM_SIZE EOS_FONT_CFG_LARGE_SIZE - 4
#define EOS_FONT_CFG_SMALL_SIZE EOS_FONT_CFG_MEDIUM_SIZE - 4

#if EOS_FONT_TYPE == EOS_FONT_C_MULTI

#define EOS_ENABLE_CHINESE_FONT 1

#if EOS_ENABLE_CHINESE_FONT

#define EOS_FONT_LARGE_NAME source_han_sans_30
#define EOS_FONT_MEDIUM_NAME source_han_sans_26
#define EOS_FONT_SMALL_NAME source_han_sans_22

#else

#define EOS_FONT_LARGE_NAME lv_font_montserrat_30
#define EOS_FONT_MEDIUM_NAME lv_font_montserrat_30
#define EOS_FONT_SMALL_NAME lv_font_montserrat_30

#endif

#elif EOS_FONT_TYPE == EOS_FONT_TTF

/**
 * Select TTF data format
 * Available formats:
 *  - EOS_FONT_TTF_DATA:    Compile TTF font data into firmware
 *  - EOS_FONT_TTF_FILE:    Load TTF font files from the file system
 */
#define EOS_FONT_TTF_TYPE EOS_FONT_TTF_FILE

#if EOS_FONT_TTF_TYPE == EOS_FONT_TTF_DATA
    #define EOS_FONT_TTF_DATA_NAME SourceHanSansSC_12M
    #define EOS_FONT_TTF_DATA_SIZE SourceHanSansSC_12M_size
#elif EOS_FONT_TTF_TYPE == EOS_FONT_TTF_FILE
    #define EOS_FONT_TTF_FILE_PATH EOS_SYS_RES_FONT_DIR "font.ttf"
#endif /* EOS_FONT_TTF_TYPE */

/**
 * Enable additional features
 */
#define EOS_FONT_TTF_ENABLE_EXTENDED 0

#if EOS_FONT_TTF_ENABLE_EXTENDED
    #define EOS_FONT_TTF_KERNING 0      /**< Kerning, unit: px */
    #define EOS_FONT_TTF_CACHE_SIZE 256 /**< TTF font cache size */
#endif /* EOS_FONT_TTF_ENABLE_EXTENDED */

#endif /* EOS_FONT_TYPE */

/************************** Display configuration **************************/

#define EOS_DISPLAY_WIDTH 390    /**< Drawing width */
#define EOS_DISPLAY_HEIGHT 450     /**< Drawing height */

#define EOS_DISPLAY_RADIUS 80 /**< Display corner radius */

#define EOS_DISPLAY_BRIGHTNESS_MIN 5 /**< Brightness 0 means turn off the screen */
#define EOS_DISPLAY_BRIGHTNESS_MAX 100

/************************** System file directory configuration **************************/

#ifndef EOS_SYS_ROOT_DIR
    #define EOS_SYS_ROOT_DIR "/" /**< System root directory For example: `/user/elenixos/` */
#endif /* EOS_SYS_ROOT_DIR */

/**
 * Select file system interface type
 * Optional options:
 *  - EOS_FS_POSIX
 *  - EOS_FS_FATFS
 *  - EOS_FS_LITTLEFS
 *  - EOS_FS_RTTHREAD
 *  - EOS_FS_CUSTOM
 */
#ifndef EOS_FS_TYPE
    #define EOS_FS_TYPE EOS_FS_POSIX
#endif /* EOS_FS_TYPE */

#if EOS_FS_TYPE == EOS_FS_CUSTOM
    #define EOS_FS_FILE_TYPE void*          /**< File data type */
    #define EOS_FS_DIR_TYPE void*           /**< Directory data type */
    #define EOS_FILE_INVALID NULL           /**< Invalid file */
    #define EOS_DIR_INVALID NULL            /**< Invalid directory */
#endif /* EOS_FS_TYPE */

#define EOS_FS_PATH_MAX 256 /**< Maximum file path length, including null terminator */

/**
 * The drive letter used by LVGL to access the file system, for example: "S:/path/to/file"
 */
#define EOS_LVGL_FS_LETTER 'Z'

#define EOS_DFW_ENABLE 0        /**< Whether to enable delayed file writer */

/************************** Language configuration **************************/

/**
 * Default system language for first boot
 * Available options:
 *  - 0 (LANG_EN):  English
 *  - 1 (LANG_ZH):  Simple Chinese
 * This is only used when the system starts for the first time.
 * If the config service has a language setting, it will be used instead.
 */
#define EOS_CONFIG_DEFAULT_LANGUAGE 0

/************************** MISC **************************/

/**
 * Enable Test app
 */
#define EOS_ENABLE_TEST_APP 1

/************************** Log **************************/

// Whether to enable assertion
#define EOS_USE_ASSERT 1
#define EOS_ASSERT_HANDLER __builtin_trap();

// Whether to enable colorful log
#define EOS_LOG_USE_COLOR 0

// Maximum number of log listeners
#define EOS_LOG_MAX 8

// Whether to enable standard output log listener
#define EOS_LOG_ENABLE_STD 1

/* Configuration end ----------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* EOS_CONFIG_H */

#endif /* Enable configuration */
