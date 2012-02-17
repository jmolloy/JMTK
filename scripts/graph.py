import re

class Graph:
    class GraphSyntaxError(Exception):
        def __init__(self, msg):
            Exception.__init__(self, msg)

    class Node:
        def __init__(self, value):
            self.value = value
            
        def __str__(self):
            return str(self.value)
        def __repr__(self):
            return str(self.value)

    def __init__(self, fromfile=None):
        self.edges = set()
        self.nodes = dict()

        if fromfile:
            self.deserialize(fromfile)

    def deserialize(self, fn):
        with open(fn) as fd:
            for l in fd.readlines():
                l = l.strip()
                if not l:
                    continue
                m = re.match(r'"(.*)"\s*-\>\s*"(.*)"', l)
                if not m:
                    raise Graph.GraphSyntaxError(l)
                lhs = m.group(1)
                rhs = m.group(2)

                self.add_edge(self.add_node(lhs), self.add_node(rhs))

    def serialize(self):
        out = []
        for e in self.edges:
            out.append('"%s" -> "%s"' % (str(e[0]), str(e[1])))
        return '\n'.join(out)

    def add_node(self, node):
        if node not in self.nodes:
            self.nodes[node] = Graph.Node(node)
        return self.nodes[node]
        
    def add_edge(self, node1, node2):
        self.edges.add( (node1, node2) )

    def find_paths(self, root, goal):
        """Return a list of paths from root to goal."""

        if not isinstance(root, Graph.Node):
            root = self.nodes[root]
        if not isinstance(goal, Graph.Node):
            goal = self.nodes[goal]

        def _cycles_on(path, n):
            l = []
            accum = []
            for seg in path:
                if seg == n:
                    l.append(tuple(accum))
                    accum = []
                    continue
                accum.append(seg)
            l.append(tuple(accum))
            return filter(lambda x: x, l)

        def _has_double_cycle(path):
            ns = set(path)
            for n in ns:
                ps = _cycles_on(path, n)
                print ps
                if len(set(ps)) < len(ps):
                    return True
            return False

        def _recurse(sp, path):
            # Find all edges from fromnode.
            nodes = [e[1] for e in self.edges if e[0] == path[-1]]
            paths = []
            for node in nodes:
                p = tuple( list(path) + [node] )
                if p in sp or _has_double_cycle(path):
                    continue
                sp.add(p)
                if node == goal:
                    paths.append(p)
                else:
                    paths += _recurse(sp, p)
            return paths

        return _recurse(set(), [root])
