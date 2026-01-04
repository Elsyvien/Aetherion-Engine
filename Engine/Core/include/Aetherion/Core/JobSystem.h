#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <type_traits>
#include <vector>

namespace Aetherion::Core {

/// @brief Priority levels for jobs
enum class JobPriority : uint8_t {
  Low = 0,
  Normal = 1,
  High = 2,
  Critical = 3
};

/// @brief Handle to track a submitted job
class JobHandle {
public:
  JobHandle() = default;
  explicit JobHandle(std::shared_future<void> future) 
    : m_future(std::move(future)) {}
  
  /// @brief Wait for job completion
  void Wait() const {
    if (m_future.valid()) {
      m_future.wait();
    }
  }
  
  /// @brief Check if job is complete
  [[nodiscard]] bool IsComplete() const {
    if (!m_future.valid()) return true;
    return m_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
  }
  
  /// @brief Check if handle is valid
  [[nodiscard]] bool IsValid() const { return m_future.valid(); }
  
private:
  std::shared_future<void> m_future;
};

/// @brief Handle to track a job with a return value
template <typename T>
class TypedJobHandle {
public:
  TypedJobHandle() = default;
  explicit TypedJobHandle(std::shared_future<T> future) 
    : m_future(std::move(future)) {}
  
  /// @brief Wait for job and get result
  [[nodiscard]] T Get() const {
    return m_future.get();
  }
  
  /// @brief Wait for job completion
  void Wait() const {
    if (m_future.valid()) {
      m_future.wait();
    }
  }
  
  /// @brief Check if job is complete
  [[nodiscard]] bool IsComplete() const {
    if (!m_future.valid()) return true;
    return m_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
  }
  
  [[nodiscard]] bool IsValid() const { return m_future.valid(); }
  
private:
  std::shared_future<T> m_future;
};

/// @brief Counter for tracking completion of multiple jobs
class JobCounter {
public:
  explicit JobCounter(uint32_t count = 0) : m_count(count) {}
  
  /// @brief Increment the counter
  void Increment(uint32_t amount = 1) {
    m_count.fetch_add(amount, std::memory_order_relaxed);
  }
  
  /// @brief Decrement the counter
  void Decrement(uint32_t amount = 1) {
    uint32_t oldCount = m_count.fetch_sub(amount, std::memory_order_release);
    if (oldCount <= amount) {
      std::lock_guard lock(m_mutex);
      m_condition.notify_all();
    }
  }
  
  /// @brief Wait until counter reaches zero
  void Wait() const {
    std::unique_lock lock(m_mutex);
    m_condition.wait(lock, [this] { return m_count.load(std::memory_order_acquire) == 0; });
  }
  
  /// @brief Check if counter is zero
  [[nodiscard]] bool IsComplete() const {
    return m_count.load(std::memory_order_acquire) == 0;
  }
  
  /// @brief Get current count
  [[nodiscard]] uint32_t GetCount() const {
    return m_count.load(std::memory_order_relaxed);
  }
  
private:
  std::atomic<uint32_t> m_count;
  mutable std::mutex m_mutex;
  mutable std::condition_variable m_condition;
};

/// @brief Thread-safe job scheduler for parallel task execution
///
/// @code
/// JobSystem jobs;
/// jobs.Initialize(4); // 4 worker threads
///
/// // Submit a simple job
/// auto handle = jobs.Submit([] {
///   DoExpensiveWork();
/// });
///
/// // Submit a job with return value
/// auto resultHandle = jobs.SubmitWithResult<int>([] {
///   return ComputeValue();
/// });
///
/// // Parallel for
/// std::vector<float> data(1000);
/// jobs.ParallelFor(0, data.size(), [&](size_t i) {
///   data[i] = ProcessItem(i);
/// });
///
/// // Wait for specific job
/// handle.Wait();
/// int result = resultHandle.Get();
///
/// // Wait for all jobs
/// jobs.WaitAll();
/// @endcode
class JobSystem {
public:
  using JobFunc = std::function<void()>;
  
  JobSystem() = default;
  ~JobSystem() { Shutdown(); }
  
  JobSystem(const JobSystem&) = delete;
  JobSystem& operator=(const JobSystem&) = delete;
  
  // ===========================================================================
  // Lifecycle
  // ===========================================================================
  
  /// @brief Initialize with specified number of worker threads
  /// @param numThreads 0 = use hardware concurrency
  void Initialize(size_t numThreads = 0) {
    if (m_initialized) return;
    
    if (numThreads == 0) {
      numThreads = std::max(1u, std::thread::hardware_concurrency());
    }
    
    m_running = true;
    m_workers.reserve(numThreads);
    
    for (size_t i = 0; i < numThreads; ++i) {
      m_workers.emplace_back([this, i] { WorkerLoop(i); });
    }
    
    m_initialized = true;
    m_threadCount = numThreads;
  }
  
