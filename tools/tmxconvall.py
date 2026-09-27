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

import argparse
import json
import sys
import os.path as path
from tmxconv import convert as tmxconvert

def convert(input_path: str, outdir: str, args):
    input_dir = path.dirname(input_path)
    with open(input_path, 'r') as f:
        world_data = json.load(f)
    
    for map in world_data['maps']:
        file_path = map['fileName']
        file_name = path.basename(file_path)

        src_path = path.normpath(path.join(input_dir, file_path))
        dst_path = path.join(outdir, file_name)

        if (args.always
                or not path.exists(dst_path)
                or path.getmtime(src_path) > path.getmtime(dst_path)):
            print(file_name, file=sys.stderr)
            with open(dst_path, 'wb') as dst_f:
                tmxconvert(src_path, dst_f)


def main():
    parser = argparse.ArgumentParser(prog='tmxconvall')
    parser.add_argument('input', help='input world json file.')
    parser.add_argument('output', help='output folder')
    parser.add_argument('-a', help='unconditionally process all maps',
                        dest='always', action='store_true')

    args = parser.parse_args()

    convert(args.input, args.output, args)


if __name__ == '__main__': main()