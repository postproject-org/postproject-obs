#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

struct Attempt {
  std::string production;
  std::string path;
  std::string id;
  std::string version;
};

// One registration at a time. Shutdown joins the thread before module unload.
class Worker {
public:
  Worker();
  ~Worker();
  bool submit(Attempt attempt);
  bool retry();
  std::string status();
  void stop();
  Worker(const Worker &) = delete;
  Worker &operator=(const Worker &) = delete;

private:
  void run();
  std::string process(const Attempt &attempt);
  std::mutex mutex_;
  std::condition_variable ready_;
  bool stopping_ = false;
  bool busy_ = false;
  std::optional<Attempt> pending_;
  std::optional<Attempt> last_;
  std::string status_ = "No recording registered in this session";
  std::thread thread_;
};
