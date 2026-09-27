#!/usr/bin/env python3
# Copyright (C) 2026  pkhead
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

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