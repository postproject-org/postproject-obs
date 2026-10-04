/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "registration.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* Informational wall time only; correctness never depends on the clock. */
static double now(void) {
  struct timespec value = {0};
  if (timespec_get(&value, TIME_UTC) != TIME_UTC)
    return 0;
  return (double)value.tv_sec + (double)value.tv_nsec / 1000000000.0;
}

static double elapsed(double start) {
  double duration = now() - start;
  return duration > 0 ? duration : 0;
}

static pp_error_code_t commit(pp_transaction_t *transaction,
                              struct registration_result *result,
                              pp_error_t **error) {
  double start = now();
  pp_error_code_t status = pp_transaction_commit(transaction, error);
  result->commit_seconds += elapsed(start);
  return status;
}

static const char *const scheme = "org.obsproject.Studio:recording-attempt";

static void diagnostic(struct registration_result *result, pp_error_t *error,
                       const char *fallback) {
  snprintf(result->diagnostic, sizeof(result->diagnostic), "%s",
           error == NULL ? fallback : pp_error_message(error));
}

pp_error_code_t select_production(const char *path, int create,
                                  struct registration_result *result) {
  pp_production_t *production = NULL;
  pp_error_t *error = NULL;
  memset(result, 0, sizeof(*result));
  pp_error_code_t status =
      create ? pp_production_create(path, "OBS recordings", &production, &error)
             : pp_production_open(path, &production, &error);
  if (status == PP_OK)
    status = pp_production_id(production, &result->asset, &error);
  if (status != PP_OK)
    diagnostic(result, error, "Could not select production");
  pp_error_release(error);
  pp_production_release(production);
  return status;
}

static pp_error_code_t find_attempt(pp_production_t *production,
                                    const char *attempt, const char *qualifier,
                                    pp_object_kind_t kind, pp_uuid_t *id,
                                    int *found, pp_error_t **error) {
  pp_object_ref_set_t *matches = NULL;
  *found = 0;
  pp_error_code_t status = pp_production_find_by_external_identifier(
      production, scheme, attempt, qualifier, &matches, error);
  if (status == PP_OK) {
    uint64_t count = pp_object_ref_set_count(matches);
    if (count > 1) {
      status = PP_ERROR_CONFLICT;
    } else if (count == 1) {
      pp_object_ref_t target = {0};
      status = pp_object_ref_set_get(matches, 0, &target, error);
      if (status == PP_OK && target.kind != kind)
        status = PP_ERROR_CONFLICT;
      if (status == PP_OK) {
        *id = target.id;
        *found = 1;
      }
    }
  }
  pp_object_ref_set_release(matches);
  return status;
}

static pp_error_code_t add_number(pp_transaction_t *transaction,
                                  const pp_object_ref_t *target,
                                  const char *property, uint64_t number,
                                  pp_error_t **error) {
  pp_metadata_input_t *input = NULL;
  pp_error_code_t status = pp_metadata_input_create_u64(number, &input, error);
  if (status == PP_OK)
    status = pp_transaction_add_metadata_value(
        transaction, target, "org.obsproject.Studio", property, input, error);
  pp_metadata_input_release(input);
  return status;
}

/* Every failure before commit rolls back. Commit failure is already terminal.
 */
#define CHECK(call)                                                            \
  do {                                                                         \
    status = (call);                                                           \
    if (status != PP_OK)                                                       \
      goto cleanup;                                                            \
  } while (0)

