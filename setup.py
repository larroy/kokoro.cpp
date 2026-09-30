#!/usr/bin/env python3
"""Project setup tool.

    python setup.py configure   # download ONNX Runtime, the model and the voices
    python setup.py build       # run CMake
"""

import hashlib
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request
import zipfile
from pathlib import Path

import click

ROOT = Path(__file__).resolve().parent
ORT_DIR = ROOT / "third_party" / "onnxruntime"
ORT_STAMP = ORT_DIR / ".archive"
MODELS_DIR = ROOT / "models"

ORT_VERSION = "1.23.2"
ORT_URL = f"https://github.com/microsoft/onnxruntime/releases/download/v{ORT_VERSION}"
# (sys.platform, arch) -> (release asset, sha256)
ORT_ARCHIVES = {
    ("linux", "x64"): (
        f"onnxruntime-linux-x64-{ORT_VERSION}.tgz",
        "1fa4dcaef22f6f7d5cd81b28c2800414350c10116f5fdd46a2160082551c5f9b",
    ),
    ("linux", "arm64"): (
        f"onnxruntime-linux-aarch64-{ORT_VERSION}.tgz",
        "7c63c73560ed76b1fac6cff8204ffe34fe180e70d6582b5332ec094810241e5c",
    ),
    ("darwin", "x64"): (
        f"onnxruntime-osx-x86_64-{ORT_VERSION}.tgz",
        "d10359e16347b57d9959f7e80a225a5b4a66ed7d7e007274a15cae86836485a6",
    ),
    ("darwin", "arm64"): (
        f"onnxruntime-osx-arm64-{ORT_VERSION}.tgz",
        "b4d513ab2b26f088c66891dbbc1408166708773d7cc4163de7bdca0e9bbb7856",
    ),
    ("win32", "x64"): (
        f"onnxruntime-win-x64-{ORT_VERSION}.zip",
        "0b38df9af21834e41e73d602d90db5cb06dbd1ca618948b8f1d66d607ac9f3cd",
    ),
    ("win32", "arm64"): (
        f"onnxruntime-win-arm64-{ORT_VERSION}.zip",
        "1cfe88b6435df3b5fb0e9f6bd7d6f5df1e887b6174de7f6e2a47bab956f3f168",
    ),
}

ARTIFACTS_URL = "https://github.com/larroy/kokoro.cpp/releases/download/voices_model_files"
# file name -> sha256
ARTIFACTS = {
    "kokoro-v1.1-zh.onnx": "eefec708cbc7aba8e8129b5c2f7cb92e1fe7d281af1e1dd451592d9ff0714a0d",
    "voices-v1.1-zh.bin": "e678019845e6cfe3b7c34531779396b28f509451b91e6535d5dc09bbf11a4be5",
}

