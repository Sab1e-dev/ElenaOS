/**
 * @file eos_lvgl_requirements.h
 * @brief Compile-time assertions for LVGL features required by ElenixOS
 *
 * Included from eos_config.h — checks only fire when lvgl.h has been
 * included before eos_config.h.  Uses defined() guards so that source
 * files without lvgl.h are unaffected.
 */

#ifndef EOS_LVGL_REQUIREMENTS_H
#define EOS_LVGL_REQUIREMENTS_H

/* Widgets (32 total) — required by SNI JavaScript bindings ---*/

#if defined(LV_USE_ANIMIMG) && !LV_USE_ANIMIMG
#error "LV_USE_ANIMIMG must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_ARC) && !LV_USE_ARC
#error "LV_USE_ARC must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_BAR) && !LV_USE_BAR
#error "LV_USE_BAR must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_BUTTON) && !LV_USE_BUTTON
#error "LV_USE_BUTTON must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_BUTTONMATRIX) && !LV_USE_BUTTONMATRIX
#error "LV_USE_BUTTONMATRIX must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_CALENDAR) && !LV_USE_CALENDAR
#error "LV_USE_CALENDAR must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_CANVAS) && !LV_USE_CANVAS
#error "LV_USE_CANVAS must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_CHART) && !LV_USE_CHART
#error "LV_USE_CHART must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_CHECKBOX) && !LV_USE_CHECKBOX
#error "LV_USE_CHECKBOX must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_DROPDOWN) && !LV_USE_DROPDOWN
#error "LV_USE_DROPDOWN must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_IMAGE) && !LV_USE_IMAGE
#error "LV_USE_IMAGE must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_IMAGEBUTTON) && !LV_USE_IMAGEBUTTON
#error "LV_USE_IMAGEBUTTON must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_KEYBOARD) && !LV_USE_KEYBOARD
#error "LV_USE_KEYBOARD must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_LABEL) && !LV_USE_LABEL
#error "LV_USE_LABEL must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_LED) && !LV_USE_LED
#error "LV_USE_LED must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_LINE) && !LV_USE_LINE
#error "LV_USE_LINE must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_LIST) && !LV_USE_LIST
#error "LV_USE_LIST must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_MENU) && !LV_USE_MENU
#error "LV_USE_MENU must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_MSGBOX) && !LV_USE_MSGBOX
#error "LV_USE_MSGBOX must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_ROLLER) && !LV_USE_ROLLER
#error "LV_USE_ROLLER must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_SCALE) && !LV_USE_SCALE
#error "LV_USE_SCALE must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_SLIDER) && !LV_USE_SLIDER
#error "LV_USE_SLIDER must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_SPAN) && !LV_USE_SPAN
#error "LV_USE_SPAN must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_SPINBOX) && !LV_USE_SPINBOX
#error "LV_USE_SPINBOX must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_SPINNER) && !LV_USE_SPINNER
#error "LV_USE_SPINNER must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_SWITCH) && !LV_USE_SWITCH
#error "LV_USE_SWITCH must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_TEXTAREA) && !LV_USE_TEXTAREA
#error "LV_USE_TEXTAREA must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_TABLE) && !LV_USE_TABLE
#error "LV_USE_TABLE must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_TABVIEW) && !LV_USE_TABVIEW
#error "LV_USE_TABVIEW must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_TILEVIEW) && !LV_USE_TILEVIEW
#error "LV_USE_TILEVIEW must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_WIN) && !LV_USE_WIN
#error "LV_USE_WIN must be enabled (required by ElenixOS)"
#endif

/* Core rendering ---------------------------------------------*/

#if defined(LV_USE_DRAW_SW) && !LV_USE_DRAW_SW
#error "LV_USE_DRAW_SW must be enabled (required by ElenixOS)"
#endif

/* Layouts ----------------------------------------------------*/

#if defined(LV_USE_FLEX) && !LV_USE_FLEX
#error "LV_USE_FLEX must be enabled (required by ElenixOS)"
#endif
#if defined(LV_USE_GRID) && !LV_USE_GRID
#error "LV_USE_GRID must be enabled (required by ElenixOS)"
#endif

/* Theme ------------------------------------------------------*/

#if defined(LV_USE_THEME_DEFAULT) && !LV_USE_THEME_DEFAULT
#error "LV_USE_THEME_DEFAULT must be enabled (required by ElenixOS)"
#endif

/* Observer ---------------------------------------------------*/

#if defined(LV_USE_OBSERVER) && !LV_USE_OBSERVER
#error "LV_USE_OBSERVER must be enabled (required by ElenixOS)"
#endif

/* Snapshot ---------------------------------------------------*/

#if defined(LV_USE_SNAPSHOT) && !LV_USE_SNAPSHOT
#error "LV_USE_SNAPSHOT must be enabled (required by ElenixOS)"
#endif

/* QR Code ----------------------------------------------------*/

#if defined(LV_USE_QRCODE) && !LV_USE_QRCODE
#error "LV_USE_QRCODE must be enabled (required by ElenixOS)"
#endif

/* Tiny TTF (CJK font) ----------------------------------------*/

#if defined(LV_USE_TINY_TTF) && !LV_USE_TINY_TTF
#error "LV_USE_TINY_TTF must be enabled (required by ElenixOS)"
#endif
#if defined(LV_TINY_TTF_FILE_SUPPORT) && !LV_TINY_TTF_FILE_SUPPORT
#error "LV_TINY_TTF_FILE_SUPPORT must be enabled (required by ElenixOS)"
#endif

/* PNG decoder (LodePNG) --------------------------------------*/

#if defined(LV_USE_LODEPNG) && !LV_USE_LODEPNG
#error "LV_USE_LODEPNG must be enabled (required by ElenixOS)"
#endif

/* Default font -----------------------------------------------*/

#if defined(LV_FONT_MONTSERRAT_14) && !LV_FONT_MONTSERRAT_14
#error "LV_FONT_MONTSERRAT_14 must be enabled (required by ElenixOS)"
#endif
#if defined(LV_FONT_MONTSERRAT_30) && !LV_FONT_MONTSERRAT_30
#error "LV_FONT_MONTSERRAT_30 must be enabled (required by ElenixOS)"
#endif

#endif /* EOS_LVGL_REQUIREMENTS_H */
