import sys, os, struct

def gcov_convert(stream):
    out = []
    for line in stream:
        if not line.startswith('GCOV-'):
            out.append(line)
            continue

        head, tail = line.split(': ', 1)
        filename = head[len("GCOV-"):]

        with open(filename, "wb") as fd:
            for number in tail.split(' ')[:-1]:
                int32 = int(number, 16)
                fd.write(struct.pack("I", int32))
    
    return '\n'.join(out)
            
if __name__ == "__main__":
    gcov_convert(sys.stdin.readlines())
