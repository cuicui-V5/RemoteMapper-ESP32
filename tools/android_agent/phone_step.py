import argparse
import json
from pathlib import Path
from uuid import uuid4

import httpx


parser = argparse.ArgumentParser()
parser.add_argument("operation", choices=["observe", "app_start", "click", "set_text", "press"])
parser.add_argument("--state-dir", type=Path, required=True)
parser.add_argument("--evidence", type=Path, required=True)
parser.add_argument("--package")
parser.add_argument("--resource-id")
parser.add_argument("--text")
parser.add_argument("--key")
args = parser.parse_args()
token = (args.state_dir / "token").read_text().strip()


def observe(client):
    response = client.get("/v1/observe")
    response.raise_for_status()
    observation = response.json()
    all_nodes = list(observation["nodes"])
    offset = observation["next_offset"]
    while offset is not None:
        response = client.get(f"/v1/observations/{observation['observation_id']}", params={"offset": offset})
        response.raise_for_status()
        page = response.json()
        all_nodes.extend(page["nodes"])
        offset = page["next_offset"]
    observation["nodes"] = all_nodes
    observation["next_offset"] = None
    return observation


with httpx.Client(base_url="http://127.0.0.1:8765", headers={"Authorization": "Bearer " + token}, timeout=90, trust_env=False) as client:
    before = observe(client)
    evidence = {"before": before}
    if args.operation != "observe":
        action = {"operation_id": str(uuid4()), "generation": before["generation"], "observation_id": before["observation_id"], "operation": args.operation}
        if args.package is not None:
            action["package"] = args.package
        if args.key is not None:
            action["key"] = args.key
        if args.text is not None:
            action["text"] = args.text
        if args.resource_id is not None:
            nodes = [node for node in before["nodes"] if node["resource_id"] == args.resource_id]
            if len(nodes) != 1:
                raise ValueError("界面中的 resourceId 需要唯一")
            action["node_id"] = nodes[0]["id"]
        evidence["request"] = action
        args.evidence.write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + "\n")
        response = client.post("/v1/actions", json=action)
        evidence["http_status"] = response.status_code
        evidence["response"] = response.json()
        args.evidence.write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + "\n")
        if response.is_error:
            print(json.dumps(evidence["response"], ensure_ascii=False))
        response.raise_for_status()
        evidence["after"] = observe(client)
    else:
        evidence["after"] = before
    args.evidence.write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + "\n")
    after = evidence["after"]
    print(json.dumps({"operation": args.operation, "result": evidence.get("response"), "app": after["app"], "nodes": [{key: node[key] for key in ("id", "resource_id", "class_name", "text", "description", "clickable")} for node in after["nodes"] if node["text"] or node["description"] or node["class_name"] == "android.widget.EditText"]}, ensure_ascii=False, indent=2))
