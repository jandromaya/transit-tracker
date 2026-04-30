// SPDX-FileCopyrightText: 2025 Stuart Parmenter
// SPDX-License-Identifier: MIT

// @file lvgl_demo_ui.c
// @brief Simple LVGL demo UI
//
// Based on ESP-IDF PARLIO RGB LED matrix LVGL example

#include "lvgl.h"
#include <esp_log.h>
#include <stdio.h>
// #include "main.h"

static const char *TAG = "lvgl_ui";
static const int bus_scroll_x_pos = 24;

// Create scrolling text label
static lv_obj_t* create_scrolling_label(lv_obj_t *parent, const char *text, int16_t x_pos,
                                        int16_t y_pos, int16_t width, uint32_t duration) {
    static lv_anim_t label_anim;
    static lv_style_t label_style;
    lv_obj_t *label = lv_label_create(parent);

    // SET UP LABEL ANIMATION/STYLE
    lv_anim_init(&label_anim);
    lv_anim_set_delay(&label_anim, 1500);
    lv_anim_set_repeat_delay(&label_anim, 1500);
    lv_anim_set_repeat_count(&label_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_duration(&label_anim, duration);

    lv_style_init(&label_style);
    lv_style_set_anim(&label_style, &label_anim);

    // CREATE/POSITION LABEL USING STYLE
    int16_t parent_width = lv_obj_get_width(parent);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_obj_set_width(label, width);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x_pos, y_pos);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_10, 0);
    lv_obj_add_style(label, &label_style, LV_STATE_DEFAULT);

    return label;
}

// scrolling label x position for buses is 24
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
void lvgl_ui(lv_obj_t *scr, lv_obj_t** label_array, int num_rows, int num_cols, bool train_ui) {
    // Set background color to black
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    // Disable scrollbars on screen
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // Get display size
    int16_t scr_width = lv_obj_get_width(scr);
    int16_t scr_height = lv_obj_get_height(scr);

    ESP_LOGI(TAG, "Creating UI for %dx%d display", scr_width, scr_height);

    int num_labels = num_cols * num_rows;

    if (train_ui) {
        for (int i = 0; i < num_labels; i += num_cols) {
            int y_pos = (i/num_cols) * 10;
            int16_t label_width = scr_width - 30;
            label_array[i] = create_scrolling_label(scr, "", 0, y_pos, label_width, 10000);
            label_array[i+1] = create_static_label(scr, "", 0, y_pos, true);
            lv_label_set_recolor(label_array[i], true);
        }
    } else {
        for (int i = 0; i < num_labels; i += num_cols) {
            int y_pos = (i/num_cols) * 10;
            int16_t label_width = scr_width - 50;
            label_array[i] = create_static_label(scr, "", 0, y_pos, false);
            label_array[i+1] = create_scrolling_label(scr, "", bus_scroll_x_pos, y_pos, label_width, 10000);
            label_array[i+2] = create_static_label(scr, "", 0, y_pos, true);
        }
    }  

    ESP_LOGI(TAG, "Demo UI created");
}
