/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "registration.h"
#include <stdio.h>

#define CHECK(call) do { if ((call) != PP_OK) goto cleanup; } while (0)

int main(int argc, char **argv) {
  if (argc != 3) return 2;
  remove(argv[1]); /* Dedicated CTest fixture. */
  pp_production_t *production = NULL;
  pp_transaction_t *transaction = NULL;
  pp_media_source_t *source = NULL;
  pp_representation_set_t *representations = NULL;
  pp_activity_set_t *activities = NULL;
  pp_error_t *error = NULL;
  int result = 1;
  pp_uuid_t asset, original, proxy, owner;
  pp_representation_kind_t kind;
  pp_content_structure_kind_t structure;
  uint64_t members, resources, fingerprints;
  CHECK(pp_production_create(argv[1], "Readback recipe", &production, &error));
  CHECK(pp_production_begin_transaction(production, &transaction, &error));
  CHECK(pp_media_source_create_file(argv[2], &source, &error));
  CHECK(pp_transaction_import_media(transaction, source, NULL, &asset, &error));
  const pp_object_ref_t target = {PP_OBJECT_ASSET, asset};
  CHECK(pp_transaction_add_external_identifier(transaction, &target,
      "org.obsproject.Studio:recording-attempt", "first-stage-committed", "media", &error));
  CHECK(pp_transaction_commit(transaction, &error));
  pp_transaction_release(transaction);
  transaction = NULL;
  CHECK(pp_production_representations(production, &asset, &representations, &error));
  CHECK(pp_representation_set_get(representations, 0, &original, &owner, &kind,
                                  &structure, &members, &resources, &fingerprints, &error));
  pp_representation_set_release(representations);
  representations = NULL;
  /* Another writer can add a proxy between the two registration commits.
   * Set order does not identify which representation OBS actually captured. */
  CHECK(pp_production_begin_transaction(production, &transaction, &error));
  CHECK(pp_transaction_add_representation(transaction, &asset, PP_REPRESENTATION_PROXY,
                                          source, &proxy, &error));
  CHECK(pp_transaction_commit(transaction, &error));
  pp_transaction_release(transaction);
  transaction = NULL;
  const struct recording attempt = {argv[2], "first-stage-committed", "32.2.2", 64, 64};
  struct registration_result registered;
  if (register_recording(argv[1], &attempt, &registered) != PP_OK) goto cleanup;
  CHECK(pp_production_activities_producing(production, &original, &activities, &error));
  if (pp_activity_set_count(activities) != 1) goto cleanup;
  pp_activity_set_release(activities);
  activities = NULL;
  CHECK(pp_production_activities_producing(production, &proxy, &activities, &error));
  if (pp_activity_set_count(activities) != 0) goto cleanup;
  result = 0;
cleanup:
  pp_error_release(error);
  pp_activity_set_release(activities);
  pp_representation_set_release(representations);
  pp_media_source_release(source);
  pp_transaction_release(transaction);
  pp_production_release(production);
  return result;
}
