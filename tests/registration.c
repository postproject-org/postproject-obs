#include "registration.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  remove(argv[1]);
  struct registration_result result = {0};
  if (select_production(argv[1], 1, &result) != PP_OK ||
      select_production(argv[1], 0, &result) != PP_OK)
    return 3;
  struct recording attempt = {"/absent/recording.mkv", "test-attempt", "32.2.2",
                              64, 64};
  if (register_recording(argv[1], &attempt, &result) == PP_OK ||
      result.diagnostic[0] == '\0')
    return 4;
  /* Failed staging left neither an asset nor a retained transaction guard. */
  attempt.path = argv[2];
  if (register_recording(argv[1], &attempt, &result) != PP_OK) {
    fprintf(stderr, "%s\n", result.diagnostic);
    return 5;
  }
  const pp_uuid_t first = result.asset;
  for (int retry = 0; retry != 2; ++retry) {
    /* Includes commit succeeded but caller discarded the acknowledgement. */
    if (register_recording(argv[1], &attempt, &result) != PP_OK ||
        memcmp(first.bytes, result.asset.bytes, sizeof(first.bytes)) != 0)
      return 6;
  }
  pp_production_t *production = NULL;
  pp_asset_set_t *assets = NULL;
  pp_error_t *error = NULL;
  int result_code = 0;
  if (pp_production_open(argv[1], &production, &error) != PP_OK ||
      pp_production_assets(production, &assets, &error) != PP_OK ||
      pp_asset_set_count(assets) != 1)
    result_code = 7;
  pp_error_release(error);
  pp_asset_set_release(assets);
  pp_production_release(production);
  return result_code;
}
