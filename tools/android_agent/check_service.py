import argparse
import json
from pathlib import Path

import httpx


parser = argparse.ArgumentParser()
parser.add_argument("--state-dir", type=Path, required=True)
parser.add_argument("--base-url", default="http://127.0.0.1:8765")
args = parser.parse_args()
token = (args.state_dir / "token").read_text().strip()
results = {}

with httpx.Client(base_url=args.base_url, timeout=15, trust_env=False) as client:
    response = client.get("/v1/health")
    assert response.status_code == 401, response.text
    results["unauthenticated"] = response.status_code
    client.headers["Authorization"] = "Bearer " + token
    response = client.get("/v1/health")
    response.raise_for_status()
    results["health"] = response.json()
    assert results["health"]["uiautomator2"] == "3.7.1"
    response = client.get("/v1/devices")
    response.raise_for_status()
    results["devices"] = response.json()["devices"]
    response = client.get("/v1/health", headers={"Origin": "http://localhost"})
    assert response.status_code == 403, response.text
    results["browser_origin"] = response.status_code
    response = client.post("/v1/actions", content=b"x" * 16385)
    assert response.status_code == 413, response.text
    results["oversized_request"] = response.status_code
    response = client.post("/v1/actions", json={})
    assert response.status_code == 422, response.text
    results["invalid_action"] = response.status_code
    response = client.post("/v1/cancel")
    response.raise_for_status()
    first_generation = response.json()["generation"]
    response = client.post("/v1/cancel")
    response.raise_for_status()
    assert response.json()["generation"] != first_generation
    results["cancellation_generation_changed"] = True
    response = client.get("/v1/observe")
    assert response.status_code == 409, response.text
    results["unselected_device"] = response.status_code

print(json.dumps(results, ensure_ascii=False, indent=2))
