#!/usr/bin/env python3
"""Project setup tool.

    uv run bootstrap.py configure   # download ONNX Runtime, the model and the voices
    uv run bootstrap.py build-ort   # build ONNX Runtime with CUDA for this GPU
    uv run bootstrap.py build       # run CMake
"""

import ctypes
import hashlib
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request
import zipfile
import os
import re
from pathlib import Path
from dataclasses import dataclass

import click

import voice_tool

ROOT = Path(__file__).resolve().parent
ORT_DIR = ROOT / "third_party" / "onnxruntime"
ORT_STAMP = ORT_DIR / ".archive"
ORT_VERSION = "1.23.2"
ORT_URL = f"https://github.com/microsoft/onnxruntime/releases/download/v{ORT_VERSION}"
MODELS_DIR = ROOT / "models"

@dataclass(frozen=True)
class CudaArchs:
    """GPU code in a CUDA build: SASS per (major, minor) plus PTX that the driver JIT-compiles for newer GPUs."""
    real: tuple[tuple[int, int], ...]
    virtual: tuple[int, int]

    def supports(self, cc: tuple[int, int]) -> bool:
        return cc >= self.virtual or any(cc[0] == major and cc[1] >= minor for major, minor in self.real)


@dataclass(frozen=True)
class OrtArchive:
    name: str
    sha256: str
    cuda_archs: CudaArchs | None = None  # None: CPU-only package


@dataclass(frozen=True)
class Gpu:
    index: int
    name: str
    compute_capability: tuple[int, int]

    @property
    def sm(self) -> str:
        return f"{self.compute_capability[0]}{self.compute_capability[1]}"


@dataclass(frozen=True)
class OrtChoice:
    archive: OrtArchive | None  # None: keep the installed source build
    reason: str


ORT_ARCHIVES = {
    ("linux", "x64"): OrtArchive(
        f"onnxruntime-linux-x64-{ORT_VERSION}.tgz",
        "1fa4dcaef22f6f7d5cd81b28c2800414350c10116f5fdd46a2160082551c5f9b"),
    ("linux", "arm64"): OrtArchive(
        f"onnxruntime-linux-aarch64-{ORT_VERSION}.tgz",
        "7c63c73560ed76b1fac6cff8204ffe34fe180e70d6582b5332ec094810241e5c"),
    ("darwin", "x64"): OrtArchive(
        f"onnxruntime-osx-x86_64-{ORT_VERSION}.tgz",
        "d10359e16347b57d9959f7e80a225a5b4a66ed7d7e007274a15cae86836485a6"),
    ("darwin", "arm64"): OrtArchive(
        f"onnxruntime-osx-arm64-{ORT_VERSION}.tgz",
        "b4d513ab2b26f088c66891dbbc1408166708773d7cc4163de7bdca0e9bbb7856"),
    ("win32", "x64"): OrtArchive(
        f"onnxruntime-win-x64-{ORT_VERSION}.zip",
        "0b38df9af21834e41e73d602d90db5cb06dbd1ca618948b8f1d66d607ac9f3cd"),
    ("win32", "arm64"): OrtArchive(
        f"onnxruntime-win-arm64-{ORT_VERSION}.zip",
        "1cfe88b6435df3b5fb0e9f6bd7d6f5df1e887b6174de7f6e2a47bab956f3f168"),
}

# Kernel architectures of the prebuilt CUDA packages, from upstream v1.23.2:
# tools/ci_build/github/azure-pipelines/stages/nuget-win-cuda-packaging-stage.yml (win32) and
# tools/ci_build/github/linux/build_cuda_c_api_package.sh (linux).
ORT_GPU_ARCHIVES = {
    ("linux", "x64"): OrtArchive(
        f"onnxruntime-linux-x64-gpu-{ORT_VERSION}.tgz",
        "2083e361072a79ce16a90dcd5f5cb3ab92574a82a3ce0ac01e5cfa3158176f53",
        CudaArchs(real=((6, 0), (7, 0), (7, 5), (8, 0)), virtual=(9, 0))),
    ("win32", "x64"): OrtArchive(
        f"onnxruntime-win-x64-gpu-{ORT_VERSION}.zip",
        "e77afdbbc2b8cb6da4e5a50d89841b48c44f3e47dce4fb87b15a2743786d0bb9",
        CudaArchs(real=((7, 5), (8, 6), (8, 9)), virtual=(9, 0))),
}

