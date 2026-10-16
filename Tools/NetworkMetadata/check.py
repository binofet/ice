#!/usr/bin/env python3
"""Verify the ICE-owned generated networking proof; no network access or packages required."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
CACHE = HERE / "Generated"
ARTIFACTS = ("network.types.generated.h", "network.generated.h",
             "network.discovery.generated.json", "network.policy.generated.json")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inputs():
    files = sorted([*HERE.glob("*.cs"), *HERE.glob("*.csproj"), HERE / "fixtures.py",
                    HERE / "check.py", HERE / "NuGet.Config", ROOT / ".clang-format",
                    HERE / "native/runtime.h"])
    return {str(p.relative_to(ROOT)): digest(p) for p in files}


def pinned_metagen():
    return subprocess.check_output(["git", "-C", str(ROOT), "ls-tree", "HEAD", "Tools/Metagen"],
                                   text=True).split()[2]


def reference_bytes(policies):
    """Independent Python scalar wire implementation; declaration order comes from policy IDs."""
    result = bytearray()
    examples = {"demo.transform": (7, {1: .125, 2: -.375, 3: 300.}),
                "demo.health": (8, {1: 65535}),
                "demo.fire_event": (9, {1: (bytes([1]) + bytes(15), 11, 42), 2: 100, 3: 0., 4: 1., 5: 0.})}
    by_id = {item["id"]: item for item in policies}
    for type_id in ("demo.transform", "demo.health", "demo.fire_event"):
        policy = by_id[type_id]
        revision, values = examples[type_id]
        result += b"INW1\x01" + bytes.fromhex(policy["fingerprint"])
        result += struct.pack("<IQQH", policy["version"], 0, revision, len(policy["fields"]))
        for field in policy["fields"]:
            value = values[field["id"]]
            codec = field["codec"]
            if codec == "signed16_cm":
                normalized = struct.unpack("<f", struct.pack("<f", value))[0]
                encoded = struct.pack("<h", round(normalized * 100))
            elif codec == "uint16_exact":
                encoded = struct.pack("<H", value)
            elif codec == "uint32_exact":
                encoded = struct.pack("<I", value)
            elif codec == "float32_finite":
                encoded = struct.pack("<f", value)
            elif codec == "entity_v1":
                encoded = value[0] + struct.pack("<QQ", value[1], value[2])
            else:
                raise ValueError(f"Unsupported independent reference codec {codec}")
            result += struct.pack("<IH", field["id"], len(encoded)) + encoded
    return result


def cached():
    manifest = json.loads((CACHE / "manifest.json").read_text())
    if manifest["inputs"] != inputs() or manifest["metagen_commit"] != pinned_metagen():
        raise ValueError("Network proof inputs changed; regenerate with --update")
    for name in ARTIFACTS:
        if digest(CACHE / name) != manifest["artifacts"][name]:
            raise ValueError(f"Changed generated artifact: {name}")
    print("Network metadata cache integrity passed")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cached-only", action="store_true")
    parser.add_argument("--update", action="store_true")
    parser.add_argument("--native-executable", type=Path, help="Also compare native golden bytes with independent Python encoding")
    parser.add_argument("--formatter", type=Path, default=ROOT / "build/quality-tools/bin/clang-format")
    args = parser.parse_args()
    if args.cached_only and args.update:
        parser.error("--cached-only and --update are mutually exclusive")
    if args.cached_only:
        cached()
    else:
        sys.path.insert(0, str(ROOT / "Tools/Metagen/Python"))
        actual = subprocess.check_output(["git", "-C", str(ROOT / "Tools/Metagen"), "rev-parse", "HEAD"], text=True).strip()
        if actual != pinned_metagen():
            raise ValueError("Regeneration must use the committed ICE Metagen submodule revision")
        subprocess.run(["git", "-C", str(ROOT / "Tools/Metagen"), "diff", "--quiet", "HEAD"], check=True)
        from metagen_source import read
        from portable_schema import normalize
        projection = read(HERE / "fixtures.py")
        projection["schema"] = normalize(projection["schema"])
        version = subprocess.check_output([str(args.formatter), "--version"], text=True)
        if "18.1.8" not in version:
            raise ValueError("Regeneration requires clang-format 18.1.8")
        work = ROOT / "build/network-metadata"
        work.mkdir(parents=True, exist_ok=True)
        subprocess.run(["dotnet", "build", str(HERE / "NetworkMetadata.csproj"), "--artifacts-path", str(work / "dotnet"),
                        "--configfile", str(HERE / "NuGet.Config"), "-p:NuGetAudit=false", "-p:UseSharedCompilation=false", "--nologo"], check=True)
        with tempfile.TemporaryDirectory(prefix="proof-", dir=work) as directory:
            temporary = Path(directory)
            projected = temporary / "python.json"
            projected.write_text(json.dumps(projection))
            output = temporary / "generated"
            subprocess.run(["dotnet", str(work / "dotnet/bin/NetworkMetadata/debug/NetworkMetadata.dll"),
                            str(projected), str(output)], check=True)
            for name in ARTIFACTS:
                if name.endswith(".h"):
                    subprocess.run([str(args.formatter), "-i", "--style=file:" + str(ROOT / ".clang-format"), str(output / name)], check=True)
            manifest = {"profile": "ice.network.scalar/1", "generator": "ice.network.scalar/1",
                        "metagen_commit": pinned_metagen(), "formatter": "clang-format 18.1.8",
                        "inputs": inputs(), "artifacts": {name: digest(output / name) for name in ARTIFACTS}}
            (output / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
            if args.update:
                CACHE.mkdir(exist_ok=True)
                for name in (*ARTIFACTS, "manifest.json"):
                    shutil.copyfile(output / name, CACHE / name)
            else:
                for name in (*ARTIFACTS, "manifest.json"):
                    if (output / name).read_bytes() != (CACHE / name).read_bytes():
                        raise ValueError(f"Generated cache differs: {name}")
            print("C#/Python network policy equivalence and deterministic generated cache passed")
    if args.native_executable:
        with tempfile.TemporaryDirectory(prefix="ice-network-golden-") as directory:
            golden = Path(directory) / "native.bin"
            subprocess.run([str(args.native_executable.resolve()), "--golden", str(golden)], check=True)
            expected = reference_bytes(json.loads((CACHE / "network.policy.generated.json").read_text()))
            if golden.read_bytes() != expected:
                raise ValueError("Native/Python full-packet golden bytes disagree")
            print(f"Native/Python canonical wire agreement passed ({len(expected)} bytes)")


if __name__ == "__main__":
    main()
