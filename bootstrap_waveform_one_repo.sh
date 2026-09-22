#!/usr/bin/env bash
set -euo pipefail

ROOT="/Users/simonholmes/Projects/Applications/Waveform_One"

if [[ ! -d "$ROOT" ]]; then
  echo "ERROR: $ROOT does not exist."
  exit 1
fi

cd "$ROOT"

mkdir -p \
  docs/{architecture,adr,prd,assembly,testing,social} \
  mechanical/fusion/WaveformOne/{components,enclosure,calibration} \
  mechanical/{reference/{datasheets,step,measurements,photos},exports/{step,stl,3mf},slicer/ElegooSlicer,prototype-notes} \
  electronics/{kicad/{control-board,power-board},schematics,pcb,gerbers,bom,datasheets,bringup} \
  firmware/esp32/{main/{audio,display,controls,protocol,system},test} \
  pi/core/src/{recognition,metadata,artwork,cache,device,state,config} \
  pi/ui/{src,qml,assets} \
  pi/systemd \
  protocol/test-vectors \
  manufacturing/{assembly,test-fixtures,flashing,calibration,packaging} \
  scripts/{build,release,diagnostics,manufacturing} \
  tests/{integration,hardware-in-loop,system} \
  .github/workflows

# Create placeholder source/readme files only when absent.
touch README.md CONTRIBUTING.md CHANGELOG.md
touch mechanical/README.md electronics/README.md
touch mechanical/fusion/WaveformOne/{WaveformOne.py,parameters.py,geometry.py,validation.py,export.py}
touch mechanical/fusion/WaveformOne/components/{hub75.py,lcd.py,raspberry_pi.py,esp32.py,encoder.py,microphone.py}
touch mechanical/fusion/WaveformOne/enclosure/{front_left.py,front_center.py,front_right.py,chassis_left.py,chassis_right.py,rear_panel.py,album_support.py,electronics_tray.py,joints.py}
touch mechanical/fusion/WaveformOne/calibration/{insert_coupon.py,clearance_coupon.py,screw_coupon.py,joint_coupon.py}
touch protocol/{specification.md,messages.md}

# Move known Waveform One Markdown docs from the repo root into docs/prd if present.
for f in \
  WAVEFORM_ONE_MECHANICAL_CAD_PRD.md \
  WAVEFORM_ONE_MECHANICAL_CAD_PRD_v2.md \
  waveform_one_v1_architecture_engineering_and_social_strategy.md \
  waveform_one_v1_architecture_engineering_and_social_strategy_v2.md \
  waveform_one_precise_purchasing_bom_v1.md
do
  if [[ -f "$f" ]]; then
    mv "$f" docs/prd/
  fi
done

# Move any remaining likely PRD/BOM markdown files conservatively.
find . -maxdepth 1 -type f \( -iname '*prd*.md' -o -iname '*bom*.md' \) -print0 |
while IFS= read -r -d '' f; do
  mv "$f" docs/prd/
done

cat > .gitignore <<'EOF'
# macOS
.DS_Store

# Python
__pycache__/
*.py[cod]
.venv/
venv/

# Rust
target/

# C/C++ / CMake
build/
cmake-build-*/
CMakeFiles/
CMakeCache.txt

# ESP-IDF
firmware/esp32/build/
firmware/esp32/sdkconfig
firmware/esp32/sdkconfig.old

# Fusion / CAD temporary and backup files
*.bak
*.tmp
*.autosave

# Slicer temporary/cache files
*.gcode
*.bgcode

# Generated mechanical exports are normally reproducible.
# Remove these ignores later if release exports should be versioned.
mechanical/exports/stl/*
mechanical/exports/3mf/*
mechanical/exports/step/*
!mechanical/exports/stl/.gitkeep
!mechanical/exports/3mf/.gitkeep
!mechanical/exports/step/.gitkeep

# Electronics generated manufacturing files
electronics/gerbers/*
!electronics/gerbers/.gitkeep

# Secrets
.env
.env.*
!.env.example
EOF

touch mechanical/exports/{step,stl,3mf}/.gitkeep
touch electronics/gerbers/.gitkeep

# Initialize Git only if this isn't already a repository.
if [[ ! -d .git ]]; then
  git init
fi

echo
echo "Waveform One repository structure initialized at:"
echo "$ROOT"
echo
echo "Top-level structure:"
find . -maxdepth 2 -type d | sort
