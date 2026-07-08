/**
 * @file eos_version.h
 * @brief Version definitions
 *
 * Follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
 */

#ifndef EOS_VERSION_H
#define EOS_VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

/* Version Definitions ---------------------------------------------------*/

#define ELENIX_OS_VERSION_MAJOR 0
#define ELENIX_OS_VERSION_MINOR 5
#define ELENIX_OS_VERSION_PATCH 0
#define ELENIX_OS_VERSION_INFO "alpha"

#define ELENIX_OS_API_LEVEL ELENIX_OS_VERSION_MAJOR

#define STRINGIFY(x) #x
#define VERSION_STRING(major, minor, patch, info) STRINGIFY(major) "." STRINGIFY(minor) "." STRINGIFY(patch) "-" info

#define ELENIX_OS_VERSION_FULL \
    VERSION_STRING(ELENIX_OS_VERSION_MAJOR, ELENIX_OS_VERSION_MINOR, ELENIX_OS_VERSION_PATCH, ELENIX_OS_VERSION_INFO)

#ifdef __cplusplus
}
#endif

#endif /* EOS_VERSION_H */
