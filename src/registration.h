#ifndef POSTPROJECT_OBS_REGISTRATION_H
#define POSTPROJECT_OBS_REGISTRATION_H

#include <postproject/postproject.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Strings are borrowed for the call. All PostProject ownership stays in C. */
struct recording {
  const char *path;
  const char *attempt;
  const char *obs_version;
  uint64_t width;
  uint64_t height;
};

struct registration_result {
  pp_production_id_t production;
  pp_asset_id_t asset;
  /* Receipts for this call's commits only; retry reads may make no commit. */
  uint32_t commit_count;
  pp_commit_receipt_t commits[2];
  char diagnostic[512];
  double staging_seconds;
  double commit_seconds;
};

pp_error_code_t select_production(const char *path, int create,
                                  struct registration_result *result);
/* Worker-only: fingerprints the recording, stages facts, and commits. */
pp_error_code_t register_recording(const char *production_path,
                                   const struct recording *recording,
                                   struct registration_result *result);

#ifdef __cplusplus
}
#endif
#endif
