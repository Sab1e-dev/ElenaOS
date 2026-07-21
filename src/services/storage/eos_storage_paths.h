/**
 * @file eos_storage_paths.h
 * @brief System storage paths definition
 *
 * This file defines all system directory paths in a centralized location.
 * All storage related path constants should be defined here to maintain
 * a clear and unified view of the system's directory structure.
 */

#ifndef EOS_STORAGE_PATHS_H
#define EOS_STORAGE_PATHS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include "eos_config.h"

/* Public macros - System Root and Base Directories -----------*/

/**
 * Root directory is defined in eos_config.h as EOS_SYS_ROOT_DIR
 * Default: "/" on Native platform, or file system path on simulator
 */

/** @brief System base directory */
#define EOS_SYS_DIR "/.sys/"

/* Configuration Storage --------------------------------------*/
/** @brief Configuration files directory */
#define EOS_CONFIG_DIR EOS_SYS_DIR "config/"

/** @brief Main configuration file */
#define EOS_CONFIG_FILE_PATH EOS_CONFIG_DIR "cfg.json"

/* Resources Storage ------------------------------------------*/
/** @brief System resources directory */
#define EOS_SYS_RES_DIR EOS_SYS_DIR "res/"

/** @brief System images resources directory */
#define EOS_SYS_RES_IMG_DIR EOS_SYS_RES_DIR "img/"

/** @brief System fonts resources directory */
#define EOS_SYS_RES_FONT_DIR EOS_SYS_RES_DIR "font/"

/* Application Management Storage -----------------------------*/
/** @brief Application management base directory */
#define EOS_APP_DIR EOS_SYS_DIR "app/"

/** @brief Installed applications directory */
#define EOS_APP_INSTALLED_DIR EOS_APP_DIR "apps/"

/** @brief Application data storage directory */
#define EOS_APP_DATA_DIR EOS_APP_DIR "app_data/"

/* Watchface Management Storage -------------------------------*/
/** @brief Watchface management base directory */
#define EOS_WATCHFACE_DIR EOS_SYS_DIR "wf/"

/** @brief Installed watchfaces directory */
#define EOS_WATCHFACE_INSTALLED_DIR EOS_WATCHFACE_DIR "faces/"

/** @brief Watchface data storage directory */
#define EOS_WATCHFACE_DATA_DIR EOS_WATCHFACE_DIR "wf_data/"

#ifdef __cplusplus
}
#endif

#endif /* EOS_STORAGE_PATHS_H */
