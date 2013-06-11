#!/usr/bin/env python

# This script takes a "layout.graph" file and produces ReST documents for all
# chapters found within, along with an index.rst.

import sys,os,re,tempfile,subprocess

# A Source File is parsed into a sequence of fragments (of class SourceFragment).
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

# A source fragment is a chunk of (optional) documentation and (optional) code.
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
        def _is_whitespace(l):
            return re.match(r'\s*$', l)

        if ds.startswith('/**'):
            ds = '   ' + ds[3:]
        lines = ds.split('\n')

        ls = [len(re.match(r'[;/*\s]*', l).group(0))
              for l in lines
              if not _is_whitespace(l)]
        l = min(ls)

        lines = [line[l:] for line in lines]
        return '\n'.join(lines)

    def raw_docstring(self):
        if not self.docstring:
            return ''
        return self._strip_prefix(self.docstring)

# A chapter comes from one or more source files, and will organise the fragments
# from each into a sensible order.
class DocumentChapter:

    def __init__(self, files, node, bdir):
        source_files = [SourceFile(f) for f in files]
        source_fragments = []
        for sf in source_files:
            if sf.has_documentation():
                source_fragments.extend(sf.fragments())

        source_fragments = self._reorder_fragments(source_fragments)

        self.node = node

        preds = list(self._get_preds())
        succs = list(self._get_succs())
 
        self.rest = self._make_rest(source_fragments)

        graph_name = str(node).replace(' ', '-') + '.svg'
        self._render_pred_succ_graph(preds, succs, os.path.join(bdir, graph_name))

    def _make_rest(self, fragments):
        def indentlines(ls, n):
            return [' '*n + l for l in ls]
        def html(x):
            return ['', '.. raw:: html', ''] + indentlines(x.splitlines(), 4) + ['    ']

        out = []
        lastfile = ''
        for frag in fragments:
            if frag.src:
                attrs = []
                if lastfile != frag.file.name:
                    attrs = [":first_of_file:"]
                    lastfile = frag.file.name

                out += ['.. coderef:: %s' % frag.file.name] + indentlines(attrs, 4) + [''] + \
                    indentlines(frag.src.splitlines(), 4) + ['']

            if frag.docstring:
                out += frag.raw_docstring().splitlines()

            out += html('<div class="anchor"></div>')

        return '\n'.join(out)

    def __str__(self):
        return self.rest

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

        return out

    def _render_pred_succ_graph(self, preds, succs, outf):
        def url(node):
            return "./" + node.value.replace(' ', '-').lower() + '.html'

        dot = ['digraph G {', 'node [shape=box fontsize=12 fontname="Droid Sans" peripheries=0 ]', 'rankdir=LR; nodesep=0.0; pad="0,0";']
        dot += ['"%s" -> "%s"' % (p, self.node) for p in preds]
        dot += ['"%s" -> "%s"' % (self.node, s) for s in succs]

        dot += ['"%s" [href="%s" target="_parent"]' % (p, url(p)) for p in preds]
        dot += ['"%s" [href="%s" target="_parent"]' % (s, url(s)) for s in succs]

        dot += ['}', '']

        dot = '\n'.join(dot)

        tf = tempfile.NamedTemporaryFile(delete=False)
        tf.write(dot)
        tf.close()

        subprocess.check_call(['dot', '-s34', '-Tsvg', '-o', outf, tf.name])

        os.unlink(tf.name)

def _make_index_rst(g):
    vs = []
    for node in g.nodes.values():
        vs.append('   ' + node.value.lower().replace(' ', '-'))
    return """
JMTK docs
=========

Contents:

.. tocgraph::
   x


.. toctree::
   :maxdepth: 2
   
%s

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
""" % ('\n'.join(vs),)

if __name__ == '__main__':
    from optparse import OptionParser
    from graph import Graph

    parser = OptionParser()
    parser.add_option("--template", dest="template")
    parser.add_option("--graph", dest="graph")
    parser.add_option("--output-dir", dest="out_dir")

    options, args = parser.parse_args()

    if not options.out_dir:
        options.out_dir = '.'

    if not options.graph:
        chapter = DocumentChapter(options.template, args, None, options.out_dir)
        print str(chapter)
        sys.exit(0)

    try:
        os.makedirs(options.out_dir)
    except:
        pass

    g = Graph(options.graph)

    for node in g.nodes.values():
        print "DOC %s (from %s)" % (node.value, ', '.join(node.files))

        chapter = DocumentChapter(node.files, node, options.out_dir)

        outfilename = "%s/%s.rst" % (options.out_dir,
                                     node.value.lower().replace(' ', '-'))
        open(outfilename, 'w').write(str(chapter))

#    open("%s/index.rst" % options.out_dir, "w").write(_make_index_rst(g))