ARCH_ALIASES = {
    "x86_64": "x64",
    "amd64": "x64",
    "aarch64": "arm64",
    "arm64": "arm64",
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def download(url: str, dest: Path, expected_sha256: str) -> None:
    """Download url to dest, verifying the checksum. dest is only created on success."""
    click.echo(f"Downloading {url}")
    tmp = dest.with_name(dest.name + ".part")
    with urllib.request.urlopen(url) as response, tmp.open("wb") as out:
        total = int(response.headers.get("Content-Length") or 0)
        with click.progressbar(length=total, label=dest.name) as bar:
            for chunk in iter(lambda: response.read(1 << 20), b""):
                out.write(chunk)
                bar.update(len(chunk))
    actual = sha256(tmp)
    if actual != expected_sha256:
        tmp.unlink()
        raise click.ClickException(
            f"Checksum mismatch for {url}: expected {expected_sha256}, got {actual}"
        )
    tmp.replace(dest)


def ort_archive() -> tuple:
    arch = ARCH_ALIASES.get(platform.machine().lower())
    key = (sys.platform, arch)
    if key not in ORT_ARCHIVES:
        raise click.ClickException(
            f"No prebuilt ONNX Runtime {ORT_VERSION} for {sys.platform}/{platform.machine()}; "
            "configure CMake with -DONNXRUNTIME_ROOT=/path/to/onnxruntime instead."
        )
    return ORT_ARCHIVES[key]


def strip_top_dir(name: str) -> str:
    """'onnxruntime-linux-x64-1.23.2/lib/x.so' -> 'lib/x.so'."""
    parts = name.split("/", 1)
    return parts[1] if len(parts) == 2 else ""


def extract(archive: Path, dest: Path) -> None:
    """Extract the release archive into dest, dropping its top-level directory and debug symbols."""
    if archive.suffix == ".zip":
        with zipfile.ZipFile(archive) as zf:
            for info in zf.infolist():
                name = strip_top_dir(info.filename)
                if not name or info.is_dir() or name.endswith(".pdb"):
                    continue
                target = dest / name
                target.parent.mkdir(parents=True, exist_ok=True)
                with zf.open(info) as src, target.open("wb") as out:
                    shutil.copyfileobj(src, out)
    else:
        with tarfile.open(archive) as tf:
            members = []
            for member in tf.getmembers():
                member.name = strip_top_dir(member.name)
                if member.name:
                    members.append(member)
            if hasattr(tarfile, "data_filter"):
                tf.extractall(dest, members, filter="data")
            else:
                tf.extractall(dest, members)


def install_onnxruntime(force: bool) -> None:
    name, checksum = ort_archive()
    if not force and ORT_STAMP.is_file() and ORT_STAMP.read_text().strip() == name:
        click.echo(f"ONNX Runtime {ORT_VERSION} already in {ORT_DIR.relative_to(ROOT)}")
        return
    if ORT_DIR.exists():
        shutil.rmtree(ORT_DIR)
    ORT_DIR.mkdir(parents=True)
    with tempfile.TemporaryDirectory() as tmp:
        archive = Path(tmp) / name
        download(f"{ORT_URL}/{name}", archive, checksum)
        extract(archive, ORT_DIR)
    ORT_STAMP.write_text(name + "\n")
    click.echo(f"Installed ONNX Runtime {ORT_VERSION} into {ORT_DIR.relative_to(ROOT)}")


def install_artifacts(force: bool) -> None:
    MODELS_DIR.mkdir(exist_ok=True)
    for name, checksum in ARTIFACTS.items():
        dest = MODELS_DIR / name
        if not force and dest.is_file() and sha256(dest) == checksum:
            click.echo(f"{dest.relative_to(ROOT)} is up to date")
            continue
        download(f"{ARTIFACTS_URL}/{name}", dest, checksum)


def run(cmd: list) -> None:
    click.echo("+ " + " ".join(cmd))
    result = subprocess.run(cmd, cwd=ROOT)
    if result.returncode != 0:
        raise click.ClickException(f"command failed with exit code {result.returncode}")


@click.group()
def cli():
    """Set up and build kokoro.cpp."""


@cli.command()
@click.option("--force", is_flag=True, help="Download again even if the files are up to date.")
def configure(force):
    """Download ONNX Runtime, the Kokoro model and the voices."""
    install_onnxruntime(force)
    install_artifacts(force)


@cli.command()
@click.option("--build-dir", default="build", show_default=True, type=click.Path(file_okay=False),
              help="CMake build directory.")
@click.option("--config", default="Release", show_default=True,
              type=click.Choice(["Debug", "Release", "RelWithDebInfo", "MinSizeRel"]),
              help="Build configuration.")
@click.option("-j", "--jobs", type=click.IntRange(min=1), help="Parallel build jobs.")
@click.argument("cmake_args", nargs=-1, type=click.UNPROCESSED)
def build(build_dir, config, jobs, cmake_args):
    """Configure and build with CMake.

    Extra arguments after `--` are passed to the CMake configure step,
    e.g. `python setup.py build -- -DKOKORO_BUILD_TESTS=OFF`.
    """
    cmake = shutil.which("cmake")
    if cmake is None:
        raise click.ClickException("cmake not found in PATH")
    run([cmake, "-S", str(ROOT), "-B", build_dir, f"-DCMAKE_BUILD_TYPE={config}", *cmake_args])
    build_cmd = [cmake, "--build", build_dir, "--config", config, "--parallel"]
    if jobs:
        build_cmd.append(str(jobs))
    run(build_cmd)


if __name__ == "__main__":
    cli()
