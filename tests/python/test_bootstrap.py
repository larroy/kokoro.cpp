"""Checks for the pure selection policy in bootstrap.py."""

import pytest
import click

import bootstrap


def test_cuda_archs_supports_win():
    archs = bootstrap.CudaArchs(real=((7, 5), (8, 6), (8, 9)), virtual=(9, 0))
    for cc, expected in [((7, 0), False), ((7, 5), True), ((8, 0), False), ((8, 6), True),
                         ((8, 9), True), ((9, 0), True), ((12, 0), True)]:
        assert archs.supports(cc) == expected, cc


def test_cuda_archs_supports_linux():
    archs = bootstrap.CudaArchs(real=((6, 0), (7, 0), (7, 5), (8, 0)), virtual=(9, 0))
    for cc, expected in [((5, 2), False), ((6, 1), True), ((7, 0), True), ((8, 6), True)]:
        assert archs.supports(cc) == expected, cc


def test_parse_gpus():
    text = "0, NVIDIA TITAN V, 7.0\n1, NVIDIA GeForce RTX 4090, 8.9\n2, Foo, [N/A]\n"
    gpus = bootstrap.parse_gpus(text)
    assert gpus == (
        bootstrap.Gpu(0, "NVIDIA TITAN V", (7, 0)),
        bootstrap.Gpu(1, "NVIDIA GeForce RTX 4090", (8, 9)),
    )


TITAN_V = bootstrap.Gpu(0, "NVIDIA TITAN V", (7, 0))
RTX_4090 = bootstrap.Gpu(0, "NVIDIA GeForce RTX 4090", (8, 9))
WIN_X64 = ("win32", "x64")
LINUX_X64 = ("linux", "x64")


def test_auto_no_gpus_picks_cpu():
    choice = bootstrap.choose_ort("auto", WIN_X64, (), None, False)
    assert choice.archive == bootstrap.ORT_ARCHIVES[WIN_X64]


def test_auto_unsupported_gpu_picks_cpu():
    choice = bootstrap.choose_ort("auto", WIN_X64, (TITAN_V,), (13, 0), False)
    assert choice.archive == bootstrap.ORT_ARCHIVES[WIN_X64]
    assert "build-ort" in choice.reason


def test_auto_linux_titan_v_picks_gpu():
    choice = bootstrap.choose_ort("auto", LINUX_X64, (TITAN_V,), (13, 0), False)
    assert choice.archive == bootstrap.ORT_GPU_ARCHIVES[LINUX_X64]


def test_auto_old_driver_picks_cpu():
    choice = bootstrap.choose_ort("auto", WIN_X64, (RTX_4090,), (12, 4), False)
    assert choice.archive == bootstrap.ORT_ARCHIVES[WIN_X64]


def test_auto_supported_gpu_picks_gpu():
    choice = bootstrap.choose_ort("auto", WIN_X64, (RTX_4090,), (13, 0), False)
    assert choice.archive == bootstrap.ORT_GPU_ARCHIVES[WIN_X64]


def test_auto_keeps_source_build():
    choice = bootstrap.choose_ort("auto", WIN_X64, (RTX_4090,), (13, 0), True)
    assert choice.archive is None


def test_cpu_mode_replaces_source_build():
    choice = bootstrap.choose_ort("cpu", WIN_X64, (RTX_4090,), (13, 0), True)
    assert choice.archive == bootstrap.ORT_ARCHIVES[WIN_X64]


def test_gpu_mode_unsupported_platform_raises():
    with pytest.raises(click.ClickException):
        bootstrap.choose_ort("gpu", ("darwin", "arm64"), (RTX_4090,), (13, 0), False)