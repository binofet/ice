#!/usr/bin/env python3
"""Standalone portable-data example: load prototypes/references, write formats and codecs."""
import argparse
from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / 'Python'))
from data_source import read
from data_model import Catalog, Loader
from data_formats import CODECS, binary_encode, json_decode, json_encode, xml_encode, pack, unpack
from portable_schema import fingerprint
from metagen_source import SchemaError


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=HERE.parents[1] / 'build/metagen-data-lab')
    parser.add_argument('--all-codecs', action='store_true')
    args = parser.parse_args()
    schemas, _ = read(HERE / 'Fixtures/party_schema.py')
    catalog = Catalog(schemas)
    authored = json_decode((HERE / 'Examples/PartyData/party.json').read_bytes())
    def resolver(uri):
        if uri != 'assets://people/players.json?revision=1':
            raise SchemaError('Example resolver has no resource for ' + uri)
        return json_decode((HERE / 'Examples/PartyData/guests.json').read_bytes())
    graph = Loader(catalog, resolver).load(authored)
    root = graph.objects[graph.root.document, graph.root.identity]
    print('Effective players:', ', '.join(p['name'] + ' (' + p['id'] + ')' for p in root['players']))
    print('External guest:', root['guest'].document + '#' + root['guest'].identity)
    args.output.mkdir(parents=True, exist_ok=True)
    digest = bytes.fromhex(fingerprint(schemas))
    for extension, content in (('json', json_encode(authored)), ('xml', xml_encode(authored)), ('mdb', binary_encode(authored, digest))):
        (args.output / ('party.' + extension)).write_bytes(content)
        for codec in CODECS if args.all_codecs else ('zstd',):
            packed = pack(content, codec)
            assert unpack(packed) == content
            (args.output / ('party.' + extension + '.' + codec + '.mcp')).write_bytes(packed)
    print(f'Authored prototype/edit operations preserved; formats and codecs verified in {args.output}')


if __name__ == '__main__':
    main()
