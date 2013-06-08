
import docutils.parsers
import docutils.statemachine
from docutils.parsers.rst import states

class MyDocutilsParser(docutils.parser.Parser):
    def __init__(self, rfc2822=None, inliner=None):
        self.statemachine = states.RSTStateMachine(
            state_classes=self.state_classes,
            initial_state="Body",
            debug=document.reporter.debug_flag)
    def parse(self, inputstring, document):
        self.setup_parse(inputstring, document)
        inputlines = docutils.statemachine.string2lines(
            inputstring, tab_width=document.settings.tab_width,
            convert_whitespace=1)
        self.statemachine.run(inputlines, document, inliner=self.inliner)
        self.finish_parse()
