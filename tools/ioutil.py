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

import io
import typing
import sys


class TextOutputIO(io.TextIOWrapper):
    def __init__(self: typing.Self, path: str, encoding: str|None=None):
        if path == '-':
            self._tty = True
            super().__init__(sys.stdout.buffer, encoding=encoding)
        else:
            self._tty = False
            super().__init__(open(path, 'wb'), encoding=encoding)
    

    def close(self):
        if not self._tty:
            return super().close()
        else:
            self.flush()


class BinaryOutputIO(io.BufferedWriter):
    def __init__(self: typing.Self, path: str):
        if path == '-':
            self._tty = True
            super().__init__(sys.stdout.buffer)
        else:
            self._tty = False
            super().__init__(open(path, 'wb'))


    def close(self):
        if not self._tty:
            return super().close()
        else:
            self.flush()


class TextInputIO(io.TextIOWrapper):
    def __init__(self: typing.Self, path: str, encoding: str|None=None):
        if path == '-':
            self._tty = True
            super().__init__(sys.stdin.buffer, encoding=encoding)
        else:
            self._tty = False
            super().__init__(open(path, 'rb'), encoding=encoding)
            

    def close(self):
        if not self._tty:
            return super().close()
        else:
            self.flush()


class BinaryInputIO(io.BufferedReader):
    def __init__(self: typing.Self, path: str):
        if path == '-':
            self._tty = True
            super().__init__(sys.stdin.buffer)
        else:
            self._tty = False
            super().__init__(open(path, 'rb'))


    def close(self):
        if not self._tty:
            return super().close()
        else:
            self.flush()


def open_output(path: str, binary: bool = False, encoding:str|None=None):
    if binary:
        return BinaryOutputIO(path)
    else:
        return TextOutputIO(path, encoding=encoding)
    

def open_input(path: str, binary: bool = False, encoding:str|None=None):
    if binary:
        return BinaryInputIO(path)
    else:
        return TextInputIO(path, encoding=encoding)
    

if __name__ == '__main__':
    out = TextOutputIO('tmp.txt')
    out.write("hi\n")
    print(out.isatty())
    out.close()