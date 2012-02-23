import re, sys, unittest, os, tempfile

class ScanTable:
    """
    Given a scantable file, this class will parse and create C code
    for looking up strings from scantables.

    FIXME: Document this!
    """
    class CyclicIncludeError(Exception):
        def __init__(self, fname):
            Exception.__init__(self, "Cyclic include for file '%s'" % fname)

    class ParseError(Exception):
        def __init__(self, tokens, msg):
            line = 0
            col = 0
            for tok in tokens:
                if tok[0] == 'NEWLINE':
                    col = 0
                    line += 1
                else:
                    col += len(tok[1])
            Exception.__init__(self, "%d:%d: %s" % (line+1,col+1,msg))

    class SemaError(Exception):
        def __init__(self, scancode, escape, msg):
            s = "scancode '0x%x': %s" % (scancode, msg)
            if escape:
                s = "scancode 'ESC(0x%x)': %s" % (scancode, msg)
            Exception.__init__(self, s)

    def __init__(self, fname):
        self.fname = fname
        self.scanned_files = set()
        self.scanner = re.Scanner([
                (r'#.*\n', lambda _,t: ('NEWLINE', t)),
                (r"'[^']*'", lambda _,t: ('STRING', t)),
                (r'"[^"]*"', lambda _,t: ('STRING', t)),
                (r'[A-Z][A-Z0-9]*', lambda _,t: ('FLAG', t)),
                (r'shift', lambda _,t: ('SHIFT', t)),
                (r'caps', lambda _,t: ('CAPS', t)),
                (r'ctrl', lambda _,t: ('CTRL', t)),
                (r'alt', lambda _,t: ('ALT', t)),
                (r'esc', lambda _,t: ('ESC', t)),
                (r'numlock', lambda _,t: ('NUMLOCK', t)),
                (r'include', lambda _,t: ('INCLUDE', t)),
                (r'[0-9a-fA-F][0-9a-fA-F]', lambda _,t: ('SCANCODE', t)),
                (r'\(', lambda _,t: ('LPAREN', t)),
                (r'\)', lambda _,t: ('RPAREN', t)),
                ('\n', lambda _,t: ('NEWLINE', t)),
                (r'\s+', lambda _,t: ('WHITESPACE', t)),
                ])

    def lex(self):
        def _lex(fname):
            tokens, remainder = self._scan(fname)
            if remainder:
                raise ScanTable.ParseError(tokens, "unknown token")
            # Strip the whitespace out - noone cares about it.
            tokens = [t for t in tokens if t[0] != 'WHITESPACE']

            # Does the file include any others?
            final_tokens = []
            tok_queue = list(tokens)
            while tok_queue:
                tok = tok_queue.pop(0)
                if tok[0] != 'INCLUDE':
                    final_tokens.append(tok)
                    continue

                tok = tok_queue.pop(0)
                if tok[0] != 'STRING':
                    raise ScanTable.ParseError(tokens[:len(tokens)-len(tok_queue)],
                                               "expected filename")
                final_tokens.extend(_lex(tok[1][1:-1]))


            return final_tokens

        tokens = _lex(self.fname)
        return tokens

    def parse(self):
        self.tables = {None: {}, 'SHIFT': {}, 'CAPS': {}, 'NUMLOCK': {}, 'CTRL': {}}
        self.flags = {}
        self.known_flags = set()

        class State:
            def __init__(self, table):
                self.scancode = None
                self.entries = {}
                self.flags = set()
                self.cur_modifier = None
                self.table = table
                self.escape = False

            def enter(self):
                if self.scancode is None:
                    return

                if self.entries and self.flags:
                    raise ScanTable.SemaError(self.scancode, self.escape, "cannot both define table entries and set flags!")

                if self.escape:
                    self.scancode |= 0x80

                for k,v in self.entries.items():
                    self.table.tables[k][self.scancode] = v
                if self.flags:
                    self.table.flags[self.scancode] = self.flags
                    self.table.known_flags = self.table.known_flags.union(self.flags)


        state = State(self)
        for tok in self.lex():
            if tok[0] == 'WHITESPACE':
                continue
            if tok[0] == 'NEWLINE':
                state.enter()
                state = State(self)
                continue
            if tok[0] == 'SCANCODE':
                state.scancode = int(tok[1], 16)
                continue
            if tok[0] == 'STRING':
                s = tok[1][1:-1]
                if not s:
                    raise ScanTable.SemaError(state.scancode, state.escape,
                               "scantable entries cannot be zero-lengthed! (modifier %s)" % state.cur_modifier)
                state.entries[state.cur_modifier] = s.decode('string_escape')
                state.cur_modifier = None
                continue
            if tok[0] in ('SHIFT', 'CAPS', 'NUMLOCK', 'CTRL'):
                state.cur_modifier = tok[0]
                continue
            if tok[0] == 'FLAG':
                state.flags.add(tok[1])
                continue
            if tok[0] == 'ESC':
                state.escape = True
                continue
        state.enter()

        return self.tables, self.flags, self.known_flags

    def lookup_table(self, table, n):
        def _c_string_at(table, idx):
            arr = []
            while table[idx] != 0:
                arr.append(chr(table[idx]))
                idx += 1
            return ''.join(arr)

        i = 0
        idx = 0
        while idx < len(table):
            if table[idx] == 0:
                i += table[idx+1]
                idx += 2

                if n < i:
                    return None
            else:
                s = _c_string_at(table, idx)
                if i == n:
                    return s
                idx += len(s)+1
                i += 1
        

    def generate_table(self, table):
        """
        Generate a table designed to be read by an automaton, with the
        algorithm in lookup_table().
        """
        def _how_many_zeroes(table, i):
            n = 0
            while i not in table and i < 256:
                i += 1
                n += 1
            return n
        t = []

        i = 0
        while i < 256:
            if i not in table:
                n = _how_many_zeroes(table, i)
                if n == 0:
                    break
                t.extend([0, n])
                i += n
                continue

            t.extend([ord(c) for c in table[i]])
            t.append(0)
            i += 1

        for num in t:
            assert num < 256

        return t

    def generate_flags_map(self, flags):
        assert len(flags) < 32
        m = {}
        for i,v in enumerate(sorted(flags)):
            m[v] = i
        return m

    def generate_flags_table(self, table, flag_map):
        t = [0] * 256
        for k,fs in table.items():
            v = 0
            for f in fs:
                v |= 1 << flag_map[f]
            t[k] = v
        return t

    def generate_c(self):
        out = []

        tk = [x.lower() if x else 'default' for x in self.tables.keys()]
        out.append("""/* Autogenerated by scantable.py, do not edit!

Exported scantables: %s */
""" % ', '.join(tk))

        # Firstly calculate the flag map and output it.
        fm = self.generate_flags_map(self.known_flags)
        for f,v in fm.items():
            out.append('#define SCAN_%s \t(1UL<<%s)' % (f,v))
        out.append('')

        # Then output the flag table as an array of chars.
        ft = self.generate_flags_table(self.flags, fm)
        out.append('static unsigned char scan_flags[256] = {%s};' %
                   ','.join([str(x) for x in ft]))
        out.append('')

        # Now output every other table.
        for name,table in self.tables.items():
            gen = self.generate_table(table)

            if name is None:
                name = 'default'

            l = len(gen)
            out.append('static const unsigned scan_%s_len = %s;' %
                       (name.lower(), l))
            out.append('static unsigned char scan_%s[%s] = {%s};' %
                       (name.lower(), l, ','.join([str(x) for x in gen])))
            out.append('')

        # Finish off by outputting the lookup algorithm.
        out.append("""static const char *lookup_scantable(unsigned char *t, int t_len, unsigned char n, int escaped) {
  int i = 0, idx = 0;
  const char *s;

  if (escaped) n |= 0x80;

  while (idx < t_len) {
    if (t[idx] == 0) {
      i += t[idx+1];
      idx += 2;
      
      if (n < i) return 0;
    } else {
      s = (const char *)&t[idx];
      if (i == n) return s;
      while (t[idx])
        ++idx;
      ++idx;
      ++i;
    }
  }
  return 0;
}
""")

        return '\n'.join(out)

    def _scan(self, fname):
        if fname in self.scanned_files:
            raise ScanTable.CyclicIncludeError(fname)

        self.scanned_files.add(fname)
        with open(fname) as fd:
            s = fd.read()
        tokens, remainder = self.scanner.scan(s)
        return tokens, remainder

