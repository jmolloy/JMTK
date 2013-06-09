# Creates the 'coderef' directive, which is used to put bits of code alongside

from sphinx.util.compat import Directive
from docutils.parsers.rst import directives
import docutils.nodes
from graph import Graph

graph = None

class tocgraph(docutils.nodes.General, docutils.nodes.Element):
    pass

class TocGraphDirective(Directive):
    has_content = True
    required_arguments = 1
    

    def run(self):

        return [tocgraph('')]

def visit_tocgraph_node(self, node):
    pass

def depart_tocgraph_node(self, node):
    pass

def steal_doctree(self, doctree, docname):
    return
    import pdb
    pdb.set_trace()
    class Visitor:
        def __init__(self):
            self.document = doctree.document
        def dispatch_visit(self, node):
            print node.__class__.__name__
            if node.__class__.__name__ == 'toctree':
                pdb.set_trace()

    doctree.walk(Visitor())

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

from sphinx.jinja2glue import BuiltinTemplateLoader
class MyTemplateBridge(BuiltinTemplateLoader):
    def render(self, templatename, ctx):
        global graph

        nodes = {k.replace(' ', '-').lower(): v for k,v in graph.nodes.items()}

        import pdb
        pdb.set_trace()
        if ctx['current_page_name'] in nodes:
            node = nodes[ctx['current_page_name']]

            nexts = []
            for node in graph.next_nodes(node):
                # FIXME: look up the title in the toc tree!
                nexts.append({'title': node.value,
                              'link': node.value.replace(' ', '-').lower() + '.html'})

            prevs = []
            for node in graph.prev_nodes(node):
                # FIXME: look up the title in the toc tree!
                prevs.append({'title': node.value,
                              'link': node.value.replace(' ', '-').lower() + '.html'})

            ctx['nexts'] = nexts
            ctx['prevs'] = prevs

        return BuiltinTemplateLoader.render(self, templatename, ctx)
        

def setup(app):
    app.add_node(tocgraph,
                 html=(visit_tocgraph_node, depart_tocgraph_node),
                 latex=(visit_tocgraph_node, depart_tocgraph_node),
                 text=(visit_tocgraph_node, depart_tocgraph_node))

    app.add_directive('tocgraph', TocGraphDirective)

#    app.config.template_bridge = 'tocgraph.MyTemplateBridge'

    # import pdb
    # pdb.set_trace()
#    assert app.env is not None
    
#    app.connect('doctree-read', steal_doctree)
    # app.connect('env-purge-doc', purge_todos)
    app.connect('html-page-context', html_page_context)

    global graph
    graph = Graph(fromfile='/home/james/Code/new-tutorials/layout.graph')
