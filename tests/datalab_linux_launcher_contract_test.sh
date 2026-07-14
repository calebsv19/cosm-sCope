#!/usr/bin/env sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)"
FIXTURE="$(mktemp -d "${TMPDIR:-/tmp}/datalab-linux-launcher.XXXXXX")"
cleanup() { rm -rf "$FIXTURE"; }
trap cleanup EXIT HUP INT TERM

PACKAGE="$FIXTURE/sCope-0.3.0-linux-x86_64-desktop-stable"
mkdir -p "$PACKAGE/bin" "$PACKAGE/resources/shared/assets/fonts" "$PACKAGE/resources/vk_renderer/shaders" \
    "$PACKAGE/resources/shaders" "$FIXTURE/home" "$FIXTURE/xdg-data" "$FIXTURE/xdg-state" "$FIXTURE/xdg-config"
cp "$ROOT/tools/packaging/linux/datalab-launcher" "$PACKAGE/bin/datalab-launcher"
cp "$ROOT/tools/packaging/linux/install-desktop-entry.sh" "$PACKAGE/share-install-desktop-entry.sh"
mkdir -p "$PACKAGE/share/icons/hicolor/scalable/apps"
cp "$ROOT/tools/packaging/linux/icons/scope.svg" "$PACKAGE/share/icons/hicolor/scalable/apps/scope.svg"
cp "$ROOT/tools/packaging/linux/scope.desktop" "$PACKAGE/share/scope.desktop"
printf '#!/usr/bin/env sh\nexit 0\n' > "$PACKAGE/bin/datalab-bin"
chmod +x "$PACKAGE/bin/datalab-launcher" "$PACKAGE/bin/datalab-bin"
touch "$PACKAGE/resources/shared/assets/fonts/Lato-Regular.ttf" "$PACKAGE/resources/vk_renderer/shaders/textured.vert.spv" "$PACKAGE/resources/shaders/textured.vert.spv"

OUTPUT="$FIXTURE/self-test.txt"
HOME="$FIXTURE/home" XDG_DATA_HOME="$FIXTURE/xdg-data" XDG_STATE_HOME="$FIXTURE/xdg-state" XDG_CONFIG_HOME="$FIXTURE/xdg-config" \
    "$PACKAGE/bin/datalab-launcher" --self-test > "$OUTPUT"
rg -q "^DATALAB_RUNTIME_DIR=$FIXTURE/xdg-data/sCope/runtime$" "$OUTPUT"
rg -q "^DATALAB_CONFIG_DIR=$FIXTURE/xdg-config/sCope$" "$OUTPUT"
rg -q "^LOG_FILE=$FIXTURE/xdg-state/sCope/logs/launcher.log$" "$OUTPUT"

mkdir -p "$PACKAGE/share/icons/hicolor/scalable/apps" "$PACKAGE/share/applications"
mv "$PACKAGE/share-install-desktop-entry.sh" "$PACKAGE/share/install-desktop-entry.sh"
chmod +x "$PACKAGE/share/install-desktop-entry.sh"
HOME="$FIXTURE/home" XDG_DATA_HOME="$FIXTURE/xdg-data" "$PACKAGE/share/install-desktop-entry.sh" >/dev/null
test -f "$FIXTURE/xdg-data/applications/scope.desktop"
test -f "$FIXTURE/xdg-data/icons/hicolor/scalable/apps/scope.svg"
rg -q '^Exec=/.*sCope-0\.3\.0-linux-x86_64-desktop-stable/bin/datalab-launcher$' "$FIXTURE/xdg-data/applications/scope.desktop"
echo "datalab Linux launcher contract passed"
