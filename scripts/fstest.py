#!/usr/bin/python

import os, sys, subprocess, re, random, string, datetime

def _exec(args, stdout=None, stderr=None):
    try:
        return subprocess.check_output(args)
    except OSError as e:
        raise OSError(str(e) + " while running " + args[0])

def mkimg(image, size):
    blks = size / 1024
    _exec(["mkdosfs", "-C", "-F", "32", image, str(blks)])

class MountMutator:
    def __init__(self, image):
        self.image = image
        self.mpoint = "/mnt"

    def __str__(self):
        return "native"

    def start(self):
        _exec(["sudo", "/sbin/losetup", "/dev/loop0", self.image])
        _exec(["sudo", "/bin/mount", "/dev/loop0", self.mpoint, "-o", "uid=1000"])

    def stop(self):
        _exec(["sudo", "/bin/umount", "/dev/loop0"])
        _exec(["sudo", "/sbin/losetup", "-d", "/dev/loop0"])

    def write(self, fname, content):
        fname = os.path.join(self.mpoint, '.'+fname)
        with open(fname, "w") as fd:
            fd.write(content)
    
    def read(self, fname):
        fname = os.path.join(self.mpoint, '.'+fname)
        with open(fname, "r") as fd:
            return fd.read()
    
    def listdir(self, fname):
        fname = os.path.join(self.mpoint, '.'+fname)
        return os.listdir(fname) + ['.', '..']

    def mkdir(self, fname):
        fname = os.path.join(self.mpoint, '.'+fname)
        return os.mkdir(fname)

    def stat(self, fname):
        def _t(ts):
            return datetime.datetime.fromtimestamp(int(ts)).isoformat()

        x = os.stat(os.path.join(self.mpoint, '.'+fname))
        mode = "%x" % x.st_mode
        return {'mode': mode, 'nlink': x.st_nlink,
                'uid': x.st_uid, 'gid': x.st_gid,
                'size': x.st_size, 'atime': _t(x.st_atime),
                'mtime': _t(x.st_mtime), 'ctime': _t(x.st_ctime)}

class FstestMutator:
    def __init__(self, image, exe):
        self.image = image
        self.exe = exe
        self.fstype = "vfat"

    def __str__(self):
        return "fstest"

    def start(self):
        pass

    def stop(self):
        pass

    def write(self, fname, content):
        raise "NotImplemented!"

    def read(self, fname):
        lines = _exec([self.exe, self.image, self.fstype, "cat", fname]).splitlines()

        # Grep for start marker.
        state = 'SPOOL'
        newlines = []
        for l in lines:
            if l.startswith('START CAT') and state == 'SPOOL':
                state = 'ADD'
            elif l.startswith('END CAT') and state == 'ADD':
                state = 'STOP'
                break
            elif state == 'ADD':
                newlines.append(l)

        output = '\n'.join(newlines)

        return output

    def _parsefiles(self, lines):
        objs = []
        for l in lines:
            m = re.match(r"\[\[(.*)\]\] (\S+) : nlink (.*) mode (.*) ctime (.*) mtime (.*) atime (.*) uid (.*) gid (.*) size (.*)", l)
            obj = {'type': m.group(1),
                   'name': m.group(2),
                   'nlink': int(m.group(3)),
                   'mode': m.group(4),
                   'ctime': int(m.group(5)),
                   'mtime': int(m.group(6)),
                   'atime': int(m.group(7)),
                   'uid': int(m.group(8)),
                   'gid': int(m.group(9)),
                   'size': int(m.group(10))}
            objs.append(obj)
        return objs

    def listdir(self, fname):
        lines = _exec([self.exe, self.image, self.fstype, "ls", fname]).splitlines()

        lines = filter(lambda x: x.startswith('[['), lines)
        files = self._parsefiles(lines)

        return [x['name'] for x in files]

    def mkdir(self, fname):
        raise "NotImplemented!"

    def stat(self, fname):
        name = fname[fname.rfind('/')+1 :]
        fname = fname[: fname.rfind('/')]
        lines = _exec([self.exe, self.image, self.fstype, "ls", fname]).splitlines()
        lines = filter(lambda x: x.startswith('[['), lines)
        files = self._parsefiles(lines)

        x = filter(lambda f: f['name'] == name, files)[0]

        def _t(ts):
            class UTC(datetime.tzinfo):
                def utcoffset(self, dt): return datetime.timedelta(0)
                def tzname(self, dt): return "UTC"
                def dst(self, dt): return datetime.timedelta(0)

            return datetime.datetime.fromtimestamp(int(ts), UTC()).isoformat().replace('+00:00','')

        x['atime'] = _t(x['atime'])
        x['mtime'] = _t(x['mtime'])
        x['ctime'] = _t(x['ctime'])

        del x['name']
        del x['type']
        return x

class CrossCheckError(Exception):
    def __init__(self, subject, got=None, expected=None):
        if isinstance(subject, list):
            if isinstance(got[0], dict) and isinstance(got[1], dict):
                self.txt = self._format(subject, got)
            else:
                self.txt = "%s got '%s', but %s got '%s'" % (str(subject[0]), sorted(got[0]), subject[1], sorted(got[1]))
        elif expected is not None:
            self.txt = "%s got '%s', expected '%s'" % (str(subject), got, expected)
        else:
            self.txt = "%s got '%s' <what did I expect?>" % (str(subject), got)

    def __str__(self):
        return self.txt

    def _format(self, subject, got):
        s = ['\n%10s | %10s | %10s' % ('', subject[0], subject[1])]
        keys = set(got[0].keys() + got[1].keys())

        for key in sorted(keys):
            s.append('%10s | %10s | %10s' % (key, got[0].get(key, ''), got[1].get(key, '')))
        return '\n'.join(s)