pp_error_code_t register_recording(const char *production_path,
                                   const struct recording *recording,
                                   struct registration_result *result) {
  pp_production_t *production = NULL;
  pp_transaction_t *transaction = NULL;
  pp_media_source_t *source = NULL;
  pp_representation_set_t *representations = NULL;
  pp_asset_set_t *assets = NULL;
  pp_error_t *error = NULL;
  pp_error_code_t status = PP_OK;
  int transaction_open = 0;
  int found = 0;
  memset(result, 0, sizeof(*result));
  CHECK(pp_production_open(production_path, &production, &error));
  CHECK(find_attempt(production, recording->attempt, "media", PP_OBJECT_ASSET,
                     &result->asset, &found, &error));
  if (!found) {
    const double staging_started = now();
    CHECK(pp_production_begin_transaction(production, &transaction, &error));
    transaction_open = 1;
    CHECK(pp_transaction_set_revision_context(
        transaction, "org.obsproject.Studio", recording->obs_version, NULL,
        "Register finalized recording", &error));
    CHECK(pp_media_source_create_file(recording->path, &source, &error));
    CHECK(pp_transaction_import_media(transaction, source, NULL, &result->asset,
                                      &error));
    const pp_object_ref_t target = {PP_OBJECT_ASSET, result->asset};
    CHECK(pp_transaction_add_external_identifier(
        transaction, &target, scheme, recording->attempt, "media", &error));
    CHECK(add_number(transaction, &target, "video_width", recording->width,
                     &error));
    CHECK(add_number(transaction, &target, "video_height", recording->height,
                     &error));
    transaction_open = 0;
    result->staging_seconds += elapsed(staging_started);
    CHECK(commit(transaction, result, &error));
    pp_transaction_release(transaction);
    transaction = NULL;
  }

  /* Import returns the asset ID; its representation is visible after commit.
   * Capture provenance is a second atomic fact, retried by its own identifier.
   */
  CHECK(pp_production_asset(production, &result->asset, &assets, &error));
  if (pp_asset_set_count(assets) != 1) {
    status = PP_ERROR_CONFLICT;
    goto cleanup;
  }
  pp_uuid_t read_id = {0};
  int64_t created_at = 0;
  const char *name = NULL;
  const char *import_source = NULL;
  CHECK(pp_asset_set_get(assets, 0, &read_id, &created_at, &name,
                         &import_source, &error));
  if (memcmp(read_id.bytes, result->asset.bytes, sizeof(read_id.bytes)) != 0) {
    status = PP_ERROR_CONFLICT;
    goto cleanup;
  }
  pp_uuid_t activity_id = {0};
  CHECK(find_attempt(production, recording->attempt, "capture",
                     PP_OBJECT_ACTIVITY, &activity_id, &found, &error));
  if (found)
    goto cleanup;
  CHECK(pp_production_representations(production, &result->asset,
                                      &representations, &error));
  pp_uuid_t representation = {0};
  pp_uuid_t asset = {0};
  pp_representation_kind_t kind = 0;
  pp_content_structure_kind_t structure = 0;
  uint64_t members = 0, resources = 0, fingerprints = 0;
  const size_t representation_count = pp_representation_set_count(representations);
  size_t originals = 0;
  if (representation_count > 256) {
    status = PP_ERROR_CONFLICT;
    goto cleanup;
  }
  for (size_t index = 0; index < representation_count; ++index) {
    pp_uuid_t candidate = {0};
    CHECK(pp_representation_set_get(representations, index, &candidate, &asset,
                                    &kind, &structure, &members, &resources,
                                    &fingerprints, &error));
    if (kind == PP_REPRESENTATION_ORIGINAL) {
      ++originals;
      if (structure != PP_CONTENT_SINGLE_RESOURCE) {
        status = PP_ERROR_CONFLICT;
        goto cleanup;
      }
      representation = candidate;
    }
  }
  if (originals != 1) {
    status = PP_ERROR_CONFLICT;
    goto cleanup;
  }
  const double staging_started = now();
  CHECK(pp_production_begin_transaction(production, &transaction, &error));
  transaction_open = 1;
  CHECK(pp_transaction_set_revision_context(
      transaction, "org.obsproject.Studio", recording->obs_version, NULL,
      "Record observed capture", &error));
  const pp_activity_edge_t output = {representation, NULL};
  CHECK(pp_transaction_create_activity(
      transaction, "org.obsproject.Studio:capture", NULL, 0, &output, 1, NULL,
      NULL, "OBS Studio", recording->obs_version, "https://obsproject.com/",
      NULL, NULL, NULL, NULL, &activity_id, &error));
  const pp_object_ref_t activity = {PP_OBJECT_ACTIVITY, activity_id};
  CHECK(pp_transaction_add_external_identifier(
      transaction, &activity, scheme, recording->attempt, "capture", &error));
  transaction_open = 0;
  result->staging_seconds += elapsed(staging_started);
  CHECK(commit(transaction, result, &error));

cleanup:
  if (status != PP_OK)
    diagnostic(result, error,
               "Recording attempt has ambiguous production facts");
  if (transaction_open) {
    pp_error_t *rollback_error = NULL;
    (void)pp_transaction_rollback(transaction, &rollback_error);
    pp_error_release(rollback_error);
  }
  pp_error_release(error);
  pp_transaction_release(transaction);
  pp_media_source_release(source);
  pp_representation_set_release(representations);
  pp_asset_set_release(assets);
  pp_production_release(production);
  return status;
}
