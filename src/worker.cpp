/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "worker.h"
#include "registration.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <chrono>
#include <obs-module.h>

Worker::Worker() : thread_([this] { run(); }) {}
Worker::~Worker() { stop(); }

bool Worker::submit(Attempt attempt) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (stopping_)
    return false;
  last_ = attempt;
  if (pending_) {
    status_ = "Queue full; latest recording retained for explicit retry";
    return false;
  }
  pending_ = std::move(attempt);
  ready_.notify_one();
  return true;
}

bool Worker::retry() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (stopping_ || busy_ || pending_ || !last_)
    return false;
  pending_ = last_;
  ready_.notify_one();
  return true;
}

std::string Worker::status() {
  std::lock_guard<std::mutex> lock(mutex_);
  return status_;
}

void Worker::stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
    pending_.reset();
    ready_.notify_one();
  }
  if (thread_.joinable())
    thread_.join();
}

void Worker::run() {
  for (;;) {
    Attempt attempt;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      ready_.wait(lock, [this] { return stopping_ || pending_.has_value(); });
      if (stopping_)
        return;
      attempt = std::move(*pending_);
      pending_.reset();
      busy_ = true;
      status_ = "Registering finalized recording";
    }
    const auto started = std::chrono::steady_clock::now();
    const auto result = process(attempt);
    blog(LOG_INFO, "[PostProject] Worker elapsed %.3f ms",
         std::chrono::duration<double, std::milli>(
             std::chrono::steady_clock::now() - started)
             .count());
    blog(LOG_INFO, "[PostProject] %s", result.c_str());
    {
      std::lock_guard<std::mutex> lock(mutex_);
      status_ = last_ && last_->id == attempt.id ? result :
          "Earlier attempt finished; latest recording available for explicit retry";
      busy_ = false;
    }
  }
}

std::string Worker::process(const Attempt &attempt) {
  const auto path = QString::fromUtf8(attempt.path.c_str());
  if (!QFileInfo(path).isFile() || QFileInfo(path).size() == 0)
    return "Registration failed: recording is absent or empty; file untouched";
  QProcess probe;
  probe.start("ffprobe",
              {"-v", "error", "-select_streams", "v:0", "-show_entries",
               "stream=width,height:format=format_name,duration", "-of", "json",
               path});
  if (!probe.waitForFinished(30000)) {
    probe.kill();
    probe.waitForFinished();
    return "Registration failed: ffprobe unavailable or timed out; file "
           "untouched";
  }
  if (probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0)
    return "Registration failed: output is not readable media; file untouched";
  const auto root =
      QJsonDocument::fromJson(probe.readAllStandardOutput()).object();
  const auto streams = root["streams"].toArray();
  const auto format = root["format"].toObject();
  if (streams.isEmpty() ||
      !format["format_name"].toString().contains("matroska") ||
      format["duration"].toString().toDouble() <= 0)
    return "Registration failed: expected finalized Matroska video; file "
           "untouched";
  const auto video = streams.first().toObject();
  const auto width = video["width"].toInt();
  const auto height = video["height"].toInt();
  if (width <= 0 || height <= 0)
    return "Registration failed: unknown video dimensions; file untouched";
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopping_)
      return "Registration cancelled before fingerprinting; file untouched";
  }
  const recording facts = {
      attempt.path.c_str(), attempt.id.c_str(), attempt.version.c_str(),
      static_cast<std::uint64_t>(width), static_cast<std::uint64_t>(height)};
  registration_result result{};
  const auto status =
      register_recording(attempt.production.c_str(), &facts, &result);
  blog(LOG_INFO, "[PostProject] Staging %.3f ms; SQLite commit calls %.3f ms",
       result.staging_seconds * 1000, result.commit_seconds * 1000);
  if (status != PP_OK)
    return std::string("Registration failed; retry this attempt: ") +
           result.diagnostic;
  return "Recording registered successfully; file untouched";
}