MIN_DRIVER_CUDA = (12, 8)
SOURCE_STAMP_PREFIX = f"source-{ORT_VERSION}-cuda-"
CUDA_RUNTIME_LIBRARIES = {
    "win32": ("cudart64_12.dll", "cublas64_12.dll", "cublasLt64_12.dll", "curand64_10.dll",
              "cufft64_11.dll", "cudnn64_9.dll"),
    "linux": ("libcudart.so.12", "libcublas.so.12", "libcublasLt.so.12", "libcurand.so.10",
              "libcufft.so.11", "libcudnn.so.9"),
}


@dataclass(frozen=True)
class Artifact:
    name: str  # file name in models/ (or voice name for SPANISH_VOICES)
    url: str
    sha256: str


ARTIFACTS_URL = "https://github.com/larroy/kokoro.cpp/releases/download/voices_model_files"
ARTIFACTS = (
    Artifact("kokoro-v1.1-zh.onnx", f"{ARTIFACTS_URL}/kokoro-v1.1-zh.onnx",
             "eefec708cbc7aba8e8129b5c2f7cb92e1fe7d281af1e1dd451592d9ff0714a0d"),
    Artifact("voices-v1.1-zh.bin", f"{ARTIFACTS_URL}/voices-v1.1-zh.bin",
             "e678019845e6cfe3b7c34531779396b28f509451b91e6535d5dc09bbf11a4be5"),
    Artifact("kokoro-v1.0.onnx",
             "https://github.com/thewh1teagle/kokoro-onnx/releases/download/model-files-v1.0/kokoro-v1.0.onnx",
             "7d5df8ecf7d4b1878015a32686053fd0eebe2bc377234608764cc0ef3636a6c5"),
)
SPANISH_VOICES_URL = ("https://huggingface.co/onnx-community/Kokoro-82M-v1.0-ONNX/resolve/"
                      "1939ad2a8e416c0acfeecc08a694d14ef25f2231/voices")
SPANISH_VOICES = (
    Artifact("ef_dora", f"{SPANISH_VOICES_URL}/ef_dora.bin",
             "f66ec66bd295acb18372e37008533a9a3228483ccd294e7538d5d9294ac9a532"),
    Artifact("em_alex", f"{SPANISH_VOICES_URL}/em_alex.bin",
             "27809e9eafdcbcfff90a3016c697568676531de2a2c39cee29c96c7bd6b83e95"),
    Artifact("em_santa", f"{SPANISH_VOICES_URL}/em_santa.bin",
             "ad43b774e1ca24d05c6161297d8aeb770ac3d29bb95daf516727af5f7d543683"),
)
SPANISH_PACK_NAME = "voices-v1.0-es.bin"  # built locally from SPANISH_VOICES
SPANISH_PACK_SHA256 = "cdaf0ecca101f3738763f6e3a13b63d46137909b8ebcf9de2be4ef2a73014c52"

ORT_GIT_URL = "https://github.com/microsoft/onnxruntime.git"
ORT_SRC_DIR = ROOT / "third_party" / "onnxruntime-src"
MIN_ORT_CMAKE = (3, 28)
CUDNN_HELP = (
    "cuDNN 9 for CUDA 12 not found. Download the archive "
    "(Windows: cudnn-windows-x86_64-9.10.2.21_cuda12-archive.zip, "
    "Linux: cudnn-linux-x86_64-9.10.2.21_cuda12-archive.tar.xz) "
    "from https://developer.download.nvidia.com/compute/cudnn/redist/cudnn/, extract it and pass "
    "--cudnn-home <extracted directory> or set CUDNN_HOME. 9.10.2 still supports sm_50 to sm_70; recent "
    "releases need sm_75 or newer. The installer layout (include\\12.x) is not supported."
)

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


