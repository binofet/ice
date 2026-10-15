"""Passive rich-data example provider. Its policies are outside the core SDK."""
import ast
from pathlib import Path

from metagen_source import SchemaError, keywords, literal, type_ref as scalar_ref


def type_ref(node, names, depth=0):
    if depth > 32:
        raise SchemaError("Type expression depth exceeded")
    if isinstance(node, ast.BinOp) and isinstance(node.op, ast.BitOr):
        if not isinstance(node.right, ast.Constant) or node.right.value is not None:
            raise SchemaError("Only T | None optional expressions are supported")
        result = type_ref(node.left, names, depth + 1)
        if result["nullable"]:
            raise SchemaError("Repeated nullable expression")
        return dict(result, nullable=True)
    if isinstance(node, ast.Name) and node.id in names:
        type_id, kind = names[node.id]
        return {"kind": kind, "nullable": False, "id": type_id}
    if isinstance(node, ast.Subscript) and isinstance(node.value, ast.Name) and node.value.id in ("list", "dict", "set"):
        parts = node.slice.elts if isinstance(node.slice, ast.Tuple) else [node.slice]
        arity = 2 if node.value.id == "dict" else 1
        if len(parts) != arity:
            raise SchemaError("Container argument arity mismatch")
        return {"kind": {"list": "sequence", "dict": "map", "set": "set"}[node.value.id],
                "nullable": False, "arguments": [type_ref(p, names, depth + 1) for p in parts]}
    return scalar_ref(node)


def read(path):
    data = Path(path).read_bytes()
    if len(data) > 1_048_576:
        raise SchemaError("Schema source exceeds its budget")
    try:
        tree = ast.parse(data)
    except (SyntaxError, ValueError, RecursionError) as error:
        raise SchemaError("Invalid schema source") from error
    declarations, mappings, names = [], [], {}
    allowed = {"dataclasses": {"dataclass", "field"}, "enum": {"IntEnum"},
               "typing": {"Annotated"}, "metagen": {"schema", "symbol", "i32", "u32", "f32", "f64"}}
    have_maps = False
    for item in tree.body:
        if isinstance(item, ast.Expr) and isinstance(item.value, ast.Constant) and isinstance(item.value.value, str):
            continue
        if isinstance(item, ast.ImportFrom):
            if item.level or item.module not in allowed or any(n.asname or n.name not in allowed[item.module] for n in item.names):
                raise SchemaError("Unsupported schema import")
            continue
        if isinstance(item, ast.Assign) and len(item.targets) == 1 and isinstance(item.targets[0], ast.Name) and item.targets[0].id == "CONTAINER_MAPS":
            if have_maps:
                raise SchemaError("Repeated CONTAINER_MAPS")
            mappings, have_maps = literal(item.value), True
            continue
        if not isinstance(item, ast.ClassDef) or item.keywords:
            raise SchemaError("Only declarative data classes, enums and module mappings are supported")
        enum = len(item.bases) == 1 and isinstance(item.bases[0], ast.Name) and item.bases[0].id == "IntEnum"
        if item.bases and not enum:
            raise SchemaError("Language inheritance is separate from prototype inheritance")
        declaration = None
        for decorator in item.decorator_list:
            if isinstance(decorator, ast.Name) and decorator.id == "dataclass":
                continue
            if not isinstance(decorator, ast.Call) or not isinstance(decorator.func, ast.Name) or decorator.func.id != "schema" or len(decorator.args) != 1 or declaration:
                raise SchemaError("Unsupported schema decorator")
            declaration = {"id": literal(decorator.args[0]), **keywords(decorator, {"version", "name", "prototype"})}
        if declaration is None or item.name in names:
            raise SchemaError("Missing schema or duplicate class")
        names[item.name] = (declaration["id"], "enum" if enum else "record")
        declarations.append((item, declaration, enum))
    schemas = []
    for item, declaration, enum in declarations:
        fields, symbols = [], []
        for member in item.body:
            if isinstance(member, ast.Pass) or (isinstance(member, ast.Expr) and isinstance(member.value, ast.Constant) and isinstance(member.value.value, str)):
                continue
            if enum:
                if not isinstance(member, ast.Assign) or len(member.targets) != 1 or not isinstance(member.targets[0], ast.Name) or not isinstance(member.value, ast.Call) or not isinstance(member.value.func, ast.Name) or member.value.func.id != "symbol" or len(member.value.args) != 2 or member.value.keywords:
                    raise SchemaError("Enums require name = symbol(stable_id, integer_value)")
                symbols.append({"id": literal(member.value.args[0]), "name": member.targets[0].id, "value": literal(member.value.args[1])})
                continue
            if not isinstance(member, ast.AnnAssign) or not isinstance(member.target, ast.Name) or not isinstance(member.value, ast.Call) or not isinstance(member.value.func, ast.Name) or member.value.func.id != "field" or member.value.args:
                raise SchemaError("Expected annotated field with literal metadata")
            options = keywords(member.value, {"default", "metadata"})
            metadata = options.get("metadata", {})
            if not isinstance(metadata, dict) or set(metadata) - {"id", "storage", "merge", "key"}:
                raise SchemaError("Unknown portable-data field metadata")
            fields.append({"id": metadata.get("id"), "name": member.target.id, "type": type_ref(member.annotation, names),
                           "storage": metadata.get("storage", "inline"), "merge": metadata.get("merge", "replace"),
                           "key": metadata.get("key", ""), "has_default": "default" in options, "default": options.get("default")})
        schemas.append({**declaration, "kind": "enum" if enum else "record", "prototype": declaration.get("prototype", False),
                        "fields": sorted(fields, key=lambda f: f["id"]), "symbols": sorted(symbols, key=lambda s: s["id"])})
    return sorted(schemas, key=lambda s: s["id"]), mappings
