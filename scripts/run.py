import os, sys, signal, subprocess, tempfile, threading

from image import Image

from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection

class Qemu:
    def __init__(self, exe_name='qemu', args=None):
        self.exe_name = exe_name
        self.args = args

    def run(self, floppy_image, trace, timeout):

        if timeout:
            def _alarm():
                child.terminate()
            t = threading.Timer(float(timeout) / 1000.0, _alarm)
            t.start()

        extra = []
        if trace:
            master, slave = os.openpty()
            extra += ["-d", "in_asm"]
            extra += ["-S", "-monitor", os.ttyname(slave)]
            tracefd, tracefn = tempfile.mkstemp()
            os.close(tracefd)

        outfd, outfn = tempfile.mkstemp()
        errfd, errfn = tempfile.mkstemp()
        child = subprocess.Popen([self.exe_name,
                                  "-fda", floppy_image,
                                  "-nographic"] +
                                 extra,
                                 stdout=outfd,
                                 stderr=errfd)
        if trace:
            os.write(master, "logfile %s\n" % tracefn)
            os.write(master, "c\n")

        child.wait()
        
        if trace:
            os.close(master)
            os.close(slave)

        # Quitting qemu causes it to mess up the console. Call stty sane
        # to unmangle it!
        if sys.stdout.isatty() or sys.stderr.isatty():
            subprocess.call(["stty", "sane", "-F", "/dev/tty"])

        if trace:
            ret = open(tracefn).readlines()
            os.unlink(tracefn)
        else:
            ret = open(outfn).readlines()

        os.unlink(outfn)
        os.unlink(errfn)
        return ret

class Runner:
    def __init__(self, image, trace=False, syms=False, timeout=None,
                 preformatted_image=os.path.join('..','floppy.img.zip')):
        self.image = image
        self.trace = trace
        self.syms = syms
        self.timeout = timeout

        assert os.path.exists(self.image)
        
        with open(self.image, 'rb') as fd:
            elffile = ELFFile(fd)
            if elffile.get_machine_arch() == 'x86':
                self.arch = 'X86'
            else:
                raise RuntimeError("Unknown architecture: %s" % elf.get_machine_arch())

            if syms:
                # Get the symbols in the file.
                self.symbols = {}
                for section in elffile.iter_sections():
                    if not isinstance(section, SymbolTableSection):
                        continue

                    for sym in section.iter_symbols():
                        self.symbols[sym['st_value']] = sym.name

        if self.arch == 'X86':
            self.model = Qemu('qemu-system-i386', [])
        else:
            raise RuntimeError("Unknown architecture: %s" % self.arch)

        fd, self.tmpimage = tempfile.mkstemp()
        os.close(fd)
        self.floppy_image = Image(self.tmpimage, preformatted_image)
        self.floppy_image.create_grub_conf()
        self.floppy_image.copy(self.image, '/kernel')

    def run(self):
        x = self.model.run(self.tmpimage, self.trace, self.timeout)
        os.unlink(self.tmpimage)
        if not self.trace:
            return x
        if not self.syms:
            return x
        s = []
        for l in x:
            colon = l.find(':')
            if colon != -1:
                try:
                    pc_loc = int(l[:colon], 0)
                    if pc_loc in self.symbols:
                        s.append(self.symbols[pc_loc] + l[colon:].strip())
                        continue
                    s.append(l.strip())
                except:
                    pass
        return s



if __name__ == "__main__":
    from optparse import OptionParser
    p = OptionParser(usage="Usage: %prog <kernel-elf> [options]")
    p.add_option('--trace', action='store_true', dest='trace', default=False,
                 help='Output an execution trace instead of the serial output')
    p.add_option('--symbols', '--syms', action='store_true', dest='syms',
                 default=False, help='Translate raw addresses in trace output to symbol names if possible')
    p.add_option('--timeout', dest='timeout', default=500, type='int',
                 help='Timeout before killing the model in milliseconds')
    p.add_option('--preformatted-image', dest='image',
                 default=os.path.join('..','floppy.img.zip'),
                 help='Path to the preformatted floppy disk image to splat the kernel onto')
    opts, args = p.parse_args()

    if not args:
        p.print_help()
        sys.exit(1)
    r = Runner(args[0], trace=opts.trace, syms=opts.syms,
               timeout=opts.timeout, preformatted_image=opts.image)
    for l in r.run():
        print l
