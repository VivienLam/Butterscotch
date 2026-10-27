// AM2R 1.5.5's check_keys (oControl, every step) in C. Included by n3ds_am2r_native.c.
//
// The keyboard part, then the gamepad part (the 3DS buttons come in as a gamepad). The Android joystick part isn't
// here: when it would run, the game's code does it all. Nothing is read back between the game's assignments, so the
// result is worked out first and stored once, with the same final values.

static int32_t gCode_check_keys = -1, gCode_is_walkzone = -1, gCode_is_past_deadzone = -1;

enum { CK_LEFT, CK_RIGHT, CK_UP, CK_DOWN, CK_A, CK_B, CK_X, CK_L, CK_R, CK_SELECT, CK_START, CK_L2, CK_R2, CK_EXIT,
    CK_MENU1, CK_MENU2, CK_COUNT };
static Var v_ctrl[CK_COUNT] = {
    { "ctrl_Left", -1, false }, { "ctrl_Right", -1, false }, { "ctrl_Up", -1, false }, { "ctrl_Down", -1, false },
    { "ctrl_A", -1, false }, { "ctrl_B", -1, false }, { "ctrl_X", -1, false }, { "ctrl_L", -1, false },
    { "ctrl_R", -1, false }, { "ctrl_Select", -1, false }, { "ctrl_Start", -1, false }, { "ctrl_L2", -1, false },
    { "ctrl_R2", -1, false }, { "ctrl_Exit", -1, false }, { "ctrl_Menu1", -1, false }, { "ctrl_Menu2", -1, false },
};
// global.opkeybtn_* in the CK_ order.
static Var v_keybtn[CK_COUNT] = {
    { "opkeybtn_left", -1, true }, { "opkeybtn_right", -1, true }, { "opkeybtn_up", -1, true },
    { "opkeybtn_down", -1, true }, { "opkeybtn_a", -1, true }, { "opkeybtn_b", -1, true }, { "opkeybtn_x", -1, true },
    { "opkeybtn_l", -1, true }, { "opkeybtn_r", -1, true }, { "opkeybtn_select", -1, true },
    { "opkeybtn_start", -1, true }, { "opkeybtn_l2", -1, true }, { "opkeybtn_r2", -1, true },
    { "opkeybtn_exit", -1, true }, { "opkeybtn_menu1", -1, true }, { "opkeybtn_menu2", -1, true },
};
// global.opxjoybtn_* in the CK_ order from CK_A (the d-pad uses gp_pad*).
static Var v_joybtn[CK_COUNT] = {
    { NULL, -1, true }, { NULL, -1, true }, { NULL, -1, true }, { NULL, -1, true },
    { "opxjoybtn_a", -1, true }, { "opxjoybtn_b", -1, true }, { "opxjoybtn_x", -1, true },
    { "opxjoybtn_l", -1, true }, { "opxjoybtn_r", -1, true }, { "opxjoybtn_sel", -1, true },
    { "opxjoybtn_str", -1, true }, { "opxjoybtn_l2", -1, true }, { "opxjoybtn_r2", -1, true },
    { "opxjoybtn_exit", -1, true }, { "opxjoybtn_menu1", -1, true }, { "opxjoybtn_menu2", -1, true },
};
VAR(walk_zone); VAR(xjoyx); VAR(xjoyy);
GVAR(controltype); GVAR(opaimstyle); GVAR(gamepadIndex); GVAR(opxjoyenable); GVAR(opxjoyvib); GVAR(vibL); GVAR(vibR);
GVAR(opxjoydz); GVAR(joydetected); GVAR(openablejoy);

// GameMaker's gamepad constants.
#define GP_PADU 32781
#define GP_PADD 32782
#define GP_PADL 32783
#define GP_PADR 32784
#define GP_AXISLH 32785
#define GP_AXISLV 32786

static GMLReal callNum(BuiltinFunc f, RValue* args, int32_t argCount) {
    RValue r = f(N.vm, args, argCount);
    GMLReal v = RValue_toReal(r);
    RValue_free(&r);
    return v;
}

static bool keyDown(GMLReal key) {
    RValue a[1] = { R(key) };
    return truthy(callNum(N.keyboardCheck, a, 1));
}

