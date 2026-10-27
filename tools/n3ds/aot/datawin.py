"""Minimal data.win (GameMaker: Studio 1.4, bytecode 15) reader: asset names, variables, functions, code."""
import struct


class DataWin:
    def __init__(self, path):
        self.b = open(path, 'rb').read()
        b = self.b
        assert b[0:4] == b'FORM'
        self.chunks = {}
        p = 8
        while p < len(b):
            name = b[p:p + 4].decode()
            ln = struct.unpack_from('<I', b, p + 4)[0]
            self.chunks[name] = (p + 8, ln)
            p += 8 + ln
        self.assets = {}
        for chunk, kind in (('SPRT', 'sprite'), ('SOND', 'sound'), ('BGND', 'background'), ('PATH', 'path'),
                            ('SCPT', 'script'), ('FONT', 'font'), ('TMLN', 'timeline'), ('OBJT', 'object'),
                            ('ROOM', 'room')):
            for i, n in enumerate(self.names(chunk)):
                if n is not None and n not in self.assets:
                    self.assets[n] = (kind, i)
        self.vari = self.read_vari()
        self.funcs = self.read_func()

    def u32(self, o):
        return struct.unpack_from('<I', self.b, o)[0]

    def i32(self, o):
        return struct.unpack_from('<i', self.b, o)[0]

    def string(self, ptr):
        if ptr == 0:
            return None
        ln = self.u32(ptr - 4)
        return self.b[ptr:ptr + ln].decode('utf-8', 'replace')

    def names(self, chunk):
        if chunk not in self.chunks:
            return []
        o, _ = self.chunks[chunk]
        n = self.u32(o)
        out = []
        for i in range(n):
            ptr = self.u32(o + 4 + 4 * i)
            out.append(self.string(self.u32(ptr)) if ptr else None)
        return out

    def read_vari(self):
        o, ln = self.chunks['VARI']
        count = (ln - 12) // 20
        out = []
        for i in range(count):
            e = o + 12 + 20 * i
            out.append(dict(name=self.string(self.u32(e)), instanceType=self.i32(e + 4), varID=self.i32(e + 8)))
        return out

    def read_func(self):
        o, ln = self.chunks['FUNC']
        n = self.u32(o)
        out = []
        for i in range(n):
            e = o + 4 + 12 * i
            out.append(self.string(self.u32(e)))
        return out

    # Variable classification: 'builtin' (varID -6), 'self', 'global'
    def var_kinds(self, name):
        kinds = set()
        for v in self.vari:
            if v['name'] != name:
                continue
            if v['varID'] == -6:
                kinds.add('builtin')
            elif v['instanceType'] == -5:
                kinds.add('global')
            elif v['instanceType'] == -7:
                kinds.add('local')
            else:
                kinds.add('self')
        return kinds
