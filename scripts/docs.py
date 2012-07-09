#!/usr/bin/env python

import sys,os,re
from docutils.core import publish_parts
from docutils.utils import SystemMessage
from pygments import highlight
from pygments.lexers import get_lexer_for_filename
from pygments.formatters import HtmlFormatter
from pygments.style import Style
from string import Template
from pygments.token import Keyword, Name, Comment, String, Error, \
     Number, Operator, Generic

class DocStyle(Style):
    default_style = ""
    styles = {
        Comment:                'italic #0b61a4',
        Comment.Preproc:        'noitalic #FFAD40',
        Keyword:                'bold #007241',
        Name.Builtin:           '#61d7a4',
        String:                 '#36d792'
        }

class SourceFile:
    def __init__(self, name):
        self.name = name
        self.lines = open(self.name).readlines()
        self.linenum = 1

    def _is_asm_type(self, line):
        if re.match(r'\s*;;;',line):
            return True
        return False

    def _doc_fragment(self):
        docstr = []
        is_asm = self._is_asm_type(self.lines[0])

        while self.lines:
            line = self.lines.pop(0)

            if is_asm and not re.match(r'\s*;;;', line):
                self.lines.insert(0, line)
                break
            elif not is_asm and re.search(r'(^|[^*])\*?\*/', line):
                self.linenum += 1
                docstr.append(
                    line[:line.find('*/')] )
                break
            docstr.append(line)

        src = ''
        if docstr[-1].find('}') != -1 and docstr[-1].find('@}') == -1:
            src = self._embed_src(docstr[-1])
            docstr[-1] = re.sub(r'{.*}', '' ,docstr[-1])
        elif docstr[-1].find('{') != -1:
            src = self._src()
            docstr[-1] = re.sub(r'{', '', docstr[-1])
        
        return SourceFragment(self, ''.join(docstr), src, self.linenum)

    def _src(self):
        src = []
        while self.lines:
            line = self.lines.pop(0)
            
            if self._is_docstring(line):
                self.lines.insert(0, line)
                break
            self.linenum += 1
            src.append(line)

        return ''.join(src)

    def _embed_src(self, line):
        m = re.search(r'{(.*),"(.*)","(.*)"}', line)
        assert m
        
        file = m.group(1)
        search_from = m.group(2)
        search_to = m.group(3)

        if not search_to:
            search_to = 'I AM A STRING THAT WILL NOT BE FOUND ANYWHERE'

        ls = open(file, 'r').readlines()

        srcls = []
        state = 'skipping'
        for l in ls:
            if state == 'skipping' and l.find(search_from) != -1:
                state = 'adding'
            if l.find(search_to) != -1:
                state = 'done'

            if state != 'skipping':
                srcls.append(l)

            if state == 'done':
                break

        return ''.join(srcls)


    def _is_docstring(self, line):
        if re.match(r'\s*/\*\*($|[^*])', line) or \
                re.match(r'\s*;;;',line):
            return True
        return False

    def fragments(self):
        return self._fragments()

    def has_documentation(self):
        for frag in self.fragments():
            if frag.docstring:
                return True
        return False

    def _fragments(self):
        if hasattr(self, 'frags'):
            return self.frags
        frags = []

        while self.lines:
            line = self.lines[0]

            if self._is_docstring(line) and line.find('#cut') != -1:
                break

            if self._is_docstring(line):
                frag = self._doc_fragment()
            else:
                frag = SourceFragment(self, None, self._src(), self.linenum)

            frags.append(frag)
        
        self.frags = frags
        return frags

class SourceFragment:
    def __init__(self, file, docstring, src, linenum):
        self.file = file
        self.docstring = docstring
        self.src = src
        self.ord = None
        self.linenum = linenum
        
        if self.docstring:
            firstline = self.docstring.split('\n')[0]
            match = re.search(r'#(\d+)', firstline)
            if match:
                self.ord = int(match.group(1))
                self.docstring = re.sub(r'#\d+', '', self.docstring, 1)

        if self.docstring and re.search(r'\*\s*$', self.docstring):
            self.docstring = re.sub(r'\*\s*$', '', self.docstring)

    def __str__(self):
        ds = self.docstring if self.docstring else ''
        src = self.src if self.src else ''
        return "%s... (%s...)" % (ds[:16], src[:16])

    def _strip_prefix(self, ds):
        lines = ds.split('\n')
        l = len(re.match(r'[;/*\s]*', lines[0]).group(0))

        lines = [line[l:] for line in lines]
        return '\n'.join(lines)

    def html_docstring(self):
        if not self.docstring:
            return ''

        ds = self._strip_prefix(self.docstring)
        try:
            x = publish_parts(ds, writer_name='html', source_path=self.file.name)
            return x['body']
        except Exception as e:
            pass

    def html_src(self):
        if not self.src:
            return ''

        try:
            return highlight(self.src,
                             get_lexer_for_filename(self.file.name.replace('.s','.asm')),
                             HtmlFormatter())
        except:
            return '<pre>'+self.src+'</pre>'

