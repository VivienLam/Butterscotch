#!/usr/bin/env python3
"""GML (GameMaker: Studio 1.4) -> C translator for the 3DS port's AOT scripts.

Translates a GML script into a C function with the VM's exact semantics (values stay RValues, every operation goes
through the helpers in n3ds_aot_rt.h that mirror the interpreter's opcodes), minus the interpreter: no instruction
decoding, no dispatch, no VM stack. Evaluation order follows the bytecode GMS 1.4 emits (call arguments right to
left, short-circuit && and ||, C precedence).

Usage: gml2c.py <data.win> <gml dir> <out.inc> script_name[=c_name] ...
"""
import re
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from datawin import DataWin  # noqa: E402

# ===[ Tokenizer ]===

TOKEN_RE = re.compile(r'''
    (?P<ws>\s+)
  | (?P<lcom>//[^\n]*)
  | (?P<bcom>/\*.*?(?:\*/|\Z))
  | (?P<num>\$[0-9A-Fa-f]+|\d+\.\d*|\.\d+|\d+)
  | (?P<str>"[^"]*"|'[^']*')
  | (?P<id>[A-Za-z_][A-Za-z0-9_]*)
  | (?P<op>==|!=|<=|>=|&&|\|\||\^\^|\+=|-=|\*=|/=|\|=|&=|\^=|<<|>>|<>|:=|\+\+|--|[-+*/%<>=!&|^~(){}\[\],;.:?])
''', re.S | re.X)


class Tok:
    def __init__(self, kind, text, line):
        self.kind, self.text, self.line = kind, text, line

    def __repr__(self):
        return f'{self.kind}:{self.text}@{self.line}'


def tokenize(src):
    toks = []
    pos = 0
    line = 1
    while pos < len(src):
        m = TOKEN_RE.match(src, pos)
        if not m:
            raise SyntaxError(f'bad char {src[pos]!r} at line {line}')
        kind = m.lastgroup
        text = m.group(kind)
        if kind not in ('ws', 'lcom', 'bcom'):
            toks.append(Tok(kind, text, line))
        line += text.count('\n')
        pos = m.end()
    toks.append(Tok('eof', '', line))
    return toks


# ===[ Parser ]===
# Expressions: ('num', value, is_int) ('str', s) ('name', n) ('dot', base, n) ('index', base, [idx...])
#              ('call', name, [args]) ('un', op, e) ('bin', op, a, b)
# Statements:  ('block', [..]) ('if', c, t, e) ('while', c, s) ('do', s, c) ('for', init, c, step, s) ('repeat', n, s)
#              ('with', t, s) ('switch', e, [(case_expr|None, [stmts])]) ('break',) ('continue',) ('exit',)
#              ('return', e) ('var', [(name, init)]) ('assign', target, op, e) ('expr', e)

KEYWORD_OPS = {'and': '&&', 'or': '||', 'xor': '^^', 'not': '!', 'mod': '%', 'div': 'div'}