class TestLexer(unittest.TestCase):
    def test_bad_include(self):
        tmp0fd, tmp0 = tempfile.mkstemp()
        
        try:
            os.write(tmp0fd, "include '/badfile/doesnt/exist'\n")
            os.close(tmp0fd)

            s = ScanTable(tmp0)

            self.assertRaises(IOError, s.lex)
        finally:
            os.remove(tmp0)

    def test_cyclic_include(self):
        tmp0fd, tmp0 = tempfile.mkstemp()
        
        try:
            os.write(tmp0fd, "include '%s'\n" % tmp0)
            os.close(tmp0fd)

            s = ScanTable(tmp0)

            self.assertRaises(ScanTable.CyclicIncludeError, s.lex)
        finally:
            os.remove(tmp0)

    def test_bad_token(self):
        tmp0fd, tmp0 = tempfile.mkstemp()
        
        try:
            os.write(tmp0fd, "0g")
            os.close(tmp0fd)

            s = ScanTable(tmp0)

            self.assertRaises(ScanTable.ParseError, s.lex)
        finally:
            os.remove(tmp0)

    def test_lex(self):

        tmp0fd, tmp0 = tempfile.mkstemp()
        tmp1fd, tmp1 = tempfile.mkstemp()

        try:
            st0 = """
        # Comment
        include '%s'

        01   'x' shift('y') caps('z')
        """ % tmp1
            st1 = """
        02   'h' ctrl('z') MOREFLAG
        1b   MYFLAG AFLAG # Comment
        """

            os.write(tmp0fd, st0)
            os.write(tmp1fd, st1)
            os.close(tmp0fd)
            os.close(tmp1fd)

            s = ScanTable(tmp0)
            toks = s.lex()

            self.assertEqual(toks,
                             [('NEWLINE', '\n'), ('NEWLINE', '# Comment\n'), ('NEWLINE', '\n'), ('SCANCODE', '02'), ('STRING', "'h'"), ('CTRL', 'ctrl'), ('LPAREN', '('), ('STRING', "'z'"), ('RPAREN', ')'), ('FLAG', 'MOREFLAG'), ('NEWLINE', '\n'), ('SCANCODE', '1b'), ('FLAG', 'MYFLAG'), ('FLAG', 'AFLAG'), ('NEWLINE', '# Comment\n'), ('NEWLINE', '\n'), ('NEWLINE', '\n'), ('SCANCODE', '01'), ('STRING', "'x'"), ('SHIFT', 'shift'), ('LPAREN', '('), ('STRING', "'y'"), ('RPAREN', ')'), ('CAPS', 'caps'), ('LPAREN', '('), ('STRING', "'z'"), ('RPAREN', ')'), ('NEWLINE', '\n')])
        finally:
            os.remove(tmp0)
            os.remove(tmp1)            

