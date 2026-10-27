// AM2R 1.5.5's oWaterFXV2 Draw (the underwater ripple: the screen redrawn in 240 one-pixel strips, each shifted by a
// sine) with its strip loop in plain C. Everything before the loop is the translated code as generated
// (n3ds_aot_gen.inc); the loop reads values nothing in it can change, so they are read once, and calls
// draw_surface_part_ext directly. Same draw calls, same values; xx and yy are left as the loop leaves them. Anything
// that isn't a number runs the translated event instead. Included by n3ds_am2r_native.c after n3ds_aot_gen.inc.

static BuiltinFunc gDrawSurfacePartExt = NULL;

static inline bool waterNum(RValue r, GMLReal* out) {
    if (!aot_isNum(&r)) return false;
    *out = aot_real(&r);
    return true;
}

static inline void waterStrip(RValue surf, GMLReal top, GMLReal w, GMLReal h, GMLReal x, GMLReal y) {
    RValue a[11] = { surf, RValue_makeInt32(0), RValue_makeReal(top), RValue_makeReal(w), RValue_makeReal(h), RValue_makeReal(x),
        RValue_makeReal(y), RValue_makeInt32(1), RValue_makeInt32(1), RValue_makeInt32(-1), RValue_makeReal(0.2) };
    RValue r = gDrawSurfacePartExt(N.vm, a, 11);
    RValue_free(&r);
}

// Reads what the loop needs; false if one isn't a number.
static bool waterLoopInputs(VMContext* ctx, Instance* self, GMLReal* in) {
    Var* V = aot_oWaterFXV2_Draw_0_V;
    AotBuiltin* B = aot_oWaterFXV2_Draw_0_B;
    if (gDrawSurfacePartExt == NULL) return false;
    RValue s = aot_self(self, &V[0]);
    if (!aot_isNum(&s)) return false;
    if (!waterNum(aot_self(self, &V[6]), &in[0]) || !waterNum(aot_self(self, &V[7]), &in[1]) ||
        !waterNum(aot_self(self, &V[8]), &in[2]) || !waterNum(aot_self(self, &V[2]), &in[3]) ||
        !waterNum(aot_self(self, &V[3]), &in[4])) return false;
    RValue r;
    bool ok = true;
    r = aot_bget(ctx, self, &B[1], 0); ok = ok && waterNum(r, &in[5]); RValue_free(&r);   // view_yview[0]
    r = aot_bget(ctx, self, &B[4], 0); ok = ok && waterNum(r, &in[6]); RValue_free(&r);   // view_xview[0]
    r = aot_bget(ctx, self, &B[5], 0); ok = ok && waterNum(r, &in[7]); RValue_free(&r);   // view_wview[0]
    r = aot_bget(ctx, self, &B[3], -1); ok = ok && waterNum(r, &in[8]); RValue_free(&r);  // y
    r = aot_tget(ctx, RValue_makeInt32(427), &V[9], -1); ok = ok && waterNum(r, &in[9]); RValue_free(&r); // oControl.widescreen_space
    return ok;
}

