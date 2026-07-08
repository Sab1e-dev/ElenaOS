/**
 * @file eos_std_widgets.c
 * @brief System standard widgets
 */

#include "eos_std_widgets.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include "eos_lang.h"
#include "eos_theme.h"
#include "eos_basic_widgets.h"
#include "eos_icon.h"
#include "eos_font.h"
/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

lv_obj_t *eos_std_info_create(lv_obj_t *scr,
                              lv_color_t icon_bg_color,
                              const char *icon,
                              const char *title_txt,
                              const char *txt)
{
    lv_obj_t *list = eos_list_create(scr);
    lv_obj_set_style_pad_row(list, 20, 0);

    eos_round_icon_create(list, icon_bg_color, icon);

    lv_obj_t *title_label = lv_label_create(list);
    lv_label_set_text(title_label, title_txt);
    eos_label_set_font_size(title_label, EOS_FONT_SIZE_LARGE);

    lv_obj_t *label = lv_label_create(list);
    if (eos_lang_get_current_id() == LANG_EN)
        lv_obj_set_width(label, lv_pct(90));
    else if (eos_lang_get_current_id() == LANG_ZH)
        lv_obj_set_width(label, lv_pct(95));
    else
        lv_obj_set_width(label, lv_pct(80));
    lv_label_set_text(label, txt);
    return list;
}

void eos_std_title_comment_create(lv_obj_t *parent, const char *title, const char *comment)
{
    lv_obj_t *lt, *lc;
    lt = eos_list_add_title(parent, title);
    eos_label_set_font_size(lt, EOS_FONT_SIZE_LARGE);
    lc = eos_list_add_comment(parent, comment);
    lv_obj_set_style_margin_ver(lc, 0, 0);
    lv_obj_align_to(lc, lt, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}
