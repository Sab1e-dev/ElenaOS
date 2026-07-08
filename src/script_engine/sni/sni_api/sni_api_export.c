/**
 * @file sni_api_export.c
 * @brief API Export Layer
 */

#include "sni_api_export.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

static bool sni_set_property(jerry_value_t obj, const char *name, jerry_value_t value)
{
    jerry_value_t result = jerry_object_set_sz(obj, name, value);
    if (jerry_value_is_exception(result))
    {
        jerry_value_free(result);
        return false;
    }

    jerry_value_free(result);
    return true;
}

static bool sni_register_methods(const sni_method_desc_t *methods, jerry_value_t target)
{
    if (methods == NULL)
    {
        return true;
    }

    for (size_t i = 0; methods[i].name != NULL; i++)
    {
        if (methods[i].handler == NULL)
        {
            continue;
        }

        jerry_value_t func = jerry_function_external(methods[i].handler);
        if (jerry_value_is_exception(func))
        {
            jerry_value_free(func);
            return false;
        }

        bool ok = sni_set_property(target, methods[i].name, func);
        jerry_value_free(func);
        if (!ok)
        {
            return false;
        }
    }

    return true;
}

bool sni_api_register_constants(const sni_constant_desc_t *constants, jerry_value_t target)
{
    if (constants == NULL)
    {
        return true;
    }

    for (size_t i = 0; constants[i].name != NULL; i++)
    {
        jerry_value_t value = jerry_undefined();

        switch (constants[i].type)
        {
            case SNI_CONST_INT:
                value = jerry_number((double)constants[i].value.i);
                break;
            case SNI_CONST_FLOAT:
                value = jerry_number(constants[i].value.f);
                break;
            case SNI_CONST_STRING:
                value = jerry_string_sz(constants[i].value.s);
                break;
            default:
                return false;
        }

        if (jerry_value_is_exception(value))
        {
            jerry_value_free(value);
            return false;
        }

        bool ok = sni_set_property(target, constants[i].name, value);
        jerry_value_free(value);
        if (!ok)
        {
            return false;
        }
    }

    return true;
}

static bool sni_register_properties(const sni_property_desc_t *properties, jerry_value_t prototype)
{
    if (properties == NULL)
    {
        return true;
    }

    for (size_t i = 0; properties[i].name != NULL; i++)
    {
        if (properties[i].getter == NULL && properties[i].setter == NULL)
        {
            continue;
        }

        jerry_property_descriptor_t desc = jerry_property_descriptor();
        desc.flags |= JERRY_PROP_IS_CONFIGURABLE_DEFINED | JERRY_PROP_IS_CONFIGURABLE;
        desc.flags |= JERRY_PROP_IS_ENUMERABLE_DEFINED | JERRY_PROP_IS_ENUMERABLE;

        if (properties[i].getter != NULL)
        {
            desc.getter = jerry_function_external(properties[i].getter);
            if (jerry_value_is_exception(desc.getter))
            {
                jerry_value_free(desc.getter);
                jerry_property_descriptor_free(&desc);
                return false;
            }
            desc.flags |= JERRY_PROP_IS_GET_DEFINED;
        }

        if (properties[i].setter != NULL)
        {
            desc.setter = jerry_function_external(properties[i].setter);
            if (jerry_value_is_exception(desc.setter))
            {
                jerry_value_free(desc.setter);
                jerry_property_descriptor_free(&desc);
                return false;
            }
            desc.flags |= JERRY_PROP_IS_SET_DEFINED;
        }

        jerry_value_t prop_name = jerry_string_sz(properties[i].name);
        if (jerry_value_is_exception(prop_name))
        {
            jerry_value_free(prop_name);
            jerry_property_descriptor_free(&desc);
            return false;
        }

        /* Skip property if a method with the same name was already registered.
         * Methods are explicit user-configured entries and always take precedence
         * over auto-discovered property accessors when names conflict. */
        jerry_value_t has_own = jerry_object_has_own(prototype, prop_name);
        if (jerry_value_is_true(has_own))
        {
            jerry_value_free(has_own);
            jerry_value_free(prop_name);
            jerry_property_descriptor_free(&desc);
            continue;
        }
        jerry_value_free(has_own);

        jerry_value_t result = jerry_object_define_own_prop(prototype, prop_name, &desc);
        jerry_value_free(prop_name);
        jerry_property_descriptor_free(&desc);

        if (jerry_value_is_exception(result))
        {
            jerry_value_free(result);
            return false;
        }

        bool ok = jerry_value_is_true(result);
        jerry_value_free(result);
        if (!ok)
        {
            return false;
        }
    }

    return true;
}

static int sni_find_class_index(const sni_class_desc_t *const classes[],
                                size_t class_count,
                                const sni_class_desc_t *target)
{
    if (target == NULL)
    {
        return -1;
    }

    for (size_t i = 0; i < class_count; i++)
    {
        if (classes[i] == target)
        {
            return (int)i;
        }
    }

    if (target->name == NULL)
    {
        return -1;
    }

    for (size_t i = 0; i < class_count; i++)
    {
        if (classes[i] != NULL && classes[i]->name != NULL && strcmp(classes[i]->name, target->name) == 0)
        {
            return (int)i;
        }
    }

    return -1;
}

