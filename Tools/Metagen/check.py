#!/usr/bin/env python3
"""Prove C#/Python schema equivalence and regenerate/check independent native consumers."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).parent / "Python"))
from metagen_source import read, SchemaError
from portable_schema import normalize, fingerprint
from generate_native import generate
from generate_rust import generate as generate_rust, generate_data as generate_rust_data
from data_source import read as read_data
from data_model import Catalog
from container_mappings import normalize as normalize_maps, generate_native_example
from graph_corpus import generate as generate_graph_corpus


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "build/metagen-output")
    parser.add_argument("--csharp-json", type=Path, help="use a previously reflected C# DLL projection")
    parser.add_argument("--update", action="store_true", help="update checked-in generated caches")
    args = parser.parse_args()
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    python_source = read(ROOT / "Tools/Metagen/Fixtures/paddle_schema.py")
    python = normalize(python_source["schema"])
    python_projection = args.output / "python-schema.json"
    python_projection.write_text(json.dumps({"source": python_source["source"], "schema": python},
                                           allow_nan=False, indent=2) + "\n")
    projection = args.csharp_json
    if projection is None:
        project = ROOT / "Tools/Metagen/Smoke/Smoke.csproj"
        subprocess.run(["dotnet", "restore", str(project), "--configfile",
                        str(ROOT / "Tools/Metagen/NuGet.Config"), "-p:NuGetAudit=false"], check=True)
        subprocess.run(["dotnet", "run", "--project", str(project), "--no-restore", "--",
                        str(args.output), str(python_projection)], check=True)
        core_tests = ROOT / "Tools/Metagen/CoreTests/CoreTests.csproj"
        subprocess.run(["dotnet", "restore", str(core_tests), "--configfile",
                        str(ROOT / "Tools/Metagen/NuGet.Config"), "-p:NuGetAudit=false"], check=True)
        subprocess.run(["dotnet", "run", "--project", str(core_tests), "--no-restore"], check=True)
        projection = args.output / "csharp-schema.json"
    csharp_source = json.loads(projection.read_text())
    csharp = normalize(csharp_source["schema"])
    if csharp != python:
        raise SchemaError("C# and Python authoritative test schemas disagree")
    data, mappings = read_data(ROOT / "Tools/Metagen/Fixtures/party_schema.py")
    Catalog(data)
    # Rich fixtures are independent declarations, not copies produced from the other provider.
    csharp_data = json.loads((projection.parent / "csharp-data.json").read_text())
    if data != csharp_data:
        raise SchemaError("C# and Python rich data schemas disagree")
    csharp_maps = json.loads((projection.parent / "container-maps.json").read_text())
    source_names = {"System.Collections.Generic.List`1": "list", "System.Collections.Generic.Dictionary`2": "dict"}
    for mapping in csharp_maps:
        mapping.pop("arity", None)
        mapping["source"] = source_names.get(mapping["source"], mapping["source"])
    if normalize_maps(csharp_maps) != normalize_maps(mappings):
        raise SchemaError("Assembly/module container mappings disagree")
    (args.output / "python-data.json").write_text(json.dumps(data, indent=2) + "\n")
    (args.output / "canonical-schema.json").write_text(json.dumps(csharp, indent=2, allow_nan=False) + "\n")
    (args.output / "provenance.json").write_text(json.dumps({
        "csharp": csharp_source["source"], "python": python_source["source"],
        "portable_sha256": fingerprint(csharp), "consumer_version": 1}, indent=2) + "\n")
    generate(csharp, args.output)
    generate_rust(csharp, args.output / "schema.generated.rs")
    generate_native_example(mappings, args.output / "containers.generated.h")
    generate_rust_data(data, args.output / "data.generated.rs")
    generate_graph_corpus(data, args.output)
    checked_in = ROOT / "Tools/Metagen/Generated"
    if args.update:
        generate(csharp, checked_in)
        generate_rust(csharp, checked_in / "schema.generated.rs")
        generate_native_example(mappings, checked_in / "containers.generated.h")
        generate_rust_data(data, checked_in / "data.generated.rs")
        generate_graph_corpus(data, checked_in)
    else:
        for name in ("schema.generated.h", "rttr.generated.cpp", "schema.generated.rs", "containers.generated.h", "data.generated.rs",
                     "graph.generated.h", "graph.corpus.mdb", "graph.catalog.mdb", "graph.party.mdb", "graph.guests.mdb"):
            if not (checked_in / name).exists() or (checked_in / name).read_bytes() != (args.output / name).read_bytes():
                raise SchemaError(f"Stale generated cache: {name}; review and run --update")
    subprocess.run([sys.executable, "-m", "unittest", "discover", "-s",
                    str(ROOT / "Tools/Metagen/Python"), "-p", "test_*.py", "-v"], check=True)
    print("Metagen passed: standalone core/custom generator; C#/Python scalar + rich data + mappings; C++/RTTR/Rust caches")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (SchemaError, OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"Metagen: {error}", file=sys.stderr)
        raise SystemExit(1)
