LOCAL_PATH:= $(call my-dir)

# WifiResTarget for taro, which surppots DBS features

include $(CLEAR_VARS)

LOCAL_VENDOR_MODULE := true
LOCAL_CERTIFICATE := platform

LOCAL_SRC_FILES := $(call all-subdir-java-files)

LOCAL_RESOURCE_DIR := $(LOCAL_PATH)/res

LOCAL_MANIFEST_FILE := AndroidManifest.xml

LOCAL_PACKAGE_NAME := WifiResTarget
LOCAL_SDK_VERSION := current

include $(BUILD_RRO_PACKAGE)

# WifiResTarget-spf for common SPF features. ex: taro and diwali.
# This is primarily for non-DBS targets.

include $(CLEAR_VARS)

LOCAL_VENDOR_MODULE := true
LOCAL_CERTIFICATE := platform

LOCAL_SRC_FILES := $(call all-subdir-java-files)

LOCAL_RESOURCE_DIR := $(LOCAL_PATH)/spf/res

LOCAL_MANIFEST_FILE := AndroidManifest-spf.xml

LOCAL_PACKAGE_NAME := WifiResTarget_spf
LOCAL_SDK_VERSION := current

include $(BUILD_RRO_PACKAGE)

# WifiResTarget-cape for cape. which supports DBS features.

include $(CLEAR_VARS)

LOCAL_VENDOR_MODULE := true
LOCAL_CERTIFICATE := platform

LOCAL_SRC_FILES := $(call all-subdir-java-files)

LOCAL_RESOURCE_DIR := $(LOCAL_PATH)/cape/res

LOCAL_MANIFEST_FILE := AndroidManifest-cape.xml

LOCAL_PACKAGE_NAME := WifiResTarget_cape
LOCAL_SDK_VERSION := current

include $(BUILD_RRO_PACKAGE)
