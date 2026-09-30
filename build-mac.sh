#!/bin/sh
set -eu
cd "$(dirname "$0")"
APP="dist/PixelCat.app"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
clang++ -std=c++17 -O2 -arch arm64 -arch x86_64 -mmacosx-version-min=12.0 -fobjc-arc -framework Cocoa -framework ImageIO src/mac.mm -o "$APP/Contents/MacOS/PixelCat"
cp assets/cat.png assets/actions.png assets/groom.png assets/flower.png assets/belly.png assets/downcast.png assets/stretch.png assets/doze.png "$APP/Contents/Resources/"
cat > "$APP/Contents/Info.plist" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleExecutable</key><string>PixelCat</string>
<key>CFBundleIdentifier</key><string>local.pixelcat.desktop</string>
<key>CFBundleName</key><string>毛毛</string>
<key>CFBundleDisplayName</key><string>毛毛</string>
<key>CFBundleVersion</key><string>0.5.0</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>LSUIElement</key><true/>
<key>NSHighResolutionCapable</key><true/>
<key>LSMinimumSystemVersion</key><string>12.0</string>
</dict></plist>
EOF
codesign --force --sign - "$APP"
echo "Built $APP"
