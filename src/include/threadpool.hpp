#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
 public:
  ThreadPool(int numThreads = std::thread::hardware_concurrency());
  ~ThreadPool();
  void enqueue(std::function<void()> task);

 private:
  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> tasks_;
  std::mutex queue_mutex_;
  std::condition_variable condition_;
  bool stop_;
};
#endif