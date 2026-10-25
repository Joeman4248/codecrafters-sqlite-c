# How GDB shows the view types from src/util/view.h. A view's data isn't
# null-terminated, so by default GDB shows text up to the next '\0', past the
# end of the view. These show only its len characters. launch.json loads this
# file when a debug session starts.

import gdb


class StringViewPrinter:
    def __init__(self, value):
        self.value = value

    def to_string(self):
        length = int(self.value["len"])
        return self.value["data"].string(encoding="utf-8", errors="replace", length=length)

    def display_hint(self):
        return "string"


class ByteViewPrinter:
    def __init__(self, value):
        self.value = value

    def to_string(self):
        return f"{int(self.value['len'])} bytes"

    def children(self):
        yield "len", self.value["len"]
        yield "data", self.value["data"]


def lookup(value):
    # the views are typedefs of unnamed structs, so they're matched by the typedef's name
    name = value.type.name
    if name == "StringView":
        return StringViewPrinter(value)
    if name == "ByteView":
        return ByteViewPrinter(value)
    return None


gdb.pretty_printers.append(lookup)
