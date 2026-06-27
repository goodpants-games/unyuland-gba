#!/usr/bin/env python3
import argparse
import ioutil
import sys
import re
import os.path as path
import typing


class ProgramError(RuntimeError):
    pass


class ModuleData:
    def __init__(self: typing.Self, symbol_name: str, bin_include: str):
        self.symbol_name = symbol_name
        self.bin_include = bin_include


def gen_libopenmpt(out, paths: list[str], mpt_c_path: str):
    if not mpt_c_path:
        raise ProgramError("argument --mpt-bank not given")    

    mod_data: list[ModuleData] = []

    for p in paths:
        dirname = path.dirname(p)
        mod_name = path.basename(path.splitext(p)[0])
        mod_name = re.sub('[^A-Za-z0-9_]', '_', mod_name)

        mod_data.append(ModuleData(
            symbol_name = mod_name,
            bin_include = f"#include <{dirname}/{mod_name}_bin.h>",
        ))

    mod_count = len(mod_data)
    
    # write public header containing module identifiers
    out.write("#pragma once\n\n")
    out.write("typedef enum {\n")
    for data in mod_data:
        out.write(f"    MOD_{data.symbol_name.upper()},\n")
    out.write("} module_id_e;\n\n")
    out.write(f"#define MODDAT_NSONGS {mod_count}\n")

    # write source file defining the module list 
    with ioutil.open_output(mpt_c_path) as mpt_out_c:
        mpt_out_c.write("#include <stddef.h>\n\n")

        for data in mod_data:
            mpt_out_c.write(data.bin_include)
            mpt_out_c.write("\n")
        mpt_out_c.write("\n")

        mpt_out_c.write(f"const void *const mpt_module_banks[{mod_count}] = {{\n")
        for data in mod_data:
            mpt_out_c.write("    ")
            mpt_out_c.write(f"{data.symbol_name}_bin,\n")
        mpt_out_c.write("};\n\n")

        mpt_out_c.write(f"const size_t mpt_module_sizes[{mod_count}] = {{\n")
        for data in mod_data:
            mpt_out_c.write("    ")
            mpt_out_c.write(f"{data.symbol_name}_bin_size,\n")
        mpt_out_c.write("};\n")


def gen_maxmod(out, paths: list[str]):
    out.write("#pragma once\n")
    out.write("#include <data/mm_soundbank.h>\n")
    out.write("#define MODDAT_NSONGS MSL_NSONGS\n")

def main() -> None:
    parser = argparse.ArgumentParser('modidx')
    parser.add_argument('-t', '--target',
                        help="'mm' (for maxmod) or 'mpt' (libopenmpt)")
    parser.add_argument('-o', '--output',
                        help="output file. stdout if omitted.")
    parser.add_argument('--mpt-bank',
                        help="output C source file used by mpt driver")
    parser.add_argument('module', nargs='+')

    args = parser.parse_args()
    
    if args.output is not None:
        out_path = args.output
    else:
        out_path = '-'
    
    with ioutil.open_output(out_path) as out:
        match args.target:
            case "mm":
                gen_maxmod(out, args.module)
            case "mpt":
                gen_libopenmpt(out, args.module, args.mpt_bank)
            case _:
                raise ProgramError(f"error: unknown target '{args.target}'")


if __name__ == '__main__':
    try:
        main()
    except ProgramError as e:
        print(e.args[0], file=sys.stderr)