#!/usr/bin/env python3
"""Component-owned build entry point for hakoniwa-zenoh-topology-viewer."""

from __future__ import annotations

import argparse
import json
import platform
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Mapping


DEFAULT_CONFIG: dict[str, Any] = {
    "version": 1,
    "build": {"type": "Release", "dir": "collector/build", "parallel": 0},
    "components": {"collector": True, "web": True},
    "validation": {"tests": True, "smoke": False},
    "paths": {
        "zenohc_root": "",
        "pdu_endpoint_root": "",
        "pdu_registry_root": "",
        "pdu_javascript_root": "",
    },
}


class ConfigError(RuntimeError):
    pass


def _parse_scalar(value: str) -> Any:
    value = value.strip()
    if not value:
        return {}
    if value.lower() == "true":
        return True
    if value.lower() == "false":
        return False
    if value.startswith(("\"", "'")) and len(value) >= 2 and value[-1] == value[0]:
        return value[1:-1]
    try:
        return int(value)
    except ValueError:
        return value


def load_simple_yaml(path: Path) -> dict[str, Any]:
    root: dict[str, Any] = {}
    stack: list[tuple[int, dict[str, Any]]] = [(-1, root)]
    for lineno, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.split("#", 1)[0].rstrip()
        if not line.strip():
            continue
        stripped = line.lstrip(" ")
        indent = len(line) - len(stripped)
        if ":" not in stripped or stripped.startswith("-"):
            raise ConfigError(f"{path}:{lineno}: expected 'key: value'")
        key, value = stripped.split(":", 1)
        while stack and indent <= stack[-1][0]:
            stack.pop()
        if not stack:
            raise ConfigError(f"{path}:{lineno}: invalid indentation")
        parent = stack[-1][1]
        if key in parent:
            raise ConfigError(f"{path}:{lineno}: duplicate key: {key}")
        parsed = _parse_scalar(value)
        parent[key] = parsed
        if isinstance(parsed, dict):
            stack.append((indent, parsed))
    return root


def _merge(defaults: Mapping[str, Any], overrides: Mapping[str, Any], prefix: str = "") -> dict[str, Any]:
    unknown = sorted(set(overrides) - set(defaults))
    if unknown:
        raise ConfigError(f"unknown key(s) under {prefix or 'root'}: {', '.join(unknown)}")
    result: dict[str, Any] = {}
    for key, default in defaults.items():
        value = overrides.get(key, default)
        location = f"{prefix}.{key}" if prefix else key
        if isinstance(default, Mapping):
            if not isinstance(value, Mapping):
                raise ConfigError(f"{location} must be a mapping")
            result[key] = _merge(default, value, location)
        else:
            result[key] = value
    return result


def resolve_config(raw: Mapping[str, Any]) -> dict[str, Any]:
    cfg = _merge(DEFAULT_CONFIG, raw)
    if cfg["version"] != 1:
        raise ConfigError("version must be 1")
    if cfg["build"]["type"] not in {"Debug", "Release", "RelWithDebInfo", "MinSizeRel"}:
        raise ConfigError("build.type is invalid")
    if not isinstance(cfg["build"]["parallel"], int) or cfg["build"]["parallel"] < 0:
        raise ConfigError("build.parallel must be a non-negative integer")
    for section, keys in {"components": ("collector", "web"), "validation": ("tests", "smoke")}.items():
        for key in keys:
            if not isinstance(cfg[section][key], bool):
                raise ConfigError(f"{section}.{key} must be true or false")
    for key, value in cfg["paths"].items():
        if not isinstance(value, str):
            raise ConfigError(f"paths.{key} must be a string")
    return cfg


def _host() -> tuple[str, str]:
    os_name = {"Darwin": "macos", "Linux": "linux", "Windows": "windows"}.get(
        platform.system(), platform.system().lower()
    )
    machine = platform.machine().lower()
    arch = {"x86_64": "x64", "amd64": "x64", "aarch64": "arm64"}.get(machine, machine)
    return os_name, arch


def _resolved_path(value: str, root: Path) -> Path | None:
    if not value:
        return None
    path = Path(value).expanduser()
    return (path if path.is_absolute() else root / path).resolve()