def cpu_archive(key: tuple[str, str | None]) -> OrtArchive:
    arch = key[1]
    if key not in ORT_ARCHIVES:
        raise click.ClickException(
            f"No prebuilt ONNX Runtime {ORT_VERSION} for {key[0]}/{arch or platform.machine()}; "
            "configure CMake with -DONNXRUNTIME_ROOT=/path/to/onnxruntime instead."
        )
    return ORT_ARCHIVES[key]


def run_capture(cmd: list[str]) -> str | None:
    """Runs cmd and returns stdout, or None when the command is missing, times out or fails."""
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
    except (FileNotFoundError, subprocess.TimeoutExpired):
        return None
    return result.stdout if result.returncode == 0 else None


def parse_gpus(text: str) -> tuple[Gpu, ...]:
    gpus = []
    for line in text.splitlines():
        if not line.strip():
            continue
        try:
            index, rest = line.split(",", 1)
            name, cc = rest.rsplit(",", 1)
            match = re.fullmatch(r"(\d+)\.(\d+)", cc.strip())
            if not match:
                continue
            gpus.append(Gpu(int(index.strip()), name.strip(),
                            (int(match.group(1)), int(match.group(2)))))
        except ValueError:
            continue
    return tuple(gpus)


def detect_gpus() -> tuple[Gpu, ...]:
    text = run_capture(["nvidia-smi", "--query-gpu=index,name,compute_cap", "--format=csv,noheader"]) or ""
    return parse_gpus(text)


def detect_driver_cuda_version() -> tuple[int, int] | None:
    match = re.search(r"CUDA Version:\s*(\d+)\.(\d+)", run_capture(["nvidia-smi"]) or "")
    return (int(match.group(1)), int(match.group(2))) if match else None


def platform_key() -> tuple[str, str | None]:
    return (sys.platform, ARCH_ALIASES.get(platform.machine().lower()))


def is_current_source_build() -> bool:
    return ORT_STAMP.is_file() and ORT_STAMP.read_text().strip().startswith(SOURCE_STAMP_PREFIX)


def auto_choice(key: tuple[str, str | None], gpus: tuple[Gpu, ...], driver_cuda: tuple[int, int] | None,
                source_build: bool) -> OrtChoice:
    if source_build:
        return OrtChoice(None, "Keeping the source-built ONNX Runtime "
                                "(pass --ort cpu or --ort gpu to replace it)")
    if not gpus:
        return OrtChoice(cpu_archive(key), "No NVIDIA GPU detected")
    if key not in ORT_GPU_ARCHIVES:
        return OrtChoice(cpu_archive(key), f"No prebuilt CUDA ONNX Runtime for {key[0]}/{key[1]}")
    if driver_cuda is not None and driver_cuda < MIN_DRIVER_CUDA:
        return OrtChoice(cpu_archive(key), f"NVIDIA driver supports CUDA {driver_cuda[0]}.{driver_cuda[1]}; "
                                           "the CUDA build needs 12.8 or newer (update the driver)")
    gpu_archive = ORT_GPU_ARCHIVES[key]
    for gpu in gpus:
        if gpu_archive.cuda_archs.supports(gpu.compute_capability):
            return OrtChoice(gpu_archive, f"Using the CUDA build for {gpu.name} (sm_{gpu.sm})")
    return OrtChoice(cpu_archive(key), f"{gpus[0].name} (sm_{gpus[0].sm}) is not supported by the prebuilt CUDA "
                                       "build; run `uv run bootstrap.py build-ort` to build ONNX Runtime for it")


