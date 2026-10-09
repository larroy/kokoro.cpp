# Thread safety

**Short answer:**
- Separate contexts are independent and may be used from different threads.
- A single context must not be used by two threads at the same time.
- The .NET wrapper (`Larroy.Kokoro` / `Kokoro.Net`) follows the same rule. It makes `Dispose` safe while a call is running, but it does not serialize calls.

The rule is stated in `include/kokoro/kokoro.h`, in the C API section of `README.md`, in `packaging/README.md` and in the remarks on `KokoroContext`.

## Why a single context is not thread safe

- **Changing settings races with synthesis.** No lock is taken anywhere in `src/`.
  - `kokoro_set_language` writes `Kokoro::forced_language_`.
  - `kokoro_set_number_language` writes `Tokenizer::number_language_`.
  - Both values are read during `create()` and `phonemize()` with no synchronization. Changing them while another thread synthesizes is a data race.
- **The G2P path has not been checked for concurrent use.** `Tokenizer` owns the cppjieba processor, `ZHG2P` and the English G2P. None of them is documented or checked as safe to share across threads.
- **Inference on its own is probably fine.** ONNX Runtime documents `Ort::Session::Run` (used in `Kokoro::_create_audio`) as safe to call concurrently. That does not make a whole `create()` call safe, and the public contract does not promise it.

## Why separate contexts are safe

- **No shared mutable global state.** The only statics in `src/` are lookup tables such as `SIBILANT` and `MISAKI` in `src/EnG2P.h`. They are `static const` inside functions: C++11 initializes them thread-safely, and they are only read afterwards.
- **Errors are per thread.** `kokoro_last_error()` reads a `thread_local` string (`g_last_error` in `src/kokoro_c.cpp`), so one thread's failure never overwrites another thread's error message.
- **Each context builds its own `Ort::Env`.** ONNX Runtime shares a single reference-counted environment internally, so several contexts can coexist.

### Cost of one context per thread

Each context loads:
- its own copy of the model (`models/kokoro-v1.0.onnx` is about 310 MB);
- its own copy of the G2P dictionaries (`dict/pinyin_phrase.txt` 9 MB, `dict/idf.utf8` 5.7 MB, `dict/jieba.dict.utf8` 4.8 MB, and others);
- its own ONNX Runtime session.

`cpu_session_options()` in `src/Kokoro.cpp` does not set an intra-op thread count, so every CPU context starts ONNX Runtime's default thread pool. Several CPU contexts compete for the same cores. On CUDA, every context holds another copy of the model in GPU memory.

## The .NET wrapper

- `KokoroContext` has no lock. `Synthesize`, `Phonemize`, `SetLanguage` and `SetNumberLanguage` call straight into the native library, so the native rule applies unchanged. Using one context from several `Task.Run` calls at once counts as concurrent use.
- **What the wrapper adds:**
  - **Safe dispose.** `KokoroContextHandle` is a `SafeHandle`. The native context is freed exactly once, and only after every running call has finished. A `Dispose` that races a `Synthesize` does not crash; later calls throw `ObjectDisposedException`. `SafeHandleTests.DisposeDuringInFlightCallsDefersRelease` and `SafeHandleTests.ConcurrentDisposeReleasesEachHandleOnce` test this.
  - **Correct error messages.** Calls are synchronous. `KokoroStatusCheck.ThrowIfFailed` reads `kokoro_last_error()` on the same thread right after the failing call, so each exception carries its own call's message even under concurrency.
- A context is not tied to the thread that created it. It may move between threads as long as only one thread uses it at a time and the hand-off is synchronized (a lock, a queue or a channel).

## Using kokoro from several threads

1. **One worker thread owns the context.** Other threads queue requests to it. `dotnet/examples/Kokoro.Interactive/SynthesisWorker.cs` (with a `BlockingCollection`) and `src/cli/interactive.cpp` work this way.
2. **Lock around each call**, e.g. `lock (gate) { audio = context.Synthesize(text, voice); }`. This is the simplest option, but only one synthesis runs at a time.
3. **A pool of N contexts**, e.g. a `Channel<KokoroContext>` that callers take a context from and return it to. Synthesis really runs in parallel, but each context has the memory, CPU-thread and GPU costs described above.
