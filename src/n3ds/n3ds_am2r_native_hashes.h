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
        { "gml_Object_oLavaSurface_Draw_0", 0x4d95db07 }, // AOT
        { "gml_Object_oTsumuri_Step_0", 0xd23a7e0f }, // AOT
        { "gml_Object_oTsumuri_Step_1", 0x109adbd0 }, // AOT
        { "gml_Object_oFXAnimSpark_Draw_0", 0x401715a6 }, // AOT
        { "gml_Object_oLavaBGFX_Draw_0", 0xc5dc1c29 }, // AOT
        { "gml_Object_oTilesRandomAlpha_Other_10", 0x724f6d34 }, // AOT
        { "gml_Object_oA4TileLight1_Step_0", 0x83f8a3fd }, // AOT
        { "gml_Object_oMeboid2_Step_0", 0x53768ff9 }, // AOT
        { "gml_Object_oBattery_Step_0", 0x3134be62 }, // AOT
        { "gml_Object_oSkorpLure_Draw_0", 0xaa862c0c }, // AOT
        { "gml_Object_oSkorpLure_Step_0", 0x5b026845 }, // AOT
        { "gml_Object_oCurrentBubble_Step_0", 0xd5b71eb1 }, // AOT
        { "gml_Object_oQueenHead_Draw_0", 0x1520e9ed }, // AOT
        { "gml_Object_oQueenHead_Step_0", 0x24c99d16 }, // AOT
        { "gml_Object_oSkorp_Step_0", 0x5744f327 }, // AOT
        { "gml_Object_oA4BlastDoor_Draw_0", 0x515d5a76 }, // AOT
        { "gml_Object_oMGammaTriggerProx_Draw_0", 0x4311d90a }, // AOT
        { "gml_Object_oRoboMine_Step_0", 0x0a077a6e }, // AOT
        { "gml_Object_oBladeBot_Step_0", 0xffe93e6a }, // AOT
        { "gml_Object_oA5ActivationBG_Draw_0", 0x356ca9f9 }, // AOT
        { "skorp_get_lure", 0xd35df08c }, // AOT
        { "gml_Object_oShielder_Step_0", 0x10da2d02 }, // AOT
        { "gml_Object_oSkreek2_Step_0", 0x19eabe4a }, // AOT
        { "gml_Object_oMoheekWater_Step_0", 0xd3bab2b5 }, // AOT
        { "gml_Object_oMOmegaTail_Draw_0", 0xeb61011b }, // AOT
        { "gml_Object_oFlitt_Step_0", 0x37d49065 }, // AOT
        { "gml_Object_oLightBug_Step_0", 0x1f4a3743 }, // AOT
        { "gml_Object_oLightBug_Draw_0", 0xc367f750 }, // AOT
        { "gml_Object_oDrillBack_Draw_0", 0x7135793e }, // AOT
        { "gml_Object_oSpikeSolid_Step_0", 0xba6c8520 }, // AOT
        { "gml_Object_oMOmega_Step_0", 0x36cda18a }, // AOT
        { "gml_Object_oShirk_Draw_0", 0x85dee90a }, // AOT
        { "gml_Object_oMachine1Beam_Draw_0", 0xeaadbf0a }, // AOT
        { "gml_Object_oShielderShield_Draw_0", 0x8651234e }, // AOT
        { "gml_Object_oShielderShield_Step_0", 0x8ccef959 }, // AOT
        { "gml_Object_oSkorpDisc_Step_0", 0x4ce89941 }, // AOT
        { "gml_Object_oMonster_Step_0", 0x6008d1c9 }, // AOT
        { "gml_Object_oMonster_Draw_0", 0xcd9e705e }, // AOT
        { "gml_Object_oAutoad_Draw_0", 0x78e1a80c }, // AOT
        { "gml_Object_oMZeta_Cocoon_Draw_0", 0x01eb7ef8 }, // AOT
        { "gml_Object_oQueenLegFront_Draw_0", 0xd5811ac6 }, // AOT
        { "gml_Object_oQueenLegFront_Step_0", 0x73a8504a }, // AOT
        { "gml_Object_oWallfire_Step_0", 0x1c8a1be7 }, // AOT
        { "gml_Object_oGravitt_Step_1", 0xc3adf838 }, // AOT
        { "gml_Object_oA3Slasher_Draw_0", 0xcb679e40 }, // AOT
        { "gml_Object_oErisPipe_Draw_0", 0xfbc89bda }, // AOT
        { "gml_Object_oA8RedLight_Draw_0", 0xc367f750 }, // AOT
        { "gml_Object_oTPO_Step_0", 0x05c0cfc3 }, // AOT
        { "gml_Object_oBackground_Draw_0", 0x49cac72a },
        { "gml_Object_oTitleAnimation_Draw_0", 0x7c755899 },
    };
    for (size_t k = 0; k < sizeof(kHashes) / sizeof(kHashes[0]); k++) {
        if (strcmp(kHashes[k].name, name) == 0) return kHashes[k].hash;
    }
    return 0;
}
