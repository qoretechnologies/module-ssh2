#!/bin/bash
#
# Verifies every source-owned data-provider i18n catalog in this module.
#
# Copyright 2026 Qore Technologies, s.r.o.

set -e

src_dir=$(cd "$(dirname "$0")/../.." && pwd)

# Load the modules from this checkout so the check cannot be satisfied by stale installed sources.
export QORE_MODULE_DIR="${src_dir}/qlib${QORE_MODULE_DIR:+:${QORE_MODULE_DIR}}"

qore-data-provider-i18n --no-color --check-source-tree --require-standard-locales \
    --require-complete-locales --output "${src_dir}/qlib"