class Parser:
    def __init__(self, toks):
        self.t = toks
        self.i = 0

    def peek(self, k=0):
        return self.t[self.i + k]

    def at(self, text):
        tok = self.peek()
        return tok.kind in ('op', 'id') and tok.text == text

    def take(self, text=None):
        tok = self.peek()
        if text is not None and not (tok.kind in ('op', 'id') and tok.text == text):
            raise SyntaxError(f'expected {text!r}, got {tok!r}')
        self.i += 1
        return tok

    def opt(self, text):
        if self.at(text):
            self.i += 1
            return True
        return False

    def program(self):
        out = []
        while self.peek().kind != 'eof':
            out.append(self.statement())
        return ('block', out)

    def statement(self):
        tok = self.peek()
        if tok.kind == 'op' and tok.text == ';':
            self.i += 1
            return ('block', [])
        if tok.kind == 'op' and tok.text == '{' or tok.kind == 'id' and tok.text == 'begin':
            close = '}' if tok.text == '{' else 'end'
            self.i += 1
            body = []
            while not self.at(close):
                body.append(self.statement())
            self.take(close)
            return ('block', body)
        if tok.kind == 'id':
            kw = tok.text
            if kw == 'var' or kw == 'globalvar':
                self.i += 1
                decls = []
                while True:
                    name = self.take().text
                    init = None
                    if self.opt('='):
                        init = self.expr()
                    decls.append((name, init))
                    if not self.opt(','):
                        break
                self.opt(';')
                if kw == 'globalvar':
                    raise SyntaxError('globalvar not supported')
                return ('var', decls)
            if kw == 'if':
                self.i += 1
                c = self.expr()
                self.opt('then')
                t = self.statement()
                e = None
                if self.opt('else'):
                    e = self.statement()
                return ('if', c, t, e)
            if kw == 'while':
                self.i += 1
                c = self.expr()
                self.opt('do')
                return ('while', c, self.statement())
            if kw == 'do':
                self.i += 1
                s = self.statement()
                self.take('until')
                c = self.expr()
                self.opt(';')
                return ('do', s, c)
            if kw == 'for':
                self.i += 1
                self.take('(')
                init = None if self.at(';') else self.simple_statement()
                self.take(';')
                c = None if self.at(';') else self.expr()
                self.take(';')
                step = None if self.at(')') else self.simple_statement()
                self.take(')')
                return ('for', init, c, step, self.statement())
            if kw == 'repeat':
                self.i += 1
                n = self.expr()
                return ('repeat', n, self.statement())
            if kw == 'with':
                self.i += 1
                t = self.expr()
                return ('with', t, self.statement())
            if kw == 'switch':
                self.i += 1
                e = self.expr()
                self.take('{')
                cases = []
                while not self.at('}'):
                    if self.opt('case'):
                        ce = self.expr()
                        self.take(':')
                        cases.append([ce, []])
                    elif self.opt('default'):
                        self.take(':')
                        cases.append([None, []])
                    else:
                        if not cases:
                            raise SyntaxError('statement before first case')
                        cases[-1][1].append(self.statement())
                self.take('}')
                return ('switch', e, cases)
            if kw in ('break', 'continue', 'exit'):
                self.i += 1
                self.opt(';')
                return (kw,)
            if kw == 'return':
                self.i += 1
                e = None if self.at(';') or self.at('}') else self.expr()
                self.opt(';')
                return ('return', e)
        s = self.simple_statement()
        self.opt(';')
        return s

    def simple_statement(self):
        target = self.postfix()
        tok = self.peek()
        if tok.kind == 'op' and tok.text in ('=', ':=', '+=', '-=', '*=', '/=', '|=', '&=', '^='):
            self.i += 1
            op = '=' if tok.text in ('=', ':=') else tok.text[0]
            return ('assign', target, op, self.expr())
        if tok.kind == 'op' and tok.text in ('++', '--'):
            self.i += 1
            return ('assign', target, tok.text[0], ('num', 1, True))
        if target[0] != 'call':
            raise SyntaxError(f'expression statement is not a call: {target!r} at {tok!r}')
        return ('expr', target)

    # precedence climbing, C-like
    BINARY = [
        ['||', '^^'],
        ['&&'],
        ['==', '!=', '<', '<=', '>', '>=', '<>', '='],
        ['|'],
        ['^'],
        ['&'],
        ['<<', '>>'],
        ['+', '-'],
        ['*', '/', '%', 'div'],
    ]

    def binop_text(self):
        tok = self.peek()
        if tok.kind == 'op':
            return tok.text
        if tok.kind == 'id' and tok.text in KEYWORD_OPS and KEYWORD_OPS[tok.text] != '!':
            return KEYWORD_OPS[tok.text]
        return None

    def expr(self, level=0):
        if level == len(self.BINARY):
            return self.unary()
        a = self.expr(level + 1)
        while True:
            op = self.binop_text()
            if op not in self.BINARY[level]:
                return a
            self.i += 1
            b = self.expr(level + 1)
            if op == '<>':
                op = '!='
            if op == '=':
                op = '=='
            a = ('bin', op, a, b)

    def unary(self):
        tok = self.peek()
        if tok.kind == 'op' and tok.text in ('!', '-', '~', '+') or tok.kind == 'id' and tok.text == 'not':
            self.i += 1
            op = '!' if tok.text == 'not' else tok.text
            e = self.unary()
            if op == '-' and e[0] == 'num':
                return ('num', -e[1], e[2])
            if op == '+':
                return e
            return ('un', op, e)
        return self.postfix()

    def postfix(self):
        e = self.primary()
        while True:
            if self.at('.'):
                self.i += 1
                e = ('dot', e, self.take().text)
            elif self.at('['):
                self.i += 1
                idx = [self.expr()]
                while self.opt(','):
                    idx.append(self.expr())
                self.take(']')
                e = ('index', e, idx)
            else:
                return e

    def primary(self):
        tok = self.take()
        if tok.kind == 'num':
            t = tok.text
            if t.startswith('$'):
                return ('num', int(t[1:], 16), True)
            if '.' in t:
                return ('num', float(t), False)
            return ('num', int(t), True)
        if tok.kind == 'str':
            return ('str', tok.text[1:-1])
        if tok.kind == 'op' and tok.text == '(':
            e = self.expr()
            self.take(')')
            return e
        if tok.kind == 'id':
            if self.at('('):
                self.i += 1
                args = []
                if not self.at(')'):
                    args.append(self.expr())
                    while self.opt(','):
                        args.append(self.expr())
                self.take(')')
                return ('call', tok.text, args)
            return ('name', tok.text)
        raise SyntaxError(f'unexpected {tok!r}')


# ===[ Code generation ]===

CONSTANTS = {
    'true': 1, 'false': 0, 'noone': -4, 'self': -1, 'other': -2, 'all': -3, 'global': -5,
    'c_aqua': 16776960, 'c_black': 0, 'c_blue': 16711680, 'c_dkgray': 4210752, 'c_fuchsia': 16711935,
    'c_gray': 8421504, 'c_green': 32768, 'c_lime': 65280, 'c_ltgray': 12632256, 'c_maroon': 128, 'c_navy': 8388608,
    'c_olive': 32896, 'c_orange': 4235519, 'c_purple': 8388736, 'c_red': 255, 'c_silver': 12632256, 'c_teal': 8421376,
    'c_white': 16777215, 'c_yellow': 65535,
    'fa_left': 0, 'fa_center': 1, 'fa_right': 2, 'fa_top': 0, 'fa_middle': 1, 'fa_bottom': 2,
    'bm_normal': 0, 'bm_add': 1, 'bm_max': 2, 'bm_subtract': 3,
    'vk_nokey': 0, 'vk_anykey': 1, 'vk_left': 37, 'vk_right': 39, 'vk_up': 38, 'vk_down': 40, 'vk_enter': 13,
    'vk_escape': 27, 'vk_space': 32, 'vk_shift': 16, 'vk_control': 17, 'vk_alt': 18,
    'os_windows': 0, 'os_macosx': 1, 'os_psp': 2, 'os_ios': 3, 'os_android': 4, 'os_symbian': 5, 'os_linux': 6,
    'os_winphone': 7, 'os_tizen': 8, 'os_win8native': 9, 'os_wiiu': 10, 'os_3ds': 11, 'os_psvita': 12, 'os_bb10': 13,
    'os_ps4': 14, 'os_xboxone': 15, 'os_ps3': 16, 'os_xbox360': 17, 'os_uwp': 18,
    'browser_not_a_browser': -1, 'os_browser': -1,  # a native build: the compiler folds these
}
MACROS = {}  # the project's constants (macros): name -> expression text, loaded in main()
REAL_CONSTANTS = {'pi': 3.141592653589793}


