"""Read the portable Metagen declaration profile without importing/evaluating code."""
import ast
import hashlib
from pathlib import Path


class SchemaError(ValueError):
    pass


def literal(node):
    try:
        return ast.literal_eval(node)
    except (ValueError, TypeError, SyntaxError) as error:
        raise SchemaError("Only literal metadata/default values are portable") from error


def keywords(call, allowed):
    result = {}
    for arg in call.keywords:
        if arg.arg not in allowed or arg.arg in result:
            raise SchemaError(f"Unsupported or duplicate argument: {arg.arg}")
        result[arg.arg] = literal(arg.value)
    return result


def type_ref(node):
    if isinstance(node, ast.BinOp) and isinstance(node.op, ast.BitOr):
        if not isinstance(node.right, ast.Constant) or node.right.value is not None:
            raise SchemaError("Only T | None is supported in the portable scalar profile")
        result = type_ref(node.left)
        if result["nullable"]:
            raise SchemaError("Repeated optional annotation")
        return dict(result, nullable=True)
    if isinstance(node, ast.Name) and node.id in ("bool", "str"):
        return {"kind": {"bool": "boolean", "str": "string"}[node.id], "nullable": False}
    if isinstance(node, ast.Subscript) and isinstance(node.value, ast.Name) and node.value.id == "Annotated":
        parts = node.slice.elts if isinstance(node.slice, ast.Tuple) else []
        if len(parts) == 2 and all(isinstance(p, ast.Name) for p in parts):
            base, spec = (p.id for p in parts)
            kinds = {("float", "f32"): "float32", ("float", "f64"): "float64",
                     ("int", "i32"): "int32", ("int", "u32"): "uint32"}
            if (base, spec) in kinds:
                return {"kind": kinds[base, spec], "nullable": False}
    raise SchemaError("Unsupported type; int/float require an explicit width")


def read(path):
    path = Path(path)
    data = path.read_bytes()
    if len(data) > 1_048_576:
        raise SchemaError("Schema source exceeds one MiB")
    try:
        tree = ast.parse(data, filename=str(path))
    except (SyntaxError, ValueError) as error:
        raise SchemaError(str(error)) from error
    types = []
    allowed_imports = {"dataclasses": {"dataclass", "field"}, "typing": {"Annotated"},
                       "metagen": {"schema", "f32", "f64", "i32", "u32"}}
    for item in tree.body:
        if isinstance(item, ast.ImportFrom):
            if item.level or item.module not in allowed_imports or any(
                    n.asname is not None or n.name not in allowed_imports[item.module] for n in item.names):
                raise SchemaError("Only explicit schema imports without aliases are supported")
            continue
        if isinstance(item, ast.Expr) and isinstance(item.value, ast.Constant) and isinstance(item.value.value, str):
            continue
        if not isinstance(item, ast.ClassDef) or item.bases or item.keywords:
            raise SchemaError("Only declarative schema classes are supported; executable statements are rejected")
        declaration = None
        for decorator in item.decorator_list:
            if isinstance(decorator, ast.Name) and decorator.id == "dataclass":
                continue
            if not isinstance(decorator, ast.Call) or not isinstance(decorator.func, ast.Name) or decorator.func.id != "schema" or declaration:
                raise SchemaError("Unsupported class decorator")
            if len(decorator.args) != 1:
                raise SchemaError("schema requires one literal type ID")
            declaration = {"id": literal(decorator.args[0]), **keywords(decorator, {"version", "name", "tags"})}
        if declaration is None:
            raise SchemaError("Schema class requires @schema")
        fields = []
        for member in item.body:
            if isinstance(member, ast.Pass):
                continue
            if isinstance(member, ast.Expr) and isinstance(member.value, ast.Constant) and isinstance(member.value.value, str):
                continue
            if not isinstance(member, ast.AnnAssign) or not isinstance(member.target, ast.Name) or not isinstance(member.value, ast.Call) or not isinstance(member.value.func, ast.Name) or member.value.func.id != "field" or member.value.args:
                raise SchemaError("Fields require annotations and field(...) with literal metadata/defaults")
            options = keywords(member.value, {"default", "metadata"})
            metadata = options.get("metadata", {})
            if not isinstance(metadata, dict) or set(metadata) - {"id", "range", "tags"}:
                raise SchemaError("Unsupported field metadata")
            bounds = metadata.get("range", [None, None])
            if not isinstance(bounds, (list, tuple)) or len(bounds) != 2:
                raise SchemaError("range requires minimum and maximum")
            fields.append({"id": metadata.get("id"), "name": member.target.id,
                           "type": type_ref(member.annotation), "has_default": "default" in options,
                           "default": options.get("default"), "minimum": bounds[0], "maximum": bounds[1],
                           "tags": metadata.get("tags", {})})
        types.append({**declaration, "fields": fields, "tags": declaration.get("tags", {})})
    return {"source": "python:" + hashlib.sha256(data).hexdigest(), "schema": types}
