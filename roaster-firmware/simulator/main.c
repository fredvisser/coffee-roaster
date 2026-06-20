#include <SDL.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lvgl.h>
#include <src/draw/snapshot/lv_snapshot.h>
#include <src/drivers/sdl/lv_sdl_keyboard.h>
#include <src/drivers/sdl/lv_sdl_mouse.h>
#include <src/drivers/sdl/lv_sdl_mousewheel.h>
#include <src/drivers/sdl/lv_sdl_window.h>

static const int kWidth = 480;
static const int kHeight = 272;
static const uint32_t kColorBackground = 0x111111;
static const uint32_t kColorHeader = 0x252525;
static const uint32_t kColorPanel = 0x1E1E1E;
static const uint32_t kColorPanelMuted = 0x262626;
static const uint32_t kColorTextPrimary = 0xF1F1F1;
static const uint32_t kColorTextMuted = 0x969696;
static const uint32_t kColorAccentReady = 0x08A86B;
static const uint32_t kColorAccentHeat = 0xFF3C45;
static const uint32_t kColorAccentCool = 0x0B607D;
static const uint32_t kColorAccentOutline = 0x454545;
static const uint32_t kColorProgress = 0xF59A23;
static const uint32_t kColorChartLine = 0xF6A21A;
static const uint32_t kColorChartGrid = 0x3B3B3B;
static const char *kTempUnit = "°F";

typedef struct Options {
  const char *screen;
  const char *outputBmp;
  float zoom;
  int warmupFrames;
  bool hidden;
} Options;

typedef struct ProfileEntry {
  const char *name;
  int finalTarget;
  bool active;
  bool selected;
} ProfileEntry;