class Gen:
    def __init__(self, dw, script_name, c_name, locals_):
        self.dw = dw
        self.script = script_name
        self.c = c_name
        self.locals = set(locals_)
        self.lines = []
        self.ind = 1
        self.tmp = 0
        self.label = 0
        self.vars = {}      # (name, global) -> index into the script's Var table
        self.builtins = {}  # name -> index into the builtin table
        self.funcs = {}     # name -> index into the function table
        self.loop_stack = []  # 'loop' | ('with',) | ('switch', end_label)
        self.with_depth = 0

    def emit(self, s):
        self.lines.append('    ' * self.ind + s)

    def new_tmp(self):
        self.tmp += 1
        return f't{self.tmp}'

    def new_label(self):
        self.label += 1
        return f'L{self.label}'

    def var_index(self, name, is_global):
        key = (name, is_global)
        if key not in self.vars:
            self.vars[key] = len(self.vars)
        return self.vars[key]

    def builtin_index(self, name):
        if name not in self.builtins:
            self.builtins[name] = len(self.builtins)
        return self.builtins[name]

    def func_index(self, name):
        if name not in self.funcs:
            self.funcs[name] = len(self.funcs)
        return self.funcs[name]

    # A bare name's meaning.
    def classify(self, name):
        if name in self.locals:
            return ('local',)
        m = re.fullmatch(r'argument(\d+)', name)
        if m:
            return ('arg', int(m.group(1)))
        if name == 'argument_count':
            return ('argc',)
        if name == 'argument':
            return ('argarr',)
        kinds = self.dw.var_kinds(name)
        if 'builtin' in kinds:
            return ('builtin',)
        if 'self' in kinds:
            return ('self',)
        if name in CONSTANTS:
            return ('const', CONSTANTS[name])
        if name in MACROS:
            return ('macro', MACROS[name])
        if name in REAL_CONSTANTS:
            return ('rconst', REAL_CONSTANTS[name])
        if name in self.dw.assets:
            return ('const', self.dw.assets[name][1])
        if 'global' in kinds:
            return ('gvar',)  # globalvar
        raise SyntaxError(f'{self.script}: unknown name {name}')

    # ---- expressions: each returns (c_expr, kind, owned) ----
    # kind: 'rv' (an RValue expression), 'bool' (a C bool), 'int' (an int literal, stored as real by assignments)
    def as_rv(self, v):
        e, kind, owned = v
        if kind == 'bool':
            return (f'RValue_makeBool({e})', 'rv', False)
        return (e, 'rv', owned)

    def truth(self, v):
        e, kind, owned = v
        if kind == 'bool':
            return e
        return f'aot_truthy({e})'

    def materialize(self, v):
        """Puts the value in a temporary (so it is evaluated now, in order); returns (name, kind, owned)."""
        e, kind, owned = v
        t = self.new_tmp()
        if kind == 'bool':
            self.emit(f'bool {t} = {e};')
        else:
            self.emit(f'RValue {t} = {e};')
        return (t, kind, owned)

    def free(self, v):
        if v[2]:
            self.emit(f'RValue_free(&{v[0]});')

    # ---- compact forms for the common shapes (smaller code; same operations in the same order) ----
    def simple(self, n):
        """('V', c_ref) for a plain self/global variable, ('N', c_real) for a number literal; None otherwise."""
        if n[0] == 'num':
            return ('N', repr(float(n[1])))
        if n[0] == 'name':
            c = self.classify(n[1])
            if c[0] == 'self':
                return ('V', f'self, &V[{self.var_index(n[1], False)}]')
            if c[0] == 'gvar':
                return ('V', f'globalScope(), &V[{self.var_index(n[1], True)}]')
            if c[0] == 'const':
                return ('N', repr(float(c[1])))
        if n[0] == 'dot' and n[1] == ('name', 'global'):
            return ('V', f'globalScope(), &V[{self.var_index(n[2], True)}]')
        return None

    def pure(self, n):
        """A C RValue expression for an operand that reads nothing but a plain variable or a literal (no side effects,
        nothing to free), or None."""
        if n[0] == 'num':
            return f'RValue_makeInt32({n[1]})' if n[2] else f'RValue_makeReal({repr(float(n[1]))})'
        if n[0] == 'name':
            c = self.classify(n[1])
            if c[0] == 'self':
                return f'aot_self(self, &V[{self.var_index(n[1], False)}])'
            if c[0] == 'gvar':
                return f'aot_global(&V[{self.var_index(n[1], True)}])'
            if c[0] == 'local':
                return f'aot_weak(L_{n[1]})'
            if c[0] == 'const':
                return f'RValue_makeInt32({c[1]})'
            if c[0] == 'arg':
                return f'aot_arg(args, argCount, {c[1]})'
        if n[0] == 'dot' and n[1] == ('name', 'global'):
            return f'aot_global(&V[{self.var_index(n[2], True)}])'
        return None

    def expr(self, n):
        k = n[0]
        if k == 'num':
            if n[2]:
                return (f'RValue_makeInt32({n[1]})', 'int', False)
            return (f'RValue_makeReal({repr(float(n[1]))})', 'rv', False)
        if k == 'str':
            s = n[1].replace('\\', '\\\\').replace('"', '\\"')
            return (f'RValue_makeString("{s}")', 'rv', False)
        if k == 'name':
            return self.read_name(n[1])
        if k == 'dot' or k == 'index':
            return self.read_lvalue(n)
        if k == 'call':
            return self.call(n)
        if k == 'un':
            op = n[1]
            if op == '!':
                c = self.cond(n[2])
                return (f'(!{c})', 'bool', False)
            v = self.materialize(self.expr(n[2]))
            if op == '-':
                r = (f'aot_neg({v[0]})', 'rv', False)
            else:
                r = (f'aot_bitnot({v[0]})', 'rv', False)
            r = self.materialize(r)
            self.free(v)
            return r
        if k == 'bin':
            op = n[1]
            if op in ('&&', '||'):
                t = self.new_tmp()
                self.emit(f'bool {t} = {self.cond(n[2])};')
                self.emit(f'if ({"" if op == "&&" else "!"}{t}) {{')
                self.ind += 1
                self.emit(f'{t} = {self.cond(n[3])};')
                self.ind -= 1
                self.emit('}')
                # The bytecode leaves an int (Push.e) on the short-circuit side and a bool otherwise: only seen when the
                # result is stored, which this script set doesn't do.
                return (t, 'bool', False)
            if op == '^^':
                a = self.cond(n[2])
                ta = self.new_tmp()
                self.emit(f'bool {ta} = {a};')
                b = self.cond(n[3])
                return (f'({ta} != ({b}))', 'bool', False)
            kind = {'==': 'CMP_EQ', '!=': 'CMP_NEQ', '<': 'CMP_LT', '<=': 'CMP_LTE', '>': 'CMP_GT', '>=': 'CMP_GTE'}.get(op)
            sa, sb = self.simple(n[2]), self.simple(n[3])
            if kind and sa and sb and sa[0] == 'V':
                # variable vs variable / number: both are plain reads, so their order doesn't matter
                fn = 'aot_cmpVV' if sb[0] == 'V' else 'aot_cmpVN'
                return (f'{fn}({sa[1]}, {sb[1]}, {kind})', 'bool', False)
            pa, pb = self.pure(n[2]), self.pure(n[3])
            if pa is not None and pb is not None:
                # two plain reads / literals: evaluated in place, in order
                if kind:
                    return (f'aot_cmp({pa}, {pb}, {kind})', 'bool', False)
                fn = {'+': 'aot_add', '-': 'aot_sub', '*': 'aot_mul', '/': 'aot_div', '%': 'aot_mod', 'div': 'aot_idiv',
                      '&': 'aot_band', '|': 'aot_bor', '^': 'aot_bxor', '<<': 'aot_shl', '>>': 'aot_shr'}[op]
                t = self.new_tmp()
                self.emit(f'RValue {t} = {fn}(ctx, {pa}, {pb});')
                return (t, 'rv', True)
            a = self.materialize(self.expr(n[2]))
            b = self.materialize(self.expr(n[3]))
            ar = self.as_rv(a)[0]
            br = self.as_rv(b)[0]
            if op in ('==', '!=', '<', '<=', '>', '>=') and False:
                pass
            if op in ('==', '!=', '<', '<=', '>', '>='):
                kind = {'==': 'CMP_EQ', '!=': 'CMP_NEQ', '<': 'CMP_LT', '<=': 'CMP_LTE', '>': 'CMP_GT', '>=': 'CMP_GTE'}[op]
                t = self.new_tmp()
                self.emit(f'bool {t} = aot_cmp({ar}, {br}, {kind});')
                self.free(a)
                self.free(b)
                return (t, 'bool', False)
            fn = {'+': 'aot_add', '-': 'aot_sub', '*': 'aot_mul', '/': 'aot_div', '%': 'aot_mod', 'div': 'aot_idiv',
                  '&': 'aot_band', '|': 'aot_bor', '^': 'aot_bxor', '<<': 'aot_shl', '>>': 'aot_shr'}[op]
            t = self.new_tmp()
            self.emit(f'RValue {t} = {fn}(ctx, {ar}, {br});')
            self.free(a)
            self.free(b)
            return (t, 'rv', True)
        raise SyntaxError(f'expr {n!r}')

    def cond(self, n):
        """A C bool expression for an if/while condition (the bytecode's Conv.v.b + BF)."""
        sv = self.simple(n)
        if sv and sv[0] == 'V':
            return f'aot_truthyV({sv[1]})'
        v = self.expr(n)
        if v[1] == 'bool':
            return v[0]
        v = self.materialize(v)
        t = self.new_tmp()
        self.emit(f'bool {t} = aot_truthy({v[0]});')
        self.free(v)
        return t

    def read_name(self, name):
        c = self.classify(name)
        if c[0] == 'local':
            return (f'aot_weak(L_{name})', 'rv', False)
        if c[0] == 'self':
            return (f'aot_self(self, &V[{self.var_index(name, False)}])', 'rv', False)
        if c[0] == 'builtin':
            return (f'aot_bget(ctx, self, &B[{self.builtin_index(name)}], -1)', 'rv', True)
        if c[0] == 'const':
            return (f'RValue_makeInt32({c[1]})', 'int', False)
        if c[0] == 'rconst':
            return (f'RValue_makeReal({c[1]!r})', 'rv', False)
        if c[0] == 'arg':
            return (f'aot_arg(args, argCount, {c[1]})', 'rv', False)
        if c[0] == 'macro':
            return self.expr(Parser(tokenize(c[1])).expr())
        if c[0] == 'gvar':
            return (f'aot_global(&V[{self.var_index(name, True)}])', 'rv', False)
        if c[0] == 'argc':
            return ('RValue_makeReal((GMLReal) argCount)', 'rv', False)
        raise SyntaxError(name)

    # Target of a dot: ('self',) ('other',) ('global',) ('expr', c_rvalue_tmp)
    def target_of(self, base):
        if base[0] == 'name':
            nm = base[1]
            if nm == 'global':
                return ('global',)
            if nm == 'other':
                return ('target', 'RValue_makeInt32(-2)')
            if nm == 'self':
                return ('target', 'RValue_makeInt32(-1)')
        v = self.materialize(self.as_rv(self.expr(base)))
        return ('target', v[0], v)

    def index_value(self, idx):
        """The array index on the stack: i, or for [i, j] the compiler's i * 32000 + j."""
        if len(idx) == 1:
            return self.materialize(self.as_rv(self.expr(idx[0])))
        if len(idx) != 2:
            raise SyntaxError('3D array')
        i = self.materialize(self.as_rv(self.expr(idx[0])))
        t = self.new_tmp()
        self.emit(f'RValue {t} = aot_mul(ctx, {i[0]}, RValue_makeInt32(32000));')
        self.free(i)
        j = self.materialize(self.as_rv(self.expr(idx[1])))
        u = self.new_tmp()
        self.emit(f'RValue {u} = aot_add(ctx, {t}, {j[0]});')
        self.free(j)
        return (u, 'rv', False)

    def read_lvalue(self, n):
        if n[0] == 'index':
            base, idx = n[1], n[2]
            iv = self.index_value(idx)
            ix = f'aot_index({iv[0]})'
            r = self.read_scalar(base, ix)
            self.free(iv)
            return r
        return self.read_scalar(n, '-1')

    def read_scalar(self, n, ix):
        """n is a name or a dot; ix the array index C expression ('-1' for none)."""
        if n[0] == 'name':
            name = n[1]
            c = self.classify(name)
            if ix != '-1':
                if c[0] == 'self':
                    t = self.new_tmp()
                    self.emit(f'RValue {t} = aot_selfArr(self, &V[{self.var_index(name, False)}], {ix});')
                    return (t, 'rv', False)
                if c[0] == 'builtin':
                    t = self.new_tmp()
                    self.emit(f'RValue {t} = aot_bget(ctx, self, &B[{self.builtin_index(name)}], {ix});')
                    return (t, 'rv', True)
                if c[0] == 'local':
                    t = self.new_tmp()
                    self.emit(f'RValue {t} = GMLArray_getOnArrayRef(&L_{name}, {ix});')
                    return (t, 'rv', False)
                if c[0] == 'argarr':
                    t = self.new_tmp()
                    self.emit(f'RValue {t} = aot_arg(args, argCount, {ix});')
                    return (t, 'rv', False)
                raise SyntaxError(f'array read on {c}')
            v = self.read_name(name)
            if v[1] != 'int' and 'makeInt32' not in v[0]:
                v = self.materialize(v)
            return v
        if n[0] == 'dot':
            tgt = self.target_of(n[1])
            name = n[2]
            if tgt[0] == 'global':
                t = self.new_tmp()
                vi = self.var_index(name, True)
                if ix == '-1':
                    self.emit(f'RValue {t} = aot_global(&V[{vi}]);')
                else:
                    self.emit(f'RValue {t} = aot_globalArr(&V[{vi}], {ix});')
                return (t, 'rv', False)
            kinds = self.dw.var_kinds(name)
            t = self.new_tmp()
            if 'builtin' in kinds:
                self.emit(f'RValue {t} = aot_tbget(ctx, {tgt[1]}, &B[{self.builtin_index(name)}], {ix});')
            elif 'self' in kinds:
                self.emit(f'RValue {t} = aot_tget(ctx, {tgt[1]}, &V[{self.var_index(name, False)}], {ix});')
            else:
                raise SyntaxError(f'{self.script}: unknown member {name}')
            if len(tgt) > 2:
                self.free(tgt[2])
            return (t, 'rv', True)
        raise SyntaxError(f'read {n!r}')

    def call(self, n):
        name, args = n[1], n[2]
        # Arguments are evaluated right to left (the bytecode pushes the last one first).
        vals = [None] * len(args)
        # A plain read / literal can be read when the array is built, if every argument evaluated after it (the
        # lower indices: they go right to left) is one too.
        for i in range(len(args) - 1, -1, -1):
            p = self.pure(args[i])
            if p is not None and not all(self.pure(x) is not None for x in args[:i]):
                p = None
            vals[i] = (p, 'rv', False) if p is not None else self.materialize(self.as_rv(self.expr(args[i])))
        t = self.new_tmp()
        if args:
            arr = self.new_tmp()
            self.emit(f'RValue {arr}[{len(args)}] = {{ {", ".join(v[0] for v in vals)} }};')
            for v in vals:
                if not v[2]:
                    pass
            self.emit(f'RValue {t} = aot_call(ctx, F[{self.func_index(name)}], {arr}, {len(args)});')
            # aot_call frees owned arguments as the VM does (weak views are left alone).
        else:
            self.emit(f'RValue {t} = aot_call(ctx, F[{self.func_index(name)}], NULL, 0);')
        return (t, 'rv', True)

    # ---- assignments ----
    def store_value(self, v, op_assign):
        """The value as the Pop leaves it: an int literal assigned with '=' is stored as a real (Pop.v.i)."""
        e, kind, owned = v
        if kind == 'int' and op_assign:
            return (f'aot_intToReal({e})', 'rv', owned)
        return self.as_rv(v)

    def assign(self, target, op, rhs):
        st = self.simple(target) if target[0] in ('name', 'dot') else None
        if st and st[0] == 'V':
            sr = self.simple(rhs)
            if op == '=' and sr:
                if sr[0] == 'N':
                    self.emit(f'aot_setVN({st[1]}, {sr[1]});')  # Pop.v.i stores a number as a real
                else:
                    self.emit(f'aot_setVV({st[1]}, {sr[1]});')
                return
            if op in '+-*' and sr and sr[0] == 'N' and rhs[0] == 'num':
                opc = {'+': 'OP_ADD', '-': 'OP_SUB', '*': 'OP_MUL'}[op]
                if rhs[2]:
                    self.emit(f'aot_opVI(ctx, {st[1]}, {opc}, {int(rhs[1])});')
                else:
                    self.emit(f'aot_opVR(ctx, {st[1]}, {opc}, {sr[1]});')
                return
        if op != '=':
            # a op= b: read a, evaluate b, combine, store
            cur = self.materialize(self.read_lvalue(target) if target[0] in ('dot', 'index') else self.read_scalar(target, '-1'))
            b = self.materialize(self.as_rv(self.expr(rhs)))
            fn = {'+': 'aot_add', '-': 'aot_sub', '*': 'aot_mul', '/': 'aot_div', '|': 'aot_bor', '&': 'aot_band',
                  '^': 'aot_bxor'}[op]
            t = self.new_tmp()
            self.emit(f'RValue {t} = {fn}(ctx, {self.as_rv(cur)[0]}, {b[0]});')
            self.free(cur)
            self.free(b)
            val = (t, 'rv', True)
        else:
            val = self.materialize(self.store_value(self.expr(rhs), True))
        self.store(target, val)
        self.free(val)

    def store(self, target, val):
        v = val[0]
        if target[0] == 'name':
            name = target[1]
            c = self.classify(name)
            if c[0] == 'local':
                self.emit(f'aot_setLocal(&L_{name}, {v});')
            elif c[0] == 'self':
                self.emit(f'aot_setSelf(self, &V[{self.var_index(name, False)}], {v});')
            elif c[0] == 'builtin':
                self.emit(f'aot_bset(ctx, self, &B[{self.builtin_index(name)}], -1, {v});')
            elif c[0] == 'gvar':
                self.emit(f'aot_setGlobal(&V[{self.var_index(name, True)}], {v});')
            else:
                raise SyntaxError(f'store to {c}')
            return
        if target[0] == 'index':
            base, idx = target[1], target[2]
            iv = self.index_value(idx)
            ix = f'aot_index({iv[0]})'
            if base[0] == 'name':
                c = self.classify(base[1])
                if c[0] == 'builtin':
                    self.emit(f'aot_bset(ctx, self, &B[{self.builtin_index(base[1])}], {ix}, {v});')
                elif c[0] == 'self':
                    self.emit(f'aot_setSelfArr(ctx, self, &V[{self.var_index(base[1], False)}], {ix}, {v});')
                else:
                    raise SyntaxError(f'array store to {c}')
            elif base[0] == 'dot':
                self.store_dot(base, ix, v)
            else:
                raise SyntaxError('array store')
            self.free(iv)
            return
        if target[0] == 'dot':
            self.store_dot(target, '-1', v)
            return
        raise SyntaxError(f'store {target!r}')

    def store_dot(self, target, ix, v):
        tgt = self.target_of(target[1])
        name = target[2]
        if tgt[0] == 'global':
            vi = self.var_index(name, True)
            if ix == '-1':
                self.emit(f'aot_setGlobal(&V[{vi}], {v});')
            else:
                self.emit(f'aot_setGlobalArr(ctx, &V[{vi}], {ix}, {v});')
            return
        kinds = self.dw.var_kinds(name)
        if 'builtin' in kinds:
            self.emit(f'aot_tbset(ctx, {tgt[1]}, &B[{self.builtin_index(name)}], {ix}, {v});')
        elif 'self' in kinds:
            self.emit(f'aot_tset(ctx, {tgt[1]}, &V[{self.var_index(name, False)}], {ix}, {v});')
        else:
            raise SyntaxError(f'{self.script}: unknown member {name}')
        if len(tgt) > 2:
            self.free(tgt[2])

    # ---- statements ----
    def block(self, stmts):
        for s in stmts:
            self.stmt(s)

    def scoped(self, s):
        self.emit('{')
        self.ind += 1
        self.stmt(s)
        self.ind -= 1
        self.emit('}')

    def stmt(self, s):
        k = s[0]
        if k == 'block':
            self.emit('{')
            self.ind += 1
            self.block(s[1])
            self.ind -= 1
            self.emit('}')
        elif k == 'var':
            for name, init in s[1]:
                if init is not None:
                    self.emit('{')
                    self.ind += 1
                    self.assign(('name', name), '=', init)
                    self.ind -= 1
                    self.emit('}')
        elif k == 'assign':
            self.emit('{')
            self.ind += 1
            self.assign(s[1], s[2], s[3])
            self.ind -= 1
            self.emit('}')
        elif k == 'expr':
            self.emit('{')
            self.ind += 1
            v = self.expr(s[1])
            self.free(v)
            self.ind -= 1
            self.emit('}')
        elif k == 'if':
            self.emit('{')
            self.ind += 1
            c = self.cond(s[1])
            self.emit(f'if ({c}) {{')
            self.ind += 1
            self.stmt(s[2])
            self.ind -= 1
            if s[3] is not None:
                self.emit('} else {')
                self.ind += 1
                self.stmt(s[3])
                self.ind -= 1
            self.emit('}')
            self.ind -= 1
            self.emit('}')
        elif k == 'while':
            self.emit('for (;;) {')
            self.ind += 1
            c = self.cond(s[1])
            self.emit(f'if (!{c}) break;')
            self.loop_stack.append('loop')
            self.stmt(s[2])
            self.loop_stack.pop()
            self.ind -= 1
            self.emit('}')
        elif k == 'do':
            self.emit('for (;;) {')
            self.ind += 1
            self.loop_stack.append('loop')
            self.stmt(s[1])
            self.loop_stack.pop()
            c = self.cond(s[2])
            self.emit(f'if ({c}) break;')
            self.ind -= 1
            self.emit('}')
        elif k == 'for':
            if s[1] is not None:
                self.stmt(s[1])
            self.emit('for (;;) {')
            self.ind += 1
            if s[2] is not None:
                c = self.cond(s[2])
                self.emit(f'if (!{c}) break;')
            self.loop_stack.append('loop')
            cont = self.new_label()
            self.loop_stack[-1] = ('for', cont)
            self.stmt(s[4])
            self.loop_stack.pop()
            self.emit(f'{cont}:;')
            if s[3] is not None:
                self.stmt(s[3])
            self.ind -= 1
            self.emit('}')
        elif k == 'repeat':
            self.emit('{')
            self.ind += 1
            v = self.materialize(self.as_rv(self.expr(s[1])))
            n = self.new_tmp()
            self.emit(f'int32_t {n} = RValue_toInt32({v[0]});')
            self.free(v)
            self.emit(f'for (; {n} > 0; {n}--) {{')
            self.ind += 1
            self.loop_stack.append('loop')
            self.stmt(s[2])
            self.loop_stack.pop()
            self.ind -= 1
            self.emit('}')
            self.ind -= 1
            self.emit('}')
        elif k == 'with':
            self.emit('{')
            self.ind += 1
            v = self.materialize(self.as_rv(self.expr(s[1])))
            self.emit(f'bool run = VM_aotWithBegin(ctx, RValue_toInt32({v[0]}));')
            self.free(v)
            self.emit('Instance* outer = self;')
            self.emit('if (!run) VM_aotWithNext(ctx); // the skipped block still runs its PopEnv')
            self.emit('while (run) {')
            self.ind += 1
            self.emit('self = (Instance*) ctx->currentInstance;')
            self.loop_stack.append('with')
            self.with_depth += 1
            self.stmt(s[2])
            self.with_depth -= 1
            self.loop_stack.pop()
            self.emit('run = VM_aotWithNext(ctx);')
            self.ind -= 1
            self.emit('}')
            self.emit('self = outer;')
            self.ind -= 1
            self.emit('}')
        elif k == 'switch':
            end = self.new_label()
            self.emit('{')
            self.ind += 1
            v = self.materialize(self.as_rv(self.expr(s[1])))
            labels = [self.new_label() for _ in s[2]]
            default = None
            for (ce, _), lab in zip(s[2], labels):
                if ce is None:
                    default = lab
                    continue
                cv = self.materialize(self.as_rv(self.expr(ce)))
                self.emit(f'if (aot_cmp({v[0]}, {cv[0]}, CMP_EQ)) goto {lab};')
                self.free(cv)
            self.emit(f'goto {default or end};')
            self.loop_stack.append(('switch', end))
            for (ce, body), lab in zip(s[2], labels):
                self.emit(f'{lab}:;')
                self.block(body)
            self.loop_stack.pop()
            self.emit(f'{end}:;')
            self.free(v)
            self.ind -= 1
            self.emit('}')
        elif k == 'break':
            top = self.loop_stack[-1] if self.loop_stack else None
            if isinstance(top, tuple) and top[0] == 'switch':
                self.emit(f'goto {top[1]};')
            elif top == 'with':
                self.emit('VM_aotWithBreak(ctx);')
                self.emit('self = outer;')
                self.emit('run = false;')
                self.emit('break;')
            elif top is None:
                raise SyntaxError('break outside loop')
            else:
                self.emit('break;')
        elif k == 'continue':
            top = self.loop_stack[-1]
            if isinstance(top, tuple) and top[0] == 'for':
                self.emit(f'goto {top[1]};')
            elif top == 'loop':
                self.emit('continue;')
            else:
                raise SyntaxError('continue in with/switch')
        elif k == 'exit' or k == 'return':
            if k == 'return' and s[1] is not None:
                self.emit('{')
                self.ind += 1
                v = self.materialize(self.as_rv(self.expr(s[1])))
                self.emit(f'RValue_free(&ret); ret = RValue_makeIndependent({v[0]});')
                self.free(v)
                self.ind -= 1
                self.emit('}')
            else:
                self.emit('RValue_free(&ret); ret = RValue_makeReal(0.0);')
            for _ in range(self.with_depth):
                self.emit('VM_aotWithBreak(ctx);')
            self.emit('goto done;')
        else:
            raise SyntaxError(f'stmt {s!r}')