def choose_ort(mode: str, key: tuple[str, str | None], gpus: tuple[Gpu, ...], driver_cuda: tuple[int, int] | None,
               source_build: bool) -> OrtChoice:
    if mode == "cpu":
        return OrtChoice(cpu_archive(key), "CPU build requested")
    if mode == "gpu":
        if key not in ORT_GPU_ARCHIVES:
            raise click.ClickException(
                f"No prebuilt CUDA ONNX Runtime {ORT_VERSION} for {key[0]}/{key[1] or platform.machine()}")
        return OrtChoice(ORT_GPU_ARCHIVES[key], "CUDA build requested")
    return auto_choice(key, gpus, driver_cuda, source_build)

def strip_top_dir(name: str) -> str:
    """'onnxruntime-linux-x64-1.23.2/lib/x.so' -> 'lib/x.so'."""
    # Strip leading ./ if present
    if name.startswith("./"):
        name = name[2:]
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


def install_onnxruntime(archive: OrtArchive, dest: Path, force: bool) -> None:
    name = archive.name
    checksum = archive.sha256
    stamp = dest / ".archive"
    if not force and stamp.is_file() and stamp.read_text().strip() == name:
        click.echo(f"ONNX Runtime {ORT_VERSION} already in {dest}")
        return
    if dest.exists():
        shutil.rmtree(dest)
    dest.mkdir(parents=True)
    with tempfile.TemporaryDirectory() as tmp:
        archive_path = Path(tmp) / name
        download(f"{ORT_URL}/{name}", archive_path, checksum)
        extract(archive_path, dest)
    stamp.write_text(name + "\n")
    click.echo(f"Installed ONNX Runtime {ORT_VERSION} into {dest}")

def can_load(name: str) -> bool:
    try:
        # winmode=0: legacy search order, which includes PATH, the same way
        # onnxruntime_providers_cuda.dll resolves its dependencies.
        ctypes.CDLL(name, winmode=0)
    except OSError:
        return False
    return True


def locate_cuda_home() -> Path | None:
    for env_name in ("CUDA_HOME", "CUDA_PATH"):
        value = os.environ.get(env_name)
        if value:
            return Path(value)
    nvcc = shutil.which("nvcc")
    if nvcc:
        return Path(nvcc).resolve().parent.parent
    if sys.platform == "win32":
        roots = Path(r"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA")
        candidates = sorted(roots.glob("v12.*"), key=lambda p: tuple(int(x) for x in p.name[1:].split(".")))
        return candidates[-1] if candidates else None
    cuda = Path("/usr/local/cuda")
    return cuda if cuda.is_dir() else None


def report_cuda_libraries() -> None:
    missing = [n for n in CUDA_RUNTIME_LIBRARIES.get(sys.platform, ()) if not can_load(n)]
    if not missing:
        click.echo("CUDA runtime libraries found")
        return
    var = "PATH" if sys.platform == "win32" else "LD_LIBRARY_PATH"
    click.secho(f"Warning: CUDA libraries not loadable: {', '.join(missing)}.", fg="yellow", err=True)
    click.secho("Install the CUDA 12 runtime (12.8 or newer) and cuDNN 9 for CUDA 12 and add their library "
                f"directories to {var}; until then kokoro runs on the CPU.", fg="yellow", err=True)
    home = locate_cuda_home()
    if home:
        lib_dir = home / ("bin" if sys.platform == "win32" else "lib64")
        click.secho(f"  CUDA toolkit found at {home}: add {lib_dir} to {var}.", fg="yellow", err=True)


def find_cmake() -> str:
    cmake = shutil.which("cmake")
    if cmake is None:
        raise click.ClickException("cmake not found in PATH")
    return cmake


def install_artifacts(force: bool) -> None:
    MODELS_DIR.mkdir(exist_ok=True)
    for artifact in ARTIFACTS:
        dest = MODELS_DIR / artifact.name
        if not force and dest.is_file() and sha256(dest) == artifact.sha256:
            click.echo(f"{dest.relative_to(ROOT)} is up to date")
            continue
        download(artifact.url, dest, artifact.sha256)
    install_spanish_voices(force)


