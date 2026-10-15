#!/usr/bin/env python3
"""Audit Doxygen coverage of the engine API using the configured compiler commands."""
import argparse
import concurrent.futures
import json
from pathlib import Path
import shlex
import sys
import re
import subprocess
import shutil
from clang import cindex as clang

ROOT = Path(__file__).resolve().parents[2]
ENGINE = ROOT / "engine"
RESOURCE_DIR = None
KINDS = {clang.CursorKind.CLASS_DECL, clang.CursorKind.STRUCT_DECL,
         clang.CursorKind.CLASS_TEMPLATE, clang.CursorKind.ENUM_DECL,
         clang.CursorKind.TYPEDEF_DECL, clang.CursorKind.TYPE_ALIAS_DECL,
         clang.CursorKind.FUNCTION_DECL, clang.CursorKind.FUNCTION_TEMPLATE,
         clang.CursorKind.CXX_METHOD, clang.CursorKind.CONSTRUCTOR,
         clang.CursorKind.DESTRUCTOR, clang.CursorKind.FIELD_DECL,
         clang.CursorKind.VAR_DECL}


def engine_file(file):
    if not file:
        return False
    path = Path(str(file)).resolve()
    return path.is_relative_to(ENGINE) and "3rdparty" not in path.parts


def qualified(cursor):
    names = []
    while cursor and cursor.kind != clang.CursorKind.TRANSLATION_UNIT:
        if cursor.spelling:
            names.append(cursor.spelling)
        cursor = cursor.semantic_parent
    return "::".join(reversed(names))


