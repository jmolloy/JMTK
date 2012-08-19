#!/usr/bin/python

import os, sys, subprocess, re

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
        return os.listdir(fname)

    def mkdir(self, fname):
        fname = os.path.join(self.mpoint, '.'+fname)
        return os.mkdir(fname)

    def stat(self, fname):
        x = os.stat(os.path.join(self.mpoint, '.'+fname))
        mode = "%x" % x.st_mode
        return {'mode': mode, 'nlink': x.st_nlink,
                'uid': x.st_uid, 'gid': x.st_gid,
                'size': x.st_size, 'atime': int(x.st_atime),
                'mtime': int(x.st_mtime), 'ctime': int(x.st_ctime)}

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

        return filter(lambda f: f['name'] == name, files)

class CrossCheckError(Exception):
    def __init__(self, subject, got=None, expected=None):
        if isinstance(subject, list):
            self.txt = "%s got '%s', but %s got '%s'" % (str(subject[0]), got[0], subject[1], got[1])
        elif expected is not None:
            self.txt = "%s got '%s', expected '%s'" % (str(subject), got, expected)
        else:
            self.txt = "%s got '%s' <what did I expect?>" % (str(subject), got)

    def __str__(self):
        return self.txt


class CrossChecker:
    def __init__(self, ms):
        assert len(ms) == 2
        self.ms = ms

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
    
    def read(self, fname):
        ls = [m.read(fname) for m in self.ms]
        if ls[0] != ls[1]:
            raise CrossCheckError(self.ms, got=ls)
        return ls[0]
    
    def listdir(self, fname):
        fs = [m.listdir(fname) for m in self.ms]
        if fs[0] != fs[1]:
            raise CrossCheckError(self.ms, got=ls)
        return fs[0]

    def mkdir(self, fname):
        [m.mkdir(fname) for m in self.ms]

    def stat(self, fname):
        ls = [m.stat(fname) for m in self.ms]
        if ls[0] != ls[1]:
            raise CrossCheckError(self.ms, got=ls)
        return ls[0]

if __name__ == "__main__":

    def make(m):
        m.mkdir("abc")
        m.write("abc/bcd", "woof caken\nwoof")
        m.write("cde", "MNNYAAHH")

    def check(m):
        out = m.read("/cde")
        if out != "MNNYAAfHH":
            raise CrossCheckError(m, got=out, expected="MNNYAAHH")
        

    img = "test2.img"
    if not os.path.exists(img):
        mkimg(img, 1024 * 1024 * 256)
        m = MountMutator(img)
        m.start()
        try:
            make(m)
        finally:
            m.stop()

    mm = MountMutator(img)
    fstm = FstestMutator(img, './build/examples/fstest')
    ccm = CrossChecker([mm, fstm])

    ccm.start()
    try:
        check(ccm)
    finally:
        ccm.stop()
