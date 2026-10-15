"""Independent Rust scalar/ICM1 generator. No dependency on the C++ or RTTR generator."""
import json
from pathlib import Path

from portable_schema import fingerprint

TYPES = {'boolean': 'bool', 'int32': 'i32', 'uint32': 'u32', 'float32': 'f32', 'float64': 'f64', 'string': 'String'}
KINDS = list(TYPES)


def name(source):
    return ''.join(part.capitalize() for part in source.split('_'))


def literal(value, kind):
    if value is None:
        return 'None'
    if kind == 'boolean':
        return str(value).lower()
    if kind == 'string':
        escapes = {'"': '\\"', '\\': '\\\\', '\n': '\\n', '\r': '\\r', '\t': '\\t'}
        return '"' + ''.join(escapes.get(c, '\\u{' + format(ord(c), 'x') + '}' if ord(c) < 32 else c)
                             for c in value) + '".to_owned()'
    return repr(value) + TYPES[kind]


def generate(schemas, output):
    lines = ['// Generated independent Rust consumer; do not edit.',
             '// Schema SHA-256: ' + fingerprint(schemas), 'use crate::wire::{Reader, put_text};',
             'use std::collections::BTreeSet;']
    for schema in schemas:
        native = name(schema['name'])
        lines += ['#[derive(Clone, Debug, PartialEq)]', f'pub struct {native} {{']
        for field in schema['fields']:
            kind = TYPES[field['type']['kind']]
            if field['type']['nullable']:
                kind = f'Option<{kind}>'
            lines.append(f"pub r#{field['name']}: {kind},")
        lines += ['}', f'impl Default for {native} {{', 'fn default() -> Self { Self {']
        for field in schema['fields']:
            value = literal(field['default'], field['type']['kind']) if field['has_default'] else 'Default::default()'
            if field['type']['nullable'] and field['has_default'] and field['default'] is not None:
                value = 'Some(' + value + ')'
            lines.append(f"r#{field['name']}: {value},")
        lines += ['} } }', f'impl {native} {{', "pub fn validate(&self) -> Result<(), &'static str> {"]
        for field in schema['fields']:
            kind = field['type']['kind']
            nullable = field['type']['nullable']
            checks = []
            if kind in ('float32', 'float64'):
                checks.append('!value.is_finite()')
            if kind == 'string':
                checks.append("value.len() > 4096 || value.contains('\\0')")
            for bound, operator in (('minimum', '<'), ('maximum', '>')):
                if field[bound] is not None:
                    checks.append(f"(*value as f64) {operator} {float(field[bound])!r}f64")
            if checks:
                lines.append(f"if let Some(value) = &self.r#{field['name']} {{" if nullable else f"{{ let value = &self.r#{field['name']};")
                lines += [f"if {' || '.join(checks)} {{ return Err(\"Invalid field {field['id']}\"); }}", '}']
        lines += ['Ok(()) }', "pub fn encode(&self) -> Result<Vec<u8>, &'static str> {", 'self.validate()?;',
                  'let mut output = b"ICM1".to_vec();', f'put_text(&mut output, {json.dumps(schema["id"])});',
                  f'output.extend_from_slice(&{schema["version"]}u32.to_le_bytes());',
                  f'output.extend_from_slice(&{len(schema["fields"])}u32.to_le_bytes());']
        for field in schema['fields']:
            kind, nullable = field['type']['kind'], field['type']['nullable']
            lines += [f"output.extend_from_slice(&{field['id']}u32.to_le_bytes());", f'output.push({KINDS.index(kind)});']
            if nullable:
                lines.append(f"if let Some(value) = &self.r#{field['name']} {{ output.push(1);")
            else:
                lines += ['output.push(1);', f"{{ let value = &self.r#{field['name']};"]
            lines.append('put_text(&mut output, value);' if kind == 'string' else 'output.push(u8::from(*value));' if kind == 'boolean' else 'output.extend_from_slice(&value.to_le_bytes());')
            lines.append('} else { output.push(0); }' if nullable else '}')
        lines += ['Ok(output) }', "pub fn decode(bytes: &[u8]) -> Result<Self, &'static str> {",
                  'if bytes.len() > 1024 * 1024 { return Err("Byte budget"); }', 'let mut source = Reader::new(bytes);',
                  'if source.take(4)? != b"ICM1" { return Err("Magic"); }',
                  f'if source.text()? != {json.dumps(schema["id"])} || source.u32()? != {schema["version"]} {{ return Err("Schema"); }}',
                  'let count = source.u32()?;', 'if count > 128 { return Err("Field budget"); }',
                  'let mut seen = BTreeSet::new();', 'let mut result = Self::default();', 'for _ in 0..count {',
                  'let id = source.u32()?;', 'let kind = source.u8()?;', 'let present = source.u8()?;',
                  'if present > 1 || !seen.insert(id) { return Err("Duplicate/presence"); }', 'match id {']
        for field in schema['fields']:
            kind, nullable = field['type']['kind'], field['type']['nullable']
            getter = {'boolean': 'boolean', 'string': 'text'}.get(kind, TYPES[kind])
            lines += [f"{field['id']} => {{", f'if kind != {KINDS.index(kind)} {{ return Err("Field kind"); }}']
            if nullable:
                lines.append(f"result.r#{field['name']} = if present == 1 {{ Some(source.{getter}()?) }} else {{ None }};")
            else:
                lines += ['if present == 0 { return Err("Required value is null"); }', f"result.r#{field['name']} = source.{getter}()?;"]
            lines.append('}')
        lines += ['_ => return Err("Unknown field"),', '} }']
        for field in schema['fields']:
            if not field['has_default']:
                lines.append(f"if !seen.contains(&{field['id']}) {{ return Err(\"Missing required field\"); }}")
        lines += ['if !source.finished() { return Err("Trailing bytes"); }', 'result.validate()?;', 'Ok(result)', '} }']
    Path(output).write_text('\n'.join(lines) + '\n')


