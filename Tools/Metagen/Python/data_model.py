"""Portable-data consumer reference model, deliberately separate from the Metagen core.

This is a bounded conformance implementation for authoring/tooling, not the ICE runtime.
It owns detached data; ordinary references are typed IDs rather than Python object pointers.
"""
import copy
from dataclasses import dataclass
import json
import posixpath
import re
from urllib.parse import unquote, urlsplit, urlunsplit

from metagen_source import SchemaError
from portable_schema import KINDS, identifier, scalar, text

MAX_NODES = 65_536
MAX_DEPTH = 48
MAX_ITEMS = 4096
MAX_TREE_BYTES = 1024 * 1024


def bounded_tree(tree):
    remaining = MAX_NODES
    remaining_bytes = MAX_TREE_BYTES
    def visit(value, depth):
        nonlocal remaining, remaining_bytes
        remaining -= 1
        remaining_bytes -= 8
        if remaining < 0 or depth > MAX_DEPTH or remaining_bytes < 0:
            raise SchemaError("Document node/depth/byte budget exceeded")
        if isinstance(value, dict):
            if len(value) > MAX_ITEMS:
                raise SchemaError("Object item budget exceeded")
            for key, item in value.items():
                text(key, 4096)
                visit(key, depth + 1)
                visit(item, depth + 1)
        elif isinstance(value, list):
            if len(value) > MAX_ITEMS:
                raise SchemaError("Container item budget exceeded")
            for item in value:
                visit(item, depth + 1)
        elif type(value) is str:
            text(value, 4096)
            remaining_bytes -= len(value.encode("utf-8"))
            if remaining_bytes < 0:
                raise SchemaError("Document byte budget exceeded")
        elif value is not None and type(value) not in (bool, int, float):
            raise SchemaError("Expected literal portable data")
        elif type(value) is float:
            scalar(value, "float64")
        elif type(value) is int and not -(2**63) <= value < 2**64:
            raise SchemaError("Integer exceeds the portable document range")
    visit(tree, 0)
    return copy.deepcopy(tree)


def key_token(value):
    # Keys are already validated against one scalar type. Never use Python bool == int equality.
    return json.dumps(value, ensure_ascii=True, allow_nan=False, separators=(",", ":"))


