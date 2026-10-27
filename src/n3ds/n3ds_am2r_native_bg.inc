// AM2R 1.5.5's oBackground Draw (the parallax backgrounds) in C. Included by n3ds_am2r_native.c, after the HUD
// (which declares xoff, widescreen).
//
//   for (i = 0; i < bgno; i += 1) if (bgid[i] != bgScreenGuide) draw_background(_tiled)_ext(bgid[i],
//       floor(view_xview[v] * xratio[i] + xoff[i] + xoffw), floor(view_yview[v] * yratio[i] + yoff[i]), ...)
// with v = 1 when oControl.widescreen, else 0, and xoffw the two edge cases' shift. Everything is read and checked
// before anything is drawn; anything unexpected runs the game's code.

VAR(bgno); VAR(bgid); VAR(tile); VAR(xratio); VAR(yratio); VAR(yoff); VAR(xscale); VAR(yscale); VAR(alpha);
static BuiltinFunc gDrawBackgroundTiledExt = NULL;

#define BG_MAX_LAYERS 32
#define BG_SCREEN_GUIDE 0 // bgScreenGuide
#define BG_A5_OUTSIDE 216 // bgA5Outside: tiled, 64 px further left
#define BG_A7_CAVE 224    // bgA7Cave: not tiled, 48 px further left

// self.<v>[i] as a number.
static bool arrNum(Instance* self, const Var* v, int32_t i, GMLReal* out) {
    RValue* slot = varSlot(self, v);
    if (slot == NULL || slot->type != RVALUE_ARRAY || slot->array == NULL) return false;
    RValue* cell = GMLArray_slot(slot->array, i);
    if (cell == NULL || !isNumber(cell)) return false;
    *out = RValue_toReal(*cell);
    return true;
}

static bool native_oBackground_Draw_0(MAYBE_UNUSED Runner* runner, Instance* self) {
    Instance* control = VM_findInstanceByTarget(N.vm, N.objControl);
    GMLReal bgno, widescreen;
    if (gDrawBackgroundTiledExt == NULL || control == NULL || !num(self, &v_bgno, &bgno) ||
        !num(control, &v_widescreen, &widescreen) || v_i.id < 0) return false;
    // i runs 0, 1, ... while i < bgno.
    int32_t count = 0;
    while (count < BG_MAX_LAYERS && gmlCmp((GMLReal) count, bgno) < 0) count++;
    if (count == BG_MAX_LAYERS && gmlCmp((GMLReal) count, bgno) < 0) return false;
    struct { GMLReal bgid, tile, xratio, yratio, xoff, yoff, xscale, yscale, alpha; } L[BG_MAX_LAYERS];
    for (int32_t k = 0; k < count; k++) {
        if (!arrNum(self, &v_bgid, k, &L[k].bgid)) return false;
        if (gmlCmp(L[k].bgid, BG_SCREEN_GUIDE) == 0) continue;
        if (!arrNum(self, &v_tile, k, &L[k].tile) || !arrNum(self, &v_xratio, k, &L[k].xratio) ||
            !arrNum(self, &v_yratio, k, &L[k].yratio) || !arrNum(self, &v_xoff, k, &L[k].xoff) ||
            !arrNum(self, &v_yoff, k, &L[k].yoff) || !arrNum(self, &v_xscale, k, &L[k].xscale) ||
            !arrNum(self, &v_yscale, k, &L[k].yscale) || !arrNum(self, &v_alpha, k, &L[k].alpha)) return false;
    }
    bool wide = truthy(widescreen);
    int32_t view = wide ? 1 : 0;
    RValue rvx = VMBuiltins_getVariable(N.vm, self, b_view_xview, "view_xview", view);
    RValue rvy = VMBuiltins_getVariable(N.vm, self, b_view_yview, "view_yview", view);
    bool viewOk = isNumber(&rvx) && isNumber(&rvy);
    GMLReal vx = viewOk ? RValue_toReal(rvx) : 0, vy = viewOk ? RValue_toReal(rvy) : 0;
    RValue_free(&rvx);
    RValue_free(&rvy);
    if (!viewOk) return false;

    GMLReal xoffw = 0;
    for (int32_t k = 0; k < count; k++) {
        if (gmlCmp(L[k].bgid, BG_SCREEN_GUIDE) == 0) continue;
        bool tiled = gmlCmp(L[k].tile, 1) == 0;
        if (!wide) {
            if (tiled) xoffw = gmlCmp(L[k].bgid, BG_A5_OUTSIDE) == 0 ? -64 : 0;
            else xoffw = gmlCmp(L[k].bgid, BG_A7_CAVE) == 0 ? -48 : 0;
        }
        GMLReal y = GMLReal_floor(vy * L[k].yratio + L[k].yoff);
        if (tiled) {
            GMLReal x = GMLReal_floor(vx * L[k].xratio + L[k].xoff + xoffw);
            RValue a[7] = { R(L[k].bgid), R(x), R(y), R(L[k].xscale), R(L[k].yscale), R(-1), R(L[k].alpha) };
            callVoid(gDrawBackgroundTiledExt, a, 7);
        } else {
            // The widescreen branch leaves xoffw out of the non-tiled x.
            GMLReal x = wide ? GMLReal_floor(vx * L[k].xratio + L[k].xoff) : GMLReal_floor(vx * L[k].xratio + L[k].xoff + xoffw);
            RValue a[8] = { R(L[k].bgid), R(x), R(y), R(L[k].xscale), R(L[k].yscale), R(0), R(-1), R(L[k].alpha) };
            callVoid(N.drawBackgroundExt, a, 8);
        }
    }
    setNum(self, &v_i, (GMLReal) count); // the game's loop variable (an instance variable) ends at bgno
    return true;
}

// AM2R 1.5.5's oTitleAnimation Draw: draw_sprite(sTitleAnimated, subim, -53, -bgyoff). The title background is 60
// frames of 426x640, two 512 KB texture pages each: on the Old 3DS every frame change reads 1 MB from the SD card and
// uploads it (70-200 ms), which made the title menu run at 20 fps with hitches. There the first frame is kept (the
// background still scrolls); its two pages stay resident. The New 3DS keeps the animation.
VAR(bgyoff);
static int32_t gSprTitleAnimated = -1;
static int8_t gTitleStill = -1;

static bool native_oTitleAnimation_Draw_0(MAYBE_UNUSED Runner* runner, Instance* self) {
    if (gTitleStill < 0) {
#ifdef __3DS__
        bool isNew3DS = false;
        APT_CheckNew3DS(&isNew3DS);
        gTitleStill = isNew3DS ? 0 : 1;
#else
        gTitleStill = getenv("BS_TITLE_ANIM") != NULL ? 0 : 1;
#endif
    }
    GMLReal bgyoff;
    if (!gTitleStill || gSprTitleAnimated < 0 || !num(self, &v_bgyoff, &bgyoff)) return false;
    drawSprite(gSprTitleAnimated, 0, -53, -bgyoff);
    return true;
}
