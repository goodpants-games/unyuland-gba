#!/usr/bin/env python3
import math
import argparse
import ioutil

TABLE_LENGTH = 256
SINE_AMP = 256

def generate(ofile) -> None:
    out_bytes = bytearray(TABLE_LENGTH * 4)

    i = 0
    for n in range(0, TABLE_LENGTH):
        value = int(math.sin(math.pi * (n / TABLE_LENGTH)) * SINE_AMP)

        out_bytes[i+0] = value & 0xFF
        out_bytes[i+1] = (value >> 8) & 0xFF
        out_bytes[i+2] = (value >> 16) & 0xFF
        out_bytes[i+3] = (value >> 24) & 0xFF
        i += 4

    ofile.write(out_bytes)


def main() -> None:
    parser = argparse.ArgumentParser(prog='mapc')
    parser.add_argument('output', help="output bin file. pass - to write to stdout.")
    args = parser.parse_args()

    with ioutil.open_output(args.output, binary=True) as ofile:
        generate(ofile)    


if __name__ == '__main__':
    main()