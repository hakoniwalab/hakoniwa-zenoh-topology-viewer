#!/usr/bin/env python3
"""Component-owned CLI skeleton for hakoniwa-zenoh-topology-viewer."""

from __future__ import annotations

import argparse
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "command",
        choices=["doctor", "configure", "build", "test", "install", "smoke"],
    )
    parser.add_argument("--config", type=Path, default=Path("hakoniwa-build.yaml"))
    parser.add_argument("--install-dir", type=Path)
    parser.add_argument("--workspace-dir", type=Path)
    args = parser.parse_args()

    print(f"[TODO] {args.command}")
    print(f"config={args.config}")
    if args.install_dir:
        print(f"install_dir={args.install_dir}")
    if args.workspace_dir:
        print(f"workspace_dir={args.workspace_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