def install_spanish_voices(force: bool) -> None:
    """Build the Spanish voices file from the onnx-community raw float32 voices."""
    dest = MODELS_DIR / SPANISH_PACK_NAME
    if not force and dest.is_file() and sha256(dest) == SPANISH_PACK_SHA256:
        click.echo(f"{dest.relative_to(ROOT)} is up to date")
        return
    with tempfile.TemporaryDirectory() as tmp_dir:
        tmp = Path(tmp_dir)
        for voice in SPANISH_VOICES:
            download(voice.url, tmp / f"{voice.name}.bin", voice.sha256)
        voices = {v.name: voice_tool.load_raw(tmp / f"{v.name}.bin") for v in SPANISH_VOICES}
    part = dest.with_name(dest.name + ".part")
    voice_tool.save_voices(part, voices)
    actual = sha256(part)
    if actual != SPANISH_PACK_SHA256:
        part.unlink()
        raise click.ClickException(f"Checksum mismatch for {dest.name}: expected {SPANISH_PACK_SHA256}, got {actual}")
    part.replace(dest)
    click.echo(f"Wrote {dest.relative_to(ROOT)}")


def cmake_version(cmake: str) -> tuple[int, int] | None:
    match = re.search(r"cmake version (\d+)\.(\d+)", run_capture([cmake, "--version"]) or "")
    return (int(match.group(1)), int(match.group(2))) if match else None


def require_cmake_for_ort() -> str:
    cmake = find_cmake()
    version = cmake_version(cmake)
    if version is None or version < MIN_ORT_CMAKE:
        found = f"{version[0]}.{version[1]}" if version else "unknown"
        raise click.ClickException(f"Building ONNX Runtime needs CMake 3.28 or newer (found {found})")
    return cmake


def find_cuda_home(option: Path | None) -> Path:
    home = option or locate_cuda_home()
    nvcc_name = "nvcc.exe" if sys.platform == "win32" else "nvcc"
    nvcc = home / "bin" / nvcc_name if home else None
    if home is None or nvcc is None or not nvcc.is_file():
        raise click.ClickException("CUDA 12 toolkit not found; pass --cuda-home or set CUDA_PATH")
    version_text = run_capture([str(nvcc), "--version"]) or ""
    match = re.search(r"release (\d+)\.(\d+)", version_text)
    ver = f"{match.group(1)}.{match.group(2)}" if match else "unknown"
    if not match or int(match.group(1)) != 12:
        raise click.ClickException(
            f"CUDA 12 toolkit required (cuDNN 9 for CUDA 12, ONNX Runtime {ORT_VERSION}); found {ver} at {home}")
    click.echo(f"CUDA toolkit {match.group(1)}.{match.group(2)} at {home}")
    return home


def cudnn_major(home: Path) -> int | None:
    header = home / "include" / "cudnn_version.h"
    if not header.is_file():
        return None
    match = re.search(r"#define CUDNN_MAJOR\s+(\d+)", header.read_text())
    return int(match.group(1)) if match else None


def find_cudnn_home(option: Path | None) -> Path:
    home = option
    if home is None:
        for env_name in ("CUDNN_HOME", "CUDNN_PATH"):
            value = os.environ.get(env_name)
            if value:
                home = Path(value)
                break
    if home is None or cudnn_major(home) != 9:
        raise click.ClickException(CUDNN_HELP)
    return home


def default_cuda_arch(gpus: tuple[Gpu, ...]) -> str:
    if not gpus:
        raise click.ClickException("No NVIDIA GPU detected; pass --cuda-arch, e.g. --cuda-arch 70")
    return ";".join(sorted({gpu.sm for gpu in gpus}))


