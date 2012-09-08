#!/usr/bin/python

import os, sys, signal, subprocess, tempfile, threading, select, termios, re

from image import Image
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection

def _setup_terminal(t):
    iflag, oflag, cflag, lflag, ispeed, ospeed, cc = termios.tcgetattr(t)

    # Emulate cfmakeraw(3)
    iflag &= ~(termios.IGNBRK | termios.BRKINT | termios.PARMRK |
               termios.ISTRIP | termios.INLCR | termios.IGNCR |
               termios.ICRNL | termios.IXON)
    oflag &= ~termios.OPOST;
    lflag &= ~(termios.ECHO | termios.ECHONL | termios.ICANON |
               termios.ISIG | termios.IEXTEN)
    cflag &= ~(termios.CSIZE | termios.PARENB)
    cflag |= termios.CS8;

    termios.tcsetattr(t, termios.TCSANOW, [iflag, oflag, cflag, lflag, ispeed, ospeed, cc])


class Bochs:
    def __init__(self, exe_name='bochs', args=None):
        self.exe_name = exe_name
        self.args = args
        self.stop = False

    def run(self, floppy_image, trace, timeout):        
        if trace:
            return "Unable to trace with Bochs."

        if timeout:
            # Bochs is slow.
            timeout *= 3;
            def _alarm():
                try:
                    child.send_signal(signal.SIGTERM);
                except:
                    pass
                self.stop = True
            t = threading.Timer(float(timeout) / 1000.0, _alarm)
            t.start()

        imaster, islave = os.openpty()

        # Put the terminal in raw mode.
        _setup_terminal(imaster)

        o = open('/tmp/x','w')
        
        args = ['-q',
                'floppya: 1_44=%s,status=inserted' % floppy_image,
                'boot: floppy',
                'display_library: nogui',
                'com1: enabled=1, mode=term, dev=%s' % os.ttyname(islave)]
        child = subprocess.Popen([self.exe_name] + args, stderr=o, stdout=o)

        out = ""
        while not self.stop:
            r,w,x = select.select([0,imaster],[],[],0.05)

            # stdin ready for reading?
            if 0 in r:
                os.write(imaster, os.read(0, 128))

            # pty ready for reading?
            if imaster in r:
                out += os.read(imaster, 128)

        try:
            # Kill bochs.
            child.send_signal(signal.SIGTERM)

            # Ensure all data is read from the child before reaping it.
            while True:
                r,w,x = select.select([imaster],[],[],0.05)
                if imaster in r:
                    out += os.read(imaster, 1024)
                    continue
                break
        except:
            
            pass
        code = child.wait()

        os.close(imaster)
        os.close(islave)
        t.cancel()

        return out.splitlines()

