#!/bin/bash
# generate_icns.sh - Converts a 1024x1024 PNG into a macOS .icns file
set -e

SOURCE_PNG="$1"
OUTPUT_ICNS="$2"

if [ -z "$SOURCE_PNG" ] || [ -z "$OUTPUT_ICNS" ]; then
    echo "Usage: $0 input.png output.icns"
    exit 1
fi

ICONSET="tmp_icon.iconset"
mkdir -p "$ICONSET"

echo "Creating icon set from $SOURCE_PNG..."

# Standard resolutions
sips -s format png -z 16 16     "$SOURCE_PNG" --out "$ICONSET/icon_16x16.png" > /dev/null
sips -s format png -z 32 32     "$SOURCE_PNG" --out "$ICONSET/icon_16x16@2x.png" > /dev/null
sips -s format png -z 32 32     "$SOURCE_PNG" --out "$ICONSET/icon_32x32.png" > /dev/null
sips -s format png -z 64 64     "$SOURCE_PNG" --out "$ICONSET/icon_32x32@2x.png" > /dev/null
sips -s format png -z 128 128   "$SOURCE_PNG" --out "$ICONSET/icon_128x128.png" > /dev/null
sips -s format png -z 256 256   "$SOURCE_PNG" --out "$ICONSET/icon_128x128@2x.png" > /dev/null
sips -s format png -z 256 256   "$SOURCE_PNG" --out "$ICONSET/icon_256x256.png" > /dev/null
sips -s format png -z 512 512   "$SOURCE_PNG" --out "$ICONSET/icon_256x256@2x.png" > /dev/null
sips -s format png -z 512 512   "$SOURCE_PNG" --out "$ICONSET/icon_512x512.png" > /dev/null
sips -s format png -z 1024 1024 "$SOURCE_PNG" --out "$ICONSET/icon_512x512@2x.png" > /dev/null

echo "Combining into $OUTPUT_ICNS..."
iconutil -c icns "$ICONSET" -o "$OUTPUT_ICNS"

# Cleanup
rm -rf "$ICONSET"
echo "Done!"