def collect_locals(ast, out):
    if isinstance(ast, tuple):
        if ast and ast[0] == 'var':
            for name, _ in ast[1]:
                out.append(name)
        for x in ast:
            collect_locals(x, out)
    elif isinstance(ast, list):
        for x in ast:
            collect_locals(x, out)


def translate(dw, src, script, c_name):
    ast = Parser(tokenize(src)).program()
    locs = []
    collect_locals(ast, locs)
    g = Gen(dw, script, c_name, locs)
    g.block(ast[1])
    body = g.lines
    out = []
    out.append(f'// ===[ {script} ]===')
    nvars = max(1, len(g.vars))
    out.append(f'static Var {c_name}_V[{nvars}] = {{')
    for (name, is_global), i in sorted(g.vars.items(), key=lambda kv: kv[1]):
        out.append(f'    {{ "{name}", -1, {"true" if is_global else "false"} }},')
    if not g.vars:
        out.append('    { NULL, -1, false },')
    out.append('};')
    nb = max(1, len(g.builtins))
    out.append(f'static AotBuiltin {c_name}_B[{nb}] = {{')
    for name, i in sorted(g.builtins.items(), key=lambda kv: kv[1]):
        out.append(f'    {{ "{name}", -1 }},')
    if not g.builtins:
        out.append('    { NULL, -1 },')
    out.append('};')
    nf = max(1, len(g.funcs))
    out.append(f'static const char* {c_name}_Fnames[{nf}] = {{')
    for name, i in sorted(g.funcs.items(), key=lambda kv: kv[1]):
        out.append(f'    "{name}",')
    if not g.funcs:
        out.append('    NULL,')
    out.append('};')
    out.append(f'static int32_t {c_name}_F[{nf}];')
    out.append('')
    out.append(f'static RValue {c_name}(VMContext* ctx, MAYBE_UNUSED RValue* args, MAYBE_UNUSED int32_t argCount) {{')
    out.append('    Instance* self = (Instance*) ctx->currentInstance;')
    out.append(f'    Var* V = {c_name}_V;')
    out.append(f'    AotBuiltin* B = {c_name}_B;')
    out.append(f'    int32_t* F = {c_name}_F;')
    out.append('    (void) V; (void) B; (void) F;')
    out.append('    RValue ret = RValue_makeReal(0.0); // a script that ends without return gives 0')
    for name in locs:
        out.append(f'    RValue L_{name} = RValue_makeUndefined();')
    out.extend(body)
    out.append('    goto done;')
    out.append('done:')
    for name in locs:
        out.append(f'    RValue_free(&L_{name});')
    out.append('    return ret;')
    out.append('}')
    out.append('')
    return '\n'.join(out), len(g.vars), len(g.builtins), len(g.funcs)


