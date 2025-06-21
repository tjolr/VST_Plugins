#!/bin/bash

# ---------------------
# CONFIG
# ---------------------
TEMPLATE_DIR="/Users/lexchristopherson/Developer/Plugins/NewPluginSkeleton"
CLAUDE_DIR="/Users/lexchristopherson/Developer/Plugins/.claude"   # copy whole folder

# ---------------------
# INPUT VALIDATION
# ---------------------
if [ -z "$1" ]; then
  echo "❌ ERROR: You must provide a plugin name."
  echo "Usage: ./bootstrap.sh MyNewPlugin"
  exit 1
fi

PLUGIN_NAME="$1"
DEST_DIR="$(pwd)/$PLUGIN_NAME"

if [ -d "$DEST_DIR" ]; then
  echo "❌ ERROR: Destination folder already exists: $DEST_DIR"
  exit 1
fi

if [ ! -d "$TEMPLATE_DIR" ]; then
  echo "❌ ERROR: Template not found at $TEMPLATE_DIR"
  exit 1
fi

# ---------------------
# COPY TEMPLATE
# ---------------------
echo "📁 Copying template..."
shopt -s dotglob
cp -R "$TEMPLATE_DIR" "$DEST_DIR"
shopt -u dotglob
cd "$DEST_DIR" || exit 1

# ---------------------
# ADD WORKFLOW
# ---------------------
echo "🛠️  Adding GitHub Actions workflow..."
mkdir -p .github/workflows
cat > .github/workflows/ci.yml <<'EOF'
name: Build

on: [push, pull_request, workflow_dispatch]

jobs:
  build:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install Ninja and pluginval
        run: |
          brew install ninja
          brew install --cask pluginval
      - name: Configure host-arch
        run: cmake -Bbuild -G Ninja -DCMAKE_OSX_ARCHITECTURES=$(uname -m)
      - name: Build
        run: cmake --build build
      - name: Run Smoke Build Gate
        run: |
          ./scripts/run_smoke.sh || exit 1
      - name: Skip UI screenshot in CI
        run: echo "UI capture skipped on headless runner"
EOF
  
# ---------------------
# RENAME FILES & FOLDERS
# ---------------------
echo "✏️  Renaming files and folders..."
find . -depth -name '*NewPluginSkeleton*' | while read -r path; do
  newpath=$(echo "$path" | sed "s/NewPluginSkeleton/$PLUGIN_NAME/g")
  mv "$path" "$newpath"
done

# ---------------------
# REPLACE IDENTIFIERS IN FILES
# ---------------------
echo "🔍 Replacing identifiers inside files..."
find . -type f -exec sed -i '' \
  -e "s/NewPluginSkeleton/$PLUGIN_NAME/g" \
  -e "s/newPluginSkeleton/$(echo "$PLUGIN_NAME" | sed 's/^./\L&/')/g" \
  -e "s/NEWPLUGINSKELETON_/$(echo "$PLUGIN_NAME" | tr '[:lower:]' '[:upper:]')_/g" \
  {} +

# ---------------------
# PATCH .jucer FILE
# ---------------------
JUCER_FILE="$PLUGIN_NAME.jucer"
if [ -f "$JUCER_FILE" ]; then
  echo "🛠️  Patching .jucer metadata..."
  sed -i '' "s/name=\"NewPluginSkeleton\"/name=\"$PLUGIN_NAME\"/g" "$JUCER_FILE"
  sed -i '' "s/targetName=\"NewPluginSkeleton\"/targetName=\"$PLUGIN_NAME\"/g" "$JUCER_FILE"
  sed -i '' "s/<MAINGROUP id=.* name=\"NewPluginSkeleton\"/<MAINGROUP id=\"auT4KH\" name=\"$PLUGIN_NAME\"/g" "$JUCER_FILE"
else
  echo "⚠️  Warning: .jucer file not found for metadata patching."
fi

# ---------------------
# PATCH PLIST FILES
# ---------------------
echo "🛠️  Patching Info-AU.plist and Info-Standalone_plugin.plist..."
find . -name "Info-*.plist" | while read -r plist; do
  sed -i '' \
    -e "s/com\.yourcompany\.NewPluginSkeleton/com.yourcompany.$PLUGIN_NAME/g" \
    -e "s/NewPluginSkeletonAUFactory/${PLUGIN_NAME}AUFactory/g" \
    -e "s/NewPluginSkeleton/$PLUGIN_NAME/g" \
    "$plist"
done

# ---------------------
# INJECT CLAUDE SETTINGS
# ---------------------
echo "🧠 Injecting Claude settings..."
rsync -a "$CLAUDE_DIR/" ".claude/"

# ---------------------
# INIT GIT
# ---------------------
echo "🌱 Initializing git..."
git init >/dev/null
git add .
git commit -m "🎉 Initial commit for $PLUGIN_NAME"

# ---------------------
# DONE
# ---------------------
echo "✅ Plugin '$PLUGIN_NAME' created at: $DEST_DIR"
