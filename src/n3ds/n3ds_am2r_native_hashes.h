#pragma once

// Bytecode hashes (codeHash in n3ds_am2r_native.c) of the AM2R 1.5.5 code each native was written from. A native only
// replaces code with exactly this hash. 0: not filled in yet (the native stays off; its hash is logged at boot).

#include <stdint.h>
#include <string.h>

static uint32_t N3DSAm2rNative_expectedHash(const char* name) {
    static const struct { const char* name; uint32_t hash; } kHashes[] = {
        { "approximatelyZero", 0xf4fcb5fd },
        { "calculateCollisionBounds", 0x4d90436d },
        { "isCollisionLeft", 0xec16a6f9 },
        { "isCollisionRight", 0x492fe679 },
        { "isCollisionTop", 0xe0399e01 },
        { "isCollisionBottom", 0xc7684f9e },
        { "isCollisionRectangle", 0x765e1ca5 },
        { "string_split", 0x3717508a },
        { "draw_mapblock", 0x46bb394f },
        { "draw_gui_map", 0x1e5a39d1 },
        { "gml_Object_oA6Dust_Draw_0", 0x8534288c },
        { "gml_Object_oControl_Step_0", 0x087b7cfa }, // not a native: the HUD skip checks oControl Step is this one
        { "gml_Object_oEnemy_Draw_0", 0x900b9ce8 },
        { "draw_gui", 0xcd167e8f },
        { "gml_Object_oLightEngine_Other_11", 0x5752b1d6 },
        { "moveTo", 0x083e4e99 },
        { "isCollisionPlatformBottom", 0xd69dd806 },
        { "platformCharacterIs", 0x07c324ff },
        { "check_keys", 0x825084dc },
        { "characterStepEvent", 0x5c1c3513 }, // AOT (n3ds_aot_gen.inc)
        { "gml_Object_oWaterFXV2_Draw_0", 0xd53d8f59 }, // AOT
        { "gml_Object_oLBubble_Step_0", 0xc7c9cc08 }, // AOT
        { "gml_Object_oLBubble_Step_1", 0xb3400315 }, // AOT
        { "gml_Object_oWater_Draw_0", 0x06c129a6 }, // AOT
        { "gml_Object_oHalzyn_Draw_0", 0x995b685c }, // AOT
        { "gml_Object_oHalzyn_Step_0", 0x2b9bed36 }, // AOT
        { "gml_Object_oHalzyn_Step_2", 0xf2c365b7 }, // AOT
        { "gml_Object_oControl_Draw_0", 0x4a81bc6f }, // AOT
        { "gml_Object_oWaterBackground_Draw_0", 0xba3d84ca }, // AOT
        { "gml_Object_oCharacter_Step_2", 0x65004316 }, // AOT
        { "gml_Object_oMeboid_Step_0", 0xd44f6239 }, // AOT
        { "gml_Object_oMBTrail_Step_0", 0x03332826 }, // AOT
        { "gml_Object_oGravitt_Step_0", 0xfaef7152 }, // AOT
        { "gml_Object_oDoor_Draw_0", 0x5538d6d6 }, // AOT
        { "gml_Object_oCharacter_Draw_0", 0xaaa9856e }, // AOT
        { "gml_Object_oControl_Draw_72", 0xbdec1ae2 }, // AOT
        { "chStepSetSprite", 0x9e35241d }, // AOT
        { "chStepControl", 0xf9b41cad }, // AOT
        { "global_control", 0x398d13f6 }, // AOT
        { "chStepMovingCheck", 0x4dbedf9b }, // AOT
        { "draw_character", 0x5b8a7018 }, // AOT
        { "draw_character_to_surface", 0xf0c66dc3 }, // AOT
        { "draw_character_from_surface", 0x42d3eb2f }, // AOT
        { "characterDrawEvent", 0xfc61c92a }, // AOT
        { "sfx_isplaying", 0x30f59f8a }, // AOT
        { "set_widescreen", 0xbc9e0dbe }, // AOT
        { "is_walkzone", 0x1cbfae62 }, // AOT
        { "is_past_deadzone", 0x4dfdeb78 }, // AOT
        { "gui_health", 0x8d2da750 }, // AOT
        { "to_string_lz", 0x1e92f9fd }, // AOT
        { "draw_txt_1button_log", 0xb7d8f945 }, // AOT
        { "draw_text_shadow_log", 0xeb00e44d }, // AOT
        { "enemy_active_check", 0xb0bf317b }, // AOT
        { "enemy_target_check", 0x43544652 }, // AOT
        { "SetSpinJumpSound", 0x3c13edd6 }, // AOT
        { "chStepFire", 0x346ef58e }, // AOT
        { "chStepSpiderBall", 0x0e4f5701 }, // AOT
        { "gml_Object_oCharacter_Step_0", 0xb0056274 }, // AOT
        { "gml_Object_oCamera_Step_0", 0xfc6cc3ca }, // AOT
        { "gml_Object_oCameraLimit_Step_0", 0x9357afcd }, // AOT
        { "gml_Object_oYumbo_Step_0", 0xdb0863d6 }, // AOT
        { "gml_Object_oHornoad_Step_0", 0x1f30a596 }, // AOT
        { "gml_Object_oBabySeptogg_Step_0", 0x98e6f028 }, // AOT
        { "gml_Object_oBeam_Step_0", 0xaa7178ed }, // AOT
        { "gml_Object_oBeam_Draw_0", 0xe7a7068d }, // AOT
        { "gml_Object_oSaveShip_Step_0", 0x0224577a }, // AOT
        { "gml_Object_oElderSeptogg_Draw_0", 0x5277e4e5 }, // AOT
        { "gml_Object_oMBTrail_Draw_0", 0x2c744524 }, // AOT
        { "gml_Object_oNotification_Draw_73", 0x2a17c361 }, // AOT
        { "gml_Object_oControl_Draw_64", 0xb4e1ce9a }, // AOT
        { "gml_Object_oControl_Draw_77", 0x25e30510 }, // AOT
        { "gml_Object_oControl_Other_14", 0xc0f083e7 }, // AOT
        { "gml_Object_oEnemy_Step_0", 0xe15502e0 }, // AOT
        { "gml_Object_oEnemy_Step_1", 0x79f5f418 }, // AOT
        { "gml_Object_oCharacter_Step_1", 0xd3ff2cbb }, // AOT
        { "gml_Object_oPickup_Draw_0", 0x399021c2 }, // AOT
        { "gml_Object_oShip1_Draw_0", 0x7c367357 }, // AOT
        { "gml_Object_oBackground_Draw_0", 0x49cac72a },
        { "gml_Object_oTitleAnimation_Draw_0", 0x7c755899 },
    };
    for (size_t k = 0; k < sizeof(kHashes) / sizeof(kHashes[0]); k++) {
        if (strcmp(kHashes[k].name, name) == 0) return kHashes[k].hash;
    }
    return 0;
}
