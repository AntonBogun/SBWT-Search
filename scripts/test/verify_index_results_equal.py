#!/bin/python3

"""
Gets a list of files output from the index results, which can be of different
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
args = vars(parser.parse_args())

max_u64 = 18446744073709551615
u64_bytes = 8
byteorder = 'little'
bool_comparison = False


def read_string_from_file(f: io.BytesIO):
    string_size = int.from_bytes(
        f.read(u64_bytes), byteorder=byteorder, signed=False
    )
    return f.read(string_size).decode()


class FileParser:
    def __init__(self, f: io.BytesIO, version: str):
        self.f = f
        file_version = read_string_from_file(f)
        if file_version != version:
            print(
                f'{f.name} has wrong version number, it should be {version}'
            )
            sys.exit(1)

    def get_next(self) -> tuple[str, int]:
        raise NotImplementedError()

    def get_next_as_bool(self) -> tuple[str, bool]:
        t = self.get_next()
        if t[0] == "result":
            return ("result", True)
        if t[0] in ("not_found", "invalid"):
            return ("result", False)
        return t


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
        next_item = int(s)
        if next_item == -1:
            return ("not_found",)
        if next_item == -2:
            return ("invalid",)
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
            return ("not_found",)
        if next_item == max_u64 - 1:
            return ("invalid",)
        if next_item == max_u64 - 2:
            return ("newline",)
        return ("result", next_item)


# pylint: disable=abstract-method,too-many-instance-attributes
class BoolParser(FileParser):
    def __init__(self, values_f: io.BytesIO, seqsizes_f: io.BytesIO):
        self.values_version = "v1.0"
        self.seqsizes_version = "v1.0"
        FileParser.__init__(self, values_f, self.values_version)
        seqsizes_name = read_string_from_file(seqsizes_f)
        if seqsizes_name != "seqsizes":
            print(
                f'{seqsizes_f.name} has wrong name,'
                ' it should be seqsizes'
            )
            sys.exit(1)
        seqsizes_version = read_string_from_file(seqsizes_f)
        if seqsizes_version != self.seqsizes_version:
            print(
                f'{seqsizes_f.name} has wrong version number,'
                f' it should be {self.seqsizes_version}'
            )
            sys.exit(1)
        self.seqsizes_f = seqsizes_f
        self.values_f = values_f
        self.index = 0
        self.working_bits = int.from_bytes(
            f.read(u64_bytes), byteorder=byteorder, signed=False
        )
        self.shift = 63
        self.next_newline = int.from_bytes(
            seqsizes_f.read(u64_bytes), byteorder=byteorder, signed=False
        )
        self.eof = False

    def get_next_as_bool(self) -> tuple[str, bool]:
        if self.eof:
            return ("EOF", )
        if self.index == self.next_newline:
            b = self.seqsizes_f.read(u64_bytes)
            if len(b) == 0:
                self.eof = True
            self.next_newline = int.from_bytes(
                b,
                byteorder=byteorder,
                signed=False
            )
            return ("newline", )
        i = bool(self.working_bits & (1 << self.shift))
        if self.shift == 0:
            self.shift = 64
            self.working_bits = int.from_bytes(
                self.values_f.read(u64_bytes),
                byteorder=byteorder,
                signed=False
            )
        self.shift -= 1
        self.index += 1
        return ("result", i)


with ExitStack() as stack:
    file_parsers = []
    files: list[io.BytesIO] = [
        stack.enter_context(open(filename, mode='rb'))
        for filename in [args['file1'], args['file2']]
    ]
    for f in files:
        file_type = read_string_from_file(f)
        if file_type == "ascii":
            file_parsers.append(AsciiParser(f))
        elif file_type == "binary":
            file_parsers.append(BinaryParser(f))
        elif file_type == "bool":
            bool_comparison = True
            seq_sizes_file = stack.enter_context(
                open(f.name[:-5] + '.seqsizes', mode='rb')
            )
            file_parsers.append(BoolParser(f, seq_sizes_file))

    line_count = 0
    position = 0
    brk = False
    while True:
        if bool_comparison:
            item1 = file_parsers[0].get_next_as_bool()
            item2 = file_parsers[1].get_next_as_bool()
        else:
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

print('The file contents match!')
