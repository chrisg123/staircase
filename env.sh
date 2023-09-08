#!/bin/bash

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    echo "This script should be sourced, not executed."
    exit 1
fi

if [ -n "${EMSDK}" ]; then
    if [ -z "$SKIP_EMSDK_MESSAGE" ]; then
        echo "EMSDK is already set."
    fi
    return 0
fi

if [ -z "${EMSDK_ROOT}" ]; then
    echo "Error: EMSDK_ROOT is not set. Please set the EMSDK_ROOT" \
        " environment variable to point to your Emscripten SDK" \
        " installation directory." >&2
    return 1
fi

. "${EMSDK_ROOT}/emsdk_env.sh" || {
    echo "Error: Failed to set up Emscripten SDK environment from" \
        " ${EMSDK_ROOT}/emsdk_env.sh" >&2
    exit 1
}
