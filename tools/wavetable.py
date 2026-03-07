#!/usr/bin/env python3
import math
import argparse
import ioutil

# generate triangle wave
def generate(ofile) -> None:
    samples: list[float] = []
    for i in range(0, 32):
        n = i / 32
        smp: float = 2.0 * abs(((n + 0.25) % 1.0) - 0.5)
        if smp < 0.0: smp = 0.0
        if smp > 1.0: smp = 1.0

        samples.append(smp)
    
    out_bytes = bytearray(16)

    j = 0
    for i in range(0, 16):
        nibble0: int = int(round(samples[j] * 0xF)) & 0xF
        nibble1: int = int(round(samples[j+1] * 0xF)) & 0xF
        j += 2

        out_bytes[i] = (nibble0 << 4) | nibble1

    ofile.write(out_bytes)


def main() -> None:
    parser = argparse.ArgumentParser(prog='wavetable')
    parser.add_argument('output', help="output bin file. pass - to write to stdout.")
    args = parser.parse_args()

    with ioutil.open_output(args.output, binary=True) as ofile:
        generate(ofile)


if __name__ == '__main__':
    main()