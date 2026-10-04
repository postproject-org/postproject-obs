# OBS Studio per-target brief

Selected for direct C construction, ownership and shutdown evidence against
PostProject `0.6.0-alpha.1`, C ABI 37 and schema 17. Source checked against OBS
Studio `32.2.2`, commit `ba2f32bdf791005443988a4955e963663e16b1ed`, pinned in
[UPSTREAM](UPSTREAM). The accepted runtime is Linux x86_64.

## Current behavior and source seam

`frontend/widgets/OBSBasic_Recording.cpp` and
`frontend/utility/BasicOutputHandler.cpp` handle recording completion. The
frontend stopped event also occurs after failure. The recording output's
`stop` signal exposes status through `libobs/obs-output.c`; the plugin obtains
that output through `frontend/OBSStudioAPI.cpp`.

Status alone does not certify playable media. The accepted muxer-abort test
terminates the runner's own `obs-ffmpeg-mux` subprocess after it opens the file.
This pinned host reports a successful stop for the empty output. The adapter
validates the finalized file independently before registration.

`libobs/callback/signal.c` serializes signal dispatch and disconnect. The plugin
disconnects signals, releases the output reference and joins its worker before
leaving `OBS_FRONTEND_EVENT_EXIT`. No frontend call follows that boundary.

## User action and smallest integration

The user creates or chooses a production from the Tools menu before capture.
One ordinary finalized local Matroska recording becomes an original asset;
width/height metadata, an application-qualified attempt identifier, revision
origin and the observed capture activity are recorded through public APIs.
OBS retains scene, encoding, filename and playback authority.

All PostProject calls and cleanup live in `src/registration.c`, compiled as
C11. `src/plugin.cpp` and `src/worker.cpp` supply a C++17/Qt frontend bridge.
Callbacks copy bounded values into one pending attempt. One joined worker
probes, fingerprints and registers it away from capture/encode callbacks.

Import and capture registration are two separate commits. Readback selects a
unique original single-resource representation; it refuses ambiguity and more
than 256 representations. A proxy inserted between commits cannot become the
capture output. Explicit retry reuses qualified attempt identities and can
finish the second fact after the first commit succeeds.

Normal-path family targets are production lifecycle, transaction lifecycle and
asset point reads. This route supplies raw-C ergonomics; the Qt shim does not
count as another consumer or as C++ Result projection evidence. Rollback,
lost acknowledgement and repeated notifications have separate contract traces.

## Build, ownership and removal

The adapter finds an installed package through `postproject.pc`; consumers
invoke no Cargo. The plugin needs OBS/libobs/frontend API 32.2.2 and Qt6 Widgets.
`BUILD_OBS_PLUGIN=OFF` builds the same C adapter and fixtures without the host.
[README](README.md#build-and-install) gives exact build commands.

C owns and releases every acquired handle. Borrowed strings stay within their
result owners' lifetimes; only copied IDs and bounded diagnostics cross into
the worker. Pre-commit errors roll back. Commit failure is terminal; retry
starts a fresh transaction. A later latest revision is never labelled as this
adapter's commit receipt.

Removing the plugin restores ordinary recording. Missing library or production
access preserves media. The adapter never deletes, rewrites or moves recordings.
Shutdown joins running work; fingerprinting or commit can delay unload. Attempt
identity is session-local, without crash-safe exactly-once delivery.

## Acceptance and selection decision

`tests/run.py` drives real recording and menu actions under Xvfb with isolated
settings. Successful recordings are fully decoded with FFmpeg. Cases cover
registration, explicit retry, production loss, shutdown during work, absent
plugin/library/production and failed muxer output. Failure creates no success
fact. `tests/registration.c` and `tests/readback.c` exercise the actual C
adapter's rollback, retry and original/proxy readback.

`tests/handoff_blender.py` exercises the maintained Blender extension against
the same production: adopt the original recording, preserve capture facts,
resolve a move and save/reopen the filename fallback with the extension disabled.
The [reproduction commands](README.md#reproduce-acceptance-and-the-handoff)
separate normal, failure and contract evidence.

This pilot closes the missing raw-C construction/cleanup route. An insufficient
completion hook or a worker that cannot be joined would block selection.
Both were checked in the accepted host. Split recording, remux, replay, streams
and persistent retry queues remain outside this slice. Automated Linux
acceptance does not imply an interactive human pass or upstream endorsement.
