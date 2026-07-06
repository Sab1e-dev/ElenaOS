/**
 * @file sni_api_export.h
 * @brief API export layer
 */

#ifndef SNI_API_EXPORT_H
#define SNI_API_EXPORT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "jerryscript.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

typedef enum
{
    SNI_CONST_INT,
    SNI_CONST_FLOAT,
    SNI_CONST_STRING,
} sni_constant_type_t;

typedef struct
{
    const char *name;
    jerry_external_handler_t handler;
} sni_method_desc_t;

typedef struct
{
    const char *name;
    jerry_external_handler_t getter;
    jerry_external_handler_t setter;
} sni_property_desc_t;

typedef struct
{
    const char *name;
    sni_constant_type_t type;
    union
    {
        int32_t i;
        double f;
        const char *s;
    } value;
} sni_constant_desc_t;

typedef struct sni_class_desc_t sni_class_desc_t;

struct sni_class_desc_t
{
    const char *name;

    jerry_external_handler_t constructor; /**< Constructor; when NULL, the class is exported as a static class */

    const sni_class_desc_t *base_class;

    const sni_method_desc_t *methods; /**< Instance methods */
    const sni_property_desc_t *properties; /**< Properties */

    const sni_method_desc_t *static_methods;
    const sni_constant_desc_t *constants;
};

/* Public function prototypes --------------------------------*/

/**
 * @brief Create an API object based on class description table
 * @param classes Class description table pointer array, the last element must be NULL
 * @return jerry_value_t API object
 */
jerry_value_t sni_api_build(const sni_class_desc_t *const classes[]);

/**
 * @brief Mount API object to specified realm
 * @param realm Target realm object
 * @param api_obj API object
 * @param name Mount name
 * @return bool Whether mount successful
 */
bool sni_api_mount(jerry_value_t realm, jerry_value_t api_obj, const char *name);

/**
 * @brief Register constant description array to target object
 * @param constants Constant description array (ends with name == NULL)
 * @param target Target object
 * @return bool Whether registration successful
 */
bool sni_api_register_constants(const sni_constant_desc_t *constants, jerry_value_t target);

/**
 * @brief Throw an error exception
 * @param message Error message
 * @return jerry_value_t Error exception object
 */
jerry_value_t sni_api_throw_error(const char *message);

#ifdef __cplusplus
}
#endif

#endif /* SNI_API_EXPORT_H */