class Catalog:
    def __init__(self, schemas):
        self.schemas = bounded_tree(schemas)
        self.types = {}
        names = set()
        if not isinstance(self.schemas, list) or not 0 < len(self.schemas) <= 128:
            raise SchemaError("Invalid type count")
        for schema in self.schemas:
            if not isinstance(schema, dict) or set(schema) != {"id", "version", "name", "kind", "prototype", "fields", "symbols"}:
                raise SchemaError("Malformed data schema")
            type_id = text(schema["id"], 128)
            if not re.fullmatch(r"[a-z][a-z0-9_.]*", type_id) or type_id in self.types:
                raise SchemaError("Invalid or duplicate type identity")
            if not identifier(schema["name"]) or schema["name"] in names or type(schema["version"]) is not int or not 0 < schema["version"] < 2**32:
                raise SchemaError("Invalid type name/version")
            if schema["kind"] not in ("record", "enum") or type(schema["prototype"]) is not bool:
                raise SchemaError("Invalid type kind/prototype policy")
            names.add(schema["name"])
            self.types[type_id] = schema
        for schema in self.schemas:
            fields, symbols = schema["fields"], schema["symbols"]
            if not isinstance(fields, list) or not isinstance(symbols, list) or max(len(fields), len(symbols)) > 128:
                raise SchemaError("Invalid member budget")
            members = symbols if schema["kind"] == "enum" else fields
            if (schema["kind"] == "enum" and (fields or not symbols or schema["prototype"])) or (schema["kind"] == "record" and symbols):
                raise SchemaError("Inconsistent enum/record shape")
            ids, member_names, values = set(), set(), set()
            for member in members:
                if not isinstance(member, dict) or type(member.get("id")) is not int or not 0 < member["id"] < 2**32 or member["id"] in ids or not isinstance(member.get("name"), str) or not re.fullmatch(r"[a-z][a-z0-9_]*", member["name"]) or member["name"] in member_names:
                    raise SchemaError("Duplicate or malformed member identity")
                ids.add(member["id"])
                member_names.add(member["name"])
                if schema["kind"] == "enum":
                    if set(member) != {"id", "name", "value"}:
                        raise SchemaError("Invalid enum symbol")
                    scalar(member["value"], "int32")
                    if member["value"] in values:
                        raise SchemaError("Enum aliases require an explicit alias policy")
                    values.add(member["value"])
                    continue
                if set(member) != {"id", "name", "type", "storage", "merge", "key", "has_default", "default"} or type(member["has_default"]) is not bool:
                    raise SchemaError("Invalid field definition")
                if not member["has_default"] and member["default"] is not None:
                    raise SchemaError("Absent default carries a value")
                self.check_type(member["type"])
                if member["storage"] not in ("inline", "local", "external", "any"):
                    raise SchemaError("Unknown occurrence storage policy")
                if member["storage"] != "inline" and not self.has_record(member["type"]):
                    raise SchemaError("Reference policy needs an object occurrence")
                kind = member["type"]["kind"]
                policy = member["merge"]
                allowed = {"record": "record", "sequence": "keyed", "map": "map", "set": "set"}
                if policy != "replace" and allowed.get(kind) != policy:
                    raise SchemaError("Merge policy/type mismatch")
                if not isinstance(member["key"], str) or (policy != "keyed" and member["key"]):
                    raise SchemaError("Identity key applies only to keyed sequences")
                if policy == "keyed":
                    leaf = member["type"]["arguments"][0]
                    if leaf["kind"] != "record" or leaf["nullable"] or member["storage"] != "inline":
                        raise SchemaError("Keyed sequences need nonnullable inline record elements")
                    key = self.fields(leaf["id"]).get(member["key"])
                    if not key or not self.is_key(key["type"]) or key["storage"] != "inline":
                        raise SchemaError("Keyed sequence requires a stable nonnullable key field")
        # Default values are checked by the same loader as authored values, after all types exist.
        checker = Loader(self)
        def default_controls(value):
            if isinstance(value, dict):
                if any(k.startswith('$') and k != '$type' for k in value):
                    raise SchemaError("Defaults cannot define identity, references or edit operations")
                for child in value.values():
                    default_controls(child)
            elif isinstance(value, list):
                for child in value:
                    default_controls(child)
        for schema in self.schemas:
            for field in schema["fields"]:
                if field["has_default"]:
                    default_controls(field["default"])
                    checker.value(field["type"], field["default"], "memory://defaults/", field["storage"], 0)

    def fields(self, type_id):
        return {f["name"]: f for f in self.types[type_id]["fields"]}

    def has_record(self, ref):
        return ref["kind"] == "record" or any(self.has_record(a) for a in ref.get("arguments", []))

    def is_key(self, ref):
        return not ref["nullable"] and ref["kind"] in ("string", "int32", "uint32", "enum")

    def check_type(self, ref, depth=0):
        if depth > 32 or not isinstance(ref, dict) or type(ref.get("nullable")) is not bool:
            raise SchemaError("Invalid type expression/depth")
        kind = ref.get("kind")
        properties = {"kind", "nullable"}
        if kind in ("record", "enum"):
            properties.add("id")
            if not isinstance(ref.get("id"), str) or ref["id"] not in self.types or self.types[ref["id"]]["kind"] != kind:
                raise SchemaError("Unresolved or mismatched named type")
        elif kind in ("sequence", "array", "map", "set"):
            properties.add("arguments")
            args = ref.get("arguments")
            if not isinstance(args, list) or len(args) != (2 if kind == "map" else 1):
                raise SchemaError("Container argument arity mismatch")
            for arg in args:
                self.check_type(arg, depth + 1)
            if kind in ("map", "set") and not self.is_key(args[0]):
                raise SchemaError("This profile requires canonical scalar map keys/set elements")
        elif not isinstance(kind, str) or kind not in KINDS:
            raise SchemaError("Unknown logical kind")
        if set(ref) != properties:
            raise SchemaError("Unexpected type-expression properties")


@dataclass(frozen=True)
class Reference:
    document: str
    identity: str
    type_id: str


@dataclass
class Graph:
    root: Reference
    objects: dict
    authored: dict


class _RecordValue(dict):
    """Internal marker: a validated record must not be re-resolved as a fresh definition."""


