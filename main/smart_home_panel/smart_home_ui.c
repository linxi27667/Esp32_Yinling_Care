#include "smart_home_ui.h"
#include "smart_home_mqtt.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "font_awesome.h"
#include "lvgl.h"

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);
LV_FONT_DECLARE(BUILTIN_ICON_FONT);
LV_FONT_DECLARE(font_awesome_30_4);

#define C_PRIMARY 0x1677FF
#define C_SUCCESS 0x22C55E
#define C_WARNING 0xF59E0B
#define C_DANGER  0xEF4444
#define C_TEXT    0x1F2937
#define C_SUB     0x6B7280
#define C_BORDER  0xE5E7EB
#define C_SURFACE 0xF8FAFC
#define C_WHITE   0xFFFFFF

typedef enum {
    PAGE_OVERVIEW = 0,
    PAGE_FLOOR,
    PAGE_SCENE,
    PAGE_DEVICE,
    PAGE_NETWORK,
    PAGE_SETTINGS,
    PAGE_PASSWORD,
    PAGE_COUNT
} page_id_t;

typedef struct {
    lv_obj_t *root;
    lv_obj_t *pages[PAGE_COUNT];
    lv_obj_t *nav_btns[6];
    lv_obj_t *time_labels[PAGE_COUNT];
    lv_obj_t *title_labels[PAGE_COUNT];
    lv_obj_t *mqtt_label;
    lv_obj_t *controller_status[SH_CTRL_COUNT];
    lv_obj_t *dev_value[SH_DEV_COUNT];
    lv_obj_t *dev_status[SH_DEV_COUNT];
    lv_obj_t *dev_online[SH_DEV_COUNT];
    lv_obj_t *metric_value[9];
    lv_obj_t *scene_status[3];
    lv_obj_t *floor_light_sw[3];
    lv_obj_t *floor_light_slider[3];
    lv_obj_t *rack2_slider;
    lv_obj_t *rack2_status;
    lv_obj_t *rain2_value;
    lv_obj_t *rain2_meta;
    lv_obj_t *network_state;
    lv_obj_t *network_ssid;
    lv_obj_t *password_title;
    lv_obj_t *password_textarea;
    lv_obj_t *keyboard;
    lv_obj_t *settings_wifi;
    lv_obj_t *settings_language;
    lv_obj_t *brightness_slider;
    page_id_t page;
    char selected_ssid[33];
    sh_state_t last;
} sh_ui_t;

static sh_ui_t g_ui;
static sh_mqtt_publish_fn_t g_publish;
static const lv_font_t *font_text;
static const lv_font_t *font_icon;
static const lv_font_t *font_icon_big;
static const char *TAG = "SmartHomeUI";
static bool g_keyboard_number_mode;

static const char *zh_nav[] = {"总览", "楼层", "场景", "设备", "网络", "设置"};
static const char *en_nav[] = {"Home", "Floors", "Scenes", "Devices", "Network", "Settings"};

static void set_text(lv_obj_t *obj, const char *text)
{
    if (obj) {
        lv_label_set_text(obj, text ? text : "--");
    }
}

static const char *tr(const char *zh, const char *en)
{
    return g_ui.last.language == SH_LANG_EN ? en : zh;
}