  /// @brief Shutdown the job system
  void Shutdown() {
    if (!m_initialized) return;
    
    {
      std::lock_guard lock(m_queueMutex);
      m_running = false;
    }
    m_condition.notify_all();
    
    for (auto& worker : m_workers) {
      if (worker.joinable()) {
        worker.join();
      }
    }
    m_workers.clear();
    m_initialized = false;
  }
  
  /// @brief Get number of worker threads
  [[nodiscard]] size_t GetThreadCount() const { return m_threadCount; }
  
  // ===========================================================================
  // Job Submission
  // ===========================================================================
  
  /// @brief Submit a job for execution
  [[nodiscard]] JobHandle Submit(JobFunc func, 
                                  JobPriority priority = JobPriority::Normal) {
    auto task = std::make_shared<std::packaged_task<void()>>(std::move(func));
    auto future = task->get_future().share();
    
    EnqueueJob([task]() { (*task)(); }, priority);
    
    return JobHandle(future);
  }
  
  /// @brief Submit a job with a return value
  template <typename T, typename Func>
  [[nodiscard]] TypedJobHandle<T> SubmitWithResult(Func&& func, 
                                                    JobPriority priority = JobPriority::Normal) {
    auto task = std::make_shared<std::packaged_task<T()>>(std::forward<Func>(func));
    auto future = task->get_future().share();
    
    EnqueueJob([task]() { (*task)(); }, priority);
    
    return TypedJobHandle<T>(future);
  }
  
  /// @brief Submit a job with dependency on another job
  [[nodiscard]] JobHandle SubmitAfter(const JobHandle& dependency, 
                                       JobFunc func,
                                       JobPriority priority = JobPriority::Normal) {
    return Submit([dep = dependency, f = std::move(func)]() mutable {
      dep.Wait();
      f();
    }, priority);
  }
  
  /// @brief Submit a job that uses a counter
  void SubmitWithCounter(JobFunc func, 
                         std::shared_ptr<JobCounter> counter,
                         JobPriority priority = JobPriority::Normal) {
    counter->Increment();
    EnqueueJob([f = std::move(func), c = std::move(counter)]() {
      f();
      c->Decrement();
    }, priority);
  }
  
  // ===========================================================================
  // Parallel Algorithms
  // ===========================================================================
  
  /// @brief Execute a function in parallel over a range
  /// @param start Starting index
  /// @param end Ending index (exclusive)
  /// @param func Function taking an index
  /// @param batchSize Number of items per job (0 = automatic)
  template <typename Func>
  void ParallelFor(size_t start, size_t end, Func&& func, size_t batchSize = 0) {
    if (start >= end) return;
    
    const size_t count = end - start;
    
    if (batchSize == 0) {
      // Automatic batch size: aim for ~4 jobs per thread
      batchSize = std::max(size_t(1), count / (m_threadCount * 4));
    }
    
    auto counter = std::make_shared<JobCounter>();
    
    for (size_t i = start; i < end; i += batchSize) {
      const size_t batchEnd = std::min(i + batchSize, end);
      
      counter->Increment();
      EnqueueJob([&func, i, batchEnd, counter]() {
        for (size_t j = i; j < batchEnd; ++j) {
          func(j);
        }
        counter->Decrement();
      }, JobPriority::Normal);
    }
    
    counter->Wait();
  }
  
  /// @brief Execute a function in parallel over a range with job handles
  /// @return Vector of job handles for each batch
  template <typename Func>
  [[nodiscard]] std::vector<JobHandle> ParallelForAsync(size_t start, size_t end, 
                                                         Func&& func, 
                                                         size_t batchSize = 0) {
    std::vector<JobHandle> handles;
    
    if (start >= end) return handles;
    
    const size_t count = end - start;
    
    if (batchSize == 0) {
      batchSize = std::max(size_t(1), count / (m_threadCount * 4));
    }
    
    for (size_t i = start; i < end; i += batchSize) {
      const size_t batchEnd = std::min(i + batchSize, end);
      
      handles.push_back(Submit([&func, i, batchEnd]() {
        for (size_t j = i; j < batchEnd; ++j) {
          func(j);
        }
      }));
    }
    
    return handles;
  }
  
  /// @brief Map a function over a collection in parallel
  template <typename InputIt, typename OutputIt, typename Func>
  void ParallelMap(InputIt first, InputIt last, OutputIt dest, Func&& func) {
    const size_t count = std::distance(first, last);
    if (count == 0) return;
    
    ParallelFor(size_t(0), count, [&](size_t i) {
      *(dest + i) = func(*(first + i));
    });
  }
  