static RValue aot_fast_oWaterFXV2_Draw_0(VMContext* ctx, bool* handled) {
    Instance* self = (Instance*) ctx->currentInstance;
    Var* V = aot_oWaterFXV2_Draw_0_V;
    AotBuiltin* B = aot_oWaterFXV2_Draw_0_B;
    int32_t* F = aot_oWaterFXV2_Draw_0_F;
    (void) V; (void) B; (void) F;
    GMLReal in[10];
    *handled = waterLoopInputs(ctx, self, in);
    RValue ret = RValue_makeReal(0.0);
    if (!*handled) return ret;
    const GMLReal amp = in[0], theta = in[1], freq = in[2], yBottom = in[3], yTop = in[4];
    const GMLReal vy = in[5], vx = in[6], vw = in[7], instY = in[8], ws = in[9];
    RValue L_height_top = RValue_makeUndefined();
    {
        RValue t2[1] = { aot_self(self, &V[0]) };
        RValue t1 = aot_call(ctx, F[0], t2, 1);
        RValue t3 = t1;
        bool t4 = aot_truthy(t3);
        RValue_free(&t3);
        if (t4) {
            {
                {
                    RValue t5 = RValue_makeInt32(1);
                    RValue t6 = RValue_makeReal(0.1);
                    RValue t7 = RValue_makeInt32(60);
                    RValue t8 = aot_bget(ctx, self, &B[0], -1);
                    RValue t9 = RValue_makeInt32(1);
                    RValue t10 = aot_sub(ctx, t8, t9);
                    RValue_free(&t8);
                    RValue t11 = t10;
                    RValue t12 = aot_sub(ctx, t7, t11);
                    RValue_free(&t11);
                    RValue t13 = t12;
                    RValue t14 = RValue_makeReal(1.5);
                    RValue t15 = aot_bxor(ctx, t13, t14);
                    RValue_free(&t13);
                    RValue t16 = t15;
                    RValue t18[1] = { t16 };
                    RValue t17 = aot_call(ctx, F[1], t18, 1);
                    RValue t19 = t17;
                    RValue t21[3] = { aot_global(&V[1]), t19, t6 };
                    RValue t20 = aot_call(ctx, F[2], t21, 3);
                    RValue t22 = t20;
                    RValue t24[1] = { t22 };
                    RValue t23 = aot_call(ctx, F[3], t24, 1);
                    RValue t25 = t23;
                    RValue t26 = RValue_makeInt32(1);
                    RValue t27 = aot_sub(ctx, t25, t26);
                    RValue_free(&t25);
                    RValue t28 = t27;
                    RValue t30[2] = { t28, t5 };
                    RValue t29 = aot_call(ctx, F[4], t30, 2);
                    RValue t31 = t29;
                    aot_setGlobal(&V[1], t31);
                    RValue_free(&t31);
                }
                {
                    RValue t32 = aot_global(&V[1]);
                    RValue t33 = t32;
                    RValue t34 = aot_sub(ctx, aot_self(self, &V[2]), aot_self(self, &V[3]));
                    RValue t35 = t34;
                    RValue t37[2] = { t35, t33 };
                    RValue t36 = aot_call(ctx, F[5], t37, 2);
                    RValue t38 = t36;
                    aot_setLocal(&L_height_top, t38);
                    RValue_free(&t38);
                }
                {
                    RValue t39 = RValue_makeInt32(0);
                    RValue t40 = aot_bget(ctx, self, &B[1], aot_index(t39));
                    RValue t41 = t40;
                    RValue t42 = RValue_makeInt32(0);
                    RValue t43 = aot_bget(ctx, self, &B[2], aot_index(t42));
                    RValue t44 = t43;
                    RValue t45 = aot_add(ctx, t41, t44);
                    RValue_free(&t41);
                    RValue_free(&t44);
                    RValue t46 = t45;
                    RValue t47 = aot_bget(ctx, self, &B[3], -1);
                    bool t48 = aot_cmp(t46, t47, CMP_GTE);
                    RValue_free(&t46);
                    RValue_free(&t47);
                    if (t48) {
                        GMLReal wh, heightTop;
                        RValue whv = aot_global(&V[1]);
                        if (!waterNum(whv, &wh) || !waterNum(aot_weak(L_height_top), &heightTop)) {
                            // Can't happen (max/min give numbers); the strips are skipped rather than run twice.
                            goto done;
                        }
                        RValue surf = aot_self(self, &V[0]);
                        GMLReal yy = 0.0, xx = 0.0;
                        bool ran = false;
                        while (gmlCmp(yy, 240.0) < 0) {
                            ran = true;
                            xx = amp * GMLReal_sin(theta + (freq * yy) / yBottom);
                            if (gmlCmp(yy, yTop) >= 0) {
                                waterStrip(surf, yy, vw + ws, wh, (vx + xx) - ws / 2, vy + yy);
                                if (gmlCmp(yy - yTop, wh) < 0) waterStrip(surf, yTop, vw + ws, yy - yTop, (vx + xx) - ws / 2, vy + yTop);
                            }
                            yy = yy + wh;
                        }
                        aot_setVN(self, &V[4], yy);
                        if (ran) aot_setVN(self, &V[5], xx);
                        else if (!waterNum(aot_self(self, &V[5]), &xx)) xx = 0.0;
                        if (gmlCmp(heightTop, wh) < 0) waterStrip(surf, yTop, vw + ws, heightTop, (vx + xx) - ws / 2, instY);
                    }
                }
            }
        }
    }
    goto done;
done:
    return ret;
}

static bool aot_fast_oWaterFXV2_Draw_0_event(MAYBE_UNUSED Runner* runner, MAYBE_UNUSED Instance* instance) {
    AotEventMark mark = aot_eventEnter("gml_Object_oWaterFXV2_Draw_0");
    bool handled = false;
    RValue r = aot_fast_oWaterFXV2_Draw_0(N.vm, &handled);
    RValue_free(&r);
    if (!handled) {
        r = aot_oWaterFXV2_Draw_0(N.vm, NULL, 0);
        RValue_free(&r);
    }
    aot_eventLeave(mark);
    return true;
}
