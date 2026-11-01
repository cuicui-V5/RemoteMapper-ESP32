import argparse
import json
import subprocess
import time
from pathlib import Path
from uuid import uuid4

import httpx


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--state-dir", type=Path, required=True)
    parser.add_argument("--adb", type=Path, required=True)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    token = (args.state_dir / "token").read_text().strip()
    evidence = []

    with httpx.Client(base_url="http://127.0.0.1:8765", headers={"Authorization": "Bearer " + token}, trust_env=False, timeout=45) as client:
        def request(method, path, body=None, expected=200):
            response = client.request(method, path, json=body)
            evidence.append({"method": method, "path": path, "status": response.status_code, "result": response.json()})
            args.output.write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + "\n")
            assert response.status_code == expected, response.text
            return response.json()

        def action(observation, key):
            return {"operation_id": str(uuid4()), "generation": observation["generation"], "observation_id": observation["observation_id"], "operation": "press", "key": key}

        request("POST", "/v1/connect", {"serial": args.serial})
        observed = request("GET", "/v1/observe")
        original = action(observed, "home")
        completed = request("POST", "/v1/actions", original)
        replayed = request("POST", "/v1/actions", original)
        assert completed == replayed
        request("POST", "/v1/actions", {**original, "key": "back"}, expected=409)
        observed = request("GET", "/v1/observe")
        cancelled = action(observed, "back")
        request("POST", "/v1/cancel", {})
        request("POST", "/v1/actions", cancelled, expected=409)
        connected = request("POST", "/v1/connect", {"serial": args.serial})
        assert connected["generation"] != observed["generation"]
        observed = request("GET", "/v1/observe")
        disconnected = action(observed, "back")
        subprocess.run([str(args.adb.resolve()), "-s", args.serial, "reconnect"], check=True, capture_output=True, text=True, timeout=10)
        subprocess.run([str(args.adb.resolve()), "-s", args.serial, "wait-for-device"], check=True, timeout=15)
        time.sleep(0.5)
        request("POST", "/v1/actions", disconnected, expected=409)
        connected = request("POST", "/v1/connect", {"serial": args.serial})
        assert connected["generation"] != observed["generation"]
        final = request("GET", "/v1/observe")
        assert final["total_nodes"] > 0
        print(json.dumps({"duplicate_request": "passed", "conflicting_id": "passed", "cancel": "passed", "real_adb_reconnect": "passed", "app": final["app"]}, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
