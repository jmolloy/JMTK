# Creates the 'coderef' directive, which is used to put bits of code alongside

from sphinx.util.compat import Directive
from docutils.parsers.rst import directives
import docutils.nodes
from graph import Graph
from sphinx.ext.graphviz import render_dot_html

graph = None

class tocgraph(docutils.nodes.General, docutils.nodes.Element):
    pass

class TocGraphDirective(Directive):
    has_content = True
    required_arguments = 1
    
    def run(self):
        env  = self.state.document.settings.env

        return [tocgraph('')]

def visit_tocgraph_node(self, node):
    global graph
    options = ['-Gbgcolor=#eeffcc',
               '-Ecolor=#133f52',
               '-Earrowhead=open']

    render_dot_html(self, node, graph.get_as_dot(), options,
                    'tocgraph', 'tocgraph', alt='TOC graphs')
    raise nodes.SkipNode

def depart_tocgraph_node(self, node):
    pass

def _mangle_name(n):
    return n.replace(' ', '-').lower()

def _get_link_object_for(docname, app):
    # title rendered as HTML
    title = app.env.longtitles.get(docname)
    title = title and app.builder.render_partial(title)['title'] or ''
    
     # Devolve to builder to get the URL.
    link = app.builder.get_target_uri(docname)
    
    return {'title': title, 'link': link}

def html_page_context(app, pagename, templatename, context, doctree):
    global graph

    nodes = {_mangle_name(k): v for k,v in graph.nodes.items()}

    if pagename in nodes:
        nexts = [_get_link_object_for(_mangle_name(node.value), app)
                 for node in graph.next_nodes(nodes[pagename])]
        prevs = [_get_link_object_for(_mangle_name(node.value), app)
                 for node in graph.prev_nodes(nodes[pagename])]

        context['nexts'] = nexts
        context['prevs'] = prevs

def setup(app):
    app.setup_extension('sphinx.ext.graphviz')
    app.add_node(tocgraph,
                 html=(visit_tocgraph_node, depart_tocgraph_node),
                 latex=(visit_tocgraph_node, depart_tocgraph_node),
                 text=(visit_tocgraph_node, depart_tocgraph_node))

    app.add_directive('tocgraph', TocGraphDirective)

    app.connect('html-page-context', html_page_context)

    global graph, svg_name
    graph = Graph(fromfile='/home/james/Code/new-tutorials/layout.graph')

    
