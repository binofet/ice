"""Apply documented compatibility fixes to a private copy of pinned RTTR sources."""
from pathlib import Path
import sys

root = Path(sys.argv[1])
changes = [
    ("src/rttr/type.h",
     "variant create(std::vector<argument> args = std::vector<argument>()) const;",
     "variant create() const;\n        variant create(std::vector<argument> args) const;"),
    ("src/rttr/type.cpp", "variant type::create(vector<argument> args) const",
     "variant type::create() const\n{\n    return create(std::vector<argument>{});\n}\n\n"
     "variant type::create(vector<argument> args) const"),
    ("CMakeLists.txt", "cmake_minimum_required (VERSION 3.0)",
     "cmake_minimum_required (VERSION 3.5)"),
]
prepared = []
for name, old, new in changes:
    path = root / name
    text = path.read_text()
    if text.count(old) != 1:
        raise SystemExit(f"Unexpected pinned RTTR source: {name}; refusing compatibility patch")
    prepared.append((path, text.replace(old, new)))
for path, text in prepared:
    path.write_text(text)
