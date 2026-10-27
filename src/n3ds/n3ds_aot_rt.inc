// Runtime for the scripts translated to C by tools/n3ds/aot/gml2c.py (n3ds_aot_gen.inc). Included by
// n3ds_am2r_native.c. Each helper does what the interpreter's opcodes do on the same values (src/vm.c's
// executeLoop): the plain number cases inline, everything else through the VM's own code (VM_aot*).

typedef struct {
    const char* name;
    int16_t id; // VMBuiltins_resolveBuiltinVarId
} AotBuiltin;

typedef struct {
    const char* name;      // the script, or the event's code name
    BuiltinFunc func;
    NativeEventCode event; // for an object event: runs func on the instance
    Var* vars;
    int32_t varCount;
    AotBuiltin* builtins;
    int32_t builtinCount;
    const char** funcNames;
    int32_t* funcs;        // FUNC indices, filled at init
    int32_t funcCount;
} AotScript;

// The helpers stay out of line: called from tens of thousands of places, inlined they would make the translated
// code several times bigger (the Old 3DS's memory matters more than a call).

// The translated event running right now (its code name), and the VM's code index when it started: code that
// checks ctx->currentCodeIndex (the HUD skip) asks here instead. Interpreted code run from inside sets its own index.
static const char* gAotEventName = NULL;
static int32_t gAotEventCodeIndex = -1;

typedef struct { const char* name; int32_t codeIndex; } AotEventMark;

static inline AotEventMark aot_eventEnter(const char* name) {
    AotEventMark mark = { gAotEventName, gAotEventCodeIndex };
    gAotEventName = name;
    gAotEventCodeIndex = N.vm->currentCodeIndex;
    return mark;
}

static inline void aot_eventLeave(AotEventMark mark) {
    gAotEventName = mark.name;
    gAotEventCodeIndex = mark.codeIndex;
}

// True while the translated <name> event runs its own code (not interpreted code it called).
static inline bool aot_inEvent(VMContext* ctx, const char* name) {
    return gAotEventName != NULL && strcmp(gAotEventName, name) == 0 && ctx->currentCodeIndex == gAotEventCodeIndex;
}

static inline RValue aot_weak(RValue v) {
    v.ownsReference = false;
    return v;
}

// Pop.v.i: an int constant is stored as a real.
static inline RValue aot_intToReal(RValue v) {
    if (v.type == RVALUE_INT32) return RValue_makeReal((GMLReal) v.int32);
    return v;
}

// Conv.v.b then BF/BT.
static NOINLINE bool aot_truthy(RValue v) {
    switch (v.type) {
        case RVALUE_INT32: return v.int32 > 0;
        case RVALUE_BOOL: return v.int32 != 0;
        case RVALUE_REAL: return v.real > (GMLReal) 0.5;
        default: return RValue_toBool(v);
    }
}

// An array index popped off the stack.
static inline int32_t aot_index(RValue v) {
    return RValue_toInt32(v);
}

static inline bool aot_isNum(const RValue* v) {
    return v->type == RVALUE_INT32 || v->type == RVALUE_REAL;
}

static inline GMLReal aot_real(const RValue* v) {
    return v->type == RVALUE_INT32 ? (GMLReal) v->int32 : v->real;
}

static NOINLINE bool aot_cmp(RValue a, RValue b, int32_t kind) {
    if (aot_isNum(&a) && aot_isNum(&b)) {
        GMLReal diff = aot_real(&a) - aot_real(&b);
        int c = GMLReal_fabs(diff) <= GML_MATH_EPSILON ? 0 : (diff < 0 ? -1 : 1);
        switch (kind) {
            case CMP_LT: return c < 0;
            case CMP_LTE: return c <= 0;
            case CMP_EQ: return c == 0;
            case CMP_NEQ: return c != 0;
            case CMP_GTE: return c >= 0;
            case CMP_GT: return c > 0;
            default: return false;
        }
    }
    return VM_aotCmpSlow(N.vm, a, b, kind);
}

#define AOT_ARITH(fname, opcode, cop)                                                           \
    static NOINLINE RValue fname(VMContext* ctx, RValue a, RValue b) {                           \
        if (aot_isNum(&a) && aot_isNum(&b)) {                                                  \
            if (a.type == RVALUE_INT32 && b.type == RVALUE_INT32)                              \
                return RValue_makeInt32((int32_t) ((uint32_t) a.int32 cop (uint32_t) b.int32)); \
            return RValue_makeReal(aot_real(&a) cop aot_real(&b));                             \
        }                                                                                      \
        return VM_aotArithSlow(ctx, opcode, a, b);                                             \
    }
