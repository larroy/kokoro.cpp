# Native RIDs packaged as Larroy.Kokoro.runtime.<Rid><Suffix>. Adding a RID means adding a row.
# Suffix '' marks the base package Larroy.Kokoro depends on. Descriptions must not contain '&' or '<'.
# Dependencies: package ids, each required at >= ORT_VERSION from bootstrap.py.
# BuildTransitive: file in packaging/ packed as buildTransitive/<package id>.targets ('' for none).
@{
  Rows = @(
    @{
      Rid = 'win-x64'; VcvarsArch = 'x64'; OrtPlatform = 'win32'; OrtArch = 'x64'; PeMachine = 0x8664
      Packages = @(
        @{
          Suffix = ''; Files = @('kokoro.dll'); Dependencies = @('Microsoft.ML.OnnxRuntime'); BuildTransitive = ''
          Description = 'kokoro.dll for win-x64 (ONNX Runtime: Microsoft.ML.OnnxRuntime). Used by Larroy.Kokoro.'
        }
        @{
          Suffix = '.cuda'; Files = @(); Dependencies = @('Microsoft.ML.OnnxRuntime.Gpu.Windows')
          BuildTransitive = 'prefer-ort-gpu.targets'
          Description = 'Optional CUDA support for Larroy.Kokoro on win-x64 via Microsoft.ML.OnnxRuntime.Gpu.Windows.'
        }
      )
    }
    @{
      Rid = 'win-arm64'; VcvarsArch = 'x64_arm64'; OrtPlatform = 'win32'; OrtArch = 'arm64'; PeMachine = 0xAA64
      Packages = @(
        @{
          Suffix = ''; Files = @('kokoro.dll'); Dependencies = @('Microsoft.ML.OnnxRuntime'); BuildTransitive = ''
          Description = 'kokoro.dll for win-arm64 (ONNX Runtime: Microsoft.ML.OnnxRuntime). Used by Larroy.Kokoro.'
        }
      )
    }
  )
}
