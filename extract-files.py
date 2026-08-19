#!/usr/bin/env -S PYTHONPATH=../../../tools/extract-utils python3
#
# SPDX-FileCopyrightText: 2024 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

from extract_utils.fixups_blob import (
    blob_fixup,
    blob_fixups_user_type,
)
from extract_utils.fixups_lib import (
    lib_fixups,
    lib_fixups_user_type,
)
from extract_utils.main import (
    ExtractUtils,
    ExtractUtilsModule,
)

namespace_imports = [
    'device/osom/ingot',
    'hardware/qcom-caf/sm8450-6.6',
    'hardware/qcom-caf/wlan',
    'vendor/qcom/opensource/commonsys/display',
    'vendor/qcom/opensource/commonsys-intf/display',
    'vendor/qcom/opensource/dataservices',
]


def lib_fixup_vendor_suffix(lib: str, partition: str, *args, **kwargs):
    return f'{lib}_{partition}' if partition == 'vendor' else None


lib_fixups: lib_fixups_user_type = {
    **lib_fixups,
    (
        'com.qualcomm.qti.dpm.api@1.0',
        'vendor.qti.hardware.dpmservice@1.0',
        'vendor.qti.hardware.dpmservice@1.1',
        'vendor.qti.hardware.qccsyshal@1.0',
        'vendor.qti.hardware.qccsyshal@1.1',
        'vendor.qti.hardware.wifidisplaysession@1.0',
        'vendor.qti.imsrtpservice@3.0',
    ): lib_fixup_vendor_suffix,
}

blob_fixups: blob_fixups_user_type = {
    'system_ext/lib64/libwfdnative.so': blob_fixup()
        .add_needed('libinput_shim.so'),
    'system_ext/lib64/vendor.qti.hardware.qccsyshal@1.2-halimpl.so': blob_fixup()
        .replace_needed('libprotobuf-cpp-full.so', 'libprotobuf-cpp-full-21.7.so'),
    (
        'vendor/lib64/libdpps.so',
        'vendor/lib64/libsnapdragoncolor-manager.so',
    ): blob_fixup()
        .replace_needed('libtinyxml2.so', 'libtinyxml2-v34.so'),
    'vendor/bin/qcc-trd': blob_fixup()
        .replace_needed('libgrpc++_unsecure.so', 'libgrpc++_unsecure_prebuilt.so'),
    'vendor/etc/display/DPU8__.xml': blob_fixup()
        .regex_replace('.*<Param62>.*\n', '') # max_fps, dropped
        .regex_replace('Param63', 'Param70') # ddr4_default_total_bw
        .regex_replace('Param64', 'Param71') # ddr4_camera_total_bw
        .regex_replace('Param65', 'Param72') # ddr5_default_total_bw
        .regex_replace('Param66', 'Param73'), # ddr5_camera_total_bw
    'vendor/etc/libnfc-hal-st.conf': blob_fixup()
        .regex_replace('NFC_DEBUG_ENABLED=1', 'NFC_DEBUG_ENABLED=0')
        .regex_replace('STNFC_HAL_LOGLEVEL=3', 'STNFC_HAL_LOGLEVEL=2')
        .regex_replace('STNFC_FW_DEBUG_ENABLED=1', 'STNFC_FW_DEBUG_ENABLED=0')
        .regex_replace('DEVICE_HOST_WHITE_LIST', 'DEVICE_HOST_ALLOW_LIST'),
    ('vendor/etc/media_codecs_cape.xml', 'vendor/etc/media_codecs_cape_vendor.xml'): blob_fixup()
        .regex_replace('.*media_codecs_(google_audio|google_c2|google_telephony|vendor_audio).*\n', ''),
    ('vendor/etc/seccomp_policy/wfdhdcphalservice.policy', 'vendor/etc/seccomp_policy/modemManager.policy'): blob_fixup()
        .add_line_if_missing('gettid: 1'),
    'vendor/lib64/hw/com.qti.chi.override.so': blob_fixup()
        .binary_regex_replace(
              b'\xe8\x0b\x40\xf9\xc8\x00\x00\xb4\x08\x01\x40\x39',
              b'\x29\x00\x80\x52\xe9\x0f\x00\xb9\x05\x00\x00\x14',
        ),
    'vendor/lib64/libcamximageformatutils.so': blob_fixup()
        .replace_needed('android.hardware.graphics.allocator-V1-ndk.so', 'android.hardware.graphics.allocator-V2-ndk.so'),
    'vendor/lib64/libdisplayqos.so': blob_fixup()
        .sig_replace(
            '68 00 b0 52', # 0x80030000 (parrot)
            'a8 00 b0 52', # 0x80050000 (cape)
        )
        .sig_replace(
            '44 50 55 38 33 30 2e 78 6d 6c', # DPU830.xml
            '44 50 55 38 5f 5f 2e 78 6d 6c', # DPU8__.xml
        ),
    'vendor/lib64/libgrpc++_unsecure_prebuilt.so': blob_fixup()
        .fix_soname(),
    'vendor/lib64/libqcodec2_core.so': blob_fixup()
        .replace_needed('android.hardware.graphics.common-V5-ndk.so', 'android.hardware.graphics.common-V7-ndk.so'),
    'vendor/lib64/vendor.libdpmframework.so': blob_fixup()
        .add_needed('libhidlbase_shim.so'),
    'vendor/lib64/vendor.qti.hardware.camera.postproc@1.0-service-impl.so': blob_fixup()
        .replace_needed('android.hardware.camera.common-V2-ndk.so', 'android.hardware.camera.common-V1-ndk.so')
        .replace_needed('android.hardware.graphics.allocator-V1-ndk.so', 'android.hardware.graphics.allocator-V2-ndk.so'),
}  # fmt: skip

module = ExtractUtilsModule(
    'ingot',
    'osom',
    blob_fixups=blob_fixups,
    lib_fixups=lib_fixups,
    namespace_imports=namespace_imports,
    add_firmware_proprietary_file=True,
)

if __name__ == '__main__':
    utils = ExtractUtils.device(module)
    utils.run()
