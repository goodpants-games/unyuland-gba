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

class BinaryInputIO(io.BufferedWriter):
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
    print("Hi")