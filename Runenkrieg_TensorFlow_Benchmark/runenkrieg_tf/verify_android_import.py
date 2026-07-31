"""Verify that the frozen desktop winner was copied unchanged into Android."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--export",
        type=Path,
        default=Path("exports/runenkrieg_frozen_winner.tflite"),
    )
    parser.add_argument(
        "--metadata",
        type=Path,
        default=Path("exports/runenkrieg_frozen_winner.json"),
    )
    parser.add_argument("--android-model", type=Path, required=True)
    parser.add_argument("--android-metadata", type=Path, required=True)
    args = parser.parse_args()

    metadata = json.loads(args.metadata.read_text(encoding="utf-8"))
    android_metadata = json.loads(args.android_metadata.read_text(encoding="utf-8"))
    expected_hash = metadata["sha256"]
    expected_bytes = int(metadata["bytes"])

    checks = {
        "export_sha256": sha256(args.export) == expected_hash,
        "android_sha256": sha256(args.android_model) == expected_hash,
        "export_bytes": args.export.stat().st_size == expected_bytes,
        "android_bytes": args.android_model.stat().st_size == expected_bytes,
        "model_bytes_identical": args.export.read_bytes() == args.android_model.read_bytes(),
        "metadata_identical": metadata == android_metadata,
    }

    result = {
        "status": "PASS" if all(checks.values()) else "FAIL",
        "sha256": expected_hash,
        "bytes": expected_bytes,
        "checks": checks,
    }
    print(json.dumps(result, indent=2))
    if result["status"] != "PASS":
        raise SystemExit(1)


if __name__ == "__main__":
    main()