EVENT_NAMES = {0: 'Create', 1: 'Destroy', 2: 'Alarm', 3: 'Step', 5: 'Keyboard', 6: 'Mouse', 7: 'Other', 8: 'Draw',
               9: 'KeyPress', 10: 'KeyRelease'}


def object_event_source(objects_dir, obj, etype, enumb):
    import xml.etree.ElementTree as ET
    root = ET.parse(os.path.join(objects_dir, obj + '.object.gmx')).getroot()
    for ev in root.iter('event'):
        if ev.get('eventtype') == str(etype) and ev.get('enumb') == str(enumb):
            actions = list(ev.iter('action'))
            if len(actions) != 1 or actions[0].findtext('id') != '603':
                raise SyntaxError(f'{obj} event {etype}/{enumb}: not a single code action')
            for a in actions[0].iter('string'):
                return a.text or ''
    raise SyntaxError(f'{obj}: no event {etype}/{enumb}')


def main():
    dw = DataWin(sys.argv[1])
    gml_root = sys.argv[2]  # the project folder (scripts/, objects/)
    import glob
    for proj in glob.glob(os.path.join(gml_root, '*.project.gmx')):
        for m in re.finditer(r'<constant name="([^"]+)">([^<]*)</constant>', open(proj, encoding='utf-8').read()):
            MACROS[m.group(1)] = m.group(2).replace('&lt;', '<').replace('&gt;', '>').replace('&amp;', '&')
    out_path = sys.argv[3]
    entries = []
    chunks = []
    failed = []
    for spec in sys.argv[4:]:
        try:
            if ':' in spec:
                obj, etype, enumb = spec.split(':')
                etype, enumb = int(etype), int(enumb)
                name = f'gml_Object_{obj}_{EVENT_NAMES[etype]}_{enumb}'
                c_name = f'aot_{obj}_{EVENT_NAMES[etype]}_{enumb}'
                src = object_event_source(os.path.join(gml_root, 'objects'), obj, etype, enumb)
                is_event = True
            else:
                name = spec
                c_name = 'aot_' + spec
                src = open(os.path.join(gml_root, 'scripts', spec + '.gml'), encoding='utf-8', errors='replace').read()
                is_event = False
            code, nv, nb, nf = translate(dw, src, name, c_name)
        except (SyntaxError, KeyError) as e:
            failed.append(f'{spec}: {e}')
            continue
        if is_event:
            code += (f'static bool {c_name}_event(MAYBE_UNUSED Runner* runner, MAYBE_UNUSED Instance* instance) {{\n'
                     f'    AotEventMark mark = aot_eventEnter("{name}");\n'
                     f'    RValue r = {c_name}(N.vm, NULL, 0);\n    RValue_free(&r);\n'
                     f'    aot_eventLeave(mark);\n    return true;\n}}\n')
        chunks.append(code)
        entries.append((name, c_name, nv, nb, nf, is_event))
    with open(out_path, 'w') as f:
        f.write('// Generated by tools/n3ds/aot/gml2c.py: do not edit.\n\n')
        f.write('\n'.join(chunks))
        f.write('\nstatic AotScript gAotScripts[] = {\n')
        for name, c_name, nv, nb, nf, is_event in entries:
            ev = f'{c_name}_event' if is_event else 'NULL'
            f.write(f'    {{ "{name}", {c_name}, {ev}, {c_name}_V, {nv}, {c_name}_B, {nb}, {c_name}_Fnames, {c_name}_F, {nf} }},\n')
        f.write('};\n')
    print(f'{len(entries)} translated -> {out_path}')
    for x in failed:
        print('  skipped', x)


if __name__ == '__main__':
    main()
