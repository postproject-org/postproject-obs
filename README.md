# PostProject finalized-recording pilot for OBS Studio

An optional frontend plugin for **OBS Studio 32.2.2** and installed
**PostProject 0.9.0-alpha.1 SDK** (C ABI 57, schema 27). All PostProject calls and
cleanup compile as C11 in `src/registration.c`. The C++17 shim supplies OBS/Qt
menus, output notifications and one joined worker. No media-processing loop
calls PostProject. The host source revision is pinned in `UPSTREAM`.

The [per-target brief](BRIEF.md) records the checked source seam, selection,
ownership, family targets and acceptance boundary.
Linux qualification against this SDK passed both native contracts, all eight
isolated host scenarios, and the recording handoff to Blender 5.2.2 LTS.
Use matching installed headers and library.

## Record and register

Use **Tools → Create PostProject production…** or **Choose PostProject
production…** before recording. OBS retains scene, encoding and file authority.
This pilot supports one finalized local Matroska recording with the simple
FFmpeg muxer, without splitting or automatic remuxing. It does not cover streams,
replay buffers, remux outputs or crash-recovery queues.

The successful output `stop` signal copies an already bounded pathname and
attempt identifier into the adapter worker. A generic frontend stop event is
not taken as proof of successful recording. The worker checks a nonempty file
and uses bounded `ffprobe` execution to verify readable Matroska video with
positive duration and dimensions. It then fingerprints/imports the file,
records a qualified OBS attempt identifier, width/height metadata and revision
origin. This staging may perform substantial file I/O; it is separate from
the SQLite commit calls and runs off capture/encode callbacks.

Output registration stages the recording, its capture activity and both qualified
attempt identifiers in one edit. Zero represented inputs honestly model unknown
live devices; no exact input snapshot or job claim is invented. Retry returns
the same identities without another commit. Earlier recordings with only their
media fact remain repairable: readback selects a unique original single-resource
representation before recording capture. More than 256 representations or an
ambiguous original is reported for explicit review.
Each phase reads one coherent view and carries its detached base into the edit.
Views close before fingerprinting or commit. Successful commits return their
own receipts; an already registered retry returns no new commit receipt.

**Tools → PostProject status / retry…** shows the last result and explicitly
retries the latest retained attempt. Both the media and capture facts have
qualified attempt identifiers. The accepted retry and lost-acknowledgement
tests reuse the same IDs without duplicating those facts. Attempt identity is
volatile and lasts until a newer notification or plugin unload. It is not
exactly-once registration across crashes, simultaneous processes or arbitrary
pathname reuse.

One worker and one pending attempt bound work. A full queue retains the latest
rejected attempt for explicit retry and logs the rejection. Shutdown stops
accepting work, clears pending work, disconnects output signals and joins the
worker before the frontend exit boundary. Already running fingerprinting or
commit must finish; no timeout detaches work into an unloaded plugin. Probe
timeout is 30 seconds. Worker elapsed, staging/commit time and callback enqueue
time are informational log measurements, not hard performance budgets.

## Build and install

```sh
PKG_CONFIG_PATH=/absolute/postproject/install/lib/pkgconfig \
  cmake -S . -B build -DBUILD_HOST_DRIVER=ON -DCMAKE_SKIP_BUILD_RPATH=ON
cmake --build build
LD_LIBRARY_PATH=/absolute/postproject/install/lib \
  ctest --test-dir build --output-on-failure
```

Requirements: CMake 3.21, C11/C++17, `pkg-config`, installed PostProject,
OBS/libobs/frontend API 32.2.2 development packages and Qt6 Widgets.
`BUILD_OBS_PLUGIN=OFF` builds the same C adapter and contract test without OBS.
No downstream target invokes Cargo or includes internal Rust headers.

On Linux, copy `build/libpostproject-obs.so` into
`~/.config/obs-studio/plugins/postproject-obs/bin/64bit/`, create the sibling
`data/` directory, and make the PostProject library available to the loader.
For a development prefix, launch OBS with `LD_LIBRARY_PATH` pointing to its
`lib/` directory. The host driver is acceptance automation; do not install it
in a normal user profile. Removing the optional plugin restores ordinary OBS.

Without a plugin or loadable PostProject library, ordinary recording remains
available. Without a chosen production, the plugin observes no capture. If
production access or registration fails, it reports the error and leaves the
playable recording untouched. It never deletes, rewrites or moves that file.

## Reproduce acceptance and the handoff

With OBS 32.2.2, Xvfb, FFmpeg/ffprobe and PostProject's maintained Python binding
installed for independent public readback:

```sh
python tests/run.py /tmp/obs-evidence \
  --library /absolute/postproject/install/lib/libpostproject.so
```

The root must not exist. Each run uses isolated OBS settings and Xvfb, creates
a genuine color-source recording through normal recording start/stop actions
and requires OBS exit status zero. Successful recordings are fully decoded
with FFmpeg. Cases
cover ordinary registration, the real retry menu, production loss after capture
starts, close during registration, missing plugin, missing library and no chosen
production. A separate recording-failure case terminates the runner's own muxer
after it opens the recording. This pinned host still reports a successful stop
for that aborted output; the worker's media validation rejects it and records
no asset or activity. Recording validation and database registration failures
have distinct diagnostics. No playback claim is made for the failed output.
The dependency-loss case requires the RPATH-free build shown above.
`tests/registration.c` reuses the actual C adapter for staging failure/rollback
and repeated notification/lost acknowledgement. Contract traces are separate
from real-host normal and failure traces.

With the maintained Blender extension built using the same SDK wheel:

```sh
BLENDER_USER_RESOURCES=/tmp/obs-blender-profile \
POSTPROJECT_ABI_TRACE=/tmp/obs-evidence/normal/blender-handoff.txt \
  /absolute/blender --background --factory-startup --python-exit-code 1 \
  --python tests/handoff_blender.py -- /absolute/extension.zip /tmp/obs-evidence/normal
```

Blender's normal save integration adopts the original recording asset, preserves
capture provenance/metadata, resolves a moved recording through its normal
action and saves/reopens the filename fallback with the extension disabled.
It reads no OBS configuration or project data.

## Ownership and source checks

The C adapter owns each production, transaction, media source, result set,
metadata input and error, and releases each with its matching public function.
Strings from asset/representation results are borrowed until their owning set
is released. Only copied IDs, timings and a bounded diagnostic leave C. No
host allocator frees PostProject memory. Pre-commit failure rolls back;
commit success or failure is terminal and releases the handle without rollback.
Retries acquire fresh handles. Neither transaction records a later
`latest_revision` as its own commit identity.

Pinned `frontend/OBSStudioAPI.cpp` defines menu/output ownership;
`libobs/obs-output.c` supplies successful completion after muxer termination;
`libobs/callback/signal.c` serializes signal disconnect with dispatch.
The adapter disconnects and releases its output reference before joining on
`OBS_FRONTEND_EVENT_EXIT`. No frontend calls occur afterward.

Accepted host/platform scope is the pinned Linux plugin route. Automated tests
drive a real graphical OBS under Xvfb; they do not claim an interactive human
pass or upstream endorsement. Sources are GPL-2.0-or-later.

## Distribution policy

Reviewed 2026-10-12: the [OBS forum resource policy](https://obsproject.com/forum/threads/forum-resource-and-ip-policy.178569/)
does not permit resources written largely with AI coding tools and requires
disclosure for any tool assistance. Review the policy before requesting a
forum listing. This experimental repository follows the normal license and
branding requirements. The pilot is independent of the OBS Project;
no listing, endorsement or upstream submission is claimed.
