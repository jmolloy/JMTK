import graph, os, sys

class Chapter:
    def __init__(self, name, graph, root):
        self.paths = graph.find_paths(root, name)
        
        self.paths = set([frozenset(p) for p in self.paths])

        if name == "skeleton":
            print self.paths

        self.name = name
        self.safe_name = self.name.replace(' ', '-')
        self.nodes = graph.nodes
    def make(self, d):
        d = os.path.join(d, self.safe_name)
        os.makedirs(d)

        l = list(self.paths)
        for i in range(len(l)):
            self._make_path(i, l[i], d)

    def _make_path(self, num, path, d):
        d = os.path.join(d, "variant-%s" % num)
        os.makedirs(d)

        contents = []

        for n in path:
            contents.append('%s: %s' % (n.value, n.files))

            for f in n.files:
                target_dir = os.path.join(d, os.path.dirname(f))
                target_file = os.path.join(d, f)
                try:
                    os.makedirs(target_dir)
                except:
                    pass
                #print "symlink(%s, %s)" % (os.path.relpath(os.path.abspath(f), target_dir), target_file )
                os.symlink( os.path.relpath(os.path.abspath(f), target_dir), target_file )

        open(os.path.join(d, "contents.txt"), "w").write('\n'.join(contents))

if __name__ == '__main__':
    graph_fn, directory, root = sys.argv[1:4]

    os.mkdir(directory)

    g = graph.Graph(fromfile=graph_fn)
    for n in g.nodes:
        c = Chapter(n, g, root)
        c.make(directory)