static void style_plain(lv_obj_t *obj)
{
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

static void style_card(lv_obj_t *obj)
{
    lv_obj_set_style_radius(obj, 16, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(C_WHITE), 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_pad_all(obj, 14, 0);
    lv_obj_set_style_shadow_width(obj, 10, 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_10, 0);
    lv_obj_set_style_shadow_color(obj, lv_color_hex(0x64748B), 0);
    lv_obj_set_style_shadow_offset_y(obj, 3, 0);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text, int color, const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(label, font ? font : font_text, 0);
    return label;
}

static lv_obj_t *make_card(lv_obj_t *parent, int w, int h)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    style_card(card);
    return card;
}

static lv_obj_t *make_chip(lv_obj_t *parent, const char *text, int color)
{
    lv_obj_t *chip = lv_label_create(parent);
    lv_label_set_text(chip, text);
    lv_obj_set_style_text_color(chip, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(chip, font_text, 0);
    lv_obj_set_style_bg_color(chip, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_10, 0);
    lv_obj_set_style_radius(chip, 10, 0);
    lv_obj_set_style_pad_hor(chip, 8, 0);
    lv_obj_set_style_pad_ver(chip, 4, 0);
    return chip;
}

static void update_clock(lv_timer_t *timer)
{
    (void)timer;
    char buf[16] = "--:--";
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    if (tm_now.tm_year >= 125) {
        strftime(buf, sizeof(buf), "%H:%M", &tm_now);
    }
    for (int i = 0; i < PAGE_COUNT; ++i) {
        set_text(g_ui.time_labels[i], buf);
    }
}

static void show_page(page_id_t page)
{
    g_ui.page = page;
    for (int i = 0; i < PAGE_COUNT; ++i) {
        if (!g_ui.pages[i]) continue;
        if (i == page) lv_obj_remove_flag(g_ui.pages[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(g_ui.pages[i], LV_OBJ_FLAG_HIDDEN);
    }
    for (int i = 0; i < 6; ++i) {
        if (!g_ui.nav_btns[i]) continue;
        uint32_t bg = i == (int)page ? 0xEAF3FF : C_WHITE;
        uint32_t fg = i == (int)page ? C_PRIMARY : C_SUB;
        lv_obj_set_style_bg_color(g_ui.nav_btns[i], lv_color_hex(bg), 0);
        lv_obj_set_style_text_color(g_ui.nav_btns[i], lv_color_hex(fg), 0);
    }
}

static void nav_event_cb(lv_event_t *e)
{
    page_id_t page = (page_id_t)(intptr_t)lv_event_get_user_data(e);
    show_page(page);
}

static lv_obj_t *create_status_bar(lv_obj_t *parent, const char *title, page_id_t page)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, LV_PCT(100), 54);
    style_plain(bar);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    g_ui.title_labels[page] = make_label(bar, title, C_TEXT, font_text);
    lv_obj_set_style_text_font(g_ui.title_labels[page], font_text, 0);

    lv_obj_t *right = lv_obj_create(bar);
    style_plain(right);
    lv_obj_set_size(right, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(right, 14, 0);

    g_ui.time_labels[page] = make_label(right, "--:--", C_TEXT, font_text);
    make_label(right, FONT_AWESOME_WIFI, C_TEXT, font_icon);
    if (page == PAGE_OVERVIEW) {
        make_chip(right, "局域网：未连接", C_WARNING);
        g_ui.mqtt_label = make_chip(right, "MQTT：等待", C_WARNING);
    }
    return bar;
}

static lv_obj_t *create_shell_page(page_id_t page, const char *title)
{
    lv_obj_t *content = lv_obj_create(g_ui.root);
    lv_obj_set_pos(content, 96, 0);
    lv_obj_set_size(content, 928, 600);
    lv_obj_set_style_bg_color(content, lv_color_hex(C_SURFACE), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_left(content, 18, 0);
    lv_obj_set_style_pad_right(content, 18, 0);
    lv_obj_set_style_pad_top(content, 10, 0);
    lv_obj_set_style_pad_bottom(content, 12, 0);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(content, 10, 0);
    create_status_bar(content, title, page);
    g_ui.pages[page] = content;
    return content;
}

static void create_sidebar(void)
{
    lv_obj_t *side = lv_obj_create(g_ui.root);
    lv_obj_set_pos(side, 0, 0);
    lv_obj_set_size(side, 96, 600);
    lv_obj_set_style_bg_color(side, lv_color_hex(C_WHITE), 0);
    lv_obj_set_style_border_width(side, 1, 0);
    lv_obj_set_style_border_color(side, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_pad_all(side, 12, 0);
    lv_obj_set_style_pad_row(side, 12, 0);
    lv_obj_set_flex_flow(side, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(side, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    const char *icons[] = {
        FONT_AWESOME_HOUSE, FONT_AWESOME_SIGNAL, FONT_AWESOME_STAR,
        FONT_AWESOME_MICROCHIP_AI, FONT_AWESOME_WIFI, FONT_AWESOME_GEAR
    };
    for (int i = 0; i < 6; ++i) {
        lv_obj_t *btn = lv_button_create(side);
        lv_obj_set_size(btn, 68, 62);
        lv_obj_set_style_radius(btn, 10, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(C_WHITE), 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 4, 0);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_event_cb(btn, nav_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        make_label(btn, icons[i], C_SUB, font_icon);
        make_label(btn, zh_nav[i], C_SUB, font_text);
        g_ui.nav_btns[i] = btn;
    }
}

static lv_obj_t *create_device_tile(lv_obj_t *parent, sh_device_id_t id, const char *icon, const char *name, const char *type)
{
    lv_obj_t *card = make_card(parent, 0, 96);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(card, 12, 0);

    make_label(card, icon, C_PRIMARY, font_icon_big);

    lv_obj_t *col = lv_obj_create(card);
    style_plain(col);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 3, 0);
    make_label(col, name, C_TEXT, font_text);
    make_label(col, type, C_SUB, font_text);
    g_ui.dev_value[id] = make_label(col, "--", C_SUB, font_text);
    g_ui.dev_status[id] = make_label(col, "等待MQTT", C_SUCCESS, font_text);
    g_ui.dev_online[id] = make_label(col, "● 离线", C_SUB, font_text);
    return card;
}

static void create_controller_cards(lv_obj_t *page)
{
    lv_obj_t *row = lv_obj_create(page);
    style_plain(row);
    lv_obj_set_size(row, LV_PCT(100), 82);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 12, 0);

    for (int i = 0; i < SH_CTRL_COUNT; ++i) {
        lv_obj_t *card = make_card(row, 0, 70);
        lv_obj_set_flex_grow(card, 1);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(card, 12, 0);
        make_label(card, FONT_AWESOME_MICROCHIP_AI, C_TEXT, font_icon_big);
        lv_obj_t *col = lv_obj_create(card);
        style_plain(col);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(col, 4, 0);
        char name[32];
        snprintf(name, sizeof(name), "%d楼控制器", i + 1);
        make_label(col, name, C_TEXT, font_text);
        g_ui.controller_status[i] = make_label(col, "● 等待MQTT", C_SUB, font_text);
    }

    lv_obj_t *summary = make_card(row, 250, 70);
    lv_obj_set_flex_flow(summary, LV_FLEX_FLOW_COLUMN);
    make_label(summary, "系统摘要", C_TEXT, font_text);
    g_ui.network_state = make_label(summary, "在线设备 --  在线传感器 --  系统状态 --", C_SUB, font_text);
}

static void create_overview_page(void)
{
    lv_obj_t *page = create_shell_page(PAGE_OVERVIEW, "智慧家庭控制面板");
    create_controller_cards(page);

    lv_obj_t *floors = lv_obj_create(page);
    style_plain(floors);
    lv_obj_set_size(floors, LV_PCT(100), 272);
    lv_obj_set_flex_flow(floors, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(floors, 12, 0);

    lv_obj_t *f1 = make_card(floors, 0, 270);
    lv_obj_set_flex_grow(f1, 1);
    lv_obj_set_flex_flow(f1, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(f1, 10, 0);
    make_label(f1, "一楼设备", C_TEXT, font_text);
    lv_obj_t *f1row = lv_obj_create(f1);
    style_plain(f1row);
    lv_obj_set_size(f1row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(f1row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(f1row, 10, 0);
    create_device_tile(f1row, SH_DEV_DOOR, FONT_AWESOME_LOCK, "大门", "门磁");
    create_device_tile(f1row, SH_DEV_HALL_LIGHT, FONT_AWESOME_SUN, "大厅灯", "吸顶灯");

    lv_obj_t *f2 = make_card(floors, 0, 270);
    lv_obj_set_flex_grow(f2, 2);
    lv_obj_set_flex_flow(f2, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(f2, 10, 0);
    make_label(f2, "二楼设备", C_TEXT, font_text);
    lv_obj_t *f2grid = lv_obj_create(f2);
    style_plain(f2grid);
    lv_obj_set_size(f2grid, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(f2grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(f2grid, 8, 0);
    lv_obj_set_style_pad_row(f2grid, 8, 0);
    create_device_tile(f2grid, SH_DEV_LIVING_LIGHT, FONT_AWESOME_SUN, "客厅灯", "吸顶灯");
    create_device_tile(f2grid, SH_DEV_BATH_LIGHT, FONT_AWESOME_BRIGHTNESS, "厕所灯", "吸顶灯");
    create_device_tile(f2grid, SH_DEV_MASTER_LIGHT, FONT_AWESOME_CLOUD_SUN, "主卧灯", "吸顶灯");
    create_device_tile(f2grid, SH_DEV_RAIN_2, FONT_AWESOME_CLOUD_DRIZZLE, "雨滴传感器", "雨滴值");
    create_device_tile(f2grid, SH_DEV_RACK_2, FONT_AWESOME_ARROWS_REPEAT, "二楼晾衣杆", "状态/模式");

    lv_obj_t *f3 = make_card(floors, 0, 270);
    lv_obj_set_flex_grow(f3, 2);
    lv_obj_set_flex_flow(f3, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(f3, 10, 0);
    make_label(f3, "三楼设备", C_TEXT, font_text);
    lv_obj_t *f3grid = lv_obj_create(f3);
    style_plain(f3grid);
    lv_obj_set_size(f3grid, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(f3grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(f3grid, 8, 0);
    lv_obj_set_style_pad_row(f3grid, 8, 0);
    create_device_tile(f3grid, SH_DEV_BALCONY_LIGHT, FONT_AWESOME_CLOUD_SUN, "阳台灯", "吸顶灯");
    create_device_tile(f3grid, SH_DEV_SKYLIGHT, FONT_AWESOME_CLOUD, "天窗", "开度");
    create_device_tile(f3grid, SH_DEV_RACK_3, FONT_AWESOME_ARROWS_REPEAT, "三楼晾衣杆", "展开度");
    create_device_tile(f3grid, SH_DEV_RAIN_3, FONT_AWESOME_CLOUD_DRIZZLE, "雨滴传感器", "雨滴值");
    create_device_tile(f3grid, SH_DEV_FLAME_3, FONT_AWESOME_CLOUD_BOLT, "火焰传感器", "火焰值");

    lv_obj_t *bottom = lv_obj_create(page);
    style_plain(bottom);
    lv_obj_set_size(bottom, LV_PCT(100), 116);
    lv_obj_set_flex_flow(bottom, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(bottom, 12, 0);

    lv_obj_t *metrics = make_card(bottom, 0, 112);
    lv_obj_set_flex_grow(metrics, 3);
    lv_obj_set_flex_flow(metrics, LV_FLEX_FLOW_COLUMN);
    make_label(metrics, "环境与系统实时数据", C_TEXT, font_text);
    lv_obj_t *mrow = lv_obj_create(metrics);
    style_plain(mrow);
    lv_obj_set_size(mrow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(mrow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(mrow, 8, 0);
    const char *metric_names[] = {"室内温度", "室内湿度", "空气质量", "二楼雨滴", "三楼雨滴", "火焰值", "天窗开度", "总功率", "固件版本"};
    for (int i = 0; i < 9; ++i) {
        lv_obj_t *tile = lv_obj_create(mrow);
        lv_obj_set_size(tile, 76, 56);
        lv_obj_set_style_radius(tile, 10, 0);
        lv_obj_set_style_border_color(tile, lv_color_hex(C_BORDER), 0);
        lv_obj_set_style_border_width(tile, 1, 0);
        lv_obj_set_style_bg_color(tile, lv_color_hex(C_WHITE), 0);
        lv_obj_set_style_pad_all(tile, 5, 0);
        lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
        g_ui.metric_value[i] = make_label(tile, "--", C_TEXT, font_text);
        make_label(tile, metric_names[i], C_SUB, font_text);
    }

    lv_obj_t *scenes = make_card(bottom, 0, 112);
    lv_obj_set_flex_grow(scenes, 1);
    lv_obj_set_flex_flow(scenes, LV_FLEX_FLOW_COLUMN);
    make_label(scenes, "当前场景", C_TEXT, font_text);
    lv_obj_t *srow = lv_obj_create(scenes);
    style_plain(srow);
    lv_obj_set_size(srow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(srow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(srow, 8, 0);
    const char *scene_names[] = {"回家模式", "离家模式", "夜间模式"};
    const char *scene_icons[] = {FONT_AWESOME_HOUSE, FONT_AWESOME_LOCK, FONT_AWESOME_MOON};
    for (int i = 0; i < 3; ++i) {
        lv_obj_t *scene = make_card(srow, 88, 58);
        lv_obj_set_style_radius(scene, 12, 0);
        lv_obj_set_style_pad_all(scene, 7, 0);
        lv_obj_set_flex_flow(scene, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(scene, 6, 0);
        make_label(scene, scene_icons[i], i == 2 ? C_SUCCESS : C_PRIMARY, font_icon);
        lv_obj_t *col = lv_obj_create(scene);
        style_plain(col);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        make_label(col, scene_names[i], C_TEXT, font_text);
        g_ui.scene_status[i] = make_label(col, "未激活", C_SUB, font_text);
    }
}

static sh_device_id_t light_id_by_index(int idx)
{
    return idx == 0 ? SH_DEV_LIVING_LIGHT : (idx == 1 ? SH_DEV_BATH_LIGHT : SH_DEV_MASTER_LIGHT);
}

static void publish_light(sh_device_id_t id, bool power, int brightness)
{
    char payload[96];
    snprintf(payload, sizeof(payload), "{\"power\":%s,\"brightness\":%d}", power ? "true" : "false", brightness);
    smart_home_mqtt_publish_command(id, payload);
}

static void light_switch_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    sh_device_id_t id = light_id_by_index(idx);
    bool checked = lv_obj_has_state((lv_obj_t *)lv_event_get_target(e), LV_STATE_CHECKED);
    int brightness = lv_slider_get_value(g_ui.floor_light_slider[idx]);
    publish_light(id, checked, brightness);
}

static void light_slider_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    sh_device_id_t id = light_id_by_index(idx);
    bool power = lv_obj_has_state(g_ui.floor_light_sw[idx], LV_STATE_CHECKED);
    int brightness = lv_slider_get_value((lv_obj_t *)lv_event_get_target(e));
    publish_light(id, power, brightness);
}

static void rack_slider_cb(lv_event_t *e)
{
    int pos = lv_slider_get_value((lv_obj_t *)lv_event_get_target(e));
    char payload[96];
    snprintf(payload, sizeof(payload), "{\"position\":%d,\"mode\":\"manual\"}", pos);
    smart_home_mqtt_publish_command(SH_DEV_RACK_2, payload);
}

static void create_floor_page(void)
{
    lv_obj_t *page = create_shell_page(PAGE_FLOOR, "二楼设备");
    lv_obj_t *auto_chip = make_chip(lv_obj_get_child(page, 0), "自动化：等待MQTT", C_SUCCESS);
    lv_obj_align(auto_chip, LV_ALIGN_RIGHT_MID, -12, 0);

    make_label(page, "A. 灯光控制", C_TEXT, font_text);
    lv_obj_t *lights = lv_obj_create(page);
    style_plain(lights);
    lv_obj_set_size(lights, LV_PCT(100), 138);
    lv_obj_set_flex_flow(lights, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(lights, 14, 0);
    const char *names[] = {"客厅灯", "厕所灯", "主卧灯"};
    int defaults[] = {80, 20, 70};
    for (int i = 0; i < 3; ++i) {
        lv_obj_t *card = make_card(lights, 0, 134);
        lv_obj_set_flex_grow(card, 1);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(card, 8, 0);
        lv_obj_t *top = lv_obj_create(card);
        style_plain(top);
        lv_obj_set_size(top, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        make_label(top, names[i], C_TEXT, font_text);
        g_ui.floor_light_sw[i] = lv_switch_create(top);
        lv_obj_set_size(g_ui.floor_light_sw[i], 52, 28);
        lv_obj_add_event_cb(g_ui.floor_light_sw[i], light_switch_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)i);
        make_label(card, "吸顶灯 / 等待MQTT", C_SUB, font_text);
        g_ui.floor_light_slider[i] = lv_slider_create(card);
        lv_obj_set_width(g_ui.floor_light_slider[i], LV_PCT(100));
        lv_slider_set_range(g_ui.floor_light_slider[i], 0, 100);
        lv_slider_set_value(g_ui.floor_light_slider[i], defaults[i], LV_ANIM_OFF);
        lv_obj_add_event_cb(g_ui.floor_light_slider[i], light_slider_cb, LV_EVENT_RELEASED, (void *)(intptr_t)i);
    }

    make_label(page, "B. 晾衣杆控制", C_TEXT, font_text);
    lv_obj_t *rack = make_card(page, LV_PCT(100), 112);
    lv_obj_set_flex_flow(rack, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(rack, 10, 0);
    lv_obj_t *r_top = lv_obj_create(rack);
    style_plain(r_top);
    lv_obj_set_size(r_top, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r_top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r_top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    make_label(r_top, "二楼晾衣杆", C_TEXT, font_text);
    g_ui.rack2_status = make_label(r_top, "模式：--    状态：--", C_SUCCESS, font_text);
    g_ui.rack2_slider = lv_slider_create(rack);
    lv_obj_set_width(g_ui.rack2_slider, LV_PCT(100));
    lv_slider_set_range(g_ui.rack2_slider, 0, 100);
    lv_obj_add_event_cb(g_ui.rack2_slider, rack_slider_cb, LV_EVENT_RELEASED, NULL);

    make_label(page, "C. 传感器", C_TEXT, font_text);
    lv_obj_t *sensor = make_card(page, LV_PCT(100), 112);
    lv_obj_set_flex_flow(sensor, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sensor, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(sensor, 26, 0);
    make_label(sensor, FONT_AWESOME_CLOUD_DRIZZLE, C_PRIMARY, font_icon_big);
    g_ui.rain2_value = make_label(sensor, "--", C_TEXT, font_text);
    lv_obj_set_style_text_font(g_ui.rain2_value, font_text, 0);
    g_ui.rain2_meta = make_label(sensor, "ADC: --\n状态: --\n更新时间: --", C_SUB, font_text);
}

static void ssid_click_cb(lv_event_t *e)
{
    const char *ssid = (const char *)lv_event_get_user_data(e);
    snprintf(g_ui.selected_ssid, sizeof(g_ui.selected_ssid), "%s", ssid);
    char title[64];
    snprintf(title, sizeof(title), "连接到 %s", ssid);
    set_text(g_ui.password_title, title);
    show_page(PAGE_PASSWORD);
}

static void password_cancel_cb(lv_event_t *e)
{
    (void)e;
    show_page(PAGE_NETWORK);
}

static void password_connect_cb(lv_event_t *e)
{
    (void)e;
    const char *pwd = lv_textarea_get_text(g_ui.password_textarea);
    char payload[160];
    snprintf(payload, sizeof(payload), "{\"ssid\":\"%s\",\"password\":\"%s\"}", g_ui.selected_ssid, pwd ? pwd : "");
    if (g_publish) {
        g_publish("smart-home/p4/network/wifi/connect", payload, 1, false);
    }
    show_page(PAGE_NETWORK);
}

static const char *kb_map_lower[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", "\n",
    "123", "z", "x", "c", "v", "b", "n", "m", "删除", "\n",
    "完成", "空格", ".", "_", "-", NULL
};

static const char *kb_map_number[] = {
    "1", "2", "3", "\n",
    "4", "5", "6", "\n",
    "7", "8", "9", "\n",
    "ABC", "0", "删除", "\n",
    "完成", ".", "_", "-", NULL
};

static void keyboard_cb(lv_event_t *e)
{
    lv_obj_t *matrix = (lv_obj_t *)lv_event_get_target(e);
    const char *txt = lv_buttonmatrix_get_button_text(matrix, lv_buttonmatrix_get_selected_button(matrix));
    if (!txt) return;
    if (!strcmp(txt, "删除")) {
        lv_textarea_delete_char(g_ui.password_textarea);
    } else if (!strcmp(txt, "空格")) {
        lv_textarea_add_char(g_ui.password_textarea, ' ');
    } else if (!strcmp(txt, "完成")) {
        password_connect_cb(e);
    } else if (!strcmp(txt, "123")) {
        g_keyboard_number_mode = true;
        lv_buttonmatrix_set_map(matrix, kb_map_number);
    } else if (!strcmp(txt, "ABC")) {
        g_keyboard_number_mode = false;
        lv_buttonmatrix_set_map(matrix, kb_map_lower);
    } else {
        lv_textarea_add_text(g_ui.password_textarea, txt);
    }
}

static void scan_cb(lv_event_t *e)
{
    (void)e;
    if (g_publish) {
        g_publish("smart-home/p4/network/wifi/scan", "{}", 1, false);
    }
}

static void create_network_page(void)
{
    lv_obj_t *page = create_shell_page(PAGE_NETWORK, "网络连接");
    g_ui.network_ssid = make_label(page, "当前状态：未连接", C_TEXT, font_text);

    lv_obj_t *list = make_card(page, LV_PCT(100), 370);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 7, 0);
    make_label(list, "可用网络", C_SUB, font_text);
    const char *ssids[] = {"Home_5G", "MiHome_2.4G", "OfficeWiFi", "ESP32-Test", "Guest"};
    const char *signals[] = {FONT_AWESOME_WIFI, FONT_AWESOME_WIFI_FAIR, FONT_AWESOME_WIFI_FAIR, FONT_AWESOME_WIFI_WEAK, FONT_AWESOME_WIFI_WEAK};
    for (int i = 0; i < 5; ++i) {
        lv_obj_t *row = lv_button_create(list);
        lv_obj_set_size(row, LV_PCT(100), 48);
        lv_obj_set_style_radius(row, 10, 0);
        lv_obj_set_style_bg_color(row, lv_color_hex(i == 0 ? 0xEAF3FF : C_WHITE), 0);
        lv_obj_set_style_shadow_width(row, 0, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_event_cb(row, ssid_click_cb, LV_EVENT_CLICKED, (void *)ssids[i]);
        lv_obj_t *left = lv_obj_create(row);
        style_plain(left);
        lv_obj_set_flex_flow(left, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(left, 12, 0);
        make_label(left, FONT_AWESOME_WIFI, C_SUB, font_icon);
        make_label(left, ssids[i], i == 0 ? C_PRIMARY : C_TEXT, font_text);
        lv_obj_t *right = lv_obj_create(row);
        style_plain(right);
        lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(right, 14, 0);
        make_label(right, FONT_AWESOME_LOCK, C_TEXT, font_icon);
        make_label(right, signals[i], C_TEXT, font_icon);
    }

    lv_obj_t *btns = lv_obj_create(page);
    style_plain(btns);
    lv_obj_set_size(btns, LV_PCT(100), 58);
    lv_obj_set_flex_flow(btns, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(btns, 14, 0);
    const char *names[] = {"手动添加网络", "重新扫描"};
    for (int i = 0; i < 2; ++i) {
        lv_obj_t *btn = lv_button_create(btns);
        lv_obj_set_size(btn, 180, 44);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(i ? C_PRIMARY : C_WHITE), 0);
        lv_obj_set_style_border_width(btn, i ? 0 : 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(C_PRIMARY), 0);
        lv_obj_add_event_cb(btn, scan_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *label = make_label(btn, names[i], i ? C_WHITE : C_PRIMARY, font_text);
        lv_obj_center(label);
    }
}

static void create_password_page(void)
{
    lv_obj_t *page = create_shell_page(PAGE_PASSWORD, "连接到 Home_5G");
    g_ui.password_title = g_ui.title_labels[PAGE_PASSWORD];

    lv_obj_t *box = make_card(page, LV_PCT(100), 114);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(box, 10, 0);
    make_label(box, "密码", C_SUB, font_text);
    g_ui.password_textarea = lv_textarea_create(box);
    lv_textarea_set_password_mode(g_ui.password_textarea, true);
    lv_textarea_set_one_line(g_ui.password_textarea, true);
    lv_obj_set_size(g_ui.password_textarea, LV_PCT(100), 42);
    lv_obj_set_style_radius(g_ui.password_textarea, 8, 0);
    lv_obj_set_style_border_color(g_ui.password_textarea, lv_color_hex(C_PRIMARY), 0);

    lv_obj_t *btns = lv_obj_create(box);
    style_plain(btns);
    lv_obj_set_size(btns, LV_PCT(100), 44);
    lv_obj_set_flex_flow(btns, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(btns, 14, 0);
    lv_obj_t *cancel = lv_button_create(btns);
    lv_obj_set_size(cancel, 160, 40);
    lv_obj_set_style_bg_color(cancel, lv_color_hex(0xEEF2F7), 0);
    lv_obj_add_event_cb(cancel, password_cancel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_center(make_label(cancel, "取消", C_TEXT, font_text));
    lv_obj_t *connect = lv_button_create(btns);
    lv_obj_set_size(connect, 160, 40);
    lv_obj_set_style_bg_color(connect, lv_color_hex(C_PRIMARY), 0);
    lv_obj_add_event_cb(connect, password_connect_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_center(make_label(connect, "连接", C_WHITE, font_text));

    g_keyboard_number_mode = false;
    g_ui.keyboard = lv_buttonmatrix_create(page);
    lv_obj_set_size(g_ui.keyboard, LV_PCT(100), 330);
    lv_buttonmatrix_set_map(g_ui.keyboard, kb_map_lower);
    lv_obj_set_style_radius(g_ui.keyboard, 10, 0);
    lv_obj_set_style_bg_color(g_ui.keyboard, lv_color_hex(0xEEF2F7), 0);
    lv_obj_set_style_border_width(g_ui.keyboard, 0, 0);
    lv_obj_set_style_pad_all(g_ui.keyboard, 8, 0);
    lv_obj_set_style_pad_row(g_ui.keyboard, 6, 0);
    lv_obj_set_style_pad_column(g_ui.keyboard, 6, 0);
    lv_obj_set_style_radius(g_ui.keyboard, 8, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(g_ui.keyboard, lv_color_hex(C_WHITE), LV_PART_ITEMS);
    lv_obj_set_style_text_color(g_ui.keyboard, lv_color_hex(C_TEXT), LV_PART_ITEMS);
    lv_obj_add_event_cb(g_ui.keyboard, keyboard_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

static void language_cb(lv_event_t *e)
{
    (void)e;
    sh_language_t next = g_ui.last.language == SH_LANG_ZH ? SH_LANG_EN : SH_LANG_ZH;
    smart_home_ui_set_language(next);
}

static void create_settings_page(void)
{
    lv_obj_t *page = create_shell_page(PAGE_SETTINGS, "设置");
    lv_obj_t *list = make_card(page, LV_PCT(100), 460);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 6, 0);

    const char *icons[] = {FONT_AWESOME_WIFI, FONT_AWESOME_GLOBE, FONT_AWESOME_BRIGHTNESS, FONT_AWESOME_VOLUME, FONT_AWESOME_CIRCLE_INFO, FONT_AWESOME_CLOUD_ARROW_UP};
    const char *names[] = {"网络设置", "语言切换", "屏幕亮度", "声音", "关于设备", "固件版本"};
    for (int i = 0; i < 6; ++i) {
        lv_obj_t *row = lv_obj_create(list);
        style_plain(row);
        lv_obj_set_size(row, LV_PCT(100), 58);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t *left = lv_obj_create(row);
        style_plain(left);
        lv_obj_set_flex_flow(left, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(left, 12, 0);
        make_label(left, icons[i], C_SUB, font_icon);
        make_label(left, names[i], C_TEXT, font_text);
        if (i == 0) g_ui.settings_wifi = make_label(row, "未连接", C_SUB, font_text);
        else if (i == 1) {
            lv_obj_t *btn = lv_button_create(row);
            lv_obj_set_size(btn, 150, 36);
            lv_obj_set_style_bg_color(btn, lv_color_hex(C_PRIMARY), 0);
            lv_obj_set_style_radius(btn, 8, 0);
            lv_obj_add_event_cb(btn, language_cb, LV_EVENT_CLICKED, NULL);
            g_ui.settings_language = make_label(btn, "简体中文", C_WHITE, font_text);
            lv_obj_center(g_ui.settings_language);
        } else if (i == 2) {
            g_ui.brightness_slider = lv_slider_create(row);
            lv_obj_set_width(g_ui.brightness_slider, 280);
            lv_slider_set_range(g_ui.brightness_slider, 1, 100);
            lv_slider_set_value(g_ui.brightness_slider, 80, LV_ANIM_OFF);
        } else if (i == 3) make_label(row, "中等", C_SUB, font_text);
        else if (i == 4) make_label(row, "ESP32-P4 智能面板", C_SUB, font_text);
        else make_label(row, SH_FIRMWARE_VERSION, C_SUB, font_text);
    }
}

static void create_placeholder_page(page_id_t page, const char *title, const char *label)
{
    lv_obj_t *p = create_shell_page(page, title);
    lv_obj_t *card = make_card(p, LV_PCT(100), 460);
    lv_obj_center(make_label(card, label, C_SUB, font_text));
}

static void device_text(sh_device_id_t id, const sh_device_state_t *d, char *value, size_t value_len, char *status, size_t status_len)
{
    if (!d || !d->valid) {
        snprintf(value, value_len, "--");
        snprintf(status, status_len, "等待MQTT");
        return;
    }
    switch (id) {
        case SH_DEV_DOOR:
            snprintf(value, value_len, "%s", d->power ? "已打开" : "已关闭");
            break;
        case SH_DEV_HALL_LIGHT:
        case SH_DEV_LIVING_LIGHT:
        case SH_DEV_BATH_LIGHT:
        case SH_DEV_MASTER_LIGHT:
        case SH_DEV_BALCONY_LIGHT:
            snprintf(value, value_len, "%s", d->power ? "已开启" : "已关闭");
            break;
        case SH_DEV_RAIN_2:
        case SH_DEV_RAIN_3:
            snprintf(value, value_len, "雨滴值：%ld%%", (long)d->value);
            break;
        case SH_DEV_SKYLIGHT:
            snprintf(value, value_len, "开度：%ld%%", (long)d->percent);
            break;
        case SH_DEV_RACK_2:
        case SH_DEV_RACK_3:
            snprintf(value, value_len, "%s：%ld%%", d->status[0] ? d->status : "位置", (long)d->percent);
            break;
        case SH_DEV_FLAME_3:
            snprintf(value, value_len, "火焰值：%ld/1023", (long)d->value);
            break;
        default:
            snprintf(value, value_len, "--");
            break;
    }
    snprintf(status, status_len, "%s", d->status[0] ? d->status : (d->online ? "正常" : "离线"));
}

static void update_metrics(void)
{
    char buf[64];
    if (g_ui.last.env.valid) {
        snprintf(buf, sizeof(buf), "%.1f℃", g_ui.last.env.temperature);
        set_text(g_ui.metric_value[0], buf);
        snprintf(buf, sizeof(buf), "%ld%%", (long)g_ui.last.env.humidity);
        set_text(g_ui.metric_value[1], buf);
        snprintf(buf, sizeof(buf), "优 PM2.5 %ld", (long)g_ui.last.env.pm25);
        set_text(g_ui.metric_value[2], buf);
        snprintf(buf, sizeof(buf), "%ldW", (long)g_ui.last.env.total_power);
        set_text(g_ui.metric_value[7], buf);
    }
    const sh_device_state_t *rain2 = &g_ui.last.devices[SH_DEV_RAIN_2];
    const sh_device_state_t *rain3 = &g_ui.last.devices[SH_DEV_RAIN_3];
    const sh_device_state_t *flame = &g_ui.last.devices[SH_DEV_FLAME_3];
    const sh_device_state_t *sky = &g_ui.last.devices[SH_DEV_SKYLIGHT];
    snprintf(buf, sizeof(buf), rain2->valid ? "%ld%%" : "--", (long)rain2->value);
    set_text(g_ui.metric_value[3], buf);
    snprintf(buf, sizeof(buf), rain3->valid ? "%ld%%" : "--", (long)rain3->value);
    set_text(g_ui.metric_value[4], buf);
    snprintf(buf, sizeof(buf), flame->valid ? "%ld/1023" : "--", (long)flame->value);
    set_text(g_ui.metric_value[5], buf);
    snprintf(buf, sizeof(buf), sky->valid ? "%ld%%" : "--", (long)sky->percent);
    set_text(g_ui.metric_value[6], buf);
    set_text(g_ui.metric_value[8], SH_FIRMWARE_VERSION);
}

void smart_home_ui_set_mqtt_publish(sh_mqtt_publish_fn_t publish_cb)
{
    g_publish = publish_cb;
}

void smart_home_ui_create(void)
{
    memset(&g_ui, 0, sizeof(g_ui));
    font_text = &BUILTIN_TEXT_FONT;
    font_icon = &BUILTIN_ICON_FONT;
    font_icon_big = &font_awesome_30_4;
    g_ui.last.language = SH_LANG_ZH;
    g_ui.last.screen_brightness = 80;
    g_ui.last.sound_level = 50;

    g_ui.root = lv_screen_active();
    lv_obj_clean(g_ui.root);
    lv_obj_set_style_bg_color(g_ui.root, lv_color_hex(C_SURFACE), 0);
    lv_obj_set_style_text_font(g_ui.root, font_text, 0);
    lv_obj_set_style_text_color(g_ui.root, lv_color_hex(C_TEXT), 0);

    create_sidebar();
    create_overview_page();
    create_floor_page();
    create_placeholder_page(PAGE_SCENE, "场景", "场景联动状态将由 MQTT 实时刷新");
    create_placeholder_page(PAGE_DEVICE, "设备", "全屋设备清单将由 MQTT 实时刷新");
    create_network_page();
    create_password_page();
    create_settings_page();
    show_page(PAGE_OVERVIEW);
    update_clock(NULL);
    lv_timer_create(update_clock, 1000, NULL);
    ESP_LOGI(TAG, "Smart home LVGL UI created");
}

void smart_home_ui_update_full(const sh_state_t *state)
{
    if (!state) return;
    if (!lvgl_port_lock(1000)) return;
    g_ui.last = *state;
    for (int i = 0; i < SH_CTRL_COUNT; ++i) smart_home_ui_update_controller((sh_controller_id_t)i, &state->controllers[i]);
    for (int i = 0; i < SH_DEV_COUNT; ++i) smart_home_ui_update_device((sh_device_id_t)i, &state->devices[i]);
    smart_home_ui_update_env(&state->env);
    smart_home_ui_update_network(&state->network);
    lvgl_port_unlock();
}

void smart_home_ui_update_controller(sh_controller_id_t id, const sh_device_state_t *controller)
{
    if (id >= SH_CTRL_COUNT || !controller) return;
    g_ui.last.controllers[id] = *controller;
    char buf[48];
    snprintf(buf, sizeof(buf), "● %s", controller->online ? "在线" : "离线");
    set_text(g_ui.controller_status[id], controller->valid ? buf : "● 等待MQTT");
    lv_obj_set_style_text_color(g_ui.controller_status[id], lv_color_hex(controller->online ? C_SUCCESS : C_SUB), 0);
}

void smart_home_ui_update_device(sh_device_id_t id, const sh_device_state_t *device)
{
    if (id >= SH_DEV_COUNT || !device) return;
    g_ui.last.devices[id] = *device;
    char value[64];
    char status[64];
    device_text(id, device, value, sizeof(value), status, sizeof(status));
    set_text(g_ui.dev_value[id], value);
    set_text(g_ui.dev_status[id], status);
    set_text(g_ui.dev_online[id], device->online ? "● 在线" : "● 离线");
    if (g_ui.dev_status[id]) lv_obj_set_style_text_color(g_ui.dev_status[id], lv_color_hex(device->online ? C_SUCCESS : C_SUB), 0);
    if (g_ui.dev_online[id]) lv_obj_set_style_text_color(g_ui.dev_online[id], lv_color_hex(device->online ? C_SUCCESS : C_SUB), 0);

    if (id == SH_DEV_LIVING_LIGHT || id == SH_DEV_BATH_LIGHT || id == SH_DEV_MASTER_LIGHT) {
        int idx = id == SH_DEV_LIVING_LIGHT ? 0 : (id == SH_DEV_BATH_LIGHT ? 1 : 2);
        if (device->power) lv_obj_add_state(g_ui.floor_light_sw[idx], LV_STATE_CHECKED);
        else lv_obj_remove_state(g_ui.floor_light_sw[idx], LV_STATE_CHECKED);
        if (device->brightness >= 0) lv_slider_set_value(g_ui.floor_light_slider[idx], device->brightness, LV_ANIM_OFF);
    }
    if (id == SH_DEV_RACK_2) {
        if (device->percent >= 0) lv_slider_set_value(g_ui.rack2_slider, device->percent, LV_ANIM_OFF);
        char buf[96];
        snprintf(buf, sizeof(buf), "模式：%s    状态：%s", device->mode[0] ? device->mode : "--", device->status[0] ? device->status : "--");
        set_text(g_ui.rack2_status, buf);
    }
    if (id == SH_DEV_RAIN_2) {
        char buf[96];
        snprintf(buf, sizeof(buf), device->valid ? "%ld%%" : "--", (long)device->value);
        set_text(g_ui.rain2_value, buf);
        snprintf(buf, sizeof(buf), "ADC: %ld\n状态: %s\n更新时间: %s", (long)device->adc,
                 device->status[0] ? device->status : "--", device->updated[0] ? device->updated : "--");
        set_text(g_ui.rain2_meta, buf);
    }
    update_metrics();
}

void smart_home_ui_update_env(const sh_env_state_t *env)
{
    if (!env) return;
    g_ui.last.env = *env;
    update_metrics();
}

void smart_home_ui_update_network(const sh_network_state_t *network)
{
    if (!network) return;
    g_ui.last.network = *network;
    char buf[128];
    snprintf(buf, sizeof(buf), "局域网：%s  MQTT：%s",
             network->network_connected ? "已连接" : "未连接",
             network->mqtt_connected ? "已连接" : "未连接");
    set_text(g_ui.network_state, buf);
    set_text(g_ui.mqtt_label, network->mqtt_connected ? "MQTT：已连接" : "MQTT：等待");
    snprintf(buf, sizeof(buf), "当前状态：%s%s%s",
             network->network_connected ? "已连接" : "未连接",
             network->ssid[0] ? " " : "",
             network->ssid);
    set_text(g_ui.network_ssid, buf);
    set_text(g_ui.settings_wifi, network->ssid[0] ? network->ssid : "未连接");
}

void smart_home_ui_set_language(sh_language_t language)
{
    g_ui.last.language = language;
    set_text(g_ui.settings_language, language == SH_LANG_ZH ? "简体中文" : "English");
    const char **nav = language == SH_LANG_ZH ? zh_nav : en_nav;
    for (int i = 0; i < 6; ++i) {
        if (g_ui.nav_btns[i] && lv_obj_get_child_cnt(g_ui.nav_btns[i]) > 1) {
            set_text(lv_obj_get_child(g_ui.nav_btns[i], 1), nav[i]);
        }
    }
    set_text(g_ui.title_labels[PAGE_OVERVIEW], tr("智慧家庭控制面板", "Smart Home Control Panel"));
    set_text(g_ui.title_labels[PAGE_FLOOR], tr("二楼设备", "Second Floor"));
    set_text(g_ui.title_labels[PAGE_NETWORK], tr("网络连接", "Network"));
    set_text(g_ui.title_labels[PAGE_SETTINGS], tr("设置", "Settings"));
}
