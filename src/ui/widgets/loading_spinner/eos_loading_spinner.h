/**
 * @file eos_loading_spinner.h
 * @brief Apple-style radial loading spinner with phased progress
 *
 * A circular indicator made of radial bars pointing toward the center.
 * Filled bars are white, idle bars are gray (0x404040). Progress is
 * driven by N equally-weighted phases; each phase maps its internal
 * 0-100% onto one sub-range of the total (e.g. 3 phases map to
 * 0-33, 33-66, 66-100). Total progress = sum of all phase contributions.
 */

#ifndef EOS_LOADING_SPINNER_H
#define EOS_LOADING_SPINNER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include "lvgl.h"

/* Public macros ----------------------------------------------*/

#define EOS_LOADING_SPINNER_DEFAULT_BARS 24
#define EOS_LOADING_SPINNER_DEFAULT_PHASES 3
#define EOS_LOADING_SPINNER_MIN_BARS 4
#define EOS_LOADING_SPINNER_MAX_BARS 64
#define EOS_LOADING_SPINNER_MAX_PHASES 8

/** @brief Default inner radius as % of the widget's smaller dimension */
#define EOS_LOADING_SPINNER_DEFAULT_INNER_RATIO 40

/** @brief Default outer radius as % of the widget's smaller dimension */
#define EOS_LOADING_SPINNER_DEFAULT_OUTER_RATIO 50

/** @brief Default bar line width in px */
#define EOS_LOADING_SPINNER_DEFAULT_BAR_WIDTH 3

/* Public typedefs --------------------------------------------*/

/** @brief Callback type for completion dim animation finish */
typedef void (*eos_loading_spinner_completed_cb_t)(void *user_data);

/**
 * @brief Loading spinner context — fields are public for caller access
 */
typedef struct
{
    lv_obj_t *root; /**< Root LVGL object (draw target, owns context) */
    uint16_t bar_count; /**< Number of radial bars */
    uint16_t inner_ratio; /**< Inner radius as % of min(w,h) */
    uint16_t outer_ratio; /**< Outer radius as % of min(w,h) */
    uint16_t bar_width; /**< Radial bar line width in px */
    uint16_t phase_count; /**< Number of progress phases (>= 1) */
    uint16_t *phase_progress; /**< Per-phase progress 0-100, heap-allocated, size phase_count */
    uint16_t display_pct; /**< Currently displayed overall percent 0-100 */
    uint32_t anim_duration_ms; /**< Progress transition duration; 0 = instant */
    lv_anim_t anim; /**< Embedded anim for smooth progress transitions */
    bool infinite; /**< Infinite (indeterminate) spinning mode flag */
    bool completed; /**< Loading completed flag */
    int32_t rotation; /**< Current rotation angle in 0.1 deg (0-3599) */
    lv_anim_t rotate_anim; /**< Continuous rotation animation */
    uint16_t emerge_progress; /**< Dot emergence: 0 to bar_count*256, drives sequential fade-in */
    lv_anim_t emerge_anim; /**< Emerge intro animation */
    lv_opa_t dim_opa; /**< Completion dim opacity: 255 = full, 0 = invisible */
    lv_anim_t dim_anim; /**< Completion dim animation */
    eos_loading_spinner_completed_cb_t completed_cb; /**< Callback when dim animation finishes */
    void *completed_cb_data; /**< User data for completed callback */
} eos_loading_spinner_t;

/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a radial loading spinner with phased progress.
 *
 * The widget is created as a child of `parent`. The caller sizes and
 * positions `spinner->root` (default 56x56). All bars render gray
 * (idle) until the first phase progress is set.
 *
 * @param parent      Parent LVGL object
 * @param bar_count   Number of radial bars (0 = default 24, clamped 4..64)
 * @param phase_count Number of progress phases (0 = default 3, clamped 1..8)
 * @return eos_loading_spinner_t* Allocated spinner context, or NULL on failure
 */
eos_loading_spinner_t *eos_loading_spinner_create(lv_obj_t *parent, uint16_t bar_count, uint16_t phase_count);

