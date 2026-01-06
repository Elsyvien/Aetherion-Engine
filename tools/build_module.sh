#!/bin/bash
# Build a hot-reload module for Aetherion Engine

set -e

MODULE_NAME=$1
MODULE_DIR="Engine/Scripting/examples"
BUILD_DIR="build-mingw"
OUTPUT_DIR="$BUILD_DIR/modules"

if [ -z "$MODULE_NAME" ]; then
    echo "Usage: ./build_module.sh <ModuleName>"
    echo "Example: ./build_module.sh RotationBehavior"
    exit 1
fi

if [ ! -f "$MODULE_DIR/${MODULE_NAME}.cpp" ]; then
    echo "Error: Module source file not found: $MODULE_DIR/${MODULE_NAME}.cpp"
    exit 1
fi

echo "Building module: $MODULE_NAME"
echo "Source: $MODULE_DIR/${MODULE_NAME}.cpp"

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Determine platform and compiler
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" || "$OSTYPE" == "cygwin" ]]; then
    # Windows
    COMPILER="g++"
    SHARED_FLAG="-shared"
    OUTPUT_EXT=".dll"
    EXTRA_FLAGS=""
else
    # Linux/macOS
    COMPILER="g++"
    SHARED_FLAG="-shared"
    OUTPUT_EXT=".so"
    EXTRA_FLAGS="-fPIC -Wl,-rpath,'\$ORIGIN'"
    
    # Check for clang
    if command -v clang++ &> /dev/null; then
        COMPILER="clang++"
    fi
fi

OUTPUT_FILE="$OUTPUT_DIR/${MODULE_NAME}${OUTPUT_EXT}"

echo "Compiler: $COMPILER"
echo "Output: $OUTPUT_FILE"
echo ""

# Build command
$COMPILER $SHARED_FLAG -std=c++17 -O2 $EXTRA_FLAGS \
    -I"Engine/" \
    "$MODULE_DIR/${MODULE_NAME}.cpp" \
    -o "$OUTPUT_FILE" \
    -L"$BUILD_DIR" \
    -lAetherionRuntime

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Module built successfully: $OUTPUT_FILE"
    echo ""
    echo "To use the module:"
    echo "1. Start the Aetherion Editor"
    echo "2. Open the Logic Copilot panel"
    echo "3. The module can now be loaded via ModuleLoader"
else
    echo ""
    echo "✗ Build failed"
    exit 1
fi
