# ONNX Runtime GitHub outage: download audit

Checked 2026-11-25. `github.com/microsoft/onnxruntime` has returned 404 since 2026-11-17, release assets
included. This records what still works, what is broken, and where 1.23.2 is still available.

**Update 2026-11-26:** the eight archives are now served by [larroy/onnxruntime release `v1.23.2`](https://github.com/larroy/onnxruntime/releases/tag/v1.23.2),
repackaged unmodified from Microsoft's 1.23.2 NuGet packages on nuget.org; `bootstrap.py configure` works again.

## Verdict

`bootstrap.py` cannot download ONNX Runtime: all eight release archives and the source clone are gone.
Every model and voice download is unaffected.

An existing `third_party/onnxruntime/` (`.archive` = `onnxruntime-win-x64-1.23.2.zip`) and an existing
`models/` keep builds working. A clean clone, `configure --force`, and CI do not.

## Probed URLs

| `bootstrap.py` download | Source | Result |
|---|---|---|
| 6 CPU archives (`onnxruntime-{linux-x64,linux-aarch64,osx-x86_64,osx-arm64,win-x64,win-arm64}-1.23.2.*`) | `ORT_URL` (`bootstrap.py:34`) → GitHub release `v1.23.2` | **404** (all six) |
| 2 CUDA archives (`linux-x64-gpu`, `win-x64-gpu`) | same | **404** (both) |
| `build-ort` source clone, tag `v1.23.2` | `ORT_GIT_URL` (`bootstrap.py:175`) | **dead**: `info/refs` → 401, repo API → 404 |
| `kokoro-v1.1-zh.onnx`, `voices-v1.1-zh.bin` | `larroy/kokoro.cpp` release `voices_model_files` | 206 OK |
| `kokoro-v1.0.onnx` | `thewh1teagle/kokoro-onnx` release `model-files-v1.0` | 206 OK |
| `kokoro-de.onnx` | HF `Godelaune/Kokoro-82M-ONNX-German-Martin` | 206 OK |
| Spanish voices `ef_dora`, `em_alex`, `em_santa` | HF `onnx-community/Kokoro-82M-v1.0-ONNX` | 206 OK |
| German voices `df_eva`, `df_victoria`, `dm_bernd`, `dm_martin` | HF `cstr/kokoro-voices-GGUF` | 206 OK |

## Blast radius

- `install_onnxruntime()` (`bootstrap.py:342-357`) fails on every platform.
- `bootstrap.py build-ort` (source build with CUDA for GPUs outside the prebuilt arch list) cannot clone
  `ORT_GIT_URL`; usable source mirrors exist, see the next section.
- `.github/workflows/ci.yml:65` runs `bootstrap.py configure` on a fresh runner and caches only `models`
  (`ci.yml:59-64`), so every matrix leg fails now.
- `.github/workflows/pack.yml:93` and `packaging/build-natives.ps1` need ORT packages per RID; they pass only
  while the `ort-packaging-${{ hashFiles('bootstrap.py') }}` cache entry survives.
- Dead documentation links: `README.md:29,284,510,757` (four languages) and `THIRD_PARTY_NOTICES.md:76`.

## Where 1.23.2 is still served (verified reachable)

| Channel | Coverage |
|---|---|
| NuGet `Microsoft.ML.OnnxRuntime` 1.23.2 (123 MB) | **Best drop-in.** `build/native/include/*.h` (11 headers incl. `onnxruntime_cxx_api.h`); `runtimes/{linux-x64,linux-arm64,osx-x64,osx-arm64,win-x64,win-arm64}/native/` with `libonnxruntime.{so,dylib,dll}`, win `onnxruntime.lib` import libs and `onnxruntime_providers_shared`; plus `android/native/onnxruntime.aar` and the iOS xcframework. |
| NuGet `Microsoft.ML.OnnxRuntime.Gpu.Windows` 1.23.2 (213 MB) | `win-x64` with `onnxruntime_providers_cuda.dll` (+ tensorrt). |
| NuGet `Microsoft.ML.OnnxRuntime.Gpu.Linux` 1.23.2 (241 MB) | `linux-x64` with `libonnxruntime_providers_cuda.so`. |
| Maven Central `com.microsoft.onnxruntime:onnxruntime-android:1.23.2` | the AAR the Android build already used (`KOKORO_ORT_AAR`). |
| PyPI `onnxruntime==1.23.2` | linux x64/aarch64, macOS x64/arm64, win amd64. **Libraries only, no headers, no win-arm64** — not usable for the C++ build. |
| Homebrew `onnxruntime` | 1.32.0 bottles on ghcr.io; the formula's source URL is the dead GitHub archive, so source builds fail. |
| conda-forge | binaries exist, latest 1.32.0; **1.23.2 is not in the index**. |
| `cdn.pyke.io` (ort crate prebuilt cache) | 1.31.0 — third-party hosting, different version. |

## Source mirrors for `build-ort` (verified 2026-11-25)

Upstream tag `v1.23.2` peels to commit `a83fc4d58cb48eb68890dd689f94f28288cf2278`
("ORT 1.23.2 cherrypick 1 (#26368)", 2025-10-21). Software Heritage has the official origin archived and
reports that revision's metadata, which is how the SHA below was corroborated independently of any mirror.

| Mirror | What it gives | Provenance |
|---|---|---|
| `https://gitcode.com/gh_mirrors/on/onnxruntime.git` | Full git mirror, tags `v1.23.0`/`v1.23.1`/`v1.23.2`. `git clone --depth 1 --branch v1.23.2` succeeded: HEAD `a83fc4d5…`, 9431 files, 857 MB. | Third-party (China-hosted). Safe **only** because git content-addressing pins the tree to the SHA; `raw.*` file URLs are WAF-blocked (418), `git clone`/`ls-remote` work. |
| Debian sid source `onnxruntime_1.23.2+dfsg-6.1` | `onnxruntime_1.23.2+dfsg.orig.tar.xz`, 38,793,400 bytes, sha256 `c601bdc18b542435e5a0149ca18dc353106e0ed87a38d96ed759f1a82a52ef43`, matching the signed `.dsc`. Root prefix `microsoft-onnxruntime-a83fc4d/` (the GitHub auto-archive of that commit). 9635 entries: `onnxruntime/`, `orttraining/`, `tools/ci_build/build.py`, `cmake/deps.txt`, `cmake/external/`, `include/`, `VERSION_NUMBER` = 1.23.2. | Official Debian archive, signed checksums. `Files-Excluded` only drops `*.onnx`, `*.bin`, testdata `.mp3`/`.png` and Windows binary patches — irrelevant with `onnxruntime_BUILD_UNIT_TESTS=OFF`. Debian also ships `libonnxruntime-dev 1.23.2+dfsg-6.1` for amd64/arm64/armhf/i386/loong64/ppc64el/riscv64/s390x, so the tree is known-buildable. `debian.tar.xz` patches are separate and not applied by a plain upstream build. |
| Software Heritage vault | Archived copy of the origin; revisions are served as **gitfast** bundles: `POST /api/1/vault/gitfast/swh:1:rev:a83fc4d5…/`, then poll, then `git clone` the bundle. `flat` answers 400 for revisions (directories only) and requests need a User-Agent. | Archive-grade, but full history and long cook times. |

Cross-check run: Debian orig tarball blobs and GitCode clone blobs for `VERSION_NUMBER` (`14bee92c…`) and
`cmake/deps.txt` (`01e5c809…`) are identical, i.e. the two mirrors carry the same upstream tree.

Build dependencies are not mirror-hosted: of the 40 URLs in `cmake/deps.txt` at that commit, 39 answer
200/206 (onnx v1.18.0, protobuf v21.12 + protoc binaries, abseil, cutlass v3.9.2, cudnn-frontend v1.12.0,
eigen, XNNPACK, googletest, …). The single 404 is `microsoft/onnxruntime-extensions` (`c24b7ba…`), fetched
only for `--use_extensions`, which `bootstrap.py:ort_build_command` does not pass.

`checkout_onnxruntime()` (`bootstrap.py:497-510`) should gain a configurable source URL defaulting to a
mirror, and must assert `git rev-parse v1.23.2^{commit}` equals `a83fc4d58cb48eb68890dd689f94f28288cf2278`
before configuring. That assertion, not the host name, is what makes a third-party mirror trustworthy.

## Migration notes

- A `.nupkg` is a zip. Extracting `build/native/include` and `runtimes/<rid>/native` reproduces today's
  `third_party/onnxruntime/` layout: headers land in `include/` instead of `include/onnxruntime/`, which
  `CMakeLists.txt:47-54` already tolerates; `linux-x64/native` has no `*.lib` (unused — CMake links with
  `find_library`).
- Any rewrite must pin the `.nupkg` SHA-256s and keep the `.archive` stamp semantics, or
  `README.md:34` / `download()` lose the checksum guarantee.
- The GPU NuGet packages carry the same upstream prebuilt kernel arch set, so moving to them loses no CUDA
  coverage. Custom `CMAKE_CUDA_ARCHITECTURES` builds remain possible from the source mirrors above.
- Unofficial mirrors (forks, SourceForge, Wayback) circulate; a hash check only proves what a mirror served.
  Prefer Microsoft-hosted NuGet/Maven/PyPI and the Debian archive, and pin the upstream commit SHA when a
  third-party git mirror is used. Keep a copy in the project's own artifact store either way.
