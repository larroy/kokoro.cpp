#!/usr/bin/env python3
"""Compare kokoro.cpp's G2P (g2p_dump) against espeak-ng as used upstream (espeak_oracle.py).

Run: uv run --group eval python eval_bench/compare_g2p.py [--language es|de] [--g2p-dump PATH] [--report PATH]
Exit code 1 when a mismatch is not listed in the language's known-diffs file.
"""

import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

import click

from espeak_oracle import espeak_phonemes

BENCH = Path(__file__).resolve().parent
ROOT = BENCH.parent
DUMP_NAME = "g2p_dump.exe" if sys.platform == "win32" else "g2p_dump"


@dataclass(frozen=True)
class BenchLanguage:
    code: str
    espeak: str
    corpus: Path
    known_diffs: Path
    espeak_fixups: tuple[tuple[str, str], ...] = ()  # (espeak symbol, Kokoro symbol) replaced before comparing


@dataclass(frozen=True)
class LineResult:
    text: str
    espeak: str
    kokoro: str
    distance: int
    known: str | None


LANGUAGES = {
    "es": BenchLanguage("es", "es", BENCH / "corpus" / "es.txt", BENCH / "corpus" / "es_known_diffs.tsv"),
    # Kokoro's vocab has no ʏ; the German G2P writes y.
    "de": BenchLanguage("de", "de", BENCH / "corpus" / "de.txt", BENCH / "corpus" / "de_known_diffs.tsv",
                        (("ʏ", "y"),)),
}


def levenshtein(a: str, b: str) -> int:
    """Edit distance over code points."""
    previous = list(range(len(b) + 1))
    for i, ca in enumerate(a, 1):
        current = [i]
        for j, cb in enumerate(b, 1):
            current.append(min(previous[j] + 1, current[j - 1] + 1, previous[j - 1] + (ca != cb)))
        previous = current
    return previous[-1]


def apply_fixups(phonemes: str, fixups: tuple[tuple[str, str], ...]) -> str:
    for old, new in fixups:
        phonemes = phonemes.replace(old, new)
    return phonemes


def content_lines(path: Path) -> list[str]:
    stripped = (line.strip() for line in path.read_text(encoding="utf-8").splitlines())
    return [line for line in stripped if line and not line.startswith("#")]


def load_known_diffs(path: Path) -> dict[str, str]:
    if not path.is_file():
        return {}
    pairs = (line.split("\t", 1) for line in content_lines(path))
    return {pair[0]: (pair[1] if len(pair) > 1 else "") for pair in pairs}


def find_dump(option: Path | None) -> Path:
    if option is not None:
        return option
    default = ROOT / "build" / "eval_bench" / DUMP_NAME
    for candidate in (default, default.parent / "Release" / DUMP_NAME):
        if candidate.is_file():
            return candidate
    raise click.ClickException(
        f"g2p_dump not found at {default}; build with `uv run bootstrap.py build` or pass --g2p-dump")


def kokoro_phonemes(dump: Path, code: str, lines: list[str]) -> list[str]:
    result = subprocess.run([str(dump), code, str(ROOT / "dict")], input="\n".join(lines) + "\n",
                            capture_output=True, text=True, encoding="utf-8", check=True)
    output = result.stdout.split("\n")[:-1]
    if len(output) != len(lines):
        raise click.ClickException(f"g2p_dump printed {len(output)} lines for {len(lines)} inputs")
    return output


def compare(lines: list[str], espeak: list[str], kokoro: list[str], known: dict[str, str]) -> list[LineResult]:
    return [LineResult(text, e, k, levenshtein(e, k), known.get(text)) for text, e, k in zip(lines, espeak, kokoro)]


def print_results(results: list[LineResult]) -> None:
    for r in results:
        if r.distance == 0:
            continue
        click.echo(f"{r.text}\n  espeak: {r.espeak}\n  kokoro: {r.kokoro}")
        if r.known is not None:
            click.echo(f"  known: {r.known}")
    exact = sum(r.distance == 0 for r in results)
    errors = sum(r.distance for r in results)
    total = max(1, sum(len(r.espeak) for r in results))
    click.echo(f"exact {exact}/{len(results)} ({100 * exact / max(1, len(results)):.1f}%), "
               f"phoneme error rate {100 * errors / total:.2f}%")
    for r in results:
        if r.distance == 0 and r.known is not None:
            click.secho(f"stale known diff: {r.text}", fg="yellow", err=True)


def write_report(path: Path, results: list[LineResult]) -> None:
    rows = ["text\tespeak\tkokoro\tdistance\tknown"]
    rows += [f"{r.text}\t{r.espeak}\t{r.kokoro}\t{r.distance}\t{r.known or ''}" for r in results]
    path.write_text("\n".join(rows) + "\n", encoding="utf-8")


@click.command()
@click.option("--language", "code", type=click.Choice(sorted(LANGUAGES)), default="es", show_default=True)
@click.option("--g2p-dump", "dump", type=click.Path(dir_okay=False, path_type=Path), help="g2p_dump executable.")
@click.option("--report", type=click.Path(dir_okay=False, path_type=Path), help="Write a TSV of every line.")
def main(code: str, dump: Path | None, report: Path | None) -> None:
    """Compare g2p_dump output with espeak-ng on the language's corpus."""
    lang = LANGUAGES[code]
    lines = content_lines(lang.corpus)
    kokoro = kokoro_phonemes(find_dump(dump), lang.code, lines)
    espeak = [apply_fixups(p.replace("ˌ", ""), lang.espeak_fixups) for p in espeak_phonemes(lines, lang.espeak)]
    results = compare(lines, espeak, kokoro, load_known_diffs(lang.known_diffs))
    print_results(results)
    if report is not None:
        write_report(report, results)
    sys.exit(1 if any(r.distance != 0 and r.known is None for r in results) else 0)


if __name__ == "__main__":
    main()
