#!/bin/bash
# Debug script to help diagnose source file issues in GitHub Actions

echo "=== Source File Debug Information ==="
echo "Current working directory: $(pwd)"
echo "Date: $(date)"
echo ""

echo "=== Directory Structure Check ==="
echo "src/ directory exists: $(test -d src && echo 'YES' || echo 'NO')"
echo "src/core/ directory exists: $(test -d src/core && echo 'YES' || echo 'NO')"
echo "src/scene/ directory exists: $(test -d src/scene && echo 'YES' || echo 'NO')"
echo ""

echo "=== Core Source Files Check ==="
for file in "src/core/Logger.cpp" "src/core/Entity.cpp" "src/core/EntityTypes.cpp" "src/core/Transform.cpp"; do
    if [ -f "$file" ]; then
        echo "✓ $file exists ($(wc -l < "$file") lines)"
    else
        echo "✗ $file MISSING"
    fi
done
echo ""

echo "=== Scene Source Files Check ==="
for file in "src/scene/SceneGraphSystem.cpp" "src/scene/SceneManager.cpp"; do
    if [ -f "$file" ]; then
        echo "✓ $file exists ($(wc -l < "$file") lines)"
    else
        echo "✗ $file MISSING"
    fi
done
echo ""

echo "=== Test Files Check ==="
for file in "src/tests/test_scene_graph.cpp" "src/tests/simple_scene_test.cpp" "src/tests/logger_test.cpp"; do
    if [ -f "$file" ]; then
        echo "✓ $file exists ($(wc -l < "$file") lines)"
    else
        echo "✗ $file MISSING"
    fi
done
echo ""

echo "=== Demo Files Check ==="
for file in "src/demos/scene_graph_demo.cpp"; do
    if [ -f "$file" ]; then
        echo "✓ $file exists ($(wc -l < "$file") lines)"
    else
        echo "✗ $file MISSING"
    fi
done
echo ""

echo "=== Git Status ==="
git status --porcelain
echo ""

echo "=== Recent Commits ==="
git log --oneline -3
echo ""

echo "=== CMakeLists.txt First 80 Lines ==="
head -80 CMakeLists.txt
echo ""

echo "=== File Permissions ==="
ls -la src/core/*.cpp | head -5
echo ""