  /// @brief Reduce a collection in parallel
  template <typename T, typename InputIt, typename ReduceFunc>
  [[nodiscard]] T ParallelReduce(InputIt first, InputIt last, T init, ReduceFunc&& reduce) {
    const size_t count = std::distance(first, last);
    if (count == 0) return init;
    
    // Each thread reduces a portion
    const size_t numBatches = std::min(count, m_threadCount * 4);
    const size_t batchSize = (count + numBatches - 1) / numBatches;
    
    std::vector<T> partialResults(numBatches);
    auto counter = std::make_shared<JobCounter>();
    
    for (size_t batch = 0; batch < numBatches; ++batch) {
      const size_t batchStart = batch * batchSize;
      const size_t batchEnd = std::min(batchStart + batchSize, count);
      
      counter->Increment();
      EnqueueJob([&, batch, batchStart, batchEnd, counter]() {
        T result = T{};
        for (size_t i = batchStart; i < batchEnd; ++i) {
          result = reduce(result, *(first + i));
        }
        partialResults[batch] = result;
        counter->Decrement();
      }, JobPriority::Normal);
    }
    
    counter->Wait();
    
    // Final reduction
    T result = init;
    for (const auto& partial : partialResults) {
      result = reduce(result, partial);
    }
    return result;
  }
  
  // ===========================================================================
  // Synchronization
  // ===========================================================================
  
  /// @brief Wait for all queued jobs to complete
  void WaitAll() {
    std::unique_lock lock(m_queueMutex);
    m_allDoneCondition.wait(lock, [this] {
      return m_pendingJobs.load(std::memory_order_relaxed) == 0;
    });
  }
  
  /// @brief Wait for multiple job handles
  static void WaitAll(const std::vector<JobHandle>& handles) {
    for (const auto& handle : handles) {
      handle.Wait();
    }
  }
  
  /// @brief Get number of pending jobs
  [[nodiscard]] size_t GetPendingJobCount() const {
    return m_pendingJobs.load(std::memory_order_relaxed);
  }
  
  /// @brief Check if job system is idle
  [[nodiscard]] bool IsIdle() const {
    return m_pendingJobs.load(std::memory_order_relaxed) == 0;
  }
  
  // ===========================================================================
  // Statistics
  // ===========================================================================
  
  /// @brief Get total jobs executed
  [[nodiscard]] size_t GetTotalJobsExecuted() const {
    return m_totalJobsExecuted.load(std::memory_order_relaxed);
  }
  
  /// @brief Reset statistics
  void ResetStats() {
    m_totalJobsExecuted.store(0, std::memory_order_relaxed);
  }

private:
  struct PrioritizedJob {
    JobFunc func;
    JobPriority priority;
    uint64_t sequence; // For FIFO within same priority
    
    bool operator<(const PrioritizedJob& other) const {
      if (priority != other.priority) {
        return static_cast<int>(priority) < static_cast<int>(other.priority);
      }
      return sequence > other.sequence; // Lower sequence = earlier
    }
  };
  
  void EnqueueJob(JobFunc func, JobPriority priority) {
    {
      std::lock_guard lock(m_queueMutex);
      m_jobQueue.push_back(PrioritizedJob{
        std::move(func),
        priority,
        m_sequenceCounter++
      });
      // Sort by priority (stable sort maintains FIFO within priority)
      std::stable_sort(m_jobQueue.begin(), m_jobQueue.end(),
        [](const PrioritizedJob& a, const PrioritizedJob& b) {
          return static_cast<int>(a.priority) > static_cast<int>(b.priority);
        });
    }
    m_pendingJobs.fetch_add(1, std::memory_order_relaxed);
    m_condition.notify_one();
  }
  
  void WorkerLoop(size_t threadIndex) {
    (void)threadIndex; // For future per-thread features
    
    while (true) {
      std::optional<PrioritizedJob> job;
      
      {
        std::unique_lock lock(m_queueMutex);
        m_condition.wait(lock, [this] {
          return !m_running || !m_jobQueue.empty();
        });
        
        if (!m_running && m_jobQueue.empty()) {
          return;
        }
        
        if (!m_jobQueue.empty()) {
          job = std::move(m_jobQueue.front());
          m_jobQueue.pop_front();
        }
      }
      
      if (job) {
        job->func();
        m_totalJobsExecuted.fetch_add(1, std::memory_order_relaxed);
        
        if (m_pendingJobs.fetch_sub(1, std::memory_order_relaxed) == 1) {
          std::lock_guard lock(m_queueMutex);
          m_allDoneCondition.notify_all();
        }
      }
    }
  }
  
  std::vector<std::thread> m_workers;
  std::deque<PrioritizedJob> m_jobQueue;
  
  mutable std::mutex m_queueMutex;
  std::condition_variable m_condition;
  std::condition_variable m_allDoneCondition;
  
  std::atomic<bool> m_running{false};
  std::atomic<bool> m_initialized{false};
  std::atomic<size_t> m_pendingJobs{0};
  std::atomic<size_t> m_totalJobsExecuted{0};
  std::atomic<uint64_t> m_sequenceCounter{0};
  size_t m_threadCount{0};
};

/// @brief Global job system singleton
inline JobSystem& GetJobSystem() {
  static JobSystem instance;
  return instance;
}

/// @brief RAII helper for ensuring job system is initialized
class ScopedJobSystem {
public:
  explicit ScopedJobSystem(size_t numThreads = 0) {
    GetJobSystem().Initialize(numThreads);
  }
  ~ScopedJobSystem() {
    GetJobSystem().Shutdown();
  }
};

} // namespace Aetherion::Core
