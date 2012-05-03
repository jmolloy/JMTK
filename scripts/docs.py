#!/usr/bin/env python

import sys,os,re
from docutils.core import publish_parts
from pygments import highlight
from pygments.lexers import get_lexer_for_filename
from pygments.formatters import HtmlFormatter
from string import Template

class SourceFile:
    def __init__(self, name):
        self.name = name
        self.lines = open(self.name).readlines()

    def _is_asm_type(self, line):
        if re.match(r'\s*;;;',line):
            return True
        return False

    def _doc_fragment(self):
        docstr = []

        while self.lines:
            line = self.lines.pop(0)
            is_asm = self._is_asm_type(line)

            if is_asm and not re.match(r'\s*;;;', line):
                self.lines.insert(0, line)
                break
            elif not is_asm and re.search(r'(^|[^*])\*?\*/', line):
                docstr.append(
                    line[:line.find('*/')] )
                break
            docstr.append(line)

        src = ''
        if docstr[-1].find('{') != -1:
            src = self._src()
            docstr[-1] = re.sub(r'{', '', docstr[-1])
        
        return SourceFragment(self, ''.join(docstr), src)

    def _src(self):
        src = []
        while self.lines:
            line = self.lines.pop(0)
            
            if self._is_docstring(line):
                self.lines.insert(0, line)
                break
            src.append(line)

        return ''.join(src)

    def _is_docstring(self, line):
        if re.match(r'\s*/\*\*($|[^*])', line) or \
                re.match(r'\s*;;;',line):
            return True
        return False

    def fragments(self):
        frags = []

        while self.lines:
            line = self.lines[0]

            if self._is_docstring(line) and line.find('#cut') != -1:
                break

            if self._is_docstring(line):
                frag = self._doc_fragment()
            else:
                frag = SourceFragment(self, None, self._src())

            frags.append(frag)
        
        return frags

class SourceFragment:
    def __init__(self, file, docstring, src):
        self.file = file
        self.docstring = docstring
        self.src = src
        self.ord = None
        
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
        return publish_parts(ds, writer_name='html')['body']

    def html_src(self):
        if not self.src:
            return ''

        try:
            return highlight(self.src,
                             get_lexer_for_filename(self.file.name),
                             HtmlFormatter())
        except:
            return '<pre>'+self.src+'</pre>'

class DocumentChapter:

    def __init__(self, template, files):
        source_files = [SourceFile(f) for f in files]
        source_fragments = []
        for sf in source_files:
            source_fragments.extend(sf.fragments())

        source_fragments = self._reorder_fragments(source_fragments)

        t = Template(open(template).read())
        self.html = t.safe_substitute(body = self._make_table(source_fragments),
                                      highlight_style = HtmlFormatter().get_style_defs('.highlight'))

    def __str__(self):
        return self.html

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
        return out

    def _make_table(self, fragments):
        out = '<table>\n'

        for frag in fragments:
            ds = frag.html_docstring()
            src = frag.html_src()

            if not ds:
                continue

            if src:
                out += '''
        <tr>
          <td valign="top" class="doc-with-src">%s</td>
          <td class="src">%s</td>
        </tr>
        ''' % (ds, src)
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
        chapter = DocumentChapter(options.template, args)
        print str(chapter)
        sys.exit(0)

    os.makedirs(options.out_dir)

    g = Graph(options.graph)

    for node in g.nodes.values():
        print "Generating documentation for '%s' from %s..." % (node.value, node.files)

        chapter = DocumentChapter(options.template, node.files)

        outfilename = "%s/%s.html" % (options.out_dir,
                                      node.value.lower().replace(' ', '-'))
        open(outfilename, 'w').write(str(chapter))
