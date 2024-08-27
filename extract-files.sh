#!/bin/bash
#
# SPDX-FileCopyrightText: 2016 The CyanogenMod Project
# SPDX-FileCopyrightText: 2017-2024 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

set -e

DEVICE=ingot
VENDOR=osom

# Load extract_utils and do some sanity checks
MY_DIR="${BASH_SOURCE%/*}"
if [[ ! -d "${MY_DIR}" ]]; then MY_DIR="${PWD}"; fi

ANDROID_ROOT="${MY_DIR}/../../.."

# If XML files don't have comments before the XML header, use this flag
# Can still be used with broken XML files by using blob_fixup
export TARGET_DISABLE_XML_FIXING=true

HELPER="${ANDROID_ROOT}/tools/extract-utils/extract_utils.sh"
if [ ! -f "${HELPER}" ]; then
    echo "Unable to find helper script at ${HELPER}"
    exit 1
fi
source "${HELPER}"

# Default to sanitizing the vendor folder before extraction
CLEAN_VENDOR=true

ONLY_FIRMWARE=
KANG=
SECTION=

while [ "${#}" -gt 0 ]; do
    case "${1}" in
        --only-firmware)
            ONLY_FIRMWARE=true
            ;;
        -n | --no-cleanup)
            CLEAN_VENDOR=false
            ;;
        -k | --kang)
            KANG="--kang"
            ;;
        -s | --section)
            SECTION="${2}"
            shift
            CLEAN_VENDOR=false
            ;;
        *)
            SRC="${1}"
            ;;
    esac
    shift
done

if [ -z "${SRC}" ]; then
    SRC="adb"
fi

function blob_fixup() {
    case "${1}" in
        vendor/bin/hw/android.hardware.security.keymint-service-qti)
            [ "$2" = "" ] && return 0
            grep -q android.hardware.security.rkp-V3-ndk.so "${2}" || "${PATCHELF}" --add-needed "android.hardware.security.rkp-V3-ndk.so" "${2}"
            ;;
        vendor/etc/libnfc-hal-st.conf)
            [ "$2" = "" ] && return 0
            sed -i "s/NFC_DEBUG_ENABLED=1/NFC_DEBUG_ENABLED=0/" "${2}"
            sed -i "s/STNFC_HAL_LOGLEVEL=3/STNFC_HAL_LOGLEVEL=2/" "${2}"
            sed -i "s/STNFC_FW_DEBUG_ENABLED=1/STNFC_FW_DEBUG_ENABLED=0/" "${2}"
            ;;
        vendor/etc/media_codecs_cape.xml|vendor/etc/media_codecs_cape_vendor.xml)
            [ "$2" = "" ] && return 0
            sed -Ei "/media_codecs_(google_audio|google_c2|google_telephony|vendor_audio)/d" "${2}"
            ;;
        *)
            return 1
            ;;
    esac

    return 0
}

function blob_fixup_dry() {
    blob_fixup "$1" ""
}

# Initialize the helper
setup_vendor "${DEVICE}" "${VENDOR}" "${ANDROID_ROOT}" false "${CLEAN_VENDOR}"

if [ -z "${ONLY_FIRMWARE}" ]; then
    extract "${MY_DIR}/proprietary-files.txt" "${SRC}" "${KANG}" --section "${SECTION}"
fi

if [ -z "${SECTION}" ]; then
    extract_firmware "${MY_DIR}/proprietary-firmware.txt" "${SRC}"
fi

"${MY_DIR}/setup-makefiles.sh"