class TestParser(unittest.TestCase):
    def test_null_entry(self):
        tmp0fd, tmp0 = tempfile.mkstemp()
        
        try:
            os.write(tmp0fd, "00 ''\n")
            os.close(tmp0fd)

            s = ScanTable(tmp0)

            self.assertRaises(ScanTable.SemaError, s.parse)
        finally:
            os.remove(tmp0)

    def test_flags_and_entry(self):
        tmp0fd, tmp0 = tempfile.mkstemp()
        
        try:
            os.write(tmp0fd, "00 'x' F\n")
            os.close(tmp0fd)

            s = ScanTable(tmp0)

            self.assertRaises(ScanTable.SemaError, s.parse)
        finally:
            os.remove(tmp0)

    def test_parse(self):
        tmp0fd, tmp0 = tempfile.mkstemp()
        
        try:
            os.write(tmp0fd, "01 'x' shift('y') caps('z')\n")
            os.write(tmp0fd, "1b 'a' shift('b') numlock('c')\n")
            os.write(tmp0fd, "1c FLAG1 FLAG2\n")
            os.write(tmp0fd, "1d FLAG1 FLAG3\n")
            os.close(tmp0fd)

            s = ScanTable(tmp0)

            self.assertEqual(s.parse(),
                             ({'SHIFT': {1: 'y', 27: 'b'}, 'NUMLOCK': {27: 'c'}, None: {1: 'x', 27: 'a'}, 'CAPS': {1: 'z'}}, {28: set(['FLAG2', 'FLAG1']), 29: set(['FLAG3', 'FLAG1'])}, set(['FLAG3', 'FLAG2', 'FLAG1'])))

        finally:
            os.remove(tmp0)

