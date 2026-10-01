"""espeak-ng phonemes exactly as upstream Kokoro produced the voices' training data.

kokoro's KPipeline builds misaki.espeak.EspeakG2P(language=...) with version None; this module copies that
G2P's backend setup and pre/post-processing without depending on misaki:
https://github.com/hexgrad/misaki/blob/main/misaki/espeak.py
"""

from collections.abc import Sequence

import espeakng_loader
import phonemizer
from phonemizer.backend.espeak.wrapper import EspeakWrapper

E2M = sorted({
    "a^ɪ": "I", "a^ʊ": "W", "d^z": "ʣ", "d^ʒ": "ʤ", "e^ɪ": "A", "o^ʊ": "O", "ə^ʊ": "Q", "s^s": "S", "t^s": "ʦ",
    "t^ʃ": "ʧ", "ɔ^ɪ": "Y",
}.items())


def make_backend(language: str) -> phonemizer.backend.EspeakBackend:
    EspeakWrapper.set_library(espeakng_loader.get_library_path())
    EspeakWrapper.set_data_path(espeakng_loader.get_data_path())
    return phonemizer.backend.EspeakBackend(language=language, preserve_punctuation=True, with_stress=True,
                                            tie="^", language_switch="remove-flags")


def postprocess(phonemes: str) -> str:
    phonemes = phonemes.strip()
    for old, new in E2M:
        phonemes = phonemes.replace(old, new)
    return phonemes.replace("^", "").replace("-", "").replace("«", "(").replace("»", ")")


def espeak_phonemes(lines: Sequence[str], language: str) -> list[str]:
    """One espeak-ng phoneme string per line, called per line as misaki does."""
    backend = make_backend(language)
    results = []
    for line in lines:
        text = line.replace("«", chr(8220)).replace("»", chr(8221)).replace("(", "«").replace(")", "»")
        phonemes = backend.phonemize([text])
        results.append(postprocess(phonemes[0]) if phonemes else "")
    return results
