/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "registration.h"
#include "worker.h"

#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QUuid>
#include <chrono>
#include <memory>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/config-file.h>

OBS_DECLARE_MODULE()

namespace {
std::unique_ptr<Worker> worker;
std::string production;
obs_output_t *recording_output = nullptr;
Attempt current;
QAction *select_action = nullptr;
QAction *create_action = nullptr;
QAction *retry_action = nullptr;
bool frontend_alive = true;

void remove_actions() {
  delete select_action;
  delete create_action;
  delete retry_action;
  select_action = create_action = retry_action = nullptr;
}

void stopped(void *, calldata_t *data) {
  const auto started = std::chrono::steady_clock::now();
  // Called under libobs's signal mutex on the output thread. No frontend I/O.
  if (calldata_int(data, "code") == OBS_OUTPUT_SUCCESS && worker) {
    if (!worker->submit(current))
      blog(LOG_WARNING,
           "[PostProject] Queue full or closing; recording untouched: %s",
           current.path.c_str());
  } else {
    blog(LOG_WARNING, "[PostProject] Failed recording was not registered");
  }
  blog(LOG_INFO, "[PostProject] Stop callback enqueue elapsed %.3f ms",
       std::chrono::duration<double, std::milli>(
           std::chrono::steady_clock::now() - started)
           .count());
}

void disconnect_output() {
  if (recording_output) {
    // disconnect takes the same mutex as signal dispatch and quiesces
    // stopped().
    signal_handler_disconnect(obs_output_get_signal_handler(recording_output),
                              "stop", stopped, nullptr);
    obs_output_release(recording_output);
    recording_output = nullptr;
  }
}

void choose(const QString &path, bool create) {
  if (path.isEmpty())
    return;
  registration_result result{};
  const auto encoded = path.toUtf8();
  if (select_production(encoded.constData(), create, &result) != PP_OK) {
    QMessageBox::warning(nullptr, "PostProject", result.diagnostic);
    return;
  }
  production = encoded.constData();
  blog(LOG_INFO, "[PostProject] Production explicitly selected");
}

void event(obs_frontend_event event, void *) {
  if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
    const auto path = qEnvironmentVariable("POSTPROJECT_OBS_PRODUCTION");
    if (!path.isEmpty())
      choose(path, qEnvironmentVariableIntValue("POSTPROJECT_OBS_CREATE") == 1);
  } else if (event == OBS_FRONTEND_EVENT_RECORDING_STARTED) {
    disconnect_output();
    if (production.empty())
      return;
    recording_output = obs_frontend_get_recording_output();
    if (!recording_output)
      return;
    obs_data_t *settings = obs_output_get_settings(recording_output);
    const char *path = obs_data_get_string(settings, "path");
    const bool supported =
        std::string(obs_output_get_id(recording_output)) == "ffmpeg_muxer" &&
        !obs_data_get_bool(settings, "split_file") &&
        !config_get_bool(obs_frontend_get_profile_config(), "Video",
                         "AutoRemux") &&
        QString::fromUtf8(path).endsWith(".mkv", Qt::CaseInsensitive) &&
        std::char_traits<char>::length(path) < 4096;
    if (supported) {
      current = {production, path, QUuid::createUuid().toString().toStdString(),
                 obs_get_version_string()};
      signal_handler_connect(obs_output_get_signal_handler(recording_output),
                             "stop", stopped, nullptr);
    } else {
      blog(
          LOG_WARNING,
          "[PostProject] Only single-file Matroska without remux is supported");
    }
    obs_data_release(settings);
  } else if (event == OBS_FRONTEND_EVENT_EXIT) {
    disconnect_output();
    worker->stop();
    obs_frontend_remove_event_callback(::event, nullptr);
    remove_actions();
    frontend_alive = false;
    blog(LOG_INFO, "[PostProject] Worker joined before frontend exit boundary");
  }
}
} // namespace

bool obs_module_load(void) {
  worker = std::make_unique<Worker>();
  select_action = static_cast<QAction *>(
      obs_frontend_add_tools_menu_qaction("Choose PostProject production…"));
  create_action = static_cast<QAction *>(
      obs_frontend_add_tools_menu_qaction("Create PostProject production…"));
  retry_action = static_cast<QAction *>(
      obs_frontend_add_tools_menu_qaction("PostProject status / retry…"));
  QObject::connect(select_action, &QAction::triggered, [] {
    choose(QFileDialog::getOpenFileName(nullptr, "Choose production", {},
                                        "PostProject (*.pproj)"),
           false);
  });
  QObject::connect(create_action, &QAction::triggered, [] {
    choose(QFileDialog::getSaveFileName(nullptr, "Create production", {},
                                        "PostProject (*.pproj)"),
           true);
  });
  QObject::connect(retry_action, &QAction::triggered, [] {
    if (QMessageBox::question(nullptr, "PostProject",
                              QString::fromStdString(worker->status()) +
                                  "\nRetry the last attempt?") ==
            QMessageBox::Yes &&
        !worker->retry())
      QMessageBox::information(nullptr, "PostProject",
                               "Busy, closing, or no attempt available");
  });
  obs_frontend_add_event_callback(event, nullptr);
  return true;
}

void obs_module_unload(void) {
  if (frontend_alive) {
    obs_frontend_remove_event_callback(event, nullptr);
    remove_actions();
  }
  disconnect_output();
  if (worker)
    worker->stop();
  worker.reset();
}
