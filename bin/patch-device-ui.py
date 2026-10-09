#!/usr/bin/env python3
"""Apply the small MeshSPY overlay to the bundled T-Deck LVGL view."""

from datetime import datetime
from pathlib import Path
import re

Import("env")

libdeps = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.get("PIOENV")
target = libdeps / "meshtastic-device-ui" / "source" / "graphics" / "TFT" / "TFTView_320x240.cpp"
marker = "// MeshSPY OUI-SPY overlay"
build_marker = "// MeshSPY build info"
build_id = env.get("MESHSPY_BUILD_ID") or datetime.now().strftime("%Y%m%d-%H%M%S")
env.Replace(MESHSPY_BUILD_ID=build_id)

overlay = f'''    // {marker}
    lv_obj_t *meshspyButton = lv_button_create(objects.tab_page_tools);
    lv_obj_set_width(meshspyButton, LV_PCT(95));
    lv_obj_set_height(meshspyButton, 30);
    add_style_settings_button_style(meshspyButton);
    lv_obj_set_style_align(meshspyButton, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_t *meshspyButtonLabel = lv_label_create(meshspyButton);
    lv_label_set_text(meshspyButtonLabel, "OUI-SPY");
    lv_obj_set_style_align(meshspyButtonLabel, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(meshspyButton, [](lv_event_t *event) {{
        if (lv_event_get_code(event) != LV_EVENT_CLICKED)
            return;
        lv_obj_t *panel = lv_obj_create(lv_layer_top());
        lv_obj_set_size(panel, 300, 240);
        lv_obj_center(panel);
        lv_obj_set_style_pad_all(panel, 8, 0);
        lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t *title = lv_label_create(panel);
        lv_label_set_text(title, "OUI-SPY controls");
        lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
        static const char *const names[] = {{"Scanner", "Flock-You", "Foxhunter", "Sky-Spy", "BLE Sniff", "Watchlist", "Mesh alerts", "GPS tags", "Alert channel"}};
        for (uint8_t index = 0; index < 9; ++index) {{
            lv_obj_t *button = lv_btn_create(panel);
            lv_obj_set_width(button, LV_PCT(100));
            lv_obj_set_height(button, 20);
            lv_obj_set_user_data(button, reinterpret_cast<void *>(static_cast<uintptr_t>(index)));
            lv_obj_t *label = lv_label_create(button);
            if (index == 8)
                lv_label_set_text_fmt(label, "%s: %s", names[index], ouiSpyUiAlertChannelName());
            else
                lv_label_set_text_fmt(label, "%s: %s", names[index],
                                      ouiSpyUiSettingEnabled(index) ? "ON" : "OFF");
            lv_obj_center(label);
            lv_obj_add_event_cb(button, [](lv_event_t *toggleEvent) {{
                if (lv_event_get_code(toggleEvent) != LV_EVENT_CLICKED)
                    return;
                lv_obj_t *button = static_cast<lv_obj_t *>(lv_event_get_target(toggleEvent));
                uint8_t index = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(button)));
                lv_obj_t *label = lv_obj_get_child(button, 0);
                static const char *const names[] = {{"Scanner", "Flock-You", "Foxhunter", "Sky-Spy", "BLE Sniff", "Watchlist", "Mesh alerts", "GPS tags", "Alert channel"}};
                if (index == 8) {{
                    ouiSpyUiCycleAlertChannel();
                    lv_label_set_text_fmt(label, "%s: %s", names[index], ouiSpyUiAlertChannelName());
                }} else {{
                    ouiSpyUiToggleSetting(index);
                    lv_label_set_text_fmt(label, "%s: %s", names[index], ouiSpyUiSettingEnabled(index) ? "ON" : "OFF");
                }}
            }}, LV_EVENT_CLICKED, nullptr);
        }}
        lv_obj_t *test = lv_btn_create(panel);
        lv_obj_set_width(test, LV_PCT(100));
        lv_obj_set_height(test, 20);
        lv_obj_t *testLabel = lv_label_create(test);
        lv_label_set_text(testLabel, "Send test alert");
        lv_obj_center(testLabel);
        lv_obj_add_event_cb(test, [](lv_event_t *testEvent) {{
            if (lv_event_get_code(testEvent) == LV_EVENT_CLICKED)
                ouiSpyUiSendTestAlert();
        }}, LV_EVENT_CLICKED, nullptr);
        lv_obj_t *save = lv_btn_create(panel);
        lv_obj_set_width(save, LV_PCT(100));
        lv_obj_set_height(save, 25);
        lv_obj_t *saveLabel = lv_label_create(save);
        lv_label_set_text(saveLabel, "Save and close");
        lv_obj_center(saveLabel);
        lv_obj_add_event_cb(save, [](lv_event_t *saveEvent) {{
            if (lv_event_get_code(saveEvent) == LV_EVENT_CLICKED) {{
                ouiSpyUiSaveSettings();
                lv_obj_del(lv_obj_get_parent(static_cast<lv_obj_t *>(lv_event_get_target(saveEvent))));
            }}
        }}, LV_EVENT_CLICKED, nullptr);
    }}, LV_EVENT_CLICKED, nullptr);
    // {marker} end
'''
build_info = f'''    // {build_marker}
    createLabel(objects.settings_about_panel, "MeshSPY build: {build_id}");
'''

if not target.exists():
    print(f"MeshSPY UI overlay: dependency source not found: {target}")
else:
    source = target.read_text()
    source = source.replace('#if defined(T_DECK)\n#include "modules/OuiSpyModule.h"\n#endif\n', '')
    include = '#if defined(T_DECK)\n#include "modules/OuiSpyUiBridge.h"\n#endif\n'
    if include not in source:
        source = source.replace('#include "graphics/view/TFT/TFTView_320x240.h"\n',
                                '#include "graphics/view/TFT/TFTView_320x240.h"\n' + include, 1)

    pattern = r'    // // MeshSPY OUI-SPY overlay\n.*?    // // MeshSPY OUI-SPY overlay end\n'
    source, removed = re.subn(pattern, '', source, count=1, flags=re.DOTALL)
    build_pattern = r'    // // MeshSPY build info\n.*?    createLabel\(objects\.settings_about_panel, "MeshSPY build: .*?"\);\n'
    source, _ = re.subn(build_pattern, '', source, count=1, flags=re.DOTALL)
    about_needle = '    createLabel(objects.settings_about_panel, fw);\n'
    if about_needle in source:
        source = source.replace(about_needle, about_needle + build_info, 1)
    needle = "    // tools buttons\n"
    if needle in source:
        target.write_text(source.replace(needle, needle + overlay, 1))
        action = "moved" if removed else "added"
        print(f"MeshSPY UI overlay: {action} OUI-SPY Tools entry")
    else:
        print("MeshSPY UI overlay: Tools insertion point not found")