class DocumentChapter:

    def __init__(self, template, files, node):
        source_files = [SourceFile(f) for f in files]
        source_fragments = []
        for sf in source_files:
            if sf.has_documentation():
                source_fragments.extend(sf.fragments())

        source_fragments = self._reorder_fragments(source_fragments)

        self.node = node

        preds = list(self._get_preds())
        succs = list(self._get_succs())

        nav = []
        for p in preds:
            s = '<span class="prev %s">%s<span class="line"></span></span>\n' % (
                'prev2' if p != preds[-1] else '', p.value)
            nav.append(s)

        nav.append('<span class="this">%s</span>\n' % node.value)

        for p in succs:
            s = '<span class="next %s">%s<span class="line"></span></span>\n' % (
                'next2' if p != succs[0] else '', p.value)
            nav.append(s)
            
        t = Template(open(template).read())
        self.html = t.safe_substitute(body = self._make_table(source_fragments),
                                      nav = ''.join(nav),
                                      highlight_style = HtmlFormatter(style=DocStyle).get_style_defs('.highlight'))

    def __str__(self):
        return self.html

    def _get_preds(self):
        if not self.node:
            return set()

        preds = set()
        for e in self.node.graph.edges:
            if e[1] == self.node:
                preds.add(e[0])
        if self.node in preds:
            preds.remove(self.node)
        return preds

    def _get_succs(self):
        if not self.node:
            return set()

        succs = set()
        for e in self.node.graph.edges:
            if e[0] == self.node:
                succs.add(e[1])
        if self.node in succs:
            succs.remove(self.node)
        return succs

    def _reorder_fragments(self, frags):
        ords = {}
        this_ord = 0
        max_ord = 0
        for frag in frags:
            if frag.ord:
                this_ord = frag.ord
                max_ord = max(max_ord, this_ord)
            if this_ord not in ords:
                ords[this_ord] = []
            ords[this_ord].append(frag)

        out = []
        for i in range(0, max_ord+1):
            if i in ords:
                out.extend(ords[i])

        # Now run another pass to set the "first"/"last" attributes on
        # source fragments.
        for i in range(0, len(out)):
            out[i].src_first = False
            out[i].src_last = False

            if not out[i].src:
                continue
            if i == 0 or not out[i-1].src:
                out[i].src_first = True
            if (i == len(out)-1) or not out[i+1].src:
                out[i].src_last = True

        return out

    def _make_table(self, fragments):
        out = '<table class="body-table">\n'

        for frag in fragments:
            ds = frag.html_docstring()
            src = frag.html_src()

            src_first = "src-first" if frag.src_first else ""
            src_last  = "src-last"  if frag.src_last else ""
            if src:
                out += '''
        <tr>
          <td valign="top" class="doc-with-src">%s</td>
          <td><div class="src %s %s">%s</div></td>
        </tr>
        ''' % (ds, src_first, src_last, src)
            else:
                out += '''
        <tr>
          <td valign="top" colspan="2" class="doc-without-src">%s</td>
        </tr>
        ''' % ds


        out += '</table>\n'
        return out;

if __name__ == '__main__':
    from optparse import OptionParser
    from graph import Graph

    parser = OptionParser()
    parser.add_option("--template", dest="template")
    parser.add_option("--graph", dest="graph")
    parser.add_option("--output-dir", dest="out_dir")

    options, args = parser.parse_args()

    if not options.graph:
        chapter = DocumentChapter(options.template, args, None)
        print str(chapter)
        sys.exit(0)

    try:
        os.makedirs(options.out_dir)
    except:
        pass

    g = Graph(options.graph)

    for node in g.nodes.values():
        print "Generating documentation for '%s' from %s..." % (node.value, node.files)

        chapter = DocumentChapter(options.template, node.files, node)

        outfilename = "%s/%s.html" % (options.out_dir,
                                      node.value.lower().replace(' ', '-'))
        open(outfilename, 'w').write(str(chapter))
