#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace Aetherion::Runtime {

enum class OnDeviceBackendType {
  None = 0,
  LlamaCpp,
  Onnx
};

struct OnDeviceInferenceResult {
  bool success{false};
  std::string state;
  std::string reason;
  std::string errorMessage;
  std::uint64_t latencyMs{0};
};

class OnDeviceInferenceBackend {
public:
  virtual ~OnDeviceInferenceBackend() = default;
  [[nodiscard]] virtual bool IsReady() const = 0;
  [[nodiscard]] virtual OnDeviceBackendType GetBackendType() const = 0;
  virtual OnDeviceInferenceResult EvaluateBehavior(
      const std::string& systemPrompt,
      const std::string& userPrompt,
      const std::string& schema) = 0;
};

class NullOnDeviceInferenceBackend final : public OnDeviceInferenceBackend {
public:
  [[nodiscard]] bool IsReady() const override { return false; }
  [[nodiscard]] OnDeviceBackendType GetBackendType() const override {
    return OnDeviceBackendType::None;
  }
  OnDeviceInferenceResult EvaluateBehavior(
      const std::string& systemPrompt,
      const std::string& userPrompt,
      const std::string& schema) override;
};

} // namespace Aetherion::Runtime
