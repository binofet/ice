"""Typed container target templates for the example profile, not text-code injection."""
import re
from metagen_source import SchemaError
from format_generated import format_cpp


def parse_template(source, arity):
    if not isinstance(source, str) or len(source) > 1024:
        raise SchemaError("Type template byte budget exceeded")
    tokens = re.findall(r"[a-zA-Z_][a-zA-Z0-9_]*(?:::[a-zA-Z_][a-zA-Z0-9_]*)*|\{[0-9]+\}|[<>,]", source)
    if ''.join(tokens) != re.sub(r'\s', '', source):
        raise SchemaError("Invalid type-template token")
    position, used = 0, set()
    def parse(depth=0):
        nonlocal position
        if depth > 16 or position >= len(tokens):
            raise SchemaError("Incomplete/deep type template")
        token = tokens[position]
        position += 1
        if token.startswith('{'):
            index = int(token[1:-1])
            if index >= arity:
                raise SchemaError("Type placeholder out of range")
            used.add(index)
            return index
        if not re.fullmatch(r'[a-zA-Z_][a-zA-Z0-9_:]*', token):
            raise SchemaError("Expected a target type name")
        args = []
        if position < len(tokens) and tokens[position] == '<':
            position += 1
            args.append(parse(depth + 1))
            while position < len(tokens) and tokens[position] == ',':
                position += 1
                args.append(parse(depth + 1))
            if position >= len(tokens) or tokens[position] != '>':
                raise SchemaError("Unbalanced type template")
            position += 1
        return token, args
    tree = parse()
    if position != len(tokens) or used != set(range(arity)):
        raise SchemaError("Template must use every argument and have no trailing tokens")
    return tree


def render(tree, arguments):
    if type(tree) is int:
        return arguments[tree]
    name, children = tree
    return name + ('<' + ', '.join(render(child, arguments) for child in children) + '>' if children else '')


def normalize(mappings):
    if not isinstance(mappings, list) or len(mappings) > 128:
        raise SchemaError("Expected a bounded mapping list")
    result, seen = [], set()
    for mapping in mappings:
        if not isinstance(mapping, dict) or set(mapping) != {'source', 'shape', 'target', 'include', 'type_template', 'adapter'}:
            raise SchemaError("Malformed container mapping")
        if not isinstance(mapping['source'], str) or not re.fullmatch(r'[a-zA-Z_][a-zA-Z0-9_.`]*', mapping['source']):
            raise SchemaError("Invalid source type identity")
        if mapping['shape'] not in ('sequence', 'map', 'set') or mapping['target'] not in ('cpp20', 'cpp23', 'rust2021'):
            raise SchemaError("Unknown mapping shape/target")
        key = mapping['source'], mapping['target']
        if key in seen:
            raise SchemaError("Ambiguous container mapping")
        seen.add(key)
        include = mapping['include']
        if not isinstance(include, str) or not re.fullmatch(r'[a-zA-Z0-9_./-]+', include) or '..' in include or include.startswith('/'):
            raise SchemaError("Invalid include path")
        if not isinstance(mapping['adapter'], str) or not re.fullmatch(r'[a-zA-Z_][a-zA-Z0-9_]*(?:::[a-zA-Z_][a-zA-Z0-9_]*)*', mapping['adapter']):
            raise SchemaError("Invalid adapter identity")
        parse_template(mapping['type_template'], 2 if mapping['shape'] == 'map' else 1)
        result.append(dict(mapping))
    return sorted(result, key=lambda m: (m['source'], m['target']))


def generate_native_example(mappings, output):
    """A real consumer: target profile changes both storage and adapter in compiled C++."""
    lines = ['// Generated mapping consumer; do not edit.', '#pragma once', '#include "sequence_adapters.h"']
    for index, target in enumerate(('cpp23', 'cpp20')):
        mapping = next(m for m in normalize(mappings) if m['source'] == 'list' and m['target'] == target)
        expression = parse_template(mapping['type_template'], 1)
        leaf = render(expression, ['std::int32_t'])
        nested = render(expression, [leaf])
        adapter = mapping['adapter']
        lines += ['#if __cplusplus > 202002L' if index == 0 else '#else', f'#include <{mapping["include"]}>',
                  'namespace metagen_examples {', f'using mapped_sequence = {leaf};',
                  f'using mapped_nested_sequence = {nested};',
                  f'using mapped_adapter = {adapter}<std::int32_t>;',
                  f'using mapped_nested_adapter = {adapter}<mapped_sequence>;',
                  'using mapped_codec = sequence_codec<mapped_adapter, int32_codec>;',
                  'using mapped_nested_codec = sequence_codec<mapped_nested_adapter, mapped_codec>;',
                  '}']
    lines.append('#endif')
    output.write_text('\n'.join(lines) + '\n')
    format_cpp([output])
