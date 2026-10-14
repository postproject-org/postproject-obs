"""An actual OBS recording is adopted and resolved by the maintained Blender path."""

import shutil
import sys
from pathlib import Path

import bpy

package, directory = sys.argv[sys.argv.index("--") + 1 :]
root = Path(directory).resolve()
module = "bl_ext.user_default.postproject_relink"
bpy.ops.extensions.package_install_files(
    filepath=str(Path(package).resolve()), repo="user_default", enable_on_install=True
)
bpy.context.preferences.addons[module].preferences.production_path = str(
    root / "shared.pproj"
)
import postproject as pp

with pp.Production.open(root / "shared.pproj") as production:
    with production.read_session() as view:
        (original_asset,) = tuple(view.assets())
    # This capture fixture starts with exactly one original representation.
    (original_representation,) = tuple(
        item for item in production.representations[original_asset.id]
        if item.kind is pp.RepresentationKind.ORIGINAL
    )
    assert len(production.activities_producing[original_representation.id]) == 1

(recording,) = tuple((root / "recordings").glob("*.mkv"))
bpy.ops.wm.read_homefile(use_empty=True)
scene = bpy.context.scene
scene.sequence_editor_create().strips.new_movie("OBS recording", str(recording), 1, 1)
project = root / "receiving.blend"
bpy.ops.wm.save_as_mainfile(filepath=str(project))
strip = scene.sequence_editor.strips_all["OBS recording"]
strip_uuid = strip["postproject_uuid"]
with pp.Production.open(root / "shared.pproj") as production:
    with production.read_session() as view:
        (adopted,) = tuple(view.assets())
    assert adopted.id == original_asset.id
    assert production.representation(original_representation.id).asset_id == adopted.id
    assert any(
        identifier.qualifier == "org.blender:strip_uuid"
        and identifier.value == strip_uuid
        for identifier in production.external_identifiers[pp.AssetRef(adopted.id)]
    )
    assert len(production.activities_producing[original_representation.id]) == 1
    properties = {item.property.property for item in production.metadata[pp.AssetRef(adopted.id)]}
    assert {"video_width", "video_height"} <= properties

(root / "moved").mkdir()
moved = root / "moved" / recording.name
shutil.move(recording, moved)
assert bpy.ops.postproject.find_missing_media() == {"FINISHED"}
assert Path(bpy.path.abspath(strip.filepath)) == moved
bpy.ops.wm.save_as_mainfile(filepath=str(project))
bpy.ops.preferences.addon_disable(module=module)
bpy.ops.wm.open_mainfile(filepath=str(project))
strip = bpy.context.scene.sequence_editor.strips_all["OBS recording"]
assert strip["postproject_uuid"] == strip_uuid
assert Path(bpy.path.abspath(strip.filepath)) == moved
print(
    "OBS → Blender: adopted original identity, preserved capture/metadata, resolved move, reopened without plugin"
)
