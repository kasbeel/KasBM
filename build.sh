#!/usr/bin/env bash
set -euo pipefail

# ─── Config ──────────────────────────────────────────────────────────────────
CXX="${CXX:-g++}"
STD="c++20"
INCLUDE_DIR="include"
SRC_DIR="src"
OUTPUT="kasbm"
JOBS="${JOBS:-$(nproc)}"

# ─── Args / Profile ─────────────────────────────────────────────────────────
PROFILE="debug"
INSTALL=false

for arg in "$@"; do
    case "$arg" in
        debug|release)
            PROFILE="$arg"
            ;;
        install)
            INSTALL=true
            ;;
        *)
            echo "Unknown argument: '$arg'. Use 'debug' or 'release' and optional 'install'." >&2
            exit 1
            ;;
    esac
done

case "$PROFILE" in
    debug)
        CXXFLAGS="-O0 -g3 -fno-omit-frame-pointer"
        LDFLAGS=""
        ;;
    release)
        CXXFLAGS="-O3 -g -march=native -DNDEBUG -flto"
        LDFLAGS="-s -flto"
        ;;
esac

COMMON_FLAGS="-std=${STD} -Wall -Wextra -I${INCLUDE_DIR} -I${INCLUDE_DIR}/external"

# ─── Sources & objects ───────────────────────────────────────────────────────
BUILD_DIR="build/kasbm/${PROFILE}"
mkdir -p "${BUILD_DIR}"

mapfile -t SOURCES < <(find "${SRC_DIR}" -name '*.cpp')

if [[ ${#SOURCES[@]} -eq 0 ]]; then
    echo "No .cpp files found in ${SRC_DIR}/" >&2
    exit 1
fi

OBJECTS=()
for src in "${SOURCES[@]}"; do
    obj="${BUILD_DIR}/$(basename "${src%.cpp}").o"
    OBJECTS+=("$obj")
done

# ─── Compile ─────────────────────────────────────────────────────────────────
echo "==> Building KasBM [${PROFILE}] with ${JOBS} jobs..."

compile_one() {
    local src="$1" obj="$2"
    echo "  CC  $src"
    ${CXX} ${COMMON_FLAGS} ${CXXFLAGS} -c "$src" -o "$obj"
}

export -f compile_one
export CXX COMMON_FLAGS CXXFLAGS

parallel_compile() {
    local pids=()
    for i in "${!SOURCES[@]}"; do
        compile_one "${SOURCES[$i]}" "${OBJECTS[$i]}" &
        pids+=($!)
        if (( ${#pids[@]} >= JOBS )); then
            wait "${pids[0]}"
            pids=("${pids[@]:1}")
        fi
    done
    wait "${pids[@]+"${pids[@]}"}"
}

parallel_compile

# ─── Link ────────────────────────────────────────────────────────────────────
echo "  LD  ${OUTPUT}"
${CXX} ${COMMON_FLAGS} ${CXXFLAGS} "${OBJECTS[@]}" -o "${OUTPUT}" ${LDFLAGS}

echo "==> Done: ./${OUTPUT}"

if [[ "$INSTALL" == true ]]; then
    INSTALL_PATH="/usr/local/bin/${OUTPUT}"
    echo "==> Installing ${OUTPUT} to ${INSTALL_PATH}"

    if ! install -m 755 "${OUTPUT}" "${INSTALL_PATH}"; then
        if [[ ${EUID} -ne 0 ]] && command -v sudo >/dev/null 2>&1; then
            echo "==> Permission denied, retrying with sudo..."
            sudo install -m 755 "${OUTPUT}" "${INSTALL_PATH}"
        else
            echo "Install failed: permission denied for ${INSTALL_PATH}." >&2
            echo "Run with sudo, for example: sudo ./build.sh ${PROFILE} install" >&2
            exit 1
        fi
    fi

    echo "==> Installed: /usr/local/bin/${OUTPUT}"
fi
