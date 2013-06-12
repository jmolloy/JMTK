# Creates the 'coderef' directive, which is used to put bits of code alongside


from sphinx.directives.code import CodeBlock
from docutils.parsers.rst import directives
import docutils.nodes

class CodeRefDirective(CodeBlock):
    required_arguments = 1

    option_spec = CodeBlock.option_spec
    option_spec['first_of_file'] = directives.flag
    # This directive serves no real purpose. It is put on all coderef instances in order
    # for the RST parser to detect how much indentation the producer is applying to that
    # block, which means that subindented code regions will show up correctly. For example,
    # if this coderef is inside a function (so needs to be indented by 2 spaces):
    #
    # .. coderef::
    #     :anchor:
    #
    #       x = 1;
    #
    # Without the :anchor:, the RST parser would assume 6 spaces is the indent for that block.
    # But actually it is 4 spaces, plus 2 spaces that should be output literally.
    option_spec['anchor'] = directives.flag

    def raw_html(self, html):
        return docutils.nodes.raw('', html, format='html')

    def run(self):
        classes = 'filename'
        if 'first_of_file' in self.options:
            classes = 'filename first_of_file'
        filename = [self.raw_html('<div class="%s">%s</div>' % (classes, self.arguments[0]))]

        # Set self.arguments[0] to be the highlight language for CodeBlock.
        if self.arguments[0].endswith('.c') or self.arguments[0].endswith('.h'):
            self.arguments[0] = 'c'
        else:
            self.arguments[0] = 'python'
            
        [literal] = CodeBlock.run(self)

        return [self.raw_html('<div class="source">')] + filename + [literal] + \
            [self.raw_html('</div>')]

def setup(app):
    app.add_directive('coderef', CodeRefDirective)