static bool padDown(GMLReal pad, GMLReal button) {
    RValue a[2] = { R(pad), R(button) };
    return truthy(callNum(N.gamepadButtonCheck, a, 2));
}

static GMLReal scriptNum(int32_t code, GMLReal a0, GMLReal a1, GMLReal a2) {
    RValue a[3] = { R(a0), R(a1), R(a2) };
    RValue r = callScript(code, a, 3);
    GMLReal v = RValue_toReal(r);
    RValue_free(&r);
    return v;
}

static RValue native_check_keys(VMContext* ctx, RValue* args, int32_t argCount) {
    Instance* self = (Instance*) ctx->currentInstance;
    GMLReal key[CK_COUNT], opaimstyle, pad = 0, xjoyenable = 0, joydetected, openablejoy;
    bool ok = self != NULL && globalNum(&v_opaimstyle, &opaimstyle) && globalNum(&v_joydetected, &joydetected) &&
        globalNum(&v_openablejoy, &openablejoy) && v_controltype.id >= 0 && v_walk_zone.id >= 0;
    for (int32_t k = 0; ok && k < CK_COUNT; k++) ok = v_ctrl[k].id >= 0 && globalNum(&v_keybtn[k], &key[k]);
    // The gamepad part runs when gamepad_is_supported() and gamepad_is_connected(global.gamepadIndex) and
    // global.opxjoyenable; the Android part when global.joydetected and global.openablejoy and the gamepad isn't
    // connected (the game's code then).
    bool supported = false, connected = false;
    if (ok) {
        supported = truthy(callNum(N.gamepadIsSupported, NULL, 0));
        ok = globalNum(&v_gamepadIndex, &pad);
        if (ok) {
            RValue a[1] = { R(pad) };
            connected = truthy(callNum(N.gamepadIsConnected, a, 1));
        }
    }
    if (ok && supported && connected) ok = globalNum(&v_opxjoyenable, &xjoyenable);
    bool gamepadPart = ok && supported && connected && truthy(xjoyenable);
    GMLReal vib = 0, vibL = 0, vibR = 0, dz = 0;
    GMLReal joyBtn[CK_COUNT] = { 0 };
    if (gamepadPart) {
        ok = globalNum(&v_opxjoyvib, &vib) && globalNum(&v_opxjoydz, &dz) && v_xjoyx.id >= 0 && v_xjoyy.id >= 0;
        if (ok && truthy(vib)) ok = globalNum(&v_vibL, &vibL) && globalNum(&v_vibR, &vibR);
        for (int32_t k = CK_A; ok && k < CK_COUNT; k++) {
            // opxjoybtn_l2 is only read with the aim style that uses it.
            if (k == CK_L2 && gmlCmp(opaimstyle, 1) != 0) continue;
            ok = globalNum(&v_joybtn[k], &joyBtn[k]);
        }
    }
    if (!ok || (truthy(joydetected) && truthy(openablejoy) && !connected)) return callOriginal(gCode_check_keys, args, argCount);

    GMLReal ctrl[CK_COUNT] = { 0 };
    GMLReal walkZone = 0;
    GMLReal controltype = 0;
    bool controltypeSet = false;
    #define CK_SET(k, v, type) do { ctrl[k] = (v); controltype = (type); controltypeSet = true; } while (0)

    // Keyboard.
    if (gmlCmp(ctrl[CK_LEFT], 0) == 0 && gmlCmp(ctrl[CK_RIGHT], 0) == 0 && !keyDown(key[CK_RIGHT]) && keyDown(key[CK_LEFT])) CK_SET(CK_LEFT, 1, 0);
    if (gmlCmp(ctrl[CK_LEFT], 0) == 0 && gmlCmp(ctrl[CK_RIGHT], 0) == 0 && !keyDown(key[CK_LEFT]) && keyDown(key[CK_RIGHT])) CK_SET(CK_RIGHT, 1, 0);
    if (gmlCmp(ctrl[CK_UP], 0) == 0 && gmlCmp(ctrl[CK_DOWN], 0) == 0 && !keyDown(key[CK_DOWN]) && keyDown(key[CK_UP])) CK_SET(CK_UP, 1, 0);
    if (gmlCmp(ctrl[CK_UP], 0) == 0 && gmlCmp(ctrl[CK_DOWN], 0) == 0 && !keyDown(key[CK_UP]) && keyDown(key[CK_DOWN])) CK_SET(CK_DOWN, 1, 0);
    static const int8_t kButtons[] = { CK_A, CK_B, CK_X, CK_L, CK_L2, CK_R, CK_R2, CK_SELECT, CK_START, CK_EXIT, CK_MENU1, CK_MENU2 };
    for (size_t b = 0; b < sizeof(kButtons); b++) {
        int32_t k = kButtons[b];
        if (k == CK_L2 && gmlCmp(opaimstyle, 1) != 0) continue; // global.opaimstyle == true
        if (keyDown(key[k])) CK_SET(k, 1, 0);
    }

    // Gamepad.
    if (gamepadPart) {
        // gamepad_set_vibration isn't a builtin here: the VM's unknown-function call does nothing.
        if (truthy(vib) && N.gamepadSetVibration != NULL) {
            RValue a[3] = { R(pad), R(vibL), R(vibR) };
            callVoid(N.gamepadSetVibration, a, 3);
        }
        RValue ah[2] = { R(pad), R(GP_AXISLH) };
        GMLReal jx = callNum(N.gamepadAxisValue, ah, 2);
        RValue av[2] = { R(pad), R(GP_AXISLV) };
        GMLReal jy = callNum(N.gamepadAxisValue, av, 2);
        setNum(self, &v_xjoyx, jx);
        setNum(self, &v_xjoyy, jy);
        if (truthy(scriptNum(gCode_is_walkzone, jx, jy, 1))) walkZone = 1;
        if (truthy(scriptNum(gCode_is_past_deadzone, jx, jy, 1))) {
            GMLReal limit = dz / 200;
            if (gmlCmp(ctrl[CK_LEFT], 0) == 0 && gmlCmp(ctrl[CK_RIGHT], 0) == 0 && gmlCmp(jx, -limit) < 0) CK_SET(CK_LEFT, -jx, 2);
            if (gmlCmp(ctrl[CK_LEFT], 0) == 0 && gmlCmp(ctrl[CK_RIGHT], 0) == 0 && gmlCmp(jx, limit) > 0) CK_SET(CK_RIGHT, jx, 2);
            if (gmlCmp(ctrl[CK_UP], 0) == 0 && gmlCmp(ctrl[CK_DOWN], 0) == 0 && gmlCmp(jy, -limit) < 0) CK_SET(CK_UP, 1, 2);
            if (gmlCmp(ctrl[CK_UP], 0) == 0 && gmlCmp(ctrl[CK_DOWN], 0) == 0 && gmlCmp(jy, limit) > 0) CK_SET(CK_DOWN, 1, 2);
        }
        static const struct { int8_t k, other; int32_t button; } kPad[] = {
            { CK_LEFT, CK_RIGHT, GP_PADL }, { CK_RIGHT, CK_LEFT, GP_PADR }, { CK_UP, CK_DOWN, GP_PADU }, { CK_DOWN, CK_UP, GP_PADD },
        };
        for (size_t d = 0; d < sizeof(kPad) / sizeof(kPad[0]); d++) {
            // (ctrl_Left == 0) && (ctrl_Right == false): Left/Right both checked whichever the direction.
            int32_t first = kPad[d].k <= CK_RIGHT ? CK_LEFT : CK_UP, second = first + 1;
            if (gmlCmp(ctrl[first], 0) == 0 && gmlCmp(ctrl[second], 0) == 0 && padDown(pad, kPad[d].button)) {
                CK_SET(kPad[d].k, 1, 2);
                walkZone = 0;
            }
        }
        for (size_t b = 0; b < sizeof(kButtons); b++) {
            int32_t k = kButtons[b];
            if (k == CK_L2 && gmlCmp(opaimstyle, 1) != 0) continue; // global.opaimstyle == 1
            if (gmlCmp(ctrl[k], 0) == 0 && padDown(pad, joyBtn[k])) CK_SET(k, 1, 2);
        }
    }
    #undef CK_SET

    for (int32_t k = 0; k < CK_COUNT; k++) setNum(self, &v_ctrl[k], ctrl[k]);
    setNum(self, &v_walk_zone, walkZone);
    if (controltypeSet) setNum(globalScope(), &v_controltype, controltype);
    return R(0.0);
}
