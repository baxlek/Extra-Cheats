#include "mods/service.hpp"
#include "mods/svc/hook.hpp"
#include "mods/svc/log.hpp"
#include "mods/svc/ui.h"

// Game includes
#include "d/d_com_inf_game.h"

DEFINE_MOD();

IMPORT_SERVICE(LogService, svc_log);
IMPORT_SERVICE(HookService, svc_hook);
IMPORT_SERVICE(UiService, svc_ui);

static bool unlimited_bottles = false;

DEFINE_HOOK(&dSv_player_item_c::setBottleItemIn, SetBottleItem);
DEFINE_HOOK(&dSv_player_item_c::setEquipBottleItemIn, SetEquipBottleItem);
DEFINE_HOOK(&dSv_player_item_record_c::addBottleNum, AddBottleNum);
DEFINE_HOOK(&dSv_player_item_record_c::setBottleNum, SetBottleNum);

static bool is_consumption(u8 current_item, u8 new_item) {
    return current_item != dItemNo_NONE_e && current_item != dItemNo_EMPTY_BOTTLE_e &&
           (new_item == dItemNo_EMPTY_BOTTLE_e ||
            (current_item == dItemNo_MILK_BOTTLE_e && new_item == dItemNo_HALF_MILK_BOTTLE_e));
}

static HookAction on_set_bottle_item(ModContext*, void* args, void*, void*) {
    if (unlimited_bottles &&
        is_consumption(mods::arg<u8>(args, 1), mods::arg<u8>(args, 2))) {
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

static HookAction on_set_equip_bottle_item(ModContext*, void* args, void*, void*) {
    if (unlimited_bottles) {
        auto* items = mods::arg<dSv_player_item_c*>(args, 0);
        u8 slot = dComIfGs_getSelectItemIndex(mods::arg<u8>(args, 1));
        if (slot >= SLOT_11 && slot <= SLOT_14 &&
            is_consumption(items->mItems[slot], mods::arg<u8>(args, 2))) {
            return HOOK_SKIP_ORIGINAL;
        }
    }
    return HOOK_CONTINUE;
}

static HookAction on_add_bottle_num(ModContext*, void* args, void*, void*) {
    // Bee larvae can be consumed one at a time without emptying the bottle.
    s16& amount = mods::arg_ref<s16>(args, 2);
    if (unlimited_bottles && amount < 0) {
        amount = 0;
    }
    return HOOK_CONTINUE;
}

static HookAction on_set_bottle_num(ModContext*, void* args, void*, void*) {
    if (unlimited_bottles) {
        auto* record = mods::arg<dSv_player_item_record_c*>(args, 0);
        u8 index = mods::arg<u8>(args, 1);
        if (index < dSv_player_item_c::BOTTLE_MAX &&
            mods::arg<u8>(args, 2) < record->mBottleNum[index]) {
            return HOOK_SKIP_ORIGINAL;
        }
    }
    return HOOK_CONTINUE;
}

static void get_unlimited_bottles(ModContext*, void*, UiControlValue* value) {
    value->bool_value = unlimited_bottles;
}

static void set_unlimited_bottles(ModContext*, void*, const UiControlValue* value) {
    unlimited_bottles = value->bool_value;
}

static ModResult build_panel(ModContext* ctx, UiElementHandle pane, void*, ModError*) {
    UiControlDesc toggle = UI_CONTROL_DESC_INIT;
    toggle.kind = UI_CONTROL_TOGGLE;
    toggle.label = "Unlimited bottle contents";
    toggle.get = get_unlimited_bottles;
    toggle.set = set_unlimited_bottles;
    return svc_ui->pane_add_control(ctx, pane, &toggle, nullptr);
}

extern "C" {
MOD_EXPORT ModResult mod_initialize(ModError*) {
    unlimited_bottles = false;

    ModResult result = mods::hook::add_pre<SetBottleItem>(on_set_bottle_item);
    if (result == MOD_OK) {
        result = mods::hook::add_pre<SetEquipBottleItem>(on_set_equip_bottle_item);
    }
    if (result == MOD_OK) {
        result = mods::hook::add_pre<AddBottleNum>(on_add_bottle_num);
    }
    if (result == MOD_OK) {
        result = mods::hook::add_pre<SetBottleNum>(on_set_bottle_num);
    }
    if (result != MOD_OK) {
        mods::log::error("failed to install bottle consumption hooks");
        return result;
    }

    UiModsPanelDesc panel = UI_MODS_PANEL_DESC_INIT;
    panel.build = build_panel;
    result = svc_ui->register_mods_panel(mod_ctx, &panel);
    if (result != MOD_OK) {
        mods::log::error("failed to register mod panel");
        return result;
    }

    mods::log::info("Extra Cheats initialized");
    return MOD_OK;
}

MOD_EXPORT ModResult mod_update(ModError*) {
    return MOD_OK;
}

MOD_EXPORT ModResult mod_shutdown(ModError*) {
    return MOD_OK;
}
}
