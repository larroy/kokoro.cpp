import struct
import sys
from array import array
from pathlib import Path

import click
import pytest
from click.testing import CliRunner, Result

import voice_tool
from conftest import constant_style, ramp_style
from voice_tool import WeightedVoice


def run_cli(*args: str | Path) -> Result:
    return CliRunner().invoke(voice_tool.cli, [str(arg) for arg in args])


def test_round_trip_is_byte_identical(tmp_path: Path, voices_file: Path) -> None:
    copy = tmp_path / "copy.bin"
    voice_tool.save_voices(copy, voice_tool.load_voices(voices_file))
    assert copy.read_bytes() == voices_file.read_bytes()


def test_load_returns_saved_voices(voices_file: Path, voices: dict[str, array]) -> None:
    assert voice_tool.load_voices(voices_file) == voices


def test_header_layout(voices_file: Path) -> None:
    assert voices_file.read_bytes()[:12] == b"VOIC" + struct.pack("<II", 1, 3)


def test_duplicate_name_later_entry_wins(tmp_path: Path) -> None:
    path = tmp_path / "dup.bin"
    entries = voice_tool.encode_voice("a", constant_style(1.0)) + voice_tool.encode_voice("a", constant_style(2.0))
    path.write_bytes(b"VOIC" + struct.pack("<II", 1, 2) + entries)
    assert voice_tool.load_voices(path) == {"a": constant_style(2.0)}


@pytest.mark.parametrize(("data", "message"), [
    (b"NOPE" + struct.pack("<II", 1, 0), "not a kokoro.cpp voices file"),
    (b"VOIC" + struct.pack("<II", 2, 0), "unsupported voices file version 2"),
    (b"VOIC" + struct.pack("<I", 1), "truncated voices file"),
])
def test_load_rejects_bad_header(tmp_path: Path, data: bytes, message: str) -> None:
    path = tmp_path / "bad.bin"
    path.write_bytes(data)
    with pytest.raises(click.ClickException, match=message):
        voice_tool.load_voices(path)


def test_load_rejects_truncated_voice(tmp_path: Path, voices_file: Path) -> None:
    path = tmp_path / "short.bin"
    path.write_bytes(voices_file.read_bytes()[:-4])
    with pytest.raises(click.ClickException, match="truncated voices file"):
        voice_tool.load_voices(path)


def test_save_rejects_wrong_length_without_writing(tmp_path: Path) -> None:
    path = tmp_path / "out.bin"
    with pytest.raises(click.ClickException, match="expected 130560 floats, got 3"):
        voice_tool.save_voices(path, {"ok": constant_style(1.0), "short": array("f", [1.0, 2.0, 3.0])})
    assert not path.exists()


@pytest.mark.parametrize(("spec", "expected"), [
    ("af_one", WeightedVoice("af_one", 1.0)),
    ("af_one:0.25", WeightedVoice("af_one", 0.25)),
    ("af_one:3", WeightedVoice("af_one", 3.0)),
])
def test_parse_weighted_voice(spec: str, expected: WeightedVoice) -> None:
    assert voice_tool.parse_weighted_voice(spec) == expected


@pytest.mark.parametrize(("spec", "message"), [
    ("af_one:abc", "weight must be a number"),
    ("af_one:", "weight must be a number"),
    ("af_one:0", "weight must be positive"),
    ("af_one:-1", "weight must be positive"),
    (":1", "missing voice name"),
])
def test_parse_weighted_voice_rejects(spec: str, message: str) -> None:
    with pytest.raises(click.BadParameter, match=message):
        voice_tool.parse_weighted_voice(spec)


def test_blend_weighted_mean(voices: dict[str, array]) -> None:
    parts = [WeightedVoice("af_one", 0.75), WeightedVoice("bf_three", 0.25)]
    assert voice_tool.blend(voices, parts) == constant_style(1.5)


def test_blend_normalizes_weights(voices: dict[str, array]) -> None:
    parts = [WeightedVoice("af_one", 1.0), WeightedVoice("bf_three", 1.0)]
    assert voice_tool.blend(voices, parts) == constant_style(2.0)


def test_blend_is_elementwise(voices: dict[str, array]) -> None:
    parts = [WeightedVoice("zf_ramp", 1.0), WeightedVoice("af_one", 1.0)]
    assert voice_tool.blend(voices, parts) == array("f", ((x + 1.0) / 2 for x in ramp_style()))


def test_blend_rejects_unknown_voice(voices: dict[str, array]) -> None:
    with pytest.raises(click.ClickException, match="unknown voice 'nosuch'"):
        voice_tool.blend(voices, [WeightedVoice("af_one", 1.0), WeightedVoice("nosuch", 1.0)])


def test_cli_blend_adds_voice(tmp_path: Path, voices_file: Path) -> None:
    out = tmp_path / "out.bin"
    result = run_cli("blend", "--voices", voices_file, "--name", "af_mix", "-o", out, "af_one:3", "bf_three")
    assert result.exit_code == 0, result.output
    assert f"Added 'af_mix' in {out} (4 voices)" in result.output
    assert voice_tool.load_voices(out)["af_mix"] == constant_style(1.5)


def test_cli_blend_replaces_in_place(voices_file: Path) -> None:
    result = run_cli("blend", "--voices", voices_file, "--name", "af_one", "-o", voices_file, "bf_three")
    assert result.exit_code == 0, result.output
    assert "Replaced 'af_one'" in result.output
    assert voice_tool.load_voices(voices_file)["af_one"] == constant_style(3.0)


def test_cli_blend_unknown_voice_writes_nothing(tmp_path: Path, voices_file: Path) -> None:
    out = tmp_path / "out.bin"
    result = run_cli("blend", "--voices", voices_file, "--name", "x", "-o", out, "nosuch")
    assert result.exit_code == 1
    assert "unknown voice 'nosuch'" in result.output
    assert not out.exists()


@pytest.mark.parametrize("parts", [["af_one:-1"], ["af_one:abc"], []])
def test_cli_blend_usage_errors(tmp_path: Path, voices_file: Path, parts: list[str]) -> None:
    result = run_cli("blend", "--voices", voices_file, "--name", "x", "-o", tmp_path / "out.bin", *parts)
    assert result.exit_code == 2


def test_cli_import_pt_without_torch(tmp_path: Path, voices_file: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setitem(sys.modules, "torch", None)
    result = run_cli("import-pt", "--voices", voices_file, "--name", "x", "-o", tmp_path / "out.bin", voices_file)
    assert result.exit_code == 1
    assert "importing .pt voices needs PyTorch" in result.output


def save_tensor(path: Path, style: array, shape: tuple[int, ...]) -> Path:
    torch = pytest.importorskip("torch")
    torch.save(torch.tensor(list(style)).reshape(shape), path)
    return path


def test_cli_import_pt_adds_voice(tmp_path: Path, voices_file: Path) -> None:
    pt_file = save_tensor(tmp_path / "voice.pt", ramp_style(), (510, 1, 256))
    out = tmp_path / "out.bin"
    result = run_cli("import-pt", "--voices", voices_file, "--name", "af_new", "-o", out, pt_file)
    assert result.exit_code == 0, result.output
    assert voice_tool.load_voices(out)["af_new"] == ramp_style()


def test_cli_import_pt_rejects_wrong_shape(tmp_path: Path, voices_file: Path) -> None:
    pt_file = save_tensor(tmp_path / "bad.pt", constant_style(1.0)[:2560], (10, 256))
    result = run_cli("import-pt", "--voices", voices_file, "--name", "x", "-o", tmp_path / "out.bin", pt_file)
    assert result.exit_code == 1
    assert "expected 510 x 256 = 130560 values, got 2560" in result.output
