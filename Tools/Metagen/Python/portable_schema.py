"""Provider-independent validation for the first scalar schema profile."""
import copy
import hashlib
import json
import math
import re
import struct

from metagen_source import SchemaError

KINDS = {"boolean", "int32", "uint32", "float32", "float64", "string"}
# Generated identifiers must be valid in C++, not merely valid Python names.
CPP_RESERVED = set("alignas alignof and and_eq asm bitand bitor auto bool break case catch char char8_t char16_t char32_t class compl concept const consteval constexpr constinit const_cast continue co_await co_return co_yield decltype default delete do double dynamic_cast else enum explicit export extern false float for friend goto if import inline int long module mutable namespace new noexcept not not_eq nullptr operator or or_eq private protected public register reinterpret_cast requires return short signed sizeof static static_assert static_cast struct switch template this thread_local throw true try typedef typeid typename union unsigned using virtual void volatile wchar_t while xor xor_eq".split())


def identifier(value):
    return isinstance(value, str) and re.fullmatch(r"[a-z][a-z0-9_]*", value) and value not in CPP_RESERVED and "__" not in value


def text(value, limit):
    if not isinstance(value, str) or "\0" in value:
        raise SchemaError("Expected NUL-free text")
    try:
        size = len(value.encode("utf-8"))
    except UnicodeEncodeError as error:
        raise SchemaError("Invalid Unicode text") from error
    if size > limit:
        raise SchemaError("Text exceeds its byte budget")
    return value


def tags(values):
    if not isinstance(values, dict) or len(values) > 32:
        raise SchemaError("Expected a bounded metadata map")
    checked = {text(k, 128): text(v, 1024) for k, v in values.items()}
    if any(key.startswith("metagen.") for key in checked):
        raise SchemaError("metagen.* metadata keys are reserved for the portable contract")
    return dict(sorted(checked.items()))


def finite(value):
    try:
        return type(value) in (int, float) and math.isfinite(value)
    except OverflowError:
        return False


def scalar(value, kind, nullable=False):
    if value is None:
        if nullable:
            return None
        raise SchemaError("Null is not permitted")
    if kind == "boolean":
        if type(value) is not bool:
            raise SchemaError("Expected a boolean")
    elif kind in ("int32", "uint32"):
        lo, hi = (-(2**31), 2**31-1) if kind == "int32" else (0, 2**32-1)
        if type(value) is not int or not lo <= value <= hi:
            raise SchemaError("Integer type or range mismatch")
    elif kind in ("float32", "float64"):
        if not finite(value):
            raise SchemaError("Expected a finite numeric value")
        value = float(value)
        if kind == "float32":
            try:
                value = struct.unpack("<f", struct.pack("<f", value))[0]
            except (OverflowError, struct.error) as error:
                raise SchemaError("float32 overflow") from error
            if not math.isfinite(value):
                raise SchemaError("float32 overflow")
    elif kind == "string":
        return text(value, 4096)
    else:
        raise SchemaError("Unsupported scalar kind")
    return value


def normalize(schemas):
    if not isinstance(schemas, list) or not 0 < len(schemas) <= 128:
        raise SchemaError("Expected one to 128 schemas")
    result = copy.deepcopy(schemas)
    type_ids = set()
    names = set()
    for schema in result:
        if not isinstance(schema, dict) or set(schema) != {"id", "version", "name", "fields", "tags"}:
            raise SchemaError("Unknown or missing schema properties")
        type_id = text(schema["id"], 128)
        if not re.fullmatch(r"[a-z][a-z0-9_.]*", type_id) or type_id in type_ids:
            raise SchemaError("Invalid or duplicate type ID")
        if not identifier(schema["name"]) or schema["name"] in names:
            raise SchemaError("Invalid or duplicate native type name")
        type_ids.add(type_id)
        names.add(schema["name"])
        if type(schema["version"]) is not int or not 1 <= schema["version"] <= 2**32-1:
            raise SchemaError("Invalid schema version")
        schema["tags"] = tags(schema["tags"])
        fields = schema["fields"]
        if not isinstance(fields, list) or not 0 < len(fields) <= 128:
            raise SchemaError("Expected one to 128 fields")
        ids, field_names = set(), set()
        for field in fields:
            if not isinstance(field, dict) or set(field) != {"id", "name", "type", "has_default", "default", "minimum", "maximum", "tags"}:
                raise SchemaError("Unknown or missing field properties")
            if type(field["id"]) is not int or not 0 < field["id"] <= 2**32-1 or field["id"] in ids:
                raise SchemaError("Invalid or duplicate field ID")
            if not identifier(field["name"]) or field["name"] in field_names or field["name"] == schema["name"]:
                raise SchemaError("Invalid or duplicate native field name")
            ids.add(field["id"])
            field_names.add(field["name"])
            ref = field["type"]
            if not isinstance(ref, dict) or set(ref) != {"kind", "nullable"} or not isinstance(ref["kind"], str) or ref["kind"] not in KINDS or type(ref["nullable"]) is not bool:
                raise SchemaError("Invalid portable type reference")
            if type(field["has_default"]) is not bool:
                raise SchemaError("has_default must be boolean")
            if not field["has_default"] and field["default"] is not None:
                raise SchemaError("A missing default cannot carry a value")
            for name in ("minimum", "maximum"):
                value = field[name]
                if value is not None:
                    if ref["kind"] in ("boolean", "string") or not finite(value):
                        raise SchemaError("Invalid numeric constraint")
                    field[name] = float(value)
            if field["minimum"] is not None and field["maximum"] is not None and field["minimum"] > field["maximum"]:
                raise SchemaError("Inverted numeric constraint")
            if field["has_default"]:
                value = scalar(field["default"], ref["kind"], ref["nullable"])
                if value is not None and ref["kind"] not in ("boolean", "string"):
                    if (field["minimum"] is not None and value < field["minimum"]) or (field["maximum"] is not None and value > field["maximum"]):
                        raise SchemaError("Default violates numeric constraints")
                field["default"] = value
            field["tags"] = tags(field["tags"])
        schema["fields"].sort(key=lambda f: f["id"])
    symbols = set()
    for schema in result:
        for name in (schema["name"], schema["name"] + "_schema", schema["name"] + "_from_record"):
            if name in symbols or name == "to_record":
                raise SchemaError("Generated native symbol collision")
            symbols.add(name)
    return sorted(result, key=lambda s: s["id"])


def fingerprint(schema):
    return hashlib.sha256(json.dumps(schema, ensure_ascii=True, allow_nan=False,
                                     sort_keys=True, separators=(",", ":")).encode()).hexdigest()
