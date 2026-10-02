# Native RIDs packaged as Larroy.Kokoro.runtime.<Rid><Suffix>. Adding a RID means adding a row.
# Suffix '' marks the base package Larroy.Kokoro depends on. Descriptions must not contain '&' or '<'.
@{
  Rows = @(
    @{
      Rid = 'win-x64'; VcvarsArch = 'x64'; OrtPlatform = 'win32'; OrtArch = 'x64'; OrtGpu = $true; PeMachine = 0x8664
      Packages = @(
        @{
          Suffix = ''; Files = @('kokoro.dll', 'onnxruntime.dll')
          Description = 'Native kokoro.dll and ONNX Runtime 1.23.2 for win-x64 (runs on CPU). Referenced by Larroy.Kokoro.'
        }
        @{
          Suffix = '.cuda'; Files = @('onnxruntime_providers_shared.dll', 'onnxruntime_providers_cuda.dll')
          Description = 'Optional CUDA execution provider for Larroy.Kokoro on win-x64. Requires CUDA 12 and cuDNN 9.'
        }
      )
    }
    @{
      Rid = 'win-arm64'; VcvarsArch = 'x64_arm64'; OrtPlatform = 'win32'; OrtArch = 'arm64'; OrtGpu = $false
      PeMachine = 0xAA64
      Packages = @(
        @{
          Suffix = ''; Files = @('kokoro.dll', 'onnxruntime.dll')
          Description = 'Native kokoro.dll and ONNX Runtime 1.23.2 for win-arm64. Referenced by Larroy.Kokoro.'
        }
      )
    }
  )
}