AOT_ARITH(aot_add, OP_ADD, +)
AOT_ARITH(aot_sub, OP_SUB, -)
AOT_ARITH(aot_mul, OP_MUL, *)
#undef AOT_ARITH

static NOINLINE RValue aot_div(VMContext* ctx, RValue a, RValue b) {
    if (aot_isNum(&a) && aot_isNum(&b)) return RValue_makeReal(aot_real(&a) / aot_real(&b));
    return VM_aotArithSlow(ctx, OP_DIV, a, b);
}

static inline RValue aot_mod(VMContext* ctx, RValue a, RValue b) {
    return VM_aotArithSlow(ctx, OP_MOD, a, b);
}

static inline RValue aot_idiv(VMContext* ctx, RValue a, RValue b) {
    return VM_aotArithSlow(ctx, OP_REM, a, b);
}

static inline RValue aot_band(VMContext* ctx, RValue a, RValue b) { return VM_aotArithSlow(ctx, OP_AND, a, b); }
static inline RValue aot_bor(VMContext* ctx, RValue a, RValue b) { return VM_aotArithSlow(ctx, OP_OR, a, b); }
static inline RValue aot_bxor(VMContext* ctx, RValue a, RValue b) { return VM_aotArithSlow(ctx, OP_XOR, a, b); }
static inline RValue aot_shl(VMContext* ctx, RValue a, RValue b) { return VM_aotArithSlow(ctx, OP_SHL, a, b); }
static inline RValue aot_shr(VMContext* ctx, RValue a, RValue b) { return VM_aotArithSlow(ctx, OP_SHR, a, b); }

static NOINLINE RValue aot_neg(RValue a) {
    return RValue_makeReal(-RValue_toReal(a));
}

static inline RValue aot_bitnot(RValue a) {
    return RValue_makeInt32(~RValue_toInt32(a));
}

// argumentN / argument[N]: the caller's value, undefined past the count.
static inline RValue aot_arg(RValue* args, int32_t argCount, int32_t n) {
    if (args == NULL || n < 0 || n >= argCount) return RValue_makeUndefined();
    RValue r = args[n];
    r.ownsReference = false;
    return r;
}

// ---- variables ----

static NOINLINE RValue aot_self(Instance* self, const Var* v) {
    RValue* slot = IntRValueHashMap_findSlot(&self->selfVars, v->id);
    if (slot == NULL) return RValue_makeUndefined();
    RValue r = *slot;
    r.ownsReference = false;
    return r;
}

static NOINLINE void aot_setSelf(Instance* self, const Var* v, RValue val) {
    Instance_setSelfVar(self, v->id, val);
}

static NOINLINE RValue aot_selfArr(Instance* self, const Var* v, int32_t index) {
    RValue* slot = IntRValueHashMap_findSlot(&self->selfVars, v->id);
    if (slot == NULL) return RValue_makeUndefined();
    return GMLArray_getOnArrayRef(slot, index);
}

static NOINLINE void aot_setSelfArr(VMContext* ctx, Instance* self, const Var* v, int32_t index, RValue val) {
    RValue* slot = IntRValueHashMap_getOrInsertUndefined(&self->selfVars, v->id);
    VM_aotArraySet(ctx, slot, index, val);
}

static NOINLINE RValue aot_global(const Var* v) {
    return aot_self(globalScope(), v);
}

static NOINLINE RValue aot_globalArr(const Var* v, int32_t index) {
    return aot_selfArr(globalScope(), v, index);
}

static NOINLINE void aot_setGlobal(const Var* v, RValue val) {
    Instance_setSelfVar(globalScope(), v->id, val);
}

static NOINLINE void aot_setGlobalArr(VMContext* ctx, const Var* v, int32_t index, RValue val) {
    aot_setSelfArr(ctx, globalScope(), v, index, val);
}

static NOINLINE void aot_setLocal(RValue* local, RValue val) {
    RValue copy = RValue_makeIndependent(val);
    RValue_free(local);
    *local = copy;
}

// Built-ins on self (index -1: not an array).
static NOINLINE RValue aot_bget(VMContext* ctx, Instance* self, const AotBuiltin* b, int32_t index) {
    return VMBuiltins_getVariable(ctx, self, b->id, b->name, index < 0 ? 0 : index);
}

