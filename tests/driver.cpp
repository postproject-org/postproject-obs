// Host automation only: no PostProject calls, registration, or media stand-in.
#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QFile>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>
#include <QWidget>
#include <obs-frontend-api.h>
#include <obs-module.h>

OBS_DECLARE_MODULE()

static void event(obs_frontend_event event, void *) {
  if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
    for (unsigned channel = 1; channel != 6; ++channel)
      obs_set_output_source(channel, nullptr);
    obs_scene_t *scene = obs_scene_create("PostProject recording fixture");
    obs_data_t *settings = obs_data_create();
    obs_data_set_int(settings, "width", 64);
    obs_data_set_int(settings, "height", 64);
    obs_data_set_int(settings, "color", 0xff4080ff);
    obs_source_t *source = obs_source_create("color_source_v3", "Color fixture",
                                             settings, nullptr);
    obs_scene_add(scene, source);
    obs_frontend_set_current_scene(obs_scene_get_source(scene));
    obs_source_release(source);
    obs_data_release(settings);
    obs_scene_release(scene);
    QTimer::singleShot(1000, [] { obs_frontend_recording_start(); });
  } else if (event == OBS_FRONTEND_EVENT_RECORDING_STARTED) {
    if (qEnvironmentVariable("POSTPROJECT_OBS_TEST_MODE") == "failure") {
      const auto path = qEnvironmentVariable("POSTPROJECT_OBS_PRODUCTION");
      QFile::rename(path, path + ".offline");
    }
    QTimer::singleShot(1500, [] { obs_frontend_recording_stop(); });
  } else if (event == OBS_FRONTEND_EVENT_RECORDING_STOPPED) {
    const auto mode = qEnvironmentVariable("POSTPROJECT_OBS_TEST_MODE");
    blog(LOG_INFO, "[Driver] Stopped; test mode=%s", mode.toUtf8().constData());
    if (mode == "retry") {
      QTimer::singleShot(2000, [] {
        auto *window = static_cast<QWidget *>(obs_frontend_get_main_window());
        QList<QAction *> actions;
        for (auto *menu : window->findChildren<QMenu *>())
          actions.append(menu->actions());
        for (auto *action : actions) {
          if (!action->text().startsWith("PostProject status / retry"))
            continue;
          QTimer::singleShot(50, [] {
            for (auto *widget : QApplication::topLevelWidgets()) {
              if (auto *box = qobject_cast<QMessageBox *>(widget))
                box->button(QMessageBox::Yes)->click();
            }
          });
          action->trigger();
          break;
        }
      });
    }
    QTimer::singleShot(mode == "shutdown" ? 0 : 4000, [] {
      static_cast<QWidget *>(obs_frontend_get_main_window())->close();
    });
  } else if (event == OBS_FRONTEND_EVENT_EXIT) {
    obs_frontend_remove_event_callback(::event, nullptr);
  }
}

bool obs_module_load(void) {
  obs_frontend_add_event_callback(event, nullptr);
  return true;
}