def generate_data(schemas, output):
    """Rich type projection: storage/handles are Rust choices, never a CLR memory layout."""
    names = {s['id']: name(s['name']) for s in schemas}
    def target(ref, storage='inline'):
        kind = ref['kind']
        if kind in TYPES:
            result = TYPES[kind]
        elif kind in ('record', 'enum'):
            result = names[ref['id']]
            if kind == 'record' and storage != 'inline':
                result = f'ObjectRef<{result}>'
        else:
            args = ref['arguments']
            result = ('Vec<' + target(args[0], storage) + '>' if kind in ('sequence', 'array') else
                      'std::collections::BTreeSet<' + target(args[0]) + '>' if kind == 'set' else
                      'std::collections::BTreeMap<' + target(args[0]) + ', ' + target(args[1], storage) + '>')
        return f'Option<{result}>' if ref['nullable'] else result
    lines = ['// Generated rich type projection; serialization/graph activation is a separate adapter.',
             '#[derive(Clone, Debug)]', 'pub struct ObjectRef<T> { pub document: String, pub identity: String, marker: std::marker::PhantomData<fn() -> T> }',
             'impl<T> ObjectRef<T> { pub fn new(document: String, identity: String) -> Self { Self { document, identity, marker: std::marker::PhantomData } } }']
    for schema in schemas:
        native = names[schema['id']]
        if schema['kind'] == 'enum':
            lines += ['#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd)]', '#[repr(i32)]', f'pub enum {native} {{']
            lines += [f'{name(s["name"])} = {s["value"]},' for s in schema['symbols']]
            lines += ['}']
        else:
            lines += ['#[derive(Clone, Debug)]', f'pub struct {native} {{']
            lines += [f'pub r#{f["name"]}: {target(f["type"], f["storage"])},' for f in schema['fields']]
            lines += ['}']
        lines += [f'impl {native} {{ pub const TYPE_ID: &\'static str = {json.dumps(schema["id"])}; pub const VERSION: u32 = {schema["version"]}; }}']
    Path(output).write_text('\n'.join(lines) + '\n')
