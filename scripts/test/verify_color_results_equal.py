#!/bin/python3

"""
Gets a list of files output from the color results, which can be of different
formats, and makes sure that their contents are the same.  (usage:
./verify_files_equal.py -l <file 1> <file 2>)
"""

import argparse
import io
import sys
from contextlib import ExitStack

parser = argparse.ArgumentParser()
parser.add_argument(
    '-x', '--file1', help='First file to compare with second', required=True
)
parser.add_argument(
    '-y', '--file2', help='Second file to compare with first', required=True
)
parser.add_argument(
    '-q',
    '--quiet',
    help='Do not print anything if contents match',
    required=False,
    action='store_true',
    default=False
)
args = vars(parser.parse_args())

max_u64 = 18446744073709551615
u64_bytes = 8
byteorder = 'little'


def read_string_from_file(f: io.BytesIO):
    string_size = int.from_bytes(
        f.read(u64_bytes), byteorder=byteorder, signed=False
    )
    return f.read(string_size).decode()


class FileParser:
    def __init__(self, f: io.BytesIO, version: str | None):
        self.f = f
        if version is None:
            return
        file_version = read_string_from_file(f)
        if file_version != version:
            print(
                f'{f.name} has wrong version number, it should be {version}'
            )
            sys.exit(1)

    def get_next(self) -> tuple[str, int]:
        raise NotImplementedError()


class AsciiParser(FileParser):
    def __init__(self, f: io.BytesIO):
        self.version = "v1.0"
        FileParser.__init__(self, f, self.version)
        self.is_at_newline = False

    def get_next(self) -> tuple[str, int]:
        if self.is_at_newline:
            self.is_at_newline = False
            return ("newline", )
        s = ""
        while True:
            next_char = self.f.read(1).decode()
            if next_char == '':
                return ("EOF",)
            if next_char == '\n':
                if s == "":
                    return ("newline", )
                self.is_at_newline = True
                break
            if next_char in (' ', ''):
                if s == "":
                    raise RuntimeError(
                        "Error, space after newline or another space"
                    )
                break
            s += next_char
        return ("result", int(s))


class BinaryParser(FileParser):
    def __init__(self, f: io.BytesIO):
        self.version = "v1.0"
        FileParser.__init__(self, f, self.version)

    def get_next(self) -> tuple[str, int]:
        b = self.f.read(u64_bytes)
        if len(b) == 0:
            return ("EOF", )
        next_item = int.from_bytes(
            b, byteorder=byteorder, signed=False
        )
        if next_item == max_u64:
            return ("newline",)
        return ("result", next_item)


class CsvParser(FileParser):
    def __init__(self, f: io.BytesIO):
        self.values = None
        FileParser.__init__(self, f, None)
        f.readline()  # skip header

    def get_next(self) -> tuple[str, int]:
        if self.values is None:
            line = f.readline().decode()
            if line == '':
                return ("EOF", )
            self.values = line.strip().split(',')
        try:
            next_index = self.values.index('1')
        except ValueError:
            self.values = None
            return ("newline", )
        self.values[next_index] = 0
        return ("result", next_index)



class PackedIntParser(FileParser):
    def __init__(self, f: io.BytesIO):
        self.version = "v1.0"
        FileParser.__init__(self, f, self.version)

    def get_next(self) -> tuple[str, int]:
        i=0
        b = self.f.read(1)
        if len(b)==0:
            return ("EOF", )
        c = b[0]
        if not(c&0x80):
            if c==0b01000010:
                return ("newline", )
            elif c&0b01000000:
                raise RuntimeError(f"invalid special character: {bin(c)}")
            else:
                return ("result", c)
        n=c&0x7f
        while True:
            i+=1
            if(i>9):
                raise RuntimeError(f"int too long ({i}): {bin(n)}")
            b=self.f.read(1)
            if len(b) == 0:
                return ("EOF", )
            c=b[0]
            n|=((c&0x7f)<<i)
            if not(c&0x80):
                return ("result",n)


with ExitStack() as stack:
    file_parsers = []
    files: list[io.BytesIO] = [
        stack.enter_context(open(filename, mode='rb'))
        for filename in [args['file1'], args['file2']]
    ]
    for f in files:
        if f.name.endswith('.csv'):
            file_parsers.append(CsvParser(f))
            continue
        file_type = read_string_from_file(f)
        if file_type == "ascii":
            file_parsers.append(AsciiParser(f))
        elif file_type == "binary":
            file_parsers.append(BinaryParser(f))
        elif file_type == "packedint":
            file_parsers.append(PackedIntParser(f))

    line_count = 0
    position = 0
    brk = False
    while True:
        item1 = file_parsers[0].get_next()
        item2 = file_parsers[1].get_next()
        if item1[0] != item2[0]:
            print(f'Objects differ at line {line_count}, position: {position}')
            print(f'item1 == {item1}')
            print(f'item2 == {item2}')
            sys.exit(1)
        if item1[0] == "EOF":
            brk = True
        elif item1[0] == "newline":
            line_count += 1
            position = 0
        elif (
            item1[0] == "result"
            and item1[1] != item2[1]
        ):
            print(f'Objects differ at line {line_count}, position: {position}')
            print(f'item1 == {item1}')
            print(f'item2 == {item2}')
            sys.exit(1)
        if brk:
            break
        position += 1

if not args['quiet']:
    print('The file contents match!')
