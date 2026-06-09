/**
 * @file eos_error.h
 * @brief ElenixOS error code definitions
 */

#ifndef EOS_ERROR_H
#define EOS_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/**
 * @brief ElenixOS return value status
 */
typedef enum
{
    EOS_OK = 0,                  /**< Operation successful */
    EOS_FAILED = -1,             /**< Generic failure */

    /* Common errors (-100 ~ -199) */
    EOS_ERR_INVALID_ARG = -100,  /**< Invalid input argument */
    EOS_ERR_VAR_NULL = -101,     /**< Required argument is NULL */
    EOS_ERR_MEM = -102,          /**< Memory related error */
    EOS_ERR_NOT_FOUND = -103,    /**< Resource not found */
    EOS_ERR_ALREADY_EXISTS = -104, /**< Resource already exists */
    EOS_ERR_BUSY = -105,         /**< Busy */
    EOS_ERR_TIMEOUT = -106,      /**< Timeout */

    /* Lifecycle/state errors (-200 ~ -299) */
    EOS_ERR_NOT_INITIALIZED = -200,     /**< Not initialized */
    EOS_ERR_ALREADY_INITIALIZED = -201, /**< Already initialized */
    EOS_ERR_INVALID_STATE = -202,       /**< Invalid state */

    /* Storage/IO errors (-300 ~ -399) */
    EOS_ERR_FILE_ERROR = -300,   /**< File related error */
    EOS_ERR_IO = -301,           /**< IO failure */
    EOS_ERR_PATH_TOO_LONG = -302, /**< Path too long */

    /* Data/format errors (-400 ~ -499) */
    EOS_ERR_JSON_ERROR = -400,   /**< JSON related error */
    EOS_ERR_VALUE_MISMATCH = -401, /**< Value mismatch */
    EOS_ERR_SDK_VERSION = -402,    /**< Package requires higher SDK/API level */

    /* Container/data-structure errors (-500 ~ -599) */
    EOS_ERR_STACK_EMPTY = -500,  /**< Stack empty */
    EOS_ERR_STACK_FULL = -501,   /**< Stack full */

    /* Device/driver errors (-600 ~ -699) */
    EOS_ERR_DEV_NOT_FOUND = -600, /**< Device not found */
    EOS_ERR_DEV_BUSY = -601,      /**< Device busy */
    EOS_ERR_DEV_ERROR = -602,     /**< Device error */
    EOS_ERR_DEV_OPS_NOT_SUPPORTED = -603, /**< Device operation not supported */

    EOS_ERR_UNKNOWN = -999,
} eos_result_t;

/* Backward compatibility alias: prefer EOS_ERR_ALREADY_EXISTS. */
#define EOS_ERR_VAR_NOT_NULL EOS_ERR_ALREADY_EXISTS

/* Public function prototypes --------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* EOS_ERROR_H */