static lv_obj_t *make_panel(lv_obj_t *parent,
                            int width,
                            int height,
                            uint32_t background,
                            uint32_t border,
                            int border_width)
{
  lv_obj_t *panel = lv_obj_create(parent);
  lv_obj_remove_style_all(panel);
  lv_obj_set_size(panel, width, height);
  lv_obj_set_style_bg_color(panel, lv_color_hex(background), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(border), 0);
  lv_obj_set_style_border_width(panel, border_width, 0);
  lv_obj_set_style_radius(panel, 0, 0);
  lv_obj_set_style_shadow_width(panel, 0, 0);
  lv_obj_set_style_pad_all(panel, 0, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  return panel;
}

static lv_obj_t *make_button(lv_obj_t *parent,
                             const char *text,
                             uint32_t background,
                             uint32_t text_color,
                             uint32_t border)
{
  lv_obj_t *button = lv_button_create(parent);
  lv_obj_set_style_radius(button, 0, 0);
  lv_obj_set_style_bg_color(button, lv_color_hex(background), 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(button, lv_color_hex(border), 0);
  lv_obj_set_style_border_width(button, 1, 0);
  lv_obj_set_style_shadow_width(button, 0, 0);
  lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *label = lv_label_create(button);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(text_color), 0);
  lv_label_set_text(label, text);
  lv_obj_center(label);
  return button;
}

static lv_obj_t *make_label(lv_obj_t *parent,
                            const char *text,
                            const lv_font_t *font,
                            uint32_t color,
                            lv_align_t align,
                            int x,
                            int y,
                            int width,
                            lv_text_align_t text_align)
{
  lv_obj_t *label = lv_label_create(parent);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
  if(width > 0) {
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(label, text_align, 0);
  }
  lv_label_set_text(label, text);
  lv_obj_align(label, align, x, y);
  return label;
}

static void add_header(lv_obj_t *root, const char *status, uint32_t accent_color)
{
  lv_obj_t *header = make_panel(root, kWidth, 30, kColorHeader, kColorAccentOutline, 1);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

  lv_obj_t *dot = make_panel(root, 10, 10, accent_color, accent_color, 0);
  lv_obj_align(dot, LV_ALIGN_TOP_LEFT, 12, 9);

  make_label(root, "ROASTER", &lv_font_montserrat_14, kColorTextPrimary, LV_ALIGN_TOP_LEFT, 28, 7, 0, LV_TEXT_ALIGN_LEFT);
  make_label(root, status, &lv_font_montserrat_14, kColorTextMuted, LV_ALIGN_TOP_RIGHT, -12, 7, 0, LV_TEXT_ALIGN_LEFT);
}

static void populate_chart(lv_obj_t *chart)
{
  static lv_coord_t values[] = {18, 30, 44, 62, 86, 108, 132, 160};
  lv_chart_series_t *series = lv_chart_add_series(chart, lv_color_hex(kColorChartLine), LV_CHART_AXIS_PRIMARY_Y);
  lv_chart_set_point_count(chart, (uint32_t)(sizeof(values) / sizeof(values[0])));
  lv_chart_set_series_values(chart, series, values, (uint32_t)(sizeof(values) / sizeof(values[0])));
  lv_chart_refresh(chart);
}

static void build_start_screen(lv_obj_t *root)
{
  add_header(root, "Start - Idle ready", kColorAccentReady);

  lv_obj_t *main_card = make_panel(root, 316, 156, kColorPanel, kColorAccentOutline, 1);
  lv_obj_align(main_card, LV_ALIGN_TOP_LEFT, 12, 44);
  make_label(main_card, "FINAL TARGET", &lv_font_montserrat_14, kColorTextMuted, LV_ALIGN_TOP_MID, 0, 18, 0, LV_TEXT_ALIGN_CENTER);
  make_label(main_card, "417", &lv_font_montserrat_48, kColorTextPrimary, LV_ALIGN_CENTER, -10, 6, 0, LV_TEXT_ALIGN_CENTER);
  make_label(main_card, kTempUnit, &lv_font_montserrat_22, kColorTextMuted, LV_ALIGN_CENTER, 80, 18, 0, LV_TEXT_ALIGN_LEFT);

  lv_obj_t *footer = make_panel(root, 316, 46, kColorPanel, kColorAccentOutline, 1);
  lv_obj_align(footer, LV_ALIGN_TOP_LEFT, 12, 210);
  make_label(footer, "Kenya Washed A*", &lv_font_montserrat_16, kColorTextPrimary, LV_ALIGN_LEFT_MID, 12, 0, 190, LV_TEXT_ALIGN_LEFT);
  lv_obj_t *save_button = make_button(footer, "Save", kColorAccentReady, kColorBackground, kColorAccentReady);
  lv_obj_set_size(save_button, 82, 30);
  lv_obj_align(save_button, LV_ALIGN_RIGHT_MID, -8, 0);

  lv_obj_t *start_button = make_button(root, "START", kColorAccentReady, kColorBackground, kColorAccentReady);
  lv_obj_set_size(start_button, 116, 84);
  lv_obj_align(start_button, LV_ALIGN_TOP_RIGHT, -12, 44);

  lv_obj_t *profiles_button = make_button(root, "Profiles", kColorPanelMuted, kColorTextPrimary, kColorAccentOutline);
  lv_obj_set_size(profiles_button, 116, 46);
  lv_obj_align(profiles_button, LV_ALIGN_TOP_RIGHT, -12, 136);

  lv_obj_t *wifi_button = make_button(root, "Wi-Fi", kColorPanelMuted, kColorTextPrimary, kColorAccentOutline);
  lv_obj_set_size(wifi_button, 116, 46);
  lv_obj_align(wifi_button, LV_ALIGN_TOP_RIGHT, -12, 190);
}

static void build_profile_list_screen(lv_obj_t *root)
{
  static const ProfileEntry entries[] = {
      {"Kenya Washed A", 412, false, false},
      {"Brazil Natural", 398, false, false},
      {"Guatemala City+", 425, false, true},
      {"Brazil Natural", 398, false, false},
  };

  add_header(root, "Profiles - Select one", kColorAccentReady);

  lv_obj_t *card = make_panel(root, 316, 214, kColorPanel, kColorAccentOutline, 1);
  lv_obj_align(card, LV_ALIGN_TOP_LEFT, 12, 44);

  lv_obj_t *list = lv_obj_create(card);
  lv_obj_set_size(list, 298, 198);
  lv_obj_align(list, LV_ALIGN_TOP_LEFT, 8, 8);
  lv_obj_set_style_radius(list, 0, 0);
  lv_obj_set_style_bg_color(list, lv_color_hex(kColorPanel), 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_style_pad_all(list, 0, 0);
  lv_obj_set_style_pad_row(list, 0, 0);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_layout(list, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);

  for(size_t index = 0; index < sizeof(entries) / sizeof(entries[0]); ++index) {
    const ProfileEntry *entry = &entries[index];
    lv_obj_t *row = lv_button_create(list);
    lv_obj_set_size(row, 298, 42);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_shadow_width(row, 0, 0);
    lv_obj_set_style_pad_left(row, 12, 0);
    lv_obj_set_style_pad_right(row, 12, 0);
    lv_obj_set_style_pad_top(row, 0, 0);
    lv_obj_set_style_pad_bottom(row, 0, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(kColorAccentOutline), 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(entry->selected ? kColorAccentReady : kColorPanelMuted), 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *name = lv_label_create(row);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(name, lv_color_hex(entry->selected ? kColorBackground : kColorTextPrimary), 0);
    lv_label_set_text(name, entry->name);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *temp = lv_label_create(row);
    lv_obj_set_style_text_font(temp, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(temp, lv_color_hex(entry->selected ? kColorBackground : kColorTextMuted), 0);
    lv_label_set_text_fmt(temp, "%d%s", entry->finalTarget, kTempUnit);
    lv_obj_align(temp, LV_ALIGN_RIGHT_MID, 0, 0);
  }

  lv_obj_t *use_button = make_button(root, "Use", kColorAccentReady, kColorBackground, kColorAccentReady);
  lv_obj_set_size(use_button, 116, 48);
  lv_obj_align(use_button, LV_ALIGN_TOP_RIGHT, -12, 44);

  lv_obj_t *graph_button = make_button(root, "Graph", kColorPanelMuted, kColorTextPrimary, kColorAccentOutline);
  lv_obj_set_size(graph_button, 116, 48);
  lv_obj_align(graph_button, LV_ALIGN_TOP_RIGHT, -12, 102);

  lv_obj_t *back_button = make_button(root, "Cancel", kColorPanelMuted, kColorTextPrimary, kColorAccentOutline);
  lv_obj_set_size(back_button, 116, 48);
  lv_obj_align(back_button, LV_ALIGN_TOP_RIGHT, -12, 160);
}

static void build_profile_graph_screen(lv_obj_t *root)
{
  add_header(root, "Profile graph - Guatemala City+", kColorAccentReady);

  lv_obj_t *card = make_panel(root, 316, 214, kColorPanel, kColorAccentOutline, 1);
  lv_obj_align(card, LV_ALIGN_TOP_LEFT, 12, 44);
  make_label(card, "Guatemala City+", &lv_font_montserrat_16, kColorTextPrimary, LV_ALIGN_TOP_LEFT, 12, 10, 180, LV_TEXT_ALIGN_LEFT);
  make_label(card, "Final 425°F", &lv_font_montserrat_16, kColorTextMuted, LV_ALIGN_TOP_RIGHT, -12, 10, 100, LV_TEXT_ALIGN_RIGHT);

  lv_obj_t *chart = lv_chart_create(card);
  lv_obj_set_size(chart, 252, 156);
  lv_obj_align(chart, LV_ALIGN_TOP_LEFT, 38, 34);
  lv_obj_set_style_radius(chart, 0, 0);
  lv_obj_set_style_bg_color(chart, lv_color_hex(0x191919), 0);
  lv_obj_set_style_border_color(chart, lv_color_hex(kColorAccentOutline), 0);
  lv_obj_set_style_border_width(chart, 1, 0);
  lv_obj_set_style_line_color(chart, lv_color_hex(kColorChartGrid), LV_PART_MAIN);
  lv_obj_set_style_line_color(chart, lv_color_hex(kColorChartLine), LV_PART_ITEMS);
  lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  lv_chart_set_axis_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 170);
  lv_chart_set_div_line_count(chart, 5, 6);
  populate_chart(chart);

  {
    static lv_coord_t reference_values[] = {10, 16, 28, 46, 66, 86, 102, 116};
    lv_chart_series_t *reference_series = lv_chart_add_series(chart, lv_color_hex(kColorAccentReady), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_series_values(chart, reference_series, reference_values, (uint32_t)(sizeof(reference_values) / sizeof(reference_values[0])));
    lv_chart_refresh(chart);
  }

  make_label(card, "425°F", &lv_font_montserrat_14, kColorTextMuted, LV_ALIGN_TOP_LEFT, 8, 36, 0, LV_TEXT_ALIGN_LEFT);
  make_label(card, "0°", &lv_font_montserrat_14, kColorTextMuted, LV_ALIGN_BOTTOM_LEFT, 8, -28, 0, LV_TEXT_ALIGN_LEFT);
  make_label(card, "0:00", &lv_font_montserrat_14, kColorTextMuted, LV_ALIGN_BOTTOM_LEFT, 38, -8, 0, LV_TEXT_ALIGN_LEFT);
  make_label(card, "12:00", &lv_font_montserrat_14, kColorTextMuted, LV_ALIGN_BOTTOM_RIGHT, -12, -8, 0, LV_TEXT_ALIGN_RIGHT);

  lv_obj_t *use_button = make_button(root, "Use", kColorAccentReady, kColorBackground, kColorAccentReady);
  lv_obj_set_size(use_button, 116, 48);
  lv_obj_align(use_button, LV_ALIGN_TOP_RIGHT, -12, 44);

  lv_obj_t *copy_button = make_button(root, "Copy", kColorPanelMuted, kColorTextPrimary, kColorAccentOutline);
  lv_obj_set_size(copy_button, 116, 48);
  lv_obj_align(copy_button, LV_ALIGN_TOP_RIGHT, -12, 102);

  lv_obj_t *back_button = make_button(root, "Back", kColorPanelMuted, kColorTextPrimary, kColorAccentOutline);
  lv_obj_set_size(back_button, 116, 48);
  lv_obj_align(back_button, LV_ALIGN_TOP_RIGHT, -12, 160);
}

static void build_roasting_screen(lv_obj_t *root)
{
  add_header(root, "Roasting - Heater on", kColorAccentHeat);

  lv_obj_t *main_card = make_panel(root, 316, 130, kColorPanel, kColorAccentOutline, 1);
  lv_obj_align(main_card, LV_ALIGN_TOP_LEFT, 12, 44);
  make_label(main_card, "BEAN TEMP", &lv_font_montserrat_14, kColorTextMuted, LV_ALIGN_TOP_LEFT, 12, 10, 0, LV_TEXT_ALIGN_LEFT);
  make_label(main_card, "356", &lv_font_montserrat_48, kColorTextPrimary, LV_ALIGN_LEFT_MID, 12, 10, 0, LV_TEXT_ALIGN_LEFT);
  make_label(main_card, kTempUnit, &lv_font_montserrat_22, kColorTextMuted, LV_ALIGN_LEFT_MID, 108, 17, 0, LV_TEXT_ALIGN_LEFT);
  make_label(main_card, "Target 394°F\nStop 425°F", &lv_font_montserrat_16, kColorTextPrimary, LV_ALIGN_RIGHT_MID, -18, 0, 126, LV_TEXT_ALIGN_RIGHT);

  lv_obj_t *footer = make_panel(root, 316, 58, kColorPanel, kColorAccentOutline, 1);
  lv_obj_align(footer, LV_ALIGN_TOP_LEFT, 12, 188);
  make_label(footer, "08:11 elapsed", &lv_font_montserrat_14, kColorTextPrimary, LV_ALIGN_TOP_LEFT, 12, 8, 0, LV_TEXT_ALIGN_LEFT);
  make_label(footer, "Fan 72% | Heat 61%", &lv_font_montserrat_14, kColorTextPrimary, LV_ALIGN_TOP_RIGHT, -12, 8, 172, LV_TEXT_ALIGN_RIGHT);

  lv_obj_t *progress = lv_bar_create(footer);
  lv_obj_remove_style_all(progress);
  lv_obj_set_size(progress, 284, 12);
  lv_obj_set_style_bg_color(progress, lv_color_hex(kColorPanelMuted), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(progress, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(progress, lv_color_hex(kColorProgress), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(progress, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_align(progress, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_bar_set_range(progress, 0, 100);
  lv_bar_set_value(progress, 68, LV_ANIM_OFF);

  lv_obj_t *stop_button = make_button(root, "STOP", kColorAccentHeat, kColorBackground, kColorAccentHeat);
  lv_obj_set_size(stop_button, 116, 124);
  lv_obj_align(stop_button, LV_ALIGN_TOP_RIGHT, -12, 44);
}

static void build_cooling_screen(lv_obj_t *root)
{
  add_header(root, "Cooling - Airflow max", kColorAccentCool);

  lv_obj_t *card = make_panel(root, 316, 144, 0x163A63, kColorAccentCool, 1);
  lv_obj_align(card, LV_ALIGN_TOP_LEFT, 12, 44);
  lv_obj_t *inner = make_panel(card, 192, 104, kColorPanel, kColorAccentCool, 1);
  lv_obj_align(inner, LV_ALIGN_LEFT_MID, 14, -6);
  make_label(inner, "COOLING ACTIVE", &lv_font_montserrat_14, 0x4EA2FF, LV_ALIGN_TOP_LEFT, 14, 14, 0, LV_TEXT_ALIGN_LEFT);
  make_label(inner, "184", &lv_font_montserrat_48, kColorTextPrimary, LV_ALIGN_LEFT_MID, 14, -2, 0, LV_TEXT_ALIGN_LEFT);
  make_label(inner, kTempUnit, &lv_font_montserrat_22, kColorTextMuted, LV_ALIGN_LEFT_MID, 108, 10, 0, LV_TEXT_ALIGN_LEFT);
  make_label(inner, "Cooling target: < 140°F", &lv_font_montserrat_22, kColorTextPrimary, LV_ALIGN_BOTTOM_LEFT, 14, -16, 164, LV_TEXT_ALIGN_LEFT);

  lv_obj_t *button = make_button(card, "Stop cooling", 0x2E77D9, kColorTextPrimary, 0x2E77D9);
  lv_obj_set_size(button, 116, 92);
  lv_obj_align(button, LV_ALIGN_RIGHT_MID, -12, -6);
}

static void build_network_screen(lv_obj_t *root)
{
  add_header(root, "Network - Ready", kColorAccentReady);
  make_label(root, "SSID", &lv_font_montserrat_14, kColorTextPrimary, LV_ALIGN_TOP_LEFT, 12, 44, 0, LV_TEXT_ALIGN_LEFT);
  make_label(root, "Password", &lv_font_montserrat_14, kColorTextPrimary, LV_ALIGN_TOP_LEFT, 12, 108, 0, LV_TEXT_ALIGN_LEFT);

  lv_obj_t *ssid = lv_textarea_create(root);
  lv_obj_set_size(ssid, 302, 38);
  lv_obj_align(ssid, LV_ALIGN_TOP_LEFT, 12, 62);
  lv_obj_set_style_radius(ssid, 0, 0);
  lv_obj_set_style_bg_color(ssid, lv_color_hex(kColorPanel), 0);
  lv_obj_set_style_bg_opa(ssid, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(ssid, lv_color_hex(kColorAccentOutline), 0);
  lv_obj_set_style_border_width(ssid, 1, 0);
  lv_obj_set_style_text_color(ssid, lv_color_hex(kColorTextPrimary), 0);
  lv_textarea_set_one_line(ssid, true);
  lv_textarea_set_text(ssid, "Roastery-Floor");

  lv_obj_t *password = lv_textarea_create(root);
  lv_obj_set_size(password, 302, 38);
  lv_obj_align(password, LV_ALIGN_TOP_LEFT, 12, 126);
  lv_obj_set_style_radius(password, 0, 0);
  lv_obj_set_style_bg_color(password, lv_color_hex(kColorPanel), 0);
  lv_obj_set_style_bg_opa(password, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(password, lv_color_hex(kColorAccentOutline), 0);
  lv_obj_set_style_border_width(password, 1, 0);
  lv_obj_set_style_text_color(password, lv_color_hex(kColorTextPrimary), 0);
  lv_textarea_set_one_line(password, true);
  lv_textarea_set_text(password, "password");

  lv_obj_t *apply_button = make_button(root, "Apply", kColorAccentReady, kColorBackground, kColorAccentReady);
  lv_obj_set_size(apply_button, 116, 72);
  lv_obj_align(apply_button, LV_ALIGN_TOP_RIGHT, -12, 62);

  lv_obj_t *done_button = make_button(root, "Done", kColorPanelMuted, kColorTextPrimary, kColorAccentOutline);
  lv_obj_set_size(done_button, 116, 52);
  lv_obj_align(done_button, LV_ALIGN_TOP_RIGHT, -12, 144);

  make_label(root, "IP: 10.0.4.22", &lv_font_montserrat_14, kColorTextMuted, LV_ALIGN_TOP_LEFT, 12, 172, 0, LV_TEXT_ALIGN_LEFT);
  make_label(root, "Firmware: 2.7.14", &lv_font_montserrat_14, kColorTextMuted, LV_ALIGN_TOP_LEFT, 12, 192, 0, LV_TEXT_ALIGN_LEFT);
}

static void build_error_screen(lv_obj_t *root)
{
  add_header(root, "Error - Start blocked", kColorAccentHeat);

  lv_obj_t *card = make_panel(root, 314, 136, kColorBackground, kColorAccentOutline, 1);
  lv_obj_align(card, LV_ALIGN_TOP_LEFT, 12, 44);

  lv_obj_t *message = make_panel(card, 198, 112, kColorPanel, kColorAccentHeat, 1);
  lv_obj_align(message, LV_ALIGN_LEFT_MID, 12, -2);
  make_label(message, "FAULT LOCKOUT", &lv_font_montserrat_14, kColorAccentHeat, LV_ALIGN_TOP_LEFT, 14, 14, 0, LV_TEXT_ALIGN_LEFT);
  make_label(message,
             "Bean temperature\nsensor\ndisconnected.",
             &lv_font_montserrat_32,
             kColorAccentHeat,
             LV_ALIGN_TOP_LEFT,
             14,
             34,
             172,
             LV_TEXT_ALIGN_LEFT);
  make_label(message,
             "Roast start is disabled until the\ncontroller reports a safe state.",
             &lv_font_montserrat_16,
             kColorTextMuted,
             LV_ALIGN_BOTTOM_LEFT,
             14,
             -14,
             172,
             LV_TEXT_ALIGN_LEFT);

  lv_obj_t *force_cooling = make_button(card, "Force cooling", kColorAccentHeat, kColorTextPrimary, kColorAccentHeat);
  lv_obj_set_size(force_cooling, 116, 64);
  lv_obj_align(force_cooling, LV_ALIGN_TOP_RIGHT, -12, 10);

  lv_obj_t *safe_idle = make_button(card, "Safe idle", kColorPanelMuted, kColorTextPrimary, kColorAccentOutline);
  lv_obj_set_size(safe_idle, 116, 48);
  lv_obj_align(safe_idle, LV_ALIGN_BOTTOM_RIGHT, -12, -12);
}

static void build_edit_setpoint_screen(lv_obj_t *root)
{
  add_header(root, "Final target - Idle override", kColorAccentReady);

  lv_obj_t *frame = make_panel(root, 384, 150, kColorBackground, kColorAccentOutline, 1);
  lv_obj_align(frame, LV_ALIGN_TOP_LEFT, 12, 44);

  lv_obj_t *card = make_panel(frame, 232, 122, kColorPanel, kColorAccentOutline, 1);
  lv_obj_align(card, LV_ALIGN_LEFT_MID, 12, -2);
  make_label(card, "FINAL TARGET", &lv_font_montserrat_14, kColorTextMuted, LV_ALIGN_TOP_MID, 0, 28, 0, LV_TEXT_ALIGN_CENTER);
  make_label(card, "418", &lv_font_montserrat_48, kColorTextPrimary, LV_ALIGN_CENTER, 0, 8, 0, LV_TEXT_ALIGN_CENTER);
  make_label(card, kTempUnit, &lv_font_montserrat_22, kColorTextMuted, LV_ALIGN_CENTER, 48, 22, 0, LV_TEXT_ALIGN_LEFT);
  make_label(card, "-", &lv_font_montserrat_32, kColorTextPrimary, LV_ALIGN_LEFT_MID, 26, 10, 0, LV_TEXT_ALIGN_LEFT);
  make_label(card, "+", &lv_font_montserrat_32, kColorTextPrimary, LV_ALIGN_RIGHT_MID, -26, 10, 0, LV_TEXT_ALIGN_RIGHT);

  lv_obj_t *done = make_button(frame, "Done", kColorAccentReady, kColorTextPrimary, kColorAccentReady);
  lv_obj_set_size(done, 116, 84);
  lv_obj_align(done, LV_ALIGN_RIGHT_MID, -12, -16);
}

static void build_scenario(const char *screen)
{
  lv_obj_t *root = lv_obj_create(NULL);
  lv_obj_remove_style_all(root);
  lv_obj_set_style_bg_color(root, lv_color_hex(kColorBackground), 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

  if(strcmp(screen, "start") == 0) {
    build_start_screen(root);
  }
  else if(strcmp(screen, "profile-list") == 0) {
    build_profile_list_screen(root);
  }
  else if(strcmp(screen, "profile-graph") == 0) {
    build_profile_graph_screen(root);
  }
  else if(strcmp(screen, "roasting") == 0) {
    build_roasting_screen(root);
  }
  else if(strcmp(screen, "cooling") == 0) {
    build_cooling_screen(root);
  }
  else if(strcmp(screen, "network") == 0) {
    build_network_screen(root);
  }
  else if(strcmp(screen, "error") == 0) {
    build_error_screen(root);
  }
  else if(strcmp(screen, "edit-setpoint") == 0) {
    build_edit_setpoint_screen(root);
  }
  else {
    build_start_screen(root);
  }

  lv_screen_load(root);
}

static bool parse_args(int argc, char **argv, Options *options)
{
  int index;

  for(index = 1; index < argc; ++index) {
    if(strcmp(argv[index], "--screen") == 0 && index + 1 < argc) {
      options->screen = argv[++index];
    }
    else if(strcmp(argv[index], "--output-bmp") == 0 && index + 1 < argc) {
      options->outputBmp = argv[++index];
    }
    else if(strcmp(argv[index], "--zoom") == 0 && index + 1 < argc) {
      options->zoom = (float)atof(argv[++index]);
    }
    else if(strcmp(argv[index], "--warmup-frames") == 0 && index + 1 < argc) {
      options->warmupFrames = atoi(argv[++index]);
    }
    else if(strcmp(argv[index], "--hidden") == 0) {
      options->hidden = true;
    }
    else if(strcmp(argv[index], "--help") == 0) {
      puts("Usage: roaster-lvgl-sim [--screen name] [--output-bmp file] [--zoom n] [--hidden]");
      return false;
    }
  }

  return true;
}

static bool save_screenshot(const char *output_path)
{
  lv_draw_buf_t *snapshot = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB888);
  SDL_Surface *surface;
  int rc;

  if(snapshot == NULL) {
    return false;
  }

  lv_draw_buf_flush_cache(snapshot, NULL);

  surface = SDL_CreateRGBSurfaceWithFormatFrom(snapshot->data,
                                               (int)snapshot->header.w,
                                               (int)snapshot->header.h,
                                               24,
                                               (int)snapshot->header.stride,
                                               SDL_PIXELFORMAT_BGR24);
  if(surface == NULL) {
    lv_draw_buf_destroy(snapshot);
    return false;
  }

  rc = SDL_SaveBMP(surface, output_path);

  SDL_FreeSurface(surface);
  lv_draw_buf_destroy(snapshot);
  return rc == 0;
}

int main(int argc, char **argv)
{
  Options options = {"start", NULL, 2.0f, 3, false};
  lv_display_t *display;
  SDL_Window *window;
  int frame;

#ifndef _WIN32
  setenv("DBUS_FATAL_WARNINGS", "0", 1);
#endif

  if(!parse_args(argc, argv, &options)) {
    return 0;
  }

  lv_init();

  display = lv_sdl_window_create(kWidth, kHeight);
  lv_sdl_window_set_zoom(display, options.zoom);
  lv_sdl_window_set_title(display, "Coffee Roaster LVGL Simulator");
  lv_sdl_mouse_create();
  lv_sdl_mousewheel_create();
  lv_sdl_keyboard_create();

  window = lv_sdl_window_get_window(display);
  if(options.hidden && window != NULL) {
    SDL_HideWindow(window);
  }

  build_scenario(options.screen);

  for(frame = 0; frame < options.warmupFrames; ++frame) {
    lv_timer_handler();
    SDL_Delay(5);
  }

  if(options.outputBmp != NULL) {
    if(!save_screenshot(options.outputBmp)) {
      fprintf(stderr, "Failed to save screenshot to %s\n", options.outputBmp);
      return 1;
    }
    return 0;
  }

  while(true) {
    lv_timer_handler();
    SDL_Delay(5);

    if(window != NULL && (SDL_GetWindowFlags(window) & SDL_WINDOW_SHOWN) == 0) {
      break;
    }
  }

  return 0;
}
