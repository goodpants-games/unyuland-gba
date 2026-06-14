#!/usr/bin/env python3
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