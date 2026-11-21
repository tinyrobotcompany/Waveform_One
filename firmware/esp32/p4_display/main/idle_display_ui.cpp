#include "idle_display_ui.h"
#include "display_fonts.h"
namespace {
lv_obj_t *panel, *delay, *custom, *level, *level_text, *format, *size, *colour, *preview, *status;
idle_display::Config *saved;
constexpr unsigned minutes[] = {0,1,2,5,10};
lv_color_t background=lv_color_hex(0x080d0a), ink=lv_color_hex(0xedf3e8), accent=lv_color_hex(0xa8d57a);
lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y) {
    auto *out=lv_label_create(parent); lv_label_set_text(out,text);
    lv_obj_set_style_text_font(out,display_font(&lv_font_montserrat_20),0);
    lv_obj_set_style_text_color(out,ink,0); lv_obj_set_pos(out,x,y); return out;
}
lv_obj_t *button(lv_obj_t *parent,const char *text,int x,int y,int width,lv_event_cb_t callback) {
    auto *out=lv_button_create(parent); lv_obj_set_pos(out,x,y); lv_obj_set_size(out,width,52);
    lv_obj_set_style_bg_color(out,lv_color_hex(0x121a13),0);
    lv_obj_set_style_border_color(out,accent,0); lv_obj_set_style_border_width(out,1,0);
    lv_obj_set_style_shadow_width(out,0,0);
    auto *caption = label(out,text,0,0);
    lv_obj_center(caption);
    lv_obj_add_event_cb(out,callback,LV_EVENT_CLICKED,nullptr);
    return out;
}
lv_obj_t *dropdown(const char *options,int x,int y,int width) {
    auto *out=lv_dropdown_create(panel); lv_dropdown_set_options(out,options);
    lv_obj_set_pos(out,x,y); lv_obj_set_size(out,width,52);
    lv_obj_set_style_bg_color(out,lv_color_hex(0x121a13),0);
    lv_obj_set_style_text_color(out,ink,0); return out;
}
void render(lv_event_t *) {
    char text[16]; idle_display::format_clock(text,sizeof(text),23,8,lv_dropdown_get_selected(format)==1);
    lv_label_set_text(preview,text);
    lv_obj_set_style_text_font(preview,display_font(lv_dropdown_get_selected(size) ? &lv_font_montserrat_48 : &lv_font_montserrat_32),0);
    lv_obj_set_style_text_color(preview,lv_dropdown_get_selected(colour) ? accent : ink,0);
    lv_label_set_text_fmt(level_text,"Idle brightness: %d%%",int(lv_slider_get_value(level)));
}
void open(lv_event_t *) {
    unsigned index=5;
    if (!saved->enabled) index=0;
    else for(unsigned i=1;i<5;++i) if(minutes[i]==saved->minutes) index=i;
    lv_dropdown_set_selected(delay,index); lv_spinbox_set_value(custom,saved->minutes);
    lv_slider_set_value(level,saved->percent,LV_ANIM_OFF);
    lv_dropdown_set_selected(format,saved->twelve_hour); lv_dropdown_set_selected(size,saved->large);
    lv_dropdown_set_selected(colour,saved->accent); lv_label_set_text(status,""); render(nullptr);
    lv_obj_remove_flag(panel,LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(panel);
}
}
void idle_display_ui_create(lv_obj_t *screen,lv_obj_t *display_page,idle_display::Config &config) {
    saved=&config;
    button(display_page,"Idle dimming & clock",670,115,280,open);
    panel=lv_obj_create(screen); lv_obj_set_size(panel,1024,600); lv_obj_center(panel);
    lv_obj_set_style_pad_all(panel,0,0); lv_obj_set_style_border_width(panel,0,0);
    lv_obj_set_style_bg_color(panel,background,0); lv_obj_set_style_bg_opa(panel,LV_OPA_COVER,0);
    lv_obj_remove_flag(panel,LV_OBJ_FLAG_SCROLLABLE);
    auto *title=label(panel,"Idle dimming & clock",48,35);
    lv_obj_set_style_text_font(title,display_font(&lv_font_montserrat_32),0);
    button(panel,"Back",850,26,120,[](lv_event_t *) {lv_obj_add_flag(panel,LV_OBJ_FLAG_HIDDEN);});
    label(panel,"Dim after no music or interaction",48,110);
    delay=dropdown("Off\n1 minute\n2 minutes\n5 minutes\n10 minutes\nCustom",48,145,330);
    label(panel,"Custom delay (1–240 minutes)",48,215);
    custom=lv_spinbox_create(panel); lv_obj_set_pos(custom,48,250); lv_obj_set_size(custom,120,52);
    lv_spinbox_set_range(custom,1,240); lv_spinbox_set_digit_format(custom,3,0); lv_spinbox_set_step(custom,1);
    button(panel,"−",190,250,65,[](lv_event_t *) {lv_spinbox_decrement(custom); lv_dropdown_set_selected(delay,5);});
    button(panel,"+",275,250,65,[](lv_event_t *) {lv_spinbox_increment(custom); lv_dropdown_set_selected(delay,5);});
    level_text=label(panel,"",48,335);
    level=lv_slider_create(panel); lv_obj_set_pos(level,60,380); lv_obj_set_size(level,330,35);
    lv_slider_set_range(level,10,100); lv_obj_set_style_bg_color(level,accent,LV_PART_INDICATOR);
    lv_obj_add_event_cb(level,render,LV_EVENT_VALUE_CHANGED,nullptr);
    auto *help=label(panel,"Dimmed main screen shows only the clock.\nTouch or music wakes it.\nNight dimming remains independent.",48,440);
    lv_obj_set_style_text_font(help,display_font(&lv_font_montserrat_16),0);
    label(panel,"Clock appearance",550,110);
    format=dropdown("24-hour\n12-hour",550,145,180);
    size=dropdown("Compact\nLarge",750,145,180);
    colour=dropdown("Soft white\nWaveform green",550,220,300);
    preview=label(panel,"",550,330);
    for(auto *control:{format,size,colour}) lv_obj_add_event_cb(control,render,LV_EVENT_VALUE_CHANGED,nullptr);
    label(panel,"Clock preview",550,405);
    status=label(panel,"",320,535); lv_obj_set_width(status,650);
    lv_obj_set_style_text_font(status,display_font(&lv_font_montserrat_16),0);
    button(panel,"Save",48,525,220,[](lv_event_t *) {
        auto candidate=*saved; const unsigned index=lv_dropdown_get_selected(delay);
        candidate.enabled=index!=0;
        candidate.minutes=index>0 && index<5 ? minutes[index] : lv_spinbox_get_value(custom);
        candidate.percent=lv_slider_get_value(level); candidate.twelve_hour=lv_dropdown_get_selected(format);
        candidate.large=lv_dropdown_get_selected(size); candidate.accent=lv_dropdown_get_selected(colour);
        if(idle_display::save(candidate)){*saved=candidate;lv_label_set_text(status,"Saved. Clock appearance also applies to night dimming.");}
        else lv_label_set_text(status,"Could not save. Previous settings remain active.");
    });
    lv_obj_add_flag(panel,LV_OBJ_FLAG_HIDDEN);
}