@dataclass
class Context:
    root: Path
    manifest: Path
    cfg: dict[str, Any]
    build_dir: Path
    state_dir: Path
    zenohc_root: Path | None
    endpoint_root: Path | None
    registry_root: Path | None
    javascript_root: Path | None
    os_name: str
    arch: str

    @property
    def web_build_dir(self) -> Path:
        return self.build_dir / "web"

    @property
    def cmake_args(self) -> list[str]:
        prefixes = [str(path) for path in (self.zenohc_root, self.endpoint_root) if path]
        return [
            f"-DCMAKE_BUILD_TYPE={self.cfg['build']['type']}",
            f"-DHAKO_ZENOH_TOPOLOGY_BUILD_TESTS={'ON' if self.cfg['validation']['tests'] else 'OFF'}",
            "-DHAKO_ZENOH_TOPOLOGY_WITH_ZENOH=ON",
            f"-DHAKO_PDU_REGISTRY_ROOT={self.registry_root or ''}",
            f"-DCMAKE_PREFIX_PATH={';'.join(prefixes)}",
        ]


def create_context(manifest: Path, state_dir: Path | None) -> Context:
    root = Path(__file__).resolve().parents[1]
    cfg = resolve_config(load_simple_yaml(manifest))
    build_dir = _resolved_path(cfg["build"]["dir"], root)
    assert build_dir is not None
    os_name, arch = _host()
    return Context(
        root=root,
        manifest=manifest,
        cfg=cfg,
        build_dir=build_dir,
        state_dir=(state_dir.resolve() if state_dir else root / ".hako"),
        zenohc_root=_resolved_path(cfg["paths"]["zenohc_root"], root),
        endpoint_root=_resolved_path(cfg["paths"]["pdu_endpoint_root"], root),
        registry_root=_resolved_path(cfg["paths"]["pdu_registry_root"], root),
        javascript_root=_resolved_path(cfg["paths"]["pdu_javascript_root"], root),
        os_name=os_name,
        arch=arch,
    )


def doctor(ctx: Context) -> list[str]:
    errors: list[str] = []
    for command in ("cmake", "ctest"):
        if not shutil.which(command):
            errors.append(f"{command} was not found on PATH")
    checks = (
        (ctx.zenohc_root, "lib/cmake/zenohc/zenohcConfig.cmake", "zenoh-c install prefix"),
        (ctx.endpoint_root, "lib/cmake/hakoniwa_pdu_endpoint/hakoniwa_pdu_endpointConfig.cmake", "PDU Endpoint install prefix"),
        (ctx.registry_root, "pdu/types/std_msgs/pdu_cpptype_cdr_conv_String.hpp", "PDU Registry source"),
    )
    for root, relative, label in checks:
        if root is None or not (root / relative).is_file():
            errors.append(f"{label} is missing required artifact: {root or 'not configured'}/{relative}")
    if ctx.cfg["components"]["web"]:
        for command in ("node", "npm"):
            if not shutil.which(command):
                errors.append(f"{command} was not found on PATH")
        if ctx.javascript_root is None or not (ctx.javascript_root / "package.json").is_file():
            errors.append("Hakoniwa PDU JavaScript source is missing package.json")
    return errors


def _run(command: list[str], cwd: Path) -> None:
    print(">", subprocess.list2cmdline(command), flush=True)
    subprocess.run(command, cwd=cwd, check=True)


def _yaml(value: Any) -> str:
    if isinstance(value, bool):
        return str(value).lower()
    if isinstance(value, int):
        return str(value)
    return json.dumps(str(value), ensure_ascii=False)


def write_resolved(ctx: Context) -> Path:
    ctx.state_dir.mkdir(parents=True, exist_ok=True)
    path = ctx.state_dir / "resolved-build.yaml"
    lines = [
        "version: 1",
        f"manifest: {_yaml(ctx.manifest)}",
        "platform:",
        f"  os: {_yaml(ctx.os_name)}",
        f"  architecture: {_yaml(ctx.arch)}",
        "build:",
        f"  type: {_yaml(ctx.cfg['build']['type'])}",
        f"  dir: {_yaml(ctx.build_dir)}",
        "components:",
        f"  collector: {_yaml(ctx.cfg['components']['collector'])}",
        f"  web: {_yaml(ctx.cfg['components']['web'])}",
        "resolved_paths:",
        f"  zenohc_root: {_yaml(ctx.zenohc_root or '')}",
        f"  pdu_endpoint_root: {_yaml(ctx.endpoint_root or '')}",
        f"  pdu_registry_root: {_yaml(ctx.registry_root or '')}",
        f"  pdu_javascript_root: {_yaml(ctx.javascript_root or '')}",
    ]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return path