class Qemu:
    def __init__(self, exe_name='qemu', args=None):
        self.exe_name = exe_name
        self.args = args

    def _check_finished(self, fd):
        os.write(fd, "info registers\n");
        r,w,x = select.select([fd],[],[],0.05)
        if fd in r:
            s = os.read(fd, 4096)
            # Look for interrupts disabled and halted.
            m = re.search(r'EFL=([0-9a-fA-F]+)', s)
            if m:
                eflags = int(m.group(1), 16)
                if eflags & 0x200 == 0 and s.find('HLT=1') != -1:
                    return True
            return False

        else:
            return True

    def run(self, floppy_image, trace, timeout):
        self.stop = False

        if timeout:
            def _alarm():
                try:
                    child.send_signal(signal.SIGKILL);
                except:
                    pass
                self.stop = True
            t = threading.Timer(float(timeout) / 1000.0, _alarm)
            t.start()

        extra = self.args + ['-boot','a']

        # Pty for communicating with qemu's monitor.
        master, slave = os.openpty()
        extra += ["-S", "-monitor", os.ttyname(slave)]
        
        if trace:
            extra += ["-d", "in_asm"]

            tracefd, tracefn = tempfile.mkstemp()
            os.close(tracefd)

        imaster, islave = os.openpty()

        # Put the terminal in raw mode.
        _setup_terminal(imaster)

        extra += ["-serial", os.ttyname(islave)]

        errfd, errfn = tempfile.mkstemp()
        child = subprocess.Popen([self.exe_name,
                                  "-fda", floppy_image,
                                  "-nographic", "-monitor", "null", "-no-kvm"] +
                                 extra,
                                 stderr=errfd)
        if trace:
            os.write(master, "logfile %s\n" % tracefn)
        os.write(master, "c\n")

        out = ""
        while not self.stop:
            r,w,x = select.select([0,imaster],[],[],0.05)

            # stdin ready for reading?
            if 0 in r:
                os.write(imaster, os.read(0, 128))

            # pty ready for reading?
            if imaster in r:
                out += os.read(imaster, 128)

            if not self.stop and self._check_finished(master):
                break

        try:
            # Kill qemu.
            child.send_signal(signal.SIGKILL)

            # Ensure all data is read from the child before reaping it.
            while True:
                r,w,x = select.select([imaster],[],[],0.05)
                if imaster in r:
                    out += os.read(imaster, 1024)
                    continue
                break
        except:
            pass

        code = child.wait()
        os.close(master)
        os.close(slave)

        if code != 0 and code != -9 and not self.stop:
            print out
            print open(errfn).read()
            raise RuntimeError("Qemu exited with code %s!" % code)

        # Quitting qemu causes it to mess up the console. Call stty sane
        # to unmangle it!
        if sys.stdout.isatty() or sys.stderr.isatty():
            subprocess.call(["stty", "sane", "-F", "/dev/tty"])

        os.close(imaster)
        os.close(islave)

        if trace:
            ret = open(tracefn).readlines()
            os.unlink(tracefn)
        else:
            ret = out.splitlines()

        os.unlink(errfn)
        t.cancel()
        return ret

class Runner:
    def __init__(self, image, trace=False, syms=False, timeout=None,
                 preformatted_image=os.path.join('..','floppy.img.zip'),
                 argv=None, keep_temps=False, qemu_opts=[]):
        self.image = image
        self.trace = trace
        self.syms = syms
        self.timeout = timeout
        self.argv = argv
        self.keep_temps = keep_temps
        self.qemu_opts = qemu_opts

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
            if os.environ.get('MODEL', '').lower() == 'bochs':
                self.model = Bochs('bochs', [])
            else:
                self.model = Qemu('qemu-system-i386', self.qemu_opts)

        else:
            raise RuntimeError("Unknown architecture: %s" % self.arch)

        fd, self.tmpimage = tempfile.mkstemp()
        os.close(fd)
        self.floppy_image = Image(self.tmpimage, preformatted_image)
        self.floppy_image.create_grub_conf(args=self.argv)
        self.floppy_image.copy(self.image, '/kernel')

    def run(self):
        x = self.model.run(self.tmpimage, self.trace, self.timeout)
        if not self.keep_temps:
            os.unlink(self.tmpimage)
        else:
            print "Image @ %s" % self.tmpimage
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
    p.add_option('--timeout', dest='timeout', default=8000, type='int',
                 help='Timeout before killing the model in milliseconds')
    p.add_option('--preformatted-image', dest='image',
                 default=os.path.join('src','floppy.img.zip'),
                 help='Path to the preformatted floppy disk image to splat the kernel onto')
    p.add_option('--keep-temps', action='store_true', dest='keep_temps')

    opts, args = p.parse_args()

    if not args:
        p.print_help()
        sys.exit(1)

    try:
        argv = args[1:]
    except:
        argv = None

    qemu_opts = []
    if 'HDD_IMAGE' in os.environ:
        qemu_opts += ['-hda', os.environ['HDD_IMAGE']]

    r = Runner(args[0], trace=opts.trace, syms=opts.syms, qemu_opts=qemu_opts,
               timeout=opts.timeout, preformatted_image=opts.image, argv=argv,
               keep_temps=opts.keep_temps)

    for l in r.run():
        print l

