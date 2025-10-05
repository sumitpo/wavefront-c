#!/bin/bash

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
INSTALL_DIR="$PROJECT_ROOT/install"

# show help
usage() {
	echo "Usage: $0 [build|clean|rebuild]"
	echo "  build    : Configure and build the project (default)"
	echo "  clean    : Remove build and install directories"
	echo "  rebuild  : Clean and then build"
	exit 1
}

# check deps
check_deps() {
	for cmd in cmake make; do
		if ! command -v "$cmd" &>/dev/null; then
			echo " Error: $cmd is not installed."
			exit 1
		fi
	done
}

# build project
build() {
	echo " Building project..."
	conan install . --output-folder="$PROJECT_ROOT" --build=missing
	cmake --preset conan-release
	cmake --build --preset conan-release

	cmake --install "$BUILD_DIR/Release" --prefix "$INSTALL_DIR"

	echo " Build and install completed!"
	# echo " Binary: $INSTALL_DIR/bin/raytracer"
}

# clean build
clean() {
	echo "Cleaning build and install directories..."
	rm -rf "$BUILD_DIR" "$INSTALL_DIR" CMakeUserPresets.json
	echo "Cleaned."
}

format() {
	find . -name "*.c" -exec clang-format -i {} \;
	find . -name "*.h" -exec clang-format -i {} \;
	find . -name "*.sh" -exec shfmt -w {} \;
	find . -name "CMakeLists.txt" -exec cmake-format --keyword-case upper --command-case upper -i {} \;
}

# rebuild project
rebuild() {
	clean
	build
}

# main logic
main() {
	# check_deps  # 取消注释以启用依赖检查
	case "${1:-build}" in
	build) build ;;
	clean) clean ;;
	rebuild) rebuild ;;
	format) format ;;
	*) usage ;;
	esac
}

# exec main func
main "$@"
