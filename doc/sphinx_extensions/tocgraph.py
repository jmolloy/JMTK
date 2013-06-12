

from sphinx.util.compat import Directive
from docutils.parsers.rst import directives
import docutils.nodes
from docutils import nodes
from graph import Graph
from sphinx.ext.graphviz import render_dot, GraphvizError

from sphinx.directives.other import TocTree

graph = None
my_app = None

def _mangle_name(n):
    return n.replace(' ', '-').lower()

class tocgraph(docutils.nodes.General, docutils.nodes.Element):
    pass

class TocGraphDirective(TocTree):
    has_content = True
    required_arguments = 1
    
    def run(self):
        env  = self.state.document.settings.env

        graph = Graph(fromfile=self.arguments[0])
    
        toctree_content = [_mangle_name(n)
                           for n in graph.nodes]
        self.content = toctree_content
        self.options['hidden'] = True

        parent_run = TocTree.run(self)

        g = tocgraph('')
        env.graph = graph

        return parent_run + [g]

def visit_tocgraph_node(self, node):
    options = ['-Gbgcolor=transparent',
               '-Earrowhead=open']

    urls = {}

    for n in my_app.env.graph.nodes:
        title = my_app.env.titles.get(_mangle_name(n)).astext()
        if title != '<no title>':
            urls[n] = '../' + my_app.builder.get_target_uri(_mangle_name(n))

    css = '../_static/default_svg.css'

    render_dot_html(self, node, get_as_dot(my_app.env.graph, urls, css), options,
                    'tocgraph', 'tocgraph', alt='TOC graphs')
    raise nodes.SkipNode

def depart_tocgraph_node(self, node):
    pass

def get_as_dot(graph, urls, css):
    dot = ['digraph G {',
           'node [shape=box fontsize=16 fontname="sans-serif" peripheries=0]',
           'graph [size="6.25,30.0" stylesheet="%s"]' % css,
           'edge [weight="1"]']
    for _from, to in graph.edges:
        dot.append('"%s" -> "%s"' % (_from.value.title(), to.value.title()))

    for k,v in urls.items():
        dot.append('"%s" [URL="%s" target="_top"]' % (k.title() ,v))
    dot.append('}')

    return '\n'.join(dot)

def render_dot_html(self, node, code, options, prefix='graphviz',
                    imgcls=None, alt=None):
    try:
        fname, outfn = render_dot(self, code, options, 'svg', prefix)
    except GraphvizError, exc:
        self.builder.warn('dot code %r: ' % code + str(exc))
        raise nodes.SkipNode

    inline = node.get('inline', False)
    if inline:
        wrapper = 'span'
    else:
        wrapper = 'p'

    self.body.append(self.starttag(node, wrapper, CLASS='graphviz'))
    imgcss = imgcls and 'class="%s"' % imgcls or ''
    svgtag = '<object data="%s" alt="%s" %s type="image/svg+xml"></object>\n' % (fname, alt, imgcss)
    self.body.append(svgtag)

    self.body.append('</%s>\n' % wrapper)
    raise nodes.SkipNode

def _get_link_object_for(docname, app):
    # title rendered as HTML
    title = app.env.longtitles.get(docname)
    title = title and app.builder.render_partial(title)['title'] or ''
    
     # Devolve to builder to get the URL.
    link = app.builder.get_target_uri(docname)
    
    return {'title': title, 'link': link}

def html_page_context(app, pagename, templatename, context, doctree):
    nodes = {_mangle_name(k): v for k,v in app.env.graph.nodes.items()}

    if pagename in nodes:
        nexts = [_get_link_object_for(_mangle_name(node.value), app)
                 for node in app.env.graph.next_nodes(nodes[pagename])]
        prevs = [_get_link_object_for(_mangle_name(node.value), app)
                 for node in app.env.graph.prev_nodes(nodes[pagename])]

        context['nexts'] = nexts
        context['prevs'] = prevs

def setup(app):
    global my_app
    my_app = app

    app.setup_extension('sphinx.ext.graphviz')
    app.add_node(tocgraph,
                 html=(visit_tocgraph_node, depart_tocgraph_node),
                 latex=(visit_tocgraph_node, depart_tocgraph_node),
                 text=(visit_tocgraph_node, depart_tocgraph_node))

    app.add_directive('tocgraph', TocGraphDirective)

    app.connect('html-page-context', html_page_context)

