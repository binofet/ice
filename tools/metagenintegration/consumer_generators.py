"""Engine-owned adapters for explicit metadata source-tooling checks."""
import importlib.util
import json
from pathlib import Path
import sys

OUTPUTS = ("schema.generated.h", "rttr.generated.cpp", "graph.generated.h")


def generate(source, output):
    """Consume the SDK's normalized proof views; generate only engine-owned outputs."""
    output = Path(output)
    # Source tooling is explicitly trusted, as in the SDK check it follows.
    # Cached-only engine builds never load the SDK or enter this function.
    sys.path.insert(0, str(Path(source) / "python"))
    from portable_schema import fingerprint
    from graph_corpus import cases
    from format_generated import format_cpp

    path = Path(__file__).parent / "generators/native_schema.py"
    spec = importlib.util.spec_from_file_location("engine_native_schema", path)
    native = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(native)
    schemas = json.loads((output / "canonical-schema.json").read_text())
    data = json.loads((output / "python-data.json").read_text())
    native.generate(schemas, output)
    digest = bytes.fromhex(fingerprint(data))
    rows = cases(data)
    header = output / 'graph.generated.h'
    header.write_text('// Generated graph conformance identity; do not edit.\n#pragma once\n'
                      '#include "ice/data/document.h"\nnamespace metagen_examples {\n'
                      'inline constexpr ice::data::schema_fingerprint graph_fingerprint = {' +
                      ', '.join('std::byte{0x' + format(b, '02x') + '}' for b in digest) + '};\n'
                      'inline constexpr unsigned graph_case_count = ' + str(len(rows)) + ';\n}\n')
    format_cpp([header])


def check_or_update(source, output, cache, update=False):
    """Require all adapter bytes as well as the SDK's portable-cache checks."""
    generate(source, output)
    for name in OUTPUTS:
        actual = (Path(output) / name).read_bytes()
        target = Path(cache) / name
        if update:
            target.write_bytes(actual)
        elif not target.is_file() or target.read_bytes() != actual:
            raise RuntimeError("Stale native consumer cache: " + name)
