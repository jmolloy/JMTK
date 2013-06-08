# Creates the 'coderef' directive, which is used to put bits of code alongside

from sphinx.util.compat import Directive
from docutils.parsers.rst import directives
import docutils.nodes

class tocgraph(docutils.nodes.General, docutils.nodes.Element):
    pass

class TocGraphDirective(Directive):
    def run(self):
        return [tocgraph('')]

def visit_tocgraph_node(self, node):
    pass

def depart_tocgraph_node(self, node):
    pass

def setup(app):
    app.add_node(tocgraph,
                 html=(visit_tocgraph_node, depart_tocgraph_node),
                 latex=(visit_tocgraph_node, depart_tocgraph_node),
                 text=(visit_tocgraph_node, depart_tocgraph_node))

    app.add_directive('tocgraph', TocGraphDirective)
#    assert app.env is not None
    
    # app.connect('doctree-resolved', process_todo_nodes)
    # app.connect('env-purge-doc', purge_todos)