def checkout_onnxruntime(src_dir: Path) -> None:
    tag = f"v{ORT_VERSION}"
    if (src_dir / ".git").exists():
        describe = (run_capture(["git", "-C", str(src_dir), "describe", "--tags", "--exact-match"]) or "").strip()
        if describe != tag:
            raise click.ClickException(f"{src_dir} is not an onnxruntime {tag} checkout; delete it or pass --src-dir")
        return
    if src_dir.exists() and any(src_dir.iterdir()):
        raise click.ClickException(f"{src_dir} exists and is not a git checkout; delete it or pass --src-dir")
    if shutil.which("git") is None:
        raise click.ClickException("git not found in PATH")
    run(["git", "clone", "--depth", "1", "--branch", tag, "--recurse-submodules", "--shallow-submodules",
         ORT_GIT_URL, str(src_dir)])


def ort_build_command(cmake: str, src_dir: Path, build_dir: Path, cuda_home: Path, cudnn_home: Path,
                      arch: str, jobs: int | None) -> list[str]:
    command = [
        sys.executable, str(src_dir / "tools" / "ci_build" / "build.py"),
        "--build_dir", str(build_dir), "--config", "Release", "--update", "--build",
        "--build_shared_lib", "--skip_tests", "--skip_submodule_sync", "--compile_no_warning_as_error",
        "--cmake_path", cmake, "--use_cuda", "--cuda_home", str(cuda_home), "--cudnn_home", str(cudnn_home),
        "--parallel", *([str(jobs)] if jobs else []),
        "--cmake_extra_defines", f"CMAKE_CUDA_ARCHITECTURES={arch}", "onnxruntime_BUILD_UNIT_TESTS=OFF",
        "onnxruntime_USE_FLASH_ATTENTION=OFF", "onnxruntime_USE_MEMORY_EFFICIENT_ATTENTION=OFF",
        "CMAKE_POLICY_VERSION_MINIMUM=3.5",
    ]
    if shutil.which("ninja") and (sys.platform != "win32" or "VCINSTALLDIR" in os.environ):
        command += ["--cmake_generator", "Ninja"]
    return command


def install_built_onnxruntime(cmake: str, build_dir: Path) -> None:
    if ORT_DIR.exists():
        shutil.rmtree(ORT_DIR)
    run([cmake, "--install", str(build_dir / "Release"), "--config", "Release", "--prefix", str(ORT_DIR)])


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
@click.option("--ort", "ort_mode", type=click.Choice(["auto", "cpu", "gpu"]), default="auto", show_default=True,
              help="ONNX Runtime package; auto uses the CUDA build when a supported NVIDIA GPU is found.")
def configure(force, ort_mode):
    """Download ONNX Runtime, the Kokoro models and the voices."""
    gpus = detect_gpus()
    if gpus:
        for gpu in gpus:
            click.echo(f"GPU {gpu.index}: {gpu.name} (sm_{gpu.sm})")
    else:
        click.echo("No NVIDIA GPU detected")
    driver = detect_driver_cuda_version() if gpus else None
    if driver is not None:
        click.echo(f"NVIDIA driver CUDA version: {driver[0]}.{driver[1]}")
    choice = choose_ort(ort_mode, platform_key(), gpus, driver, is_current_source_build())
    click.echo(choice.reason)
    if choice.archive is not None:
        install_onnxruntime(choice.archive, ORT_DIR, force)
    if choice.archive is None or choice.archive.cuda_archs is not None:
        report_cuda_libraries()
    install_artifacts(force)


@cli.command("fetch-ort")
@click.option("--platform", "platform_name", type=click.Choice(["win32", "linux", "darwin"]), required=True)
@click.option("--arch", type=click.Choice(["x64", "arm64"]), required=True)
@click.option("--gpu", is_flag=True, help="CUDA package instead of the CPU one.")
@click.option("--dest", type=click.Path(file_okay=False, path_type=Path), required=True,
              help="Directory to extract into (replaced unless it already holds this archive).")
@click.option("--force", is_flag=True, help="Download again even if DEST is up to date.")
def fetch_ort(platform_name: str, arch: str, gpu: bool, dest: Path, force: bool) -> None:
    """Download a pinned ONNX Runtime package into DEST, e.g. for cross builds and packaging."""
    choice = choose_ort("gpu" if gpu else "cpu", (platform_name, arch), (), None, False)
    install_onnxruntime(choice.archive, dest.resolve(), force)


