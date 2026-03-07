#!/usr/bin/env python3
import math
import argparse
import ioutil
import typing
import sys

def gen_tri() -> list[float]:
    samples: list[float] = []
    for i in range(0, 32):
        n = i / 32
        smp: float = 2.0 * abs(((n + 0.25) % 1.0) - 0.5)
        if smp < 0.0: smp = 0.0
        if smp > 1.0: smp = 1.0

        samples.append(smp)
    
    return samples

def gen_noise() -> list[float]:
    samples: list[float] = []

    drum_buf: int = 1
    for i in range(0, 32):
        smp = float(drum_buf & 1)
        smp = (smp * 2.0 - 1.0) / 2
        smp = (smp + 1.0) / 2.0
        print(smp)
        new_buf = drum_buf >> 1
        if ((drum_buf + new_buf) & 1) == 1:
            new_buf += 10 << 2
        drum_buf = new_buf

        if smp < 0.0: smp = 0.0
        if smp > 1.0: smp = 1.0

        samples.append(smp)
    
    return samples


def generate(ofile: typing.BinaryIO, wave: str) -> None:
    match wave:
        case 'triangle': 
            samples = gen_tri()
        case 'noise':
            samples = gen_noise()
        case _:
            print("error: invalid wave mode. expected 'triangle' or 'noise'.",
                  file=sys.stderr)
            exit(1)
    
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
    parser.add_argument('output',
                        help="output bin file. pass - to write to stdout.")
    parser.add_argument('-w', '--wave', dest='wave',
                        help="wave type: \"triangle\" or \"noise\"")
    
    args = parser.parse_args()

    with ioutil.open_output(args.output, binary=True) as ofile:
        generate(ofile, args.wave)


if __name__ == '__main__':
    main()