from array import array
from pathlib import Path

import pytest

import voice_tool


def constant_style(value: float) -> array:
    return array("f", [value]) * voice_tool.STYLE_LEN


def ramp_style() -> array:
    return array("f", (float(i % 7) for i in range(voice_tool.STYLE_LEN)))


@pytest.fixture
def voices() -> dict[str, array]:
    return {"af_one": constant_style(1.0), "bf_three": constant_style(3.0), "zf_ramp": ramp_style()}


@pytest.fixture
def voices_file(tmp_path: Path, voices: dict[str, array]) -> Path:
    path = tmp_path / "voices.bin"
    voice_tool.save_voices(path, voices)
    return path