static NOINLINE void aot_bset(VMContext* ctx, Instance* self, const AotBuiltin* b, int32_t index, RValue val) {
    VMBuiltins_setVariable(ctx, self, b->id, b->name, val, index < 0 ? 0 : index);
}

// target.<var> (target: an object, an instance id, or -1 self / -2 other).
static NOINLINE RValue aot_tget(VMContext* ctx, RValue target, const Var* v, int32_t index) {
    return VM_aotReadVar(ctx, RValue_toInt32(target), v->id, -1, v->name, index >= 0, index);
}

static NOINLINE void aot_tset(VMContext* ctx, RValue target, const Var* v, int32_t index, RValue val) {
    VM_aotWriteVar(ctx, RValue_toInt32(target), v->id, -1, v->name, index >= 0, index, val);
}

static NOINLINE RValue aot_tbget(VMContext* ctx, RValue target, const AotBuiltin* b, int32_t index) {
    return VM_aotReadVar(ctx, RValue_toInt32(target), VARIABLE_BUILTIN, b->id, b->name, index >= 0, index);
}

static NOINLINE void aot_tbset(VMContext* ctx, RValue target, const AotBuiltin* b, int32_t index, RValue val) {
    VM_aotWriteVar(ctx, RValue_toInt32(target), VARIABLE_BUILTIN, b->id, b->name, index >= 0, index, val);
}

// ---- calls (handleCall): the arguments' owned values are freed, as the VM does ----

static RValue aot_call(VMContext* ctx, int32_t funcIndex, RValue* args, int32_t argCount) {
    VM_periodicTick();
    RValue result;
    FuncCallCache* cache = &ctx->funcCallCache[funcIndex];
    if (cache->builtin != NULL) {
        result = ((BuiltinFunc) cache->builtin)(ctx, args, argCount);
    } else if (cache->scriptCodeIndex >= 0) {
        result = VM_callCodeIndex(ctx, cache->scriptCodeIndex, args, argCount);
    } else {
        result = RValue_makeUndefined();
    }
    for (int32_t i = 0; i < argCount; i++) RValue_free(&args[i]);
    return result;
}

// ---- compact forms (gml2c's common shapes) ----
// a.k.a. aot_cmp(aot_self(inst, a), aot_self(inst2, b), kind): reads of plain variables.
static NOINLINE bool aot_cmpVV(Instance* ia, const Var* a, Instance* ib, const Var* b, int32_t kind) {
    return aot_cmp(aot_self(ia, a), aot_self(ib, b), kind);
}

// A variable against a number literal (int or real: the comparison's result is the same either way).
static NOINLINE bool aot_cmpVN(Instance* ia, const Var* a, GMLReal n, int32_t kind) {
    return aot_cmp(aot_self(ia, a), RValue_makeReal(n), kind);
}

static NOINLINE bool aot_truthyV(Instance* inst, const Var* v) {
    return aot_truthy(aot_self(inst, v));
}

// v = <number literal> (Pop.v.i / Pop.v.d: stored as a real).
static NOINLINE void aot_setVN(Instance* inst, const Var* v, GMLReal n) {
    Instance_setSelfVar(inst, v->id, RValue_makeReal(n));
}

// v = w (Pop.v.v of a plain read).
static NOINLINE void aot_setVV(Instance* inst, const Var* v, Instance* src, const Var* w) {
    Instance_setSelfVar(inst, v->id, aot_self(src, w));
}

static RValue aot_arith(VMContext* ctx, int32_t opcode, RValue a, RValue b) {
    switch (opcode) {
        case OP_ADD: return aot_add(ctx, a, b);
        case OP_SUB: return aot_sub(ctx, a, b);
        default: return aot_mul(ctx, a, b);
    }
}

// v op= <int literal> / <real literal>.
static NOINLINE void aot_opVI(VMContext* ctx, Instance* inst, const Var* v, int32_t opcode, int32_t n) {
    RValue r = aot_arith(ctx, opcode, aot_self(inst, v), RValue_makeInt32(n));
    Instance_setSelfVar(inst, v->id, r);
    RValue_free(&r);
}

static NOINLINE void aot_opVR(VMContext* ctx, Instance* inst, const Var* v, int32_t opcode, GMLReal n) {
    RValue r = aot_arith(ctx, opcode, aot_self(inst, v), RValue_makeReal(n));
    Instance_setSelfVar(inst, v->id, r);
    RValue_free(&r);
}
