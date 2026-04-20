// SPDX-FileCopyrightText: 2025 Stuart Parmenter
// SPDX-License-Identifier: MIT

// @file lvgl_demo_ui.c
// @brief Simple LVGL demo UI
//
// Based on ESP-IDF PARLIO RGB LED matrix LVGL example

#include "lvgl.h"
#include <esp_log.h>
#include <stdio.h>

static const char *TAG = "lvgl_ui";

// Create scrolling text label
static lv_obj_t* create_scrolling_label(lv_obj_t *parent, const char *text, int16_t y_pos, uint32_t duration) {
    static lv_anim_t label_anim;
    static lv_style_t label_style;
    lv_obj_t *label = lv_label_create(parent);

    // SET UP LABEL ANIMATION/STYLE
    lv_anim_init(&label_anim);
    lv_anim_set_delay(&label_anim, 3000);
    lv_anim_set_repeat_delay(&label_anim, 3000);
    lv_anim_set_repeat_count(&label_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_duration(&label_anim, duration);

    lv_style_init(&label_style);
    lv_style_set_anim(&label_style, &label_anim);

    // CREATE/POSITION LABEL USING STYLE
    int16_t parent_width = lv_obj_get_width(parent);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_obj_set_width(label, parent_width - 48);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, 24, y_pos);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_10, 0);
    lv_obj_add_style(label, &label_style, LV_STATE_DEFAULT);

    return label;
}

// Create static text label
static lv_obj_t* create_static_label(lv_obj_t *parent, const char *text, int16_t x_pos, int16_t y_pos, bool right_align) {
    lv_obj_t *label = lv_label_create(parent);

    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_SCROLLABLE);
    lv_label_set_text(label, text);

    if (right_align) {
        lv_obj_align(label, LV_ALIGN_TOP_RIGHT, -x_pos, y_pos);
    } else {
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, x_pos, y_pos);
    }

    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_10, 0);
    return label; 
}

// Main UI creation function
void lvgl_ui(lv_obj_t *scr, lv_obj_t** label_array) {
  // Set background color to black
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

  // Disable scrollbars on screen
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  // Get display size
  int16_t scr_width = lv_obj_get_width(scr);
  int16_t scr_height = lv_obj_get_height(scr);

  ESP_LOGI(TAG, "Creating UI for %dx%d display", scr_width, scr_height);

  // Title: Scrolling text
  label_array[0] = create_static_label(scr, "7", 0, 0, false);
  label_array[1] = create_scrolling_label(scr, "Congress Plaza & Michigan", 0, 10000);
  label_array[2] = create_static_label(scr, "DUE", 0, 0, true);
  label_array[3] = create_static_label(scr, "126", 0, 10, false);
  label_array[4] = create_scrolling_label(scr, "Congress Plaza & Michigan", 10, 10000);
  label_array[5] = create_static_label(scr, "DLY", 0, 10, true);
  label_array[6] = create_static_label(scr, "J14", 0, 20, false);
  label_array[7] = create_scrolling_label(scr, "Congress Plaza & Michigan", 20, 10000);
  label_array[8] = create_static_label(scr, "25", 0, 20, true);

  ESP_LOGI(TAG, "Demo UI created");
}
