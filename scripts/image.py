import subprocess, os, sys, zipfile

class Image:
    def __init__(self, fname, preformatted_img):
        self.preformatted_img = preformatted_img
        self.fname = fname
        if not os.path.exists(fname) or os.path.getsize(fname) == 0:
            self._extract_zip(fname)

    def _extract_zip(self, fname):
        if not zipfile.is_zipfile(self.preformatted_img):
            raise RuntimeError("Preformatted image '%s' is not in ZIP format!" % 
                               self.preformatted_img)

        f = zipfile.ZipFile(self.preformatted_img, 'r')
        nl = f.namelist()
        assert len(nl) == 1
        data = f.read(nl[0])
        with open(fname, 'w') as fd:
            fd.write(data)

    def _exec(self, args, stdout=None, stderr=None):
        try:
            child = subprocess.Popen(args,
                                     stdout=stdout,
                                     stderr=stderr)
            child.wait()
        except OSError as e:
            raise OSError(str(e) + " while running " + args[0])

    def copy(self, localname, imagepath):
        self._exec(["mdel", "-i", self.fname, "::"+imagepath],
                   stderr=subprocess.PIPE)
        self._exec(["mcopy", "-n", "-i", self.fname,
                    localname, "::"+imagepath])

    def create_grub_conf(self, title="JamesM's kernel development tutorials",
                         root="(fd0)", kernel="/kernel", args=None):
        if args is None:
            args = ""
        else:
            args = " ".join(args)

        s = """timeout=0
default=0

title %s
root %s
kernel %s %s
boot
""" % (title, root, kernel, args)

        open('/tmp/grub.cfg', 'w').write(s)
        self.copy('/tmp/grub.cfg', '/grub/grub.cfg')

if __name__ == "__main__":
    directory = sys.argv[1]
    kernel = sys.argv[2]
    outimg = sys.argv[3]

    i = Image(outimg, os.path.join(directory, 'floppy.img.zip'))
    i.create_grub_conf()
    i.copy(kernel, '/kernel')