/**
 * @brief Set the number of progress phases (reallocates the internal array).
 *
 * Existing phase progress values are preserved up to min(old, new).
 * Newly added phases start at 0; excess phases are dropped.
 *
 * @param spinner     Spinner context
 * @param phase_count New phase count (clamped to 1..EOS_LOADING_SPINNER_MAX_PHASES)
 */
void eos_loading_spinner_set_phase_count(eos_loading_spinner_t *spinner, uint16_t phase_count);

/**
 * @brief Set the internal progress of one phase (0-100).
 *
 * Each phase has equal weight. Total visual progress is the sum of
 * each phase's contribution:
 *   total = (phase_progress[i] / 100) x phase_span[i]
 * where phase_span[i] = 100 / phase_count (last phase absorbs integer
 * division remainder so the total always spans exactly 0-100).
 *
 * @param spinner Spinner context
 * @param index   Phase index (0-based, clamped to phase_count-1)
 * @param pct     Internal progress 0-100 (clamped)
 */
void eos_loading_spinner_set_phase_progress(eos_loading_spinner_t *spinner, uint16_t index, uint16_t pct);

/**
 * @brief Set all phase progress values at once (single redraw).
 * @param spinner Spinner context
 * @param values  Array of phase_count values (0-100 each) or NULL to zero all
 */
void eos_loading_spinner_set_all_phases(eos_loading_spinner_t *spinner, const uint16_t *values);

/**
 * @brief Set the inner and outer radii of the radial bars.
 *
 * Values are percentages of the widget's smaller dimension (width or height).
 *
 * @param spinner     Spinner context
 * @param inner_ratio Inner radius as % of min(w,h), clamped 0..outer_ratio
 * @param outer_ratio Outer radius as % of min(w,h), clamped inner_ratio..200
 */
void eos_loading_spinner_set_radii(eos_loading_spinner_t *spinner, uint16_t inner_ratio, uint16_t outer_ratio);

/**
 * @brief Set the radial bar line width.
 * @param spinner Spinner context
 * @param width   Bar line width in px (clamped 1..16)
 */
void eos_loading_spinner_set_bar_width(eos_loading_spinner_t *spinner, uint16_t width);

/**
 * @brief Get the computed overall progress (0-100).
 * @param spinner Spinner context
 * @return uint16_t Overall percent 0-100
 */
uint16_t eos_loading_spinner_get_progress(const eos_loading_spinner_t *spinner);

/**
 * @brief Set the progress transition animation duration.
 * @param spinner     Spinner context
 * @param duration_ms Animation duration in ms; 0 = instant (default)
 */
void eos_loading_spinner_set_anim_duration(eos_loading_spinner_t *spinner, uint32_t duration_ms);

/**
 * @brief Delete the spinner widget and free its context.
 *
 * Safe no-op if already deleted. Also freed automatically when
 * the root object's parent is deleted (via LV_EVENT_DELETE hook).
 *
 * @param spinner Spinner context
 */
void eos_loading_spinner_delete(eos_loading_spinner_t *spinner);

/**
 * @brief Enable or disable infinite (indeterminate) spinning mode.
 *
 * In infinite mode, the spinner ignores phase progress and draws
 * rotating dots with a comet-tail effect (trailing dots dim and shrink).
 * The determinate phased-progress mode is the default.
 *
 * @param spinner  Spinner context
 * @param infinite true = infinite mode, false = determinate mode
 */
void eos_loading_spinner_set_infinite(eos_loading_spinner_t *spinner, bool infinite);

/**
 * @brief Mark loading as complete and start dim animation.
 *
 * The spinner continues rotating while its dots fade from full
 * brightness to transparent over ~400ms.  When the animation
 * finishes, @p cb is called with @p user_data so the caller can
 * remove the loading overlay and reveal the app.
 *
 * @param spinner   Spinner context
 * @param cb        Callback fired when dim animation completes (may be NULL)
 * @param user_data User data passed to callback
 */
void eos_loading_spinner_set_completed(eos_loading_spinner_t *spinner,
                                       eos_loading_spinner_completed_cb_t cb,
                                       void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* EOS_LOADING_SPINNER_H */