class Loader:
    def __init__(self, catalog, resolver=None):
        self.catalog = catalog
        self.resolver = resolver
        self.documents = {}
        self.index = {}
        self.effective = {}
        self.active_prototypes = set()
        self.work = 0

    def spend(self, depth):
        self.work += 1
        if depth > MAX_DEPTH or self.work > MAX_NODES * 4:
            raise SchemaError("Graph evaluation budget exceeded")

    def add_document(self, uri, document):
        if len(self.documents) >= 16:
            raise SchemaError("External document budget exceeded")
        document = bounded_tree(document)
        if not isinstance(document, dict) or set(document) != {"schemas", "objects", "root"} or not isinstance(document["objects"], list) or not isinstance(document["schemas"], dict):
            raise SchemaError("Expected schema versions, object table and root reference")
        for type_id, version in document["schemas"].items():
            if type_id not in self.catalog.types or type(version) is not int or version != self.catalog.types[type_id]["version"]:
                raise SchemaError("Document schema version mismatch; migrate explicitly")
        def dependencies(ref):
            if ref["kind"] in ("record", "enum") and ref["id"] not in document["schemas"]:
                raise SchemaError("Missing version for a named type dependency")
            for arg in ref.get("arguments", []):
                dependencies(arg)
        for type_id in document["schemas"]:
            for field in self.catalog.types[type_id]["fields"]:
                dependencies(field["type"])
        # Register before reference resolution so ordinary cross-document cycles are allowed.
        self.documents[uri] = document
        def index(value):
            if isinstance(value, dict):
                if "$id" in value:
                    identity = text(value["$id"], 128)
                    if not identity or "$type" not in value or (uri, identity) in self.index:
                        raise SchemaError("Empty/duplicate object identity or missing type")
                    if not isinstance(value["$type"], str) or value["$type"] not in document["schemas"] or self.catalog.types[value["$type"]]["kind"] != "record":
                        raise SchemaError("Unknown object type")
                    self.index[uri, identity] = value
                for child in value.values():
                    index(child)
            elif isinstance(value, list):
                for child in value:
                    index(child)
        for entry in document["objects"]:
            if not isinstance(entry, dict) or "$id" not in entry:
                raise SchemaError("Object table entries require explicit definitions/identities")
            index(entry)

    def reference(self, raw, current_uri, depth):
        self.spend(depth)
        if not isinstance(raw, dict):
            raise SchemaError("Expected a tagged reference")
        if set(raw) == {"$ref"}:
            uri, identity = current_uri, text(raw["$ref"], 128)
        elif set(raw) == {"$uri"}:
            target = text(raw["$uri"], 4096)
            try:
                parsed = urlsplit(target)
                base = urlsplit(current_uri)
                identity = text(unquote(parsed.fragment, errors="strict"), 128)
            except (ValueError, UnicodeError) as error:
                raise SchemaError("Malformed external URI") from error
            if parsed.scheme:
                uri = urlunsplit((parsed.scheme, parsed.netloc, posixpath.normpath(parsed.path), parsed.query, ""))
            else:
                path = parsed.path or base.path
                if parsed.path and not path.startswith('/'):
                    path = posixpath.join(posixpath.dirname(base.path), path)
                uri = urlunsplit((base.scheme, parsed.netloc or base.netloc, posixpath.normpath(path),
                                  parsed.query if parsed.path or '?' in target else base.query, ""))
            if uri not in self.documents:
                if self.resolver is None:
                    raise SchemaError("No resolver for external object")
                self.add_document(uri, self.resolver(uri))
        else:
            raise SchemaError("Ambiguous or malformed reference")
        if not identity or (uri, identity) not in self.index:
            raise SchemaError("Unresolved object identity")
        return Reference(uri, identity, self.index[uri, identity]["$type"])

    def value(self, ref, raw, uri, storage="inline", depth=0):
        self.spend(depth)
        if raw is None:
            if ref["nullable"]:
                return None
            raise SchemaError("Null is not permitted")
        kind = ref["kind"]
        if kind in KINDS:
            return scalar(raw, kind)
        if kind == "enum":
            symbols = self.catalog.types[ref["id"]]["symbols"]
            if type(raw) is str:
                match = next((s for s in symbols if s["name"] == raw), None)
                if match:
                    return match["value"]
            if type(raw) is int and any(s["value"] == raw for s in symbols):
                return raw
            raise SchemaError("Unknown closed enum value")
        if kind == "record":
            if isinstance(raw, Reference):
                if raw.type_id != ref["id"]:
                    raise SchemaError("Inherited reference type mismatch")
                return raw
            if isinstance(raw, _RecordValue):
                if raw["$type"] != ref["id"] or storage not in ("inline", "any"):
                    raise SchemaError("Materialized inline record occurrence mismatch")
                return copy.deepcopy(raw)
            if isinstance(raw, dict) and ("$ref" in raw or "$uri" in raw):
                form = "local" if "$ref" in raw else "external"
                if storage not in (form, "any"):
                    raise SchemaError("Reference form violates occurrence attribute")
                result = self.reference(raw, uri, depth + 1)
                if result.type_id != ref["id"]:
                    raise SchemaError("Reference target type mismatch")
                return result
            if storage not in ("inline", "any"):
                raise SchemaError("Inline object violates reference-only attribute")
            if not isinstance(raw, dict) or raw.get("$type", ref["id"]) != ref["id"]:
                raise SchemaError("Inline object type mismatch")
            if "$id" in raw:
                return self.object(Reference(uri, raw["$id"], ref["id"]), depth + 1)
            return self.record(raw, ref["id"], uri, depth + 1)
        if not isinstance(raw, list) or len(raw) > MAX_ITEMS:
            raise SchemaError("Expected a bounded container")
        args = ref["arguments"]
        if kind == "map":
            entries = {}
            for pair in raw:
                if not isinstance(pair, list) or len(pair) != 2:
                    raise SchemaError("Map entries are explicit key/value pairs")
                key = self.value(args[0], pair[0], uri, depth=depth + 1)
                token = key_token(key)
                if token in entries:
                    raise SchemaError("Duplicate canonical map key")
                entries[token] = [key, self.value(args[1], pair[1], uri, storage, depth + 1)]
            return [entries[k] for k in sorted(entries)]
        result = [self.value(args[0], item, uri, storage, depth + 1) for item in raw]
        if kind == "set":
            entries = {key_token(item): item for item in result}
            if len(entries) != len(result):
                raise SchemaError("Duplicate set element")
            return [entries[k] for k in sorted(entries)]
        return result

    def object(self, reference, depth=0):
        self.spend(depth)
        address = reference.document, reference.identity
        if address in self.effective:
            return copy.deepcopy(self.effective[address])
        if address in self.active_prototypes:
            raise SchemaError("Prototype/inline containment cycle")
        self.active_prototypes.add(address)
        raw = self.index[address]
        result = self.record(raw, reference.type_id, reference.document, depth + 1)
        self.active_prototypes.remove(address)
        self.effective[address] = result
        return copy.deepcopy(result)

    def record(self, raw, type_id, uri, depth, inherited=None):
        self.spend(depth)
        fields = self.catalog.fields(type_id)
        if raw.get("$type", type_id) != type_id:
            raise SchemaError("Record type mismatch")
        if inherited is not None and "$id" in inherited:
            raise SchemaError("Edit a named inline object's definition; occurrence merging cannot fork its identity")
        if inherited is not None and "$id" in raw:
            raise SchemaError("A record merge cannot introduce a new identity")
        if set(raw) - (set(fields) | {"$id", "$type", "$prototype"}):
            raise SchemaError("Unknown record field/control operation")
        base = inherited or {}
        if "$prototype" in raw:
            if inherited is not None or not self.catalog.types[type_id]["prototype"]:
                raise SchemaError("Prototype inheritance is not allowed here")
            prototype = self.reference(raw["$prototype"], uri, depth + 1)
            if prototype.type_id != type_id:
                raise SchemaError("Prototype type mismatch")
            base = self.object(prototype, depth + 1)
        result = _RecordValue({"$type": type_id})
        if "$id" in raw:
            result["$id"] = raw["$id"]
        for name, field in fields.items():
            if name in raw:
                item = raw[name]
                if isinstance(item, dict) and set(item) == {"$reset"}:
                    if item["$reset"] is not True or not field["has_default"]:
                        raise SchemaError("Reset needs a declared default")
                    item = copy.deepcopy(field["default"])
                elif isinstance(item, dict) and set(item) == {"$replace"}:
                    item = item["$replace"]
                elif isinstance(item, dict) and "$ops" in item:
                    if set(item) != {"$ops"} or name not in base:
                        raise SchemaError("Container edits need an inherited value")
                    item = self.patch(field, base[name], item["$ops"], uri, depth + 1)
                elif field["merge"] == "record" and name in base and isinstance(item, dict) and isinstance(base[name], dict):
                    item = self.record(item, field["type"]["id"], uri, depth + 1, base[name])
                result[name] = self.value(field["type"], item, uri, field["storage"], depth + 1)
            elif name in base:
                result[name] = copy.deepcopy(base[name])
            elif field["has_default"]:
                result[name] = self.value(field["type"], field["default"], uri, field["storage"], depth + 1)
            else:
                raise SchemaError(f"Missing required field: {type_id}.{name}")
            if field["merge"] == "keyed" and result[name] is not None:
                keys = [key_token(v[field["key"]]) for v in result[name]]
                if len(keys) != len(set(keys)):
                    raise SchemaError("Duplicate keyed sequence identity")
        return result

    def patch(self, field, base, operations, uri, depth):
        if not isinstance(operations, list) or len(operations) > 128:
            raise SchemaError("Expected a bounded operation list")
        policy, ref = field["merge"], field["type"]
        result = copy.deepcopy(base)
        if policy not in ("keyed", "map", "set") or result is None:
            raise SchemaError("Container does not permit patch inheritance")
        args = ref["arguments"]
        if policy == "keyed":
            identity = field["key"]
            def find(key):
                for i, value in enumerate(result):
                    if key_token(value[identity]) == key_token(key):
                        return i
                raise SchemaError("Missing keyed element/anchor")
            key_type = self.catalog.fields(args[0]["id"])[identity]["type"]
            def checked_key(raw):
                return self.value(key_type, raw, uri, depth=depth + 1)
            if len({key_token(v[identity]) for v in result}) != len(result):
                raise SchemaError("Duplicate inherited sequence identity")
            for op in operations:
                self.spend(depth)
                if not isinstance(op, dict) or op.get("op") not in ("insert", "update", "remove", "move"):
                    raise SchemaError("Unknown keyed operation")
                kind = op["op"]
                required = {"op", "value"} if kind == "insert" else {"op", "id", "changes"} if kind == "update" else {"op", "id"}
                anchors = set(op) & {"before", "after"}
                if set(op) - required - ({"before", "after"} if kind in ("insert", "move") else set()) or not required <= set(op) or len(anchors) > 1:
                    raise SchemaError("Malformed keyed operation")
                if kind == "insert":
                    element = self.value(args[0], op["value"], uri, depth=depth + 1)
                    key = element[identity]
                    if any(key_token(v[identity]) == key_token(key) for v in result):
                        raise SchemaError("Duplicate sequence insertion")
                else:
                    key = checked_key(op["id"])
                    index = find(key)
                    if kind == "remove":
                        result.pop(index)
                        continue
                    if kind == "update":
                        changes = op["changes"]
                        if not isinstance(changes, dict) or identity in changes or any(k.startswith('$') for k in changes):
                            raise SchemaError("Update cannot mutate identity/control fields")
                        result[index] = self.record(changes, args[0]["id"], uri, depth + 1, result[index])
                        continue
                    element = result.pop(index)
                position = len(result)
                if anchors:
                    anchor = next(iter(anchors))
                    anchor_key = checked_key(op[anchor])
                    if key_token(anchor_key) == key_token(key):
                        raise SchemaError("An element cannot anchor itself")
                    position = find(anchor_key) + (anchor == "after")
                result.insert(position, element)
        else:
            entries = {key_token(pair[0] if policy == "map" else pair): pair for pair in result}
            for op in operations:
                self.spend(depth)
                if not isinstance(op, dict):
                    raise SchemaError("Malformed container operation")
                kind = op.get("op")
                if not isinstance(kind, str):
                    raise SchemaError("An operation name must be text")
                required = {"op", "key", "value"} if policy == "map" and kind == "put" else {"op", "key", "changes"} if policy == "map" and kind == "update" else {"op", "key"}
                if set(op) != required or kind not in ({"put", "update", "remove"} if policy == "map" else {"add", "remove"}):
                    raise SchemaError("Unknown/malformed map or set operation")
                key = self.value(args[0], op["key"], uri, depth=depth + 1)
                token = key_token(key)
                if kind in ("remove", "update") and token not in entries:
                    raise SchemaError("Missing map/set edit target")
                if kind == "remove":
                    del entries[token]
                elif kind == "add":
                    if token in entries:
                        raise SchemaError("Duplicate set insertion")
                    entries[token] = key
                elif kind == "put":
                    entries[token] = [key, self.value(args[1], op["value"], uri, field["storage"], depth + 1)]
                else:
                    if args[1]["kind"] != "record" or field["storage"] != "inline" or not isinstance(op["changes"], dict) or any(k.startswith('$') for k in op["changes"]):
                        raise SchemaError("Map update requires inline records; use put to replace a value")
                    entries[token] = [key, self.record(op["changes"], args[1]["id"], uri, depth + 1, entries[token][1])]
            result = [entries[k] for k in sorted(entries)]
        return result

    def load(self, document, uri="memory://main/party.json"):
        if self.documents:
            raise SchemaError("Loader instances are single-transaction")
        self.add_document(uri, document)
        root = self.reference(self.documents[uri]["root"], uri, 0)
        # A reference can discover another document. Validate the entire discovered object table.
        roots = set()
        while len(self.effective) < len(self.index) or len(roots) < len(self.documents):
            for origin, authored in list(self.documents.items()):
                if origin not in roots:
                    self.reference(authored["root"], origin, 0)
                    roots.add(origin)
            for (origin, identity), raw in list(self.index.items()):
                self.object(Reference(origin, identity, raw["$type"]))
        return Graph(root, copy.deepcopy(self.effective), copy.deepcopy(self.documents))