def scan(record):
    command = shlex.split(record["command"])
    arguments = command[1:]
    position = arguments.index("-o")
    del arguments[position:position + 2]
    arguments.remove("-c")
    arguments.remove(record["file"])
    arguments += ["-Wno-unused-private-field", "-Wno-unused-function", "-Wno-unused-const-variable", "-Wno-unused-variable"]
    if RESOURCE_DIR:
        arguments += ["-resource-dir", RESOURCE_DIR]
    else:
        compiler = shutil.which("clang-18") or shutil.which("clang") or command[0]
        resource = subprocess.run([compiler, "-print-resource-dir"], text=True,
                                  capture_output=True)
        if resource.returncode != 0:
            raise RuntimeError("API audit requires Clang builtin headers: install clang-18 "
                               "or pass --resource-dir pointing at a Clang resource directory")
        arguments += ["-resource-dir", resource.stdout.strip()]
    unit = clang.Index.create().parse(record["file"], args=arguments,
                                     options=clang.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES)
    errors = [str(d) for d in unit.diagnostics if d.severity >= clang.Diagnostic.Error]
    required, documented, contracts = {}, set(), []
    stack = [unit.cursor]
    while stack:
        cursor = stack.pop()
        if cursor.location.file and not engine_file(cursor.location.file):
            continue
        stack.extend(cursor.get_children())
        if cursor.kind not in KINDS or not engine_file(cursor.location.file):
            continue
        key = cursor.get_usr()
        if not key:
            continue
        if cursor.raw_comment:
            documented.add(key)
        path = Path(str(cursor.location.file)).resolve()
        if path.name not in ("api.h", "api.cpp") and cursor.kind in (
                clang.CursorKind.FUNCTION_DECL, clang.CursorKind.CXX_METHOD,
                clang.CursorKind.CONSTRUCTOR, clang.CursorKind.FUNCTION_TEMPLATE):
            tokens = [t.spelling for t in cursor.get_tokens()]
            if not any(tokens[i:i + 2] in (["=", "delete"], ["=", "default"])
                       for i in range(len(tokens) - 1)):
                parameters = list(cursor.get_arguments())
                if cursor.kind == clang.CursorKind.FUNCTION_TEMPLATE:
                    parameters = [p for p in cursor.get_children()
                                  if p.kind == clang.CursorKind.PARM_DECL]
                names = [p.spelling for p in parameters]
                tags = re.findall(r"@param(?:\s*\[[^\]]*\])*\s+(\S+)", cursor.raw_comment or "")
                unknown = [tag for tag in tags if tag != "..." and tag not in names]
                if unknown:
                    errors.append(f"{path.relative_to(ROOT)}:{cursor.location.line}: "
                                  f"Doxygen parameter tags do not name arguments: {', '.join(unknown)}")
                contracts.append({"usr": key, "name": qualified(cursor),
                                  "file": str(path.relative_to(ROOT)),
                                  "line": cursor.location.line,
                                  "arguments": names})
        if "include" not in path.parts or path.name == "api.h":
            continue  # Native GL declarations follow the platform specification.
        owner = cursor
        hidden = False
        while owner and owner.kind != clang.CursorKind.TRANSLATION_UNIT:
            if owner.access_specifier in (clang.AccessSpecifier.PRIVATE,
                                          clang.AccessSpecifier.PROTECTED):
                hidden = True
                break
            owner = owner.semantic_parent
        if hidden:
            continue
        parent = cursor.semantic_parent
        if cursor.kind == clang.CursorKind.VAR_DECL and parent.kind not in (
                clang.CursorKind.NAMESPACE, clang.CursorKind.CLASS_DECL,
                clang.CursorKind.STRUCT_DECL):
            continue
        # Deleted/defaulted lifecycle operations use the owning type's documented contract.
        tokens = [token.spelling for token in cursor.get_tokens()]
        if any(tokens[i:i + 2] in (["=", "delete"], ["=", "default"])
               for i in range(len(tokens) - 1)):
            continue
        required[key] = {"usr": key, "parameters": [{"name": p.spelling, "type": p.type.spelling, "offset": p.location.offset} for p in cursor.get_arguments()], "name": qualified(cursor), "file": str(path.relative_to(ROOT)),
                         "line": cursor.location.line, "kind": cursor.kind.name}
    return required, documented, errors, contracts


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", default="build/desktop-debug")
    parser.add_argument("--resource-dir", help="Clang builtin header resource directory")
    parser.add_argument("--report", help="write a JSON audit report")
    args = parser.parse_args()
    global RESOURCE_DIR
    RESOURCE_DIR = args.resource_dir
    database = json.loads((ROOT / args.build_dir / "compile_commands.json").read_text())
    seen, records = set(), []
    for record in database:
        path = Path(record["file"]).resolve()
        if (path.is_relative_to(ENGINE / "src") or path.name == "public_headers.cpp") and path not in seen:
            records.append(record)
            seen.add(path)
    if not records:
        raise RuntimeError("No engine compiler commands found; configure the desktop preset")
    required, documented, errors, contracts = {}, set(), [], []
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as workers:
        for declarations, comments, failures, arguments in workers.map(scan, records):
            contracts.extend(arguments)
            required.update(declarations)
            documented.update(comments)
            errors.extend(failures)
    missing = sorted((declaration for key, declaration in required.items()
                      if key not in documented), key=lambda d: (d["file"], d["line"]))
    grouped = {}
    for contract in contracts:
        grouped.setdefault(contract["usr"], {})[(contract["file"], contract["line"])] = contract
    naming = []
    for declarations in grouped.values():
        items = list(declarations.values())
        if len({tuple(d["arguments"]) for d in items}) > 1:
            naming.append({"issue": "argument names differ between declarations", "declarations": items})
        for declaration in items:
            if any(not re.fullmatch(r"[a-z][a-z0-9_]*", arg) for arg in declaration["arguments"]):
                naming.append({"issue": "arguments must be named in snake_case", "declarations": [declaration]})
    report = {"naming": naming, "declarations": len(required), "missing": missing, "errors": sorted(set(errors)), "api": list(required.values())}
    if args.report:
        Path(args.report).write_text(json.dumps(report, indent=2) + "\n")
    for failure in report["errors"]:
        print(failure)
    for declaration in missing:
        print(f'{declaration["file"]}:{declaration["line"]}: undocumented {declaration["name"]}')
    for issue in naming:
        declaration = issue["declarations"][0]
        print(f'{declaration["file"]}:{declaration["line"]}: {issue["issue"]}: {declaration["name"]}')
    print(f'Argument naming: {len(naming)} issues')
    print(f'API documentation: {len(required)} declarations; {len(missing)} undocumented')
    return int(bool(missing or errors or naming))


if __name__ == "__main__":
    raise SystemExit(main())
