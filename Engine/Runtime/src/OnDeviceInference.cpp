#include "Aetherion/Runtime/OnDeviceInference.h"

namespace Aetherion::Runtime {

OnDeviceInferenceResult NullOnDeviceInferenceBackend::EvaluateBehavior(
    const std::string& /*systemPrompt*/,
    const std::string& /*userPrompt*/,
    const std::string& /*schema*/) {
  OnDeviceInferenceResult result;
  result.success = false;
  result.errorMessage = "On-device inference backend not configured.";
  return result;
}

} // namespace Aetherion::Runtime