jerry_value_t sni_api_build(const sni_class_desc_t *const classes[])
{
    if (classes == NULL)
    {
        return jerry_undefined();
    }

    size_t class_count = 0;
    while (classes[class_count] != NULL)
    {
        class_count++;
    }

    jerry_value_t class_values = jerry_object();
    if (jerry_value_is_exception(class_values))
    {
        jerry_value_free(class_values);
        return jerry_undefined();
    }

    if (class_count == 0)
    {
        return class_values;
    }

    jerry_value_t prototypes[class_count];
    jerry_value_t class_entries[class_count];

    for (size_t i = 0; i < class_count; i++)
    {
        prototypes[i] = jerry_undefined();
        class_entries[i] = jerry_undefined();
    }

    bool ok = true;

    for (size_t i = 0; i < class_count && ok; i++)
    {
        const sni_class_desc_t *desc = classes[i];
        if (desc == NULL || desc->name == NULL)
        {
            ok = false;
            break;
        }

        if (desc->constructor != NULL)
        {
            class_entries[i] = jerry_function_external(desc->constructor);
            if (jerry_value_is_exception(class_entries[i]))
            {
                ok = false;
                break;
            }

            prototypes[i] = jerry_object();
            if (jerry_value_is_exception(prototypes[i]))
            {
                ok = false;
                break;
            }

            if (!sni_set_property(class_entries[i], "prototype", prototypes[i]))
            {
                ok = false;
                break;
            }

            if (!sni_register_methods(desc->methods, prototypes[i]))
            {
                ok = false;
                break;
            }

            if (!sni_register_properties(desc->properties, prototypes[i]))
            {
                ok = false;
                break;
            }
        }
        else
        {
            // Static class: validate no instance methods/properties (sentinel-only array is treated as empty)
            bool has_methods = (desc->methods != NULL && desc->methods[0].name != NULL);
            bool has_properties = (desc->properties != NULL && desc->properties[0].name != NULL);
            if (has_methods || has_properties)
            {
                ok = false;
                break;
            }

            class_entries[i] = jerry_object();
            if (jerry_value_is_exception(class_entries[i]))
            {
                ok = false;
                break;
            }
        }

        if (!sni_register_methods(desc->static_methods, class_entries[i]))
        {
            ok = false;
            break;
        }

        if (!sni_api_register_constants(desc->constants, class_entries[i]))
        {
            ok = false;
            break;
        }

        if (!sni_set_property(class_values, desc->name, class_entries[i]))
        {
            ok = false;
            break;
        }
    }

    for (size_t i = 0; i < class_count && ok; i++)
    {
        const sni_class_desc_t *desc = classes[i];
        if (desc->constructor == NULL || desc->base_class == NULL)
        {
            continue;
        }

        int base_index = sni_find_class_index(classes, class_count, desc->base_class);
        if (base_index < 0)
        {
            ok = false;
            break;
        }

        if (jerry_value_is_undefined(prototypes[(size_t)base_index]))
        {
            ok = false;
            break;
        }

        jerry_value_t result = jerry_object_set_proto(prototypes[i], prototypes[(size_t)base_index]);
        if (jerry_value_is_exception(result))
        {
            jerry_value_free(result);
            ok = false;
            break;
        }

        bool set_ok = jerry_value_is_true(result);
        jerry_value_free(result);
        if (!set_ok)
        {
            ok = false;
            break;
        }
    }

    for (size_t i = 0; i < class_count; i++)
    {
        if (!jerry_value_is_undefined(prototypes[i]))
        {
            jerry_value_free(prototypes[i]);
        }

        if (!jerry_value_is_undefined(class_entries[i]))
        {
            jerry_value_free(class_entries[i]);
        }
    }

    if (!ok)
    {
        jerry_value_free(class_values);
        return jerry_undefined();
    }

    return class_values;
}

bool sni_api_mount(jerry_value_t realm, jerry_value_t api_obj, const char *name)
{
    if (name == NULL)
    {
        return false;
    }

    if (jerry_value_is_exception(realm) || jerry_value_is_exception(api_obj) || !jerry_value_is_object(api_obj))
    {
        return false;
    }

    jerry_value_t key = jerry_string_sz(name);
    if (jerry_value_is_exception(key))
    {
        jerry_value_free(key);
        return false;
    }

    jerry_value_t result = jerry_object_set(realm, key, api_obj);

    jerry_value_free(key);

    if (jerry_value_is_exception(result))
    {
        jerry_value_free(result);
        return false;
    }

    jerry_value_free(result);

    return true;
}

jerry_value_t sni_api_throw_error(const char *message)
{
    return jerry_throw_value(jerry_error_sz(JERRY_ERROR_TYPE, message), true);
}
