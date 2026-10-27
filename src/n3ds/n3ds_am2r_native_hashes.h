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
        { "gml_Object_oEnemy_Draw_0", 0x900b9ce8 },
        { "draw_gui", 0xcd167e8f },
        { "gml_Object_oLightEngine_Other_11", 0x5752b1d6 },
        { "moveTo", 0x083e4e99 },
        { "isCollisionPlatformBottom", 0xd69dd806 },
        { "platformCharacterIs", 0x07c324ff },
    };
    for (size_t k = 0; k < sizeof(kHashes) / sizeof(kHashes[0]); k++) {
        if (strcmp(kHashes[k].name, name) == 0) return kHashes[k].hash;
    }
    return 0;
}
