#pragma once

#include <cstdint>

namespace Aetherion::Runtime {

class AIInferenceBudget {
public:
  void SetMaxRequestsPerFrame(int maxRequests) noexcept {
    m_maxRequestsPerFrame = (maxRequests > 0) ? maxRequests : 1;
  }

  [[nodiscard]] int GetMaxRequestsPerFrame() const noexcept {
    return m_maxRequestsPerFrame;
  }

  [[nodiscard]] int GetUsedRequests() const noexcept { return m_usedRequests; }

  [[nodiscard]] int GetRemainingRequests() const noexcept {
    return m_maxRequestsPerFrame - m_usedRequests;
  }

  bool TryConsume(std::uint64_t frameIndex, int cost = 1) noexcept {
    if (frameIndex != m_lastFrameIndex) {
      m_lastFrameIndex = frameIndex;
      m_usedRequests = 0;
    }

    if (cost <= 0) {
      return true;
    }

    if (m_usedRequests + cost > m_maxRequestsPerFrame) {
      return false;
    }

    m_usedRequests += cost;
    return true;
  }

private:
  int m_maxRequestsPerFrame{8};
  int m_usedRequests{0};
  std::uint64_t m_lastFrameIndex{0};
};

} // namespace Aetherion::Runtime