class TestTables(unittest.TestCase):
    def test_tables(self):
        tmp0fd, tmp0 = tempfile.mkstemp()
        
        try:
            os.write(tmp0fd, """
01      '\x1b'          # Escape
02      '1' shift('!')
03      '2' shift('@')
0a      '9' shift('(')
0b      '0' shift(')')
0c      '-' shift('_')
0d      '=' shift('+')
0e      '\x08'          # Backspace
ff      'cornercase'
""")
            os.close(tmp0fd)

            s = ScanTable(tmp0)
            tables,_,_ = s.parse()

            table = tables[None]
            gen = s.generate_table(table)
            for i in range(0,256):
                self.assertEqual(s.lookup_table(gen, i), table.get(i,None))

        finally:
            os.remove(tmp0)

    def test_escape(self):
        tmp0fd, tmp0 = tempfile.mkstemp()
        
        try:
            os.write(tmp0fd, """
esc(01)      '2'
""")
            os.close(tmp0fd)

            s = ScanTable(tmp0)
            tables,_,_ = s.parse()

            table = tables[None]
            gen = s.generate_table(table)
            self.assertEqual(s.lookup_table(gen, 0x81), '2')

        finally:
            os.remove(tmp0)

    def test_flag_tables(self):
        tmp0fd, tmp0 = tempfile.mkstemp()
        
        try:
            os.write(tmp0fd, """
01      '\x1b'          # Escape
02      F1
03      F2
0a      F2 F3
0b      F2 F3 F1
0c      '-'
""")
            os.close(tmp0fd)

            s = ScanTable(tmp0)
            _,table,flags = s.parse()
            self.assertEqual(flags, set(['F1','F2','F3']))

            fm = s.generate_flags_map(flags)
            self.assertEqual(fm, {'F1': 0, 'F2': 1, 'F3': 2})

            gen = s.generate_flags_table(table, fm)
            gen_gold = [0]*256
            gen_gold[2] = 1
            gen_gold[3] = 2
            gen_gold[10] = 6
            gen_gold[11] = 7
            self.assertEqual(gen, gen_gold)
            
        finally:
            os.remove(tmp0)


if __name__ == '__main__':
    if sys.argv[1] == 'test':
        sys.argv.pop(0)
        unittest.main()
    else:
        st = ScanTable(sys.argv[1])
        try:
            st.parse()
        
            s = st.generate_c()
        except Exception as e:
            print '%s:error:%s' % (os.path.basename(sys.argv[0]), str(e))
            sys.exit(1)
            raise

        with open(sys.argv[2], 'w') as fd:
            fd.write(s)
