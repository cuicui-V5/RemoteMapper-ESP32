import argparse
import json
from pathlib import Path

import httpx


parser = argparse.ArgumentParser()
parser.add_argument("method", choices=["GET", "POST"])
parser.add_argument("path")
parser.add_argument("--state-dir", type=Path, required=True)
parser.add_argument("--body", type=Path)
parser.add_argument("--output", type=Path)
args = parser.parse_args()
token = (args.state_dir / "token").read_text().strip()
payload = json.loads(args.body.read_text()) if args.body else None
with httpx.Client(base_url="http://127.0.0.1:8765", timeout=90, trust_env=False) as client:
    response = client.request(args.method, args.path, headers={"Authorization": "Bearer " + token}, json=payload)
    response.raise_for_status()
    result = json.dumps(response.json(), ensure_ascii=False, indent=2)
    if args.output:
        args.output.write_text(result + "\n")
    print(result)