def configure(ctx: Context) -> None:
    if ctx.cfg["components"]["collector"]:
        ctx.build_dir.mkdir(parents=True, exist_ok=True)
        _run(["cmake", "-S", str(ctx.root / "collector"), "-B", str(ctx.build_dir), *ctx.cmake_args], ctx.root)


def _prepare_web(ctx: Context) -> None:
    if ctx.web_build_dir.exists():
        shutil.rmtree(ctx.web_build_dir)
    shutil.copytree(ctx.root / "web", ctx.web_build_dir, ignore=shutil.ignore_patterns("node_modules", "dist"))
    package_path = ctx.web_build_dir / "package.json"
    package = json.loads(package_path.read_text(encoding="utf-8"))
    package["dependencies"]["hakoniwa-pdu-javascript"] = f"file:{ctx.javascript_root}"
    package_path.write_text(json.dumps(package, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def build(ctx: Context) -> None:
    configure(ctx)
    if ctx.cfg["components"]["collector"]:
        command = ["cmake", "--build", str(ctx.build_dir), "--config", ctx.cfg["build"]["type"]]
        if ctx.cfg["build"]["parallel"]:
            command.extend(["--parallel", str(ctx.cfg["build"]["parallel"])])
        _run(command, ctx.root)
    if ctx.cfg["components"]["web"]:
        _prepare_web(ctx)
        _run(["npm", "install"], ctx.web_build_dir)
        _run(["npm", "run", "build"], ctx.web_build_dir)


def test(ctx: Context) -> None:
    if ctx.cfg["components"]["collector"] and ctx.cfg["validation"]["tests"]:
        _run(["ctest", "--test-dir", str(ctx.build_dir), "--output-on-failure"], ctx.root)
    if ctx.cfg["components"]["web"] and ctx.cfg["validation"]["tests"]:
        _run(["npm", "test"], ctx.web_build_dir)


def _git_revision(root: Path) -> str:
    result = subprocess.run(["git", "rev-parse", "HEAD"], cwd=root, text=True, capture_output=True, check=False)
    return result.stdout.strip() if result.returncode == 0 else "unknown"


def _dependency(prefix: Path, component_id: str) -> tuple[str, str]:
    receipt = prefix / "share" / "hakoniwa" / "receipts" / f"{component_id}.yaml"
    version = revision = "unknown"
    section = ""
    if receipt.is_file():
        for raw in receipt.read_text(encoding="utf-8").splitlines():
            if raw and not raw.startswith(" ") and raw.endswith(":"):
                section = raw[:-1]
            elif section == "component" and raw.startswith("  ") and ":" in raw:
                key, value = raw.strip().split(":", 1)
                parsed = _parse_scalar(value)
                if key == "version":
                    version = str(parsed)
                elif key == "source_revision":
                    revision = str(parsed)
    return version, revision


def write_receipt(ctx: Context, prefix: Path) -> Path:
    resolved = Path("share/hakoniwa/receipts/resolved/hakoniwa-zenoh-topology-viewer.yaml")
    (prefix / resolved).parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(ctx.state_dir / "resolved-build.yaml", prefix / resolved)
    candidates = (
        ("bin/hako-zenoh-topology-collector", "executable"),
        ("lib/libhako_zenoh_topology_agent.so", "library"),
        ("include/hako_zenoh_topology_agent.h", "header"),
        ("share/hakoniwa/zenoh-topology-viewer/web/index.html", "web-application"),
        ("share/hakoniwa/zenoh-topology-viewer/config/inventory.json", "config"),
    )
    artifacts = [(path, kind) for path, kind in candidates if (prefix / path).exists()]
    if not artifacts:
        raise ConfigError("no installed Viewer artifacts were found")
    endpoint_version, endpoint_revision = _dependency(prefix, "hakoniwa-pdu-endpoint")
    zenoh_version, zenoh_revision = _dependency(prefix, "zenoh-c")
    lines = [
        "schema_version: 1",
        "component:",
        "  id: hakoniwa-zenoh-topology-viewer",
        "  version: 0.1.0",
        f"  source_revision: {_yaml(_git_revision(ctx.root))}",
        "platform:",
        f"  os: {_yaml(ctx.os_name)}",
        f"  architecture: {_yaml(ctx.arch)}",
        f"  toolchain: {_yaml(platform.python_compiler() or 'unknown')}",
        "install:",
        f"  prefix: {_yaml(prefix)}",
        "capabilities:",
        f"  collector: {_yaml(ctx.cfg['components']['collector'])}",
        f"  topology_agent: {_yaml(ctx.cfg['components']['collector'])}",
        f"  browser_viewer: {_yaml(ctx.cfg['components']['web'])}",
        "  tcp_multiplexer: true",
        "  hakoniwa_core: false",
        "build_limits: {}",
        "dependencies:",
        "  hakoniwa-pdu-endpoint:",
        f"    version: {_yaml(endpoint_version)}",
        f"    source_revision: {_yaml(endpoint_revision)}",
        "    build_limits: {}",
        "  zenoh-c:",
        f"    version: {_yaml(zenoh_version)}",
        f"    source_revision: {_yaml(zenoh_revision)}",
        "    build_limits: {}",
        "artifacts:",
    ]
    for path, kind in artifacts:
        lines.extend([f"  - path: {_yaml(path)}", f"    kind: {kind}"])
    lines.append(f"resolved_manifest: {_yaml(resolved.as_posix())}")
    receipt = prefix / "share/hakoniwa/receipts/hakoniwa-zenoh-topology-viewer.yaml"
    receipt.parent.mkdir(parents=True, exist_ok=True)
    receipt.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return receipt


def install(ctx: Context, prefix: Path) -> None:
    if ctx.cfg["components"]["collector"]:
        _run(["cmake", "--install", str(ctx.build_dir), "--prefix", str(prefix)], ctx.root)
    share = prefix / "share" / "hakoniwa" / "zenoh-topology-viewer"
    if ctx.cfg["components"]["web"]:
        dist = ctx.web_build_dir / "dist"
        if not (dist / "index.html").is_file():
            raise ConfigError("web build is missing; run build first")
        shutil.copytree(dist, share / "web", dirs_exist_ok=True)
    shutil.copytree(ctx.root / "config" / "foundation", share / "config", dirs_exist_ok=True)
    print(f"Component Receipt: {write_receipt(ctx, prefix)}")


def smoke(ctx: Context, prefix: Path) -> None:
    if ctx.cfg["components"]["collector"]:
        _run([str(prefix / "bin" / "hako-zenoh-topology-collector"), "--help"], ctx.root)
    if ctx.cfg["components"]["web"] and not (
        prefix / "share/hakoniwa/zenoh-topology-viewer/web/index.html"
    ).is_file():
        raise ConfigError("installed web application is missing")
    print("Topology Viewer smoke: PASS")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["doctor", "configure", "build", "test", "install", "smoke"])
    parser.add_argument("--config", type=Path)
    parser.add_argument("--install-dir", type=Path)
    parser.add_argument("--state-dir", type=Path)
    args = parser.parse_args(argv)
    root = Path(__file__).resolve().parents[1]
    manifest = (args.config or root / "hakoniwa-build.yaml").expanduser()
    if not manifest.is_absolute():
        manifest = (Path.cwd() / manifest).resolve()
    if not manifest.is_file():
        raise ConfigError(f"build manifest not found: {manifest}")
    ctx = create_context(manifest, args.state_dir)
    errors = doctor(ctx)
    resolved = write_resolved(ctx)
    print(f"Resolved configuration: {resolved}")
    for error in errors:
        print(f"ERROR: {error}", file=sys.stderr)
    if args.command == "doctor":
        return 1 if errors else 0
    if errors:
        raise ConfigError("doctor found blocking prerequisites")
    if args.command == "configure":
        configure(ctx)
    elif args.command == "build":
        build(ctx)
    elif args.command == "test":
        test(ctx)
    else:
        if args.install_dir is None:
            raise ConfigError(f"{args.command} requires --install-dir")
        prefix = args.install_dir.expanduser().resolve()
        if args.command == "install":
            install(ctx, prefix)
        else:
            smoke(ctx, prefix)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ConfigError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(2)
    except subprocess.CalledProcessError as exc:
        raise SystemExit(exc.returncode or 1)
