#!/usr/bin/env bash
set -euo pipefail

# ─── Config ──────────────────────────────────────────────────────────────────
CXX="${CXX:-g++}"
STD="c++20"
INCLUDE_DIR="include"
SRC_DIR="src"
OUTPUT="kasbm"
JOBS="${JOBS:-$(nproc)}"

# ─── Profile ─────────────────────────────────────────────────────────────────
PROFILE="${1:-debug}"

case "$PROFILE" in
    debug)
        CXXFLAGS="-O0 -g3 -fno-omit-frame-pointer"
        LDFLAGS=""
        ;;
    release)
        CXXFLAGS="-O3 -g -march=native -DNDEBUG -flto"
        LDFLAGS="-s -flto"
        ;;
    *)
        echo "Unknown profile: '$PROFILE'. Use 'debug' or 'release'." >&2
        exit 1
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
