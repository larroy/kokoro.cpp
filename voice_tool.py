#!/usr/bin/env python3
"""Add voices to a kokoro.cpp voices file (see docs/adding-voices.md).

    uv run voice_tool.py blend --name af_maplevale -o models/voices-custom.bin af_maple:0.7 bf_vale:0.3
    uv run --group voice voice_tool.py import-pt --name af_myvoice -o models/voices-custom.bin my_voice.pt
"""

import struct
import sys
from array import array
from dataclasses import dataclass
from pathlib import Path
from typing import BinaryIO, Callable, Sequence

import click

ROOT = Path(__file__).resolve().parent
DEFAULT_VOICES = ROOT / "models" / "voices-v1.1-zh.bin"
MAGIC = b"VOIC"
VERSION = 1
ROWS, DIM = 510, 256
STYLE_LEN = ROWS * DIM


def read_exact(f: BinaryIO, size: int, path: Path) -> bytes:
    data = f.read(size)
    if len(data) != size:
        raise click.ClickException(f"{path}: truncated voices file")
    return data


def read_u32(f: BinaryIO, path: Path) -> int:
    return struct.unpack("<I", read_exact(f, 4, path))[0]


def read_voice(f: BinaryIO, path: Path) -> tuple[str, array]:
    name = read_exact(f, read_u32(f, path), path).decode("utf-8")
    style = array("f", read_exact(f, 4 * read_u32(f, path), path))
    if sys.byteorder == "big":
        style.byteswap()
    return name, style


def load_voices(path: Path) -> dict[str, array]:
    """Read a voices file. A later voice with a duplicate name wins, as in the C++ loader."""
    with open(path, "rb") as f:
        if f.read(4) != MAGIC:
            raise click.ClickException(f"{path}: not a kokoro.cpp voices file")
        version, count = struct.unpack("<II", read_exact(f, 8, path))
        if version != VERSION:
            raise click.ClickException(f"{path}: unsupported voices file version {version}")
        return dict(read_voice(f, path) for _ in range(count))


def le_bytes(style: array) -> bytes:
    if sys.byteorder == "little":
        return style.tobytes()
    swapped = array("f", style)
    swapped.byteswap()
    return swapped.tobytes()


def encode_voice(name: str, style: array) -> bytes:
    if len(style) != STYLE_LEN:
        raise click.ClickException(f"{name}: expected {STYLE_LEN} floats, got {len(style)}")
    name_bytes = name.encode("utf-8")
    return struct.pack("<I", len(name_bytes)) + name_bytes + struct.pack("<I", len(style)) + le_bytes(style)


def save_voices(path: Path, voices: dict[str, array]) -> None:
    # Encode everything first so a validation error never leaves a half-written file.
    payload = b"".join(encode_voice(name, style) for name, style in voices.items())
    path.write_bytes(MAGIC + struct.pack("<II", VERSION, len(voices)) + payload)


@dataclass(frozen=True)
class WeightedVoice:
    name: str
    weight: float


def parse_weight(spec: str, text: str) -> float:
    try:
        weight = float(text)
    except ValueError:
        raise click.BadParameter(f"'{spec}': weight must be a number") from None
    if weight <= 0:
        raise click.BadParameter(f"'{spec}': weight must be positive")
    return weight


def parse_weighted_voice(spec: str) -> WeightedVoice:
    """Parse `<voice>[:<weight>]`; the weight defaults to 1."""
    name, sep, weight = spec.partition(":")
    if not name:
        raise click.BadParameter(f"'{spec}': missing voice name")
    return WeightedVoice(name, parse_weight(spec, weight) if sep else 1.0)


def blend(voices: dict[str, array], parts: Sequence[WeightedVoice]) -> array:
    """Normalized weighted mean of the given voices."""
    missing = next((p.name for p in parts if p.name not in voices), None)
    if missing is not None:
        raise click.ClickException(f"unknown voice '{missing}'")
    total = sum(p.weight for p in parts)
    weights = [p.weight / total for p in parts]
    styles = [voices[p.name] for p in parts]
    return array("f", (sum(w * x for w, x in zip(weights, column)) for column in zip(*styles)))


def load_pt(path: Path) -> array:
    try:
        import torch
    except ImportError:
        raise click.ClickException(
            "importing .pt voices needs PyTorch: run with uv run --group voice (or uv sync --group voice)"
        ) from None
    tensor = torch.load(path, map_location="cpu", weights_only=True)
    if not isinstance(tensor, torch.Tensor):
        raise click.ClickException(f"{path}: expected a tensor, got {type(tensor).__name__}")
    if tensor.numel() != STYLE_LEN:
        raise click.ClickException(
            f"{path}: expected {ROWS} x {DIM} = {STYLE_LEN} values, got {tensor.numel()}"
        )
    return array("f", tensor.detach().float().flatten().tolist())


def load_raw(path: Path) -> array:
    """Read a voice stored as ROWS x DIM little-endian float32 values (onnx-community `voices/*.bin`)."""
    data = path.read_bytes()
    if len(data) != STYLE_LEN * 4:
        raise click.ClickException(f"{path}: expected {STYLE_LEN * 4} bytes ({ROWS} x {DIM} float32), got {len(data)}")
    style = array("f", data)
    if sys.byteorder == "big":
        style.byteswap()
    return style


def store_voice(voices: dict[str, array], name: str, style: array, output: Path) -> None:
    replaced = name in voices
    voices[name] = style
    save_voices(output, voices)
    click.echo(f"{'Replaced' if replaced else 'Added'} '{name}' in {output} ({len(voices)} voices)")


def voice_file_options(f: Callable) -> Callable:
    """Options shared by every command: input file, new voice name, output file."""
    options = [
        click.option("--voices", default=DEFAULT_VOICES, show_default=True,
                     type=click.Path(exists=True, dir_okay=False, path_type=Path),
                     help="Voices file to start from."),
        click.option("--name", required=True,
                     help="Name of the new voice; an existing voice with this name is replaced."),
        click.option("-o", "--output", required=True, type=click.Path(dir_okay=False, path_type=Path),
                     help="Voices file to write."),
    ]
    for option in reversed(options):
        f = option(f)
    return f


@click.group()
def cli() -> None:
    """Add voices to a kokoro.cpp voices file."""


@cli.command("blend")
@voice_file_options
@click.argument("parts", nargs=-1, required=True, metavar="VOICE[:WEIGHT]...",
                callback=lambda ctx, param, value: tuple(map(parse_weighted_voice, value)))
def blend_command(voices: Path, name: str, output: Path, parts: tuple[WeightedVoice, ...]) -> None:
    """Add a weighted average of existing voices."""
    table = load_voices(voices)
    store_voice(table, name, blend(table, parts), output)


@cli.command("import-pt")
@voice_file_options
@click.argument("pt_file", type=click.Path(exists=True, dir_okay=False, path_type=Path))
def import_pt_command(voices: Path, name: str, output: Path, pt_file: Path) -> None:
    """Add a voice from a PyTorch tensor (.pt) of shape (510, 1, 256)."""
    style = load_pt(pt_file)
    store_voice(load_voices(voices), name, style, output)


if __name__ == "__main__":
    cli()
