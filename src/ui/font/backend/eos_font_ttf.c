/**
 * @file eos_font_ttf.c
 * @brief TTF file
 */

#include "eos_config.h"
#if EOS_FONT_TYPE == EOS_FONT_TTF
#include "eos_font.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include "eos_theme.h"

/* Macros and Definitions -------------------------------------*/
LV_FONT_DECLARE(EOS_FONT_ICON);
#if EOS_FONT_TTF_TYPE == EOS_FONT_TTF_DATA
EOS_FONT_DATA_DECLARE(EOS_FONT_TTF_DATA_NAME);
EOS_FONT_DATA_SIZE_DECLARE(EOS_FONT_TTF_DATA_SIZE);
#endif /* EOS_FONT_TTF_TYPE */
/* Variables --------------------------------------------------*/
static lv_font_t *font_large;
static lv_font_t *font_medium;
static lv_font_t *font_small;
static bool _font_inited = false;
#if EOS_FONT_TTF_TYPE == EOS_FONT_TTF_FILE
static char _font_path[128] = EOS_SYS_RES_FONT_DIR EOS_FONT_TTF_FILE_PATH;
#endif
/* Function Implementations -----------------------------------*/

lv_font_t *eos_font_init(void)
{
    if (_font_inited)
    {
        EOS_LOG_W("Font system already initialized, returning cached font");
        return font_medium;
    }

    EOS_LOG_I("Font system init");

#if EOS_FONT_TTF_TYPE == EOS_FONT_TTF_DATA

#if EOS_FONT_TTF_ENABLE_EXTENDED
    font_large = lv_tiny_ttf_create_data_ex(EOS_FONT_TTF_DATA_NAME,
                                            EOS_FONT_TTF_DATA_SIZE,
                                            EOS_FONT_SIZE_LARGE,
                                            EOS_FONT_TTF_KERNING,
                                            EOS_FONT_TTF_CACHE_SIZE);
    font_medium = lv_tiny_ttf_create_data_ex(EOS_FONT_TTF_DATA_NAME,
                                             EOS_FONT_TTF_DATA_SIZE,
                                             EOS_FONT_SIZE_MEDIUM,
                                             EOS_FONT_TTF_KERNING,
                                             EOS_FONT_TTF_CACHE_SIZE);
    font_small = lv_tiny_ttf_create_data_ex(EOS_FONT_TTF_DATA_NAME,
                                            EOS_FONT_TTF_DATA_SIZE,
                                            EOS_FONT_SIZE_SMALL,
                                            EOS_FONT_TTF_KERNING,
                                            EOS_FONT_TTF_CACHE_SIZE);
#else
    font_large = lv_tiny_ttf_create_data(EOS_FONT_TTF_DATA_NAME, EOS_FONT_TTF_DATA_SIZE, EOS_FONT_SIZE_LARGE);
    font_medium = lv_tiny_ttf_create_data(EOS_FONT_TTF_DATA_NAME, EOS_FONT_TTF_DATA_SIZE, EOS_FONT_SIZE_MEDIUM);
    font_small = lv_tiny_ttf_create_data(EOS_FONT_TTF_DATA_NAME, EOS_FONT_TTF_DATA_SIZE, EOS_FONT_SIZE_SMALL);
#endif /* EOS_FONT_TTF_ENABLE_EXTENDED */

#elif EOS_FONT_TTF_TYPE == EOS_FONT_TTF_FILE

#if EOS_FONT_TTF_ENABLE_EXTENDED
    font_large =
        lv_tiny_ttf_create_file_ex(_font_path, EOS_FONT_SIZE_LARGE, EOS_FONT_TTF_KERNING, EOS_FONT_TTF_CACHE_SIZE);
    font_medium =
        lv_tiny_ttf_create_file_ex(_font_path, EOS_FONT_SIZE_MEDIUM, EOS_FONT_TTF_KERNING, EOS_FONT_TTF_CACHE_SIZE);
    font_small =
        lv_tiny_ttf_create_file_ex(_font_path, EOS_FONT_SIZE_SMALL, EOS_FONT_TTF_KERNING, EOS_FONT_TTF_CACHE_SIZE);
#else
    font_large = lv_tiny_ttf_create_file(_font_path, EOS_FONT_SIZE_LARGE);
    font_medium = lv_tiny_ttf_create_file(_font_path, EOS_FONT_SIZE_MEDIUM);
    font_small = lv_tiny_ttf_create_file(_font_path, EOS_FONT_SIZE_SMALL);
#endif /* EOS_FONT_TTF_ENABLE_EXTENDED */

#endif /* EOS_FONT_TTF_TYPE */

    if (!font_large || !font_medium || !font_small)
    {
        EOS_LOG_E("Some fonts failed to load!");
        return NULL;
    }
    else
    {
        font_large->fallback = font_medium;
        font_medium->fallback = font_small;
        font_small->fallback = &EOS_FONT_ICON;
        EOS_LOG_D("All TTF fonts loaded successfully");
    }
    _font_inited = true;
    return font_medium;
}

void eos_font_deinit(void)
{
    if (!_font_inited)
        return;

    EOS_LOG_I("Font system deinit");

    if (font_large)
    {
        lv_tiny_ttf_destroy(font_large);
        font_large = NULL;
    }
    if (font_medium)
    {
        lv_tiny_ttf_destroy(font_medium);
        font_medium = NULL;
    }
    if (font_small)
    {
        lv_tiny_ttf_destroy(font_small);
        font_small = NULL;
    }

    _font_inited = false;
}

lv_font_t *eos_font_reload(const char *path)
{
#if EOS_FONT_TTF_TYPE == EOS_FONT_TTF_FILE
    if (path && path[0])
    {
        snprintf(_font_path, sizeof(_font_path), "%s", path);
        EOS_LOG_I("Font path changed to: %s", _font_path);
    }
#endif

    eos_font_deinit();

    lv_font_t *default_font = eos_font_init();
    if (!default_font)
        return NULL;

    eos_theme_set(lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), default_font);

    return default_font;
}

lv_font_t *_select_font(eos_font_size_t size)
{
    switch (size)
    {
        case EOS_FONT_SIZE_LARGE:
            return font_large;
        case EOS_FONT_SIZE_MEDIUM:
            return font_medium;
        case EOS_FONT_SIZE_SMALL:
            return font_small;
        default:
            if (size >= EOS_FONT_SIZE_LARGE)
                return font_large;
            else if (size > EOS_FONT_SIZE_SMALL)
                return font_medium;
            else
                return font_small;
    }
}

void eos_label_set_font_size(lv_obj_t *label, eos_font_size_t size)
{
    EOS_CHECK_PTR_RETURN(label);
    lv_obj_set_style_text_font(label, _select_font(size), 0);
}
#endif /* EOS_FONT_TYPE */
