#!/usr/bin/env bash
set -euo pipefail

CONF="${CONF:-PAGING}"
KERN_SRC="${KERN_SRC:-${GITHUB_WORKSPACE:-/work/kern}}"
OS161_SRC="${OS161_SRC:-/home/os161user/os161/src}"
OS161_ROOT="${OS161_ROOT:-/home/os161user/os161/root}"

echo "Using kernel source: ${KERN_SRC}"
echo "Using OS/161 source: ${OS161_SRC}"
echo "Using OS/161 root: ${OS161_ROOT}"
echo "Kernel configuration: ${CONF}"

if [ ! -d "${KERN_SRC}/conf" ]; then
    echo "Cannot find OS/161 kernel sources at ${KERN_SRC}" >&2
    exit 1
fi

mkdir -p "${OS161_SRC}/kern"
find "${OS161_SRC}/kern" -mindepth 1 -maxdepth 1 -exec rm -rf {} +

(
    cd "${KERN_SRC}"
    tar \
        --exclude=.git \
        --exclude=compile \
        -cf /tmp/os161-kern.tar .
)

tar -C "${OS161_SRC}/kern" -xf /tmp/os161-kern.tar

mkdir -p "${OS161_SRC}/kern/compile"
chmod +x "${OS161_SRC}/kern/conf/config" "${OS161_SRC}/kern/conf/newvers.sh"

cd "${OS161_SRC}/kern/conf"
./config "${CONF}"

cd "${OS161_SRC}/kern/compile/${CONF}"
bmake depend
bmake
bmake install

cd "${OS161_ROOT}"
if command -v timeout >/dev/null 2>&1; then
    timeout 45s sys161 kernel "q"
else
    sys161 kernel "q"
fi
