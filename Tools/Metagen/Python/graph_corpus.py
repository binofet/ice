"""Generate cross-language graph cases from the executable portable-data reference model."""
import copy
import dataclasses
import json
from pathlib import Path

from data_formats import binary_encode
from data_model import Catalog, Loader, Reference
from format_generated import format_cpp
from metagen_source import SchemaError
from portable_schema import fingerprint

URI = 'memory://main/party.json'
GUEST = 'assets://people/players.json?revision=1'


def projection(value):
    if isinstance(value, Reference):
        return {'$resolved': dataclasses.asdict(value)}
    if isinstance(value, dict):
        return {key: projection(item) for key, item in value.items()}
    if isinstance(value, list):
        return [projection(item) for item in value]
    return value


def view(graph):
    return {'root': projection(graph.root), 'objects': [
        {'document': uri, 'identity': identity, 'value': projection(value)}
        for (uri, identity), value in sorted(graph.objects.items())]}


def cases(schemas):
    examples = Path(__file__).resolve().parents[1] / 'Examples/PartyData'
    base = json.loads((examples / 'party.json').read_text())
    guest = json.loads((examples / 'guests.json').read_text())
    result = []
    def add(name, change=None, definitions=None, accepted=True, guest_change=None):
        document = copy.deepcopy(base)
        external = {GUEST: copy.deepcopy(guest)}
        if change:
            change(document)
        if guest_change:
            guest_change(external[GUEST])
        schema = copy.deepcopy(schemas if definitions is None else definitions)
        row = {'name': name, 'schema': schema, 'document': document, 'external': external, 'accept': accepted}
        def read(uri):
            if uri not in external:
                raise SchemaError('Missing fixture URI: ' + uri)
            return external[uri]
        try:
            graph = Loader(Catalog(schema), read).load(document, URI)
        except SchemaError:
            if accepted:
                raise
        else:
            if not accepted:
                raise SchemaError('Negative corpus case was accepted: ' + name)
            row['view'] = view(graph)
        result.append(row)
    add('baseline')
    add('forward-references', lambda d: d['objects'].reverse())
    add('empty-keyed-edit', lambda d: d['objects'][2].update(players={'$ops': []}))
    add('whole-container-replacement', lambda d: d['objects'][2].update(players={'$replace': []}))
    add('reset-null-and-inherit', lambda d: d['objects'].append({
        '$id': 'child', '$type': 'party.player', '$prototype': {'$ref': 'host'},
        'name': {'$reset': True}, 'friend': None, 'scores': {'$replace': []}}))
    add('unicode-canonical-keys', lambda d: (
        d['objects'][1].update(settings=[['☃', []], ['é', [1]], ['😎', [None]], ['\n', []], ['\x7f', []]],
                               tags=['z', '☃', 'é', '😎', '\n', '\x7f']),
        d['objects'][2].pop('settings'), d['objects'][2].pop('tags')))
    add('URI-dot-segments-query', lambda d: d['objects'][1].update(
        guest={'$uri': 'assets://people/sub/../players.json?revision=1#guest'}))
    add('escaped-object-identity', lambda d: d['objects'][1].update(
        guest={'$uri': GUEST + '#guest%20%E2%98%83'}), guest_change=lambda d: (
            d['objects'][0].update({'$id': 'guest ☃'}), d.update(root={'$ref': 'guest ☃'})))
    add('cross-document-reference-cycle', guest_change=lambda d: d['objects'][0].update(
        friend={'$uri': URI + '#host'}))
    def inherited_guest(d):
        d['objects'][0] = {'$id': 'guest', '$type': 'party.player', 'id': 'guest',
                           '$prototype': {'$uri': URI + '#host'}}
    add('cross-document-prototype-scope', guest_change=inherited_guest)
    rich = copy.deepcopy(schemas)
    session = next(t for t in rich if t['id'] == 'party.session')
    player_type = {'kind': 'record', 'nullable': False, 'id': 'party.player'}
    def field(identity, name, value_type, merge):
        return {'id': identity, 'name': name, 'type': value_type, 'storage': 'inline',
                'merge': merge, 'key': '', 'has_default': False, 'default': None}
    session['fields'] += [field(6, 'details', player_type, 'record'), field(7, 'captains', {
        'kind': 'map', 'nullable': False, 'arguments': [{'kind': 'string', 'nullable': False}, player_type]}, 'map')]
    def player(identity):
        return {'$type': 'party.player', 'id': identity, 'scores': [[10], []]}
    def extended(d):
        d['objects'][1].update(details=player('details'), captains=[['red', player('captain')]])
        d['objects'][2].update(details={'name': 'Overridden'}, captains={'$ops': [
            {'op': 'update', 'key': 'red', 'changes': {'name': 'Captain'}}]})
    add('record-and-map-record-merging', extended, rich)
    enums = copy.deepcopy(schemas)
    session = next(t for t in enums if t['id'] == 'party.session')
    enum_type = {'kind': 'enum', 'nullable': False, 'id': 'party.mode'}
    session['fields'][1]['type']['arguments'][0] = enum_type
    session['fields'][2]['type']['arguments'][0] = enum_type
    def enum_keys(d):
        d['objects'][1].update(settings=[['casual', []], [7, [None]]], tags=['casual', 7])
        d['objects'][2].update(settings={'$ops': [{'op': 'put', 'key': 'competitive', 'value': [1]}]},
                               tags={'$ops': [{'op': 'remove', 'key': 0}, {'op': 'add', 'key': 'casual'}]})
    add('typed-enum-map-and-set-keys', enum_keys, enums)
    floats = copy.deepcopy(schemas)
    player_schema = next(t for t in floats if t['id'] == 'party.player')
    for identity, name, kind, default in ((6, 'ratio', 'float32', 2.8), (7, 'precision', 'float64', 1.234567890123)):
        f = field(identity, name, {'kind': kind, 'nullable': False}, 'replace')
        f.update(has_default=True, default=default)
        player_schema['fields'].append(f)
    add('explicit-float-width-defaults', definitions=floats)
    operations = [
        ('missing-remove', [{'op': 'remove', 'id': 'missing'}]),
        ('duplicate-insert', [{'op': 'insert', 'value': player('alice')}]),
        ('missing-anchor', [{'op': 'insert', 'value': player('carol'), 'after': 'missing'}]),
        ('self-anchor', [{'op': 'move', 'id': 'alice', 'before': 'alice'}]),
        ('two-anchors', [{'op': 'move', 'id': 'alice', 'before': 'bob', 'after': 'bob'}]),
        ('identity-update', [{'op': 'update', 'id': 'alice', 'changes': {'id': 'new'}}]),
        ('control-update', [{'op': 'update', 'id': 'alice', 'changes': {'$prototype': {'$ref': 'host'}}}]),
        ('remove-then-update', [{'op': 'remove', 'id': 'alice'}, {'op': 'update', 'id': 'alice', 'changes': {}}]),
    ]
    for name, ops in operations:
        add(name, lambda d, ops=ops: d['objects'][2].update(players={'$ops': ops}), accepted=False)
    invalid_fields = [('players', [player('same'), player('same')]),
                      ('players', [dict(player('a'), scores=[[2**32]])]),
                      ('players', [dict(player('a'), mode=True)]),
                      ('settings', [['a', []], ['a', [1]]]), ('settings', [['a', [True]]]),
                      ('settings', {'a': [1]}), ('tags', ['a', 'a']), ('tags', [None]), ('players', [None])]
    for index, (name, value) in enumerate(invalid_fields):
        add('bad-container-' + str(index), lambda d, n=name, v=value: d['objects'][1].update({n: v}), accepted=False)
    for index, target in enumerate(({'$ref': 'missing'}, {'$ref': 'base'}, {'$ref': 'host', '$uri': 'x#y'},
                                    player('inline'), {'$uri': GUEST + '#guest'})):
        add('bad-occurrence-' + str(index), lambda d, t=target: d['objects'][1].update(host=t), accepted=False)
    add('prototype-cycle', lambda d: d['objects'][1].update({'$prototype': {'$ref': 'game'}}), accepted=False)
    add('cross-document-prototype-cycle', lambda d: d['objects'][0].update({'$prototype': {'$uri': GUEST + '#guest'}}),
        accepted=False, guest_change=inherited_guest)
    add('wrong-prototype-type', lambda d: d['objects'][2].update({'$prototype': {'$ref': 'host'}}), accepted=False)
    add('duplicate-definition', lambda d: d['objects'].append(copy.deepcopy(d['objects'][0])), accepted=False)
    add('unknown-table-entry', lambda d: d['objects'].append(123), accepted=False)
    add('malformed-op-kind', lambda d: d['objects'][2].update(tags={'$ops': [{'op': [], 'key': 'pub'}]}), accepted=False)
    add('missing-map-remove', lambda d: d['objects'][2].update(settings={'$ops': [{'op': 'remove', 'key': 'missing'}]}), accepted=False)
    add('duplicate-set-add', lambda d: d['objects'][2].update(tags={'$ops': [{'op': 'add', 'key': 'pub'}]}), accepted=False)
    add('external-root-validation', accepted=False, guest_change=lambda d: d.update(root={'$ref': 'missing'}))
    add('document-version', lambda d: d['schemas'].update({'party.player': 2}), accepted=False)
    add('missing-version-dependency', lambda d: d['schemas'].pop('party.mode'), accepted=False)
    add('schema-version-is-not-bool', lambda d: d['schemas'].update({'party.player': True}), accepted=False)
    opt_out = copy.deepcopy(schemas)
    next(t for t in opt_out if t['id'] == 'party.session')['prototype'] = False
    add('prototype-opt-in', definitions=opt_out, accepted=False)
    replacement = copy.deepcopy(schemas)
    next(t for t in replacement if t['id'] == 'party.session')['fields'][0].update(merge='replace', key='')
    add('container-merge-opt-in', definitions=replacement, accepted=False)
    add('unknown-record-field', lambda d: d['objects'][0].update(unexpected=True), accepted=False)
    add('primitive-cannot-patch', lambda d: d['objects'].append({
        '$id': 'child', '$type': 'party.player', '$prototype': {'$ref': 'host'}, 'name': {'$ops': []}}), accepted=False)
    return result


def generate(schemas, output):
    rows = cases(schemas)
    digest = bytes.fromhex(fingerprint(schemas))
    output = Path(output)
    (output / 'graph.corpus.mdb').write_bytes(binary_encode(rows, digest))
    examples = Path(__file__).resolve().parents[1] / 'Examples/PartyData'
    for name, tree in [('graph.catalog.mdb', schemas),
                       ('graph.party.mdb', json.loads((examples / 'party.json').read_text())),
                       ('graph.guests.mdb', json.loads((examples / 'guests.json').read_text()))]:
        (output / name).write_bytes(binary_encode(tree, digest))
    header = output / 'graph.generated.h'
    header.write_text('// Generated graph conformance identity; do not edit.\n#pragma once\n'
                      '#include "ice/data/document.h"\nnamespace metagen_examples {\n'
                      'inline constexpr ice::data::schema_fingerprint graph_fingerprint = {' +
                      ', '.join('std::byte{0x' + format(b, '02x') + '}' for b in digest) + '};\n'
                      'inline constexpr unsigned graph_case_count = ' + str(len(rows)) + ';\n}\n')
    format_cpp([header])
