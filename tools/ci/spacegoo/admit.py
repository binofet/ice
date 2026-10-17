"""Offline trusted-collector gate. No provider requests or merge side effects."""

import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from contract import DIGEST, admission, bounded_json, digest, need, policy, verify_merge, verify_report
import json


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("job", "merge"))
    parser.add_argument("--policy", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--evidence-sha256", required=True,
                        help="Digest independently recorded by the trusted collector, not the author")
    parser.add_argument("--admission", type=Path)
    args = parser.parse_args(argv)
    try:
        config = policy(bounded_json(args.policy))
        need(DIGEST.fullmatch(args.evidence_sha256)
             and digest(args.evidence) == args.evidence_sha256, "Collected evidence digest differs")
        data = bounded_json(args.evidence)
        if args.mode == "merge":
            need(args.admission is None, "Merge mode consumes its trusted collector snapshot")
            result = verify_merge(config, data)
        else:
            need(args.admission is not None, "Job mode requires the independently sealed admission")
            request = bounded_json(args.admission, 65536)
            admission(request, config, request["repo"], request["id"], digest(args.policy))
            result = verify_report(config, request, data)
        print(json.dumps(result))
        return 0
    except (ValueError, OSError, KeyError, TypeError) as error:
        print(json.dumps({"status": "refused", "diagnostic": str(error)[:2000], "merge_performed": False}))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