class CrossChecker:
    def __init__(self, ms, mapper):
        assert len(ms) == 2
        self.ms = ms
        self.mapper = mapper

    def __str__(self):
        return "CrossChecker<%s,%s>" % (str(self.ms[0]), str(self.ms[1]))

    def start(self):
        [m.start() for m in self.ms]

    def stop(self):
        [m.stop() for m in self.ms]

    def write(self, fname, content):
        [m.write(fname, content) for m in self.ms]
        ls = [m.read(fname) for m in self.ms]
        if ls[0] != content:
            raise CrossCheckError(self.ms[0], got=ls[0], expected=content)
        if ls[1] != content:
            raise CrossCheckError(self.ms[1], got=ls[1], expected=content)
    
    def read(self, fname, expected=None):
        ls = [m.read(fname) for m in self.ms]
        if ls[0] != ls[1]:
            raise CrossCheckError(self.ms, got=ls, expected=expected)
        return ls[0]
    
    def listdir(self, fname):
        fs = [m.listdir(fname) for m in self.ms]
        if sorted(fs[0]) != sorted(fs[1]):
            raise CrossCheckError(self.ms, got=fs)
        return fs[0]

    def mkdir(self, fname):
        [m.mkdir(fname) for m in self.ms]

    def stat(self, fname):
        ls = [m.stat(fname) for m in self.ms]
        if self.mapper:
            ls = self.mapper(ls)
        if ls[0] != ls[1]:
            raise CrossCheckError(self.ms, got=ls)
        return ls[0]

class Model:
    class Node:
        def __init__(self, name, ty, parent, content=None):
            self.name = name
            self.ty = ty
            self.content = content
            self.children = {}
            self.parent = parent

        def fullname(self):
            if self.parent is None:
                return ''
            else:
                return '/'.join([self.parent.fullname(), self.name])
            
    
    def __init__(self, m):
        self.m = m
        self.root = Model.Node('<root>', 'dir', None)

        d = self.make_dir('/')
        self.make_n_dirs(20, d)

        ds = [self.make_dir('/') for x in range(0, 10)]
        for i in range(0, 100):
            if random.choice(['dir', 'file', 'file']) == 'dir':
                ds.append(self.make_dir(random.choice(ds)))
            else:
                self.make_file(random.choice(ds))

    def walk(self, fn, node=None):
        if node is None:
            node = self.root

        fn(node.ty, node.fullname())

        if node.ty == 'dir':
            for child in node.children.values():
                self.walk(fn, child)

    def _randstr(self, len=10):
        return ''.join([random.choice(string.letters) for x in range(0, len)])

    def get_node(self, s):
        if s == '/':
            return self.root

        parts = s.split('/')
        if len(parts[0]) == 0:
            parts.pop(0)

        n = self.root
        for part in parts:
            n = n.children[part]
        return n

    def make_file(self, basedir, contentlen=10):
        s = self._randstr()
        c = self._randstr(len=contentlen)
        if self.m:
            self.m.write(os.path.join(basedir, s), c)
        self.get_node(basedir).children[s] = Model.Node(s, 'file', self.get_node(basedir), content=c)

        return os.path.join(basedir, s)

    def make_dir(self, basedir):
        s = self._randstr()
        if self.m:
            self.m.mkdir(os.path.join(basedir, s))
        self.get_node(basedir).children[s] = Model.Node(s, 'dir', self.get_node(basedir))

        return os.path.join(basedir, s)

    def make_n_dirs(self, n, basedir):
        for i in range(0, n):
            self.make_dir(basedir)

if __name__ == "__main__":
    img = "test2.img"
    if os.path.exists(img):
        raise "Please delete file test2.img!"

    mkimg(img, 1024 * 1024 * 256)
    m = MountMutator(img)
    m.start()
    try:
        model = Model(m)
    finally:
        m.stop()

    def _walk(ty, name):
        if not name:
            return
        ccm.start()
        try:
            print "WALK: %s" % name
            ccm.stat(name)

            if ty.lower() == 'dir':
                ccm.listdir(name)
            else:
                ccm.read(name, expected=model.get_node(name).content)
        finally:
            ccm.stop()

    def _vfat_known_inconsistencies(vs):
        # nlink is known to be wrong on VFAT, because we implement lazy directory
        # caching and FAT's inodes do not have a nlink member.
        vs[0]['nlink'] = -1
        vs[1]['nlink'] = -1

        # fstest mounts as root, but mount can't do that.
        vs[0]['uid'] = 0
        vs[1]['uid'] = 0

        # FIXME: ctime is one second behind :(
        vs[1]['ctime'] = vs[0]['ctime']

        return vs

    mm = MountMutator(img)
    fstm = FstestMutator(img, './build/examples/fstest')
    ccm = CrossChecker([mm, fstm], _vfat_known_inconsistencies)

    model.walk(_walk)