@cli.command()
@click.option("--build-dir", default="build", show_default=True, type=click.Path(file_okay=False),
              help="CMake build directory.")
@click.option("--config", default="Release", show_default=True,
              type=click.Choice(["Debug", "Release", "RelWithDebInfo", "MinSizeRel"]),
              help="Build configuration.")
@click.option("-j", "--jobs", type=click.IntRange(min=1), help="Parallel build jobs.")
@click.argument("cmake_args", nargs=-1, type=click.UNPROCESSED)
def build(build_dir, config, jobs, cmake_args):
    """Configure and build with CMake, using the Ninja generator if available.

    Extra arguments after `--` are passed to the CMake configure step,
    e.g. `uv run bootstrap.py build -- -DKOKORO_BUILD_TESTS=OFF`.
    """
    cmake = find_cmake()
    generator = []
    if not any(arg.startswith("-G") for arg in cmake_args):
        if shutil.which("ninja"):
            generator = ["-G", "Ninja"]
        else:
            click.secho("Warning: ninja not found in PATH, using the default CMake generator.",
                        fg="yellow", err=True)
    run([cmake, "-S", str(ROOT), "-B", build_dir, *generator, f"-DCMAKE_BUILD_TYPE={config}",
         *cmake_args])
    build_cmd = [cmake, "--build", build_dir, "--config", config, "--parallel"]
    if jobs:
        build_cmd.append(str(jobs))
    run(build_cmd)


@cli.command("build-ort")
@click.option("--cuda-arch", "cuda_arch", help='CUDA architectures, e.g. 70 or "75;86" (default: the detected GPUs)')
@click.option("--cuda-home", "cuda_home_opt", type=click.Path(file_okay=False, path_type=Path),
              help="CUDA toolkit directory (default: CUDA_HOME, CUDA_PATH or the installed toolkit).")
@click.option("--cudnn-home", "cudnn_home_opt", type=click.Path(file_okay=False, path_type=Path),
              help="cuDNN 9 for CUDA 12 archive directory (default: CUDNN_HOME or CUDNN_PATH).")
@click.option("--src-dir", "src_dir", default=ORT_SRC_DIR, show_default=True, type=click.Path(file_okay=False,
              path_type=Path), help="ONNX Runtime source checkout.")
@click.option("--build-dir", "build_dir", default=None, type=click.Path(file_okay=False, path_type=Path),
              help="ONNX Runtime build directory (default: <src-dir>/build).")
@click.option("-j", "--jobs", type=click.IntRange(min=1), help="Parallel build jobs.")
def build_ort(cuda_arch, cuda_home_opt, cudnn_home_opt, src_dir, build_dir, jobs):
    """Build ONNX Runtime with CUDA from source, for GPUs the prebuilt CUDA package does not cover."""
    arch = cuda_arch or default_cuda_arch(detect_gpus())
    if not re.fullmatch(r"\d{2,3}(;\d{2,3})*", arch):
        raise click.ClickException(f"invalid --cuda-arch '{arch}'")
    cmake = require_cmake_for_ort()
    cuda_home = find_cuda_home(cuda_home_opt)
    cudnn_home = find_cudnn_home(cudnn_home_opt)
    build_dir = build_dir or src_dir / "build"
    checkout_onnxruntime(src_dir)
    run(ort_build_command(cmake, src_dir, build_dir, cuda_home, cudnn_home, arch, jobs))
    install_built_onnxruntime(cmake, build_dir)
    ORT_STAMP.write_text(f"{SOURCE_STAMP_PREFIX}{arch}\n")
    click.echo(f"Installed ONNX Runtime {ORT_VERSION} with CUDA (sm {arch}) into {ORT_DIR.relative_to(ROOT)}")
    report_cuda_libraries()


if __name__ == "__main__":
    cli()
