/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "sensors.ingot"

#include <V2_1/SubHal.h>
#include <dlfcn.h>
#include <log/log.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace android {
namespace hardware {
namespace sensors {
namespace V2_1 {
namespace implementation {

using ::android::sp;
using ::android::hardware::hidl_handle;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::sensors::V1_0::OperationMode;
using ::android::hardware::sensors::V1_0::RateLevel;
using ::android::hardware::sensors::V1_0::Result;
using ::android::hardware::sensors::V1_0::SensorFlagBits;
using ::android::hardware::sensors::V1_0::SharedMemInfo;

using GetSubHal = ISensorsSubHal* (*)(uint32_t*);

static constexpr char kInnerSubHal[] = "sensors.ssc.so";
static constexpr char kRgbType[] = "qti.sensor.rgb";
static constexpr auto kRgbTimeout = std::chrono::seconds(1);
static constexpr int kMaxRetries = 3;

class SensorsSubHal;

class ProxyCallback : public IHalProxyCallback {
  public:
    ProxyCallback(SensorsSubHal* parent, const sp<IHalProxyCallback>& upstream)
        : mParent(parent), mUpstream(upstream) {}

    Return<void> onDynamicSensorsConnected_2_1(const hidl_vec<SensorInfo>& sensors) override {
        return mUpstream->onDynamicSensorsConnected_2_1(sensors);
    }

    Return<void> onDynamicSensorsConnected(const hidl_vec<V1_0::SensorInfo>& sensors) override {
        return mUpstream->onDynamicSensorsConnected(sensors);
    }

    Return<void> onDynamicSensorsDisconnected(const hidl_vec<int32_t>& handles) override {
        return mUpstream->onDynamicSensorsDisconnected(handles);
    }

    void postEvents(const std::vector<Event>& events, ScopedWakelock wakelock) override;

    ScopedWakelock createScopedWakelock(bool lock) override {
        return mUpstream->createScopedWakelock(lock);
    }

  private:
    SensorsSubHal* mParent;
    sp<IHalProxyCallback> mUpstream;
};

/*
 * The stk3a6x driver in the SLPI drops an RGB enable that arrives while a new
 * sensor instance is still applying its first (ALS) client config. The RGB
 * stream then never starts. Re-send the RGB request once the instance is up.
 */
class SensorsSubHal : public ISensorsSubHal {
  public:
    explicit SensorsSubHal(ISensorsSubHal* inner) : mInner(inner) {
        mWatchdog = std::thread([this] { watchdog(); });
    }

    // ISensors
    Return<void> getSensorsList_2_1(getSensorsList_2_1_cb cb) override {
        return mInner->getSensorsList_2_1([&](const hidl_vec<SensorInfo>& list) {
            for (const auto& s : list) {
                bool wakeUp = s.flags & static_cast<uint32_t>(SensorFlagBits::WAKE_UP);
                if (!wakeUp && s.typeAsString == kRgbType) mRgbHandle = s.sensorHandle;
            }
            cb(list);
        });
    }

    Return<Result> setOperationMode(OperationMode mode) override {
        return mInner->setOperationMode(mode);
    }

    Return<Result> activate(int32_t handle, bool enabled) override {
        Return<Result> ret = mInner->activate(handle, enabled);
        if (handle == mRgbHandle) {
            std::lock_guard<std::mutex> lock(mLock);
            mRgbEnabled = enabled;
            if (enabled) arm(0);
            mCv.notify_all();
        }
        return ret;
    }

    Return<Result> batch(int32_t handle, int64_t periodNs, int64_t latencyNs) override {
        if (handle == mRgbHandle) {
            std::lock_guard<std::mutex> lock(mLock);
            mRgbPeriodNs = periodNs;
            mRgbLatencyNs = latencyNs;
        }
        return mInner->batch(handle, periodNs, latencyNs);
    }

    Return<Result> flush(int32_t handle) override { return mInner->flush(handle); }

    Return<Result> injectSensorData_2_1(const Event& event) override {
        return mInner->injectSensorData_2_1(event);
    }

    Return<void> registerDirectChannel(const SharedMemInfo& mem,
                                       registerDirectChannel_cb cb) override {
        return mInner->registerDirectChannel(mem, cb);
    }

    Return<Result> unregisterDirectChannel(int32_t channel) override {
        return mInner->unregisterDirectChannel(channel);
    }

    Return<void> configDirectReport(int32_t handle, int32_t channel, RateLevel rate,
                                    configDirectReport_cb cb) override {
        return mInner->configDirectReport(handle, channel, rate, cb);
    }

    // ISensorsSubHal
    Return<void> debug(const hidl_handle& fd, const hidl_vec<hidl_string>& args) override {
        return mInner->debug(fd, args);
    }

    const std::string getName() override { return mInner->getName(); }

    Return<Result> initialize(const sp<IHalProxyCallback>& callback) override {
        return mInner->initialize(new ProxyCallback(this, callback));
    }

    void onEvents(const std::vector<Event>& events) {
        if (mRgbSeen) return;
        for (const auto& e : events) {
            if (e.sensorHandle == mRgbHandle) mRgbSeen = true;
        }
    }

  private:
    // Called with mLock held.
    void arm(int retries) {
        mRgbSeen = false;
        mRetries = retries;
        mDeadline = std::chrono::steady_clock::now() + kRgbTimeout;
        mArmed = true;
    }

    void watchdog() {
        std::unique_lock<std::mutex> lock(mLock);
        while (true) {
            mCv.wait(lock, [this] { return mArmed; });
            if (mCv.wait_until(lock, mDeadline, [this] { return !mRgbEnabled; })) {
                mArmed = false;
                continue;
            }
            if (mRgbSeen || mRetries >= kMaxRetries) {
                mArmed = false;
                continue;
            }
            int32_t handle = mRgbHandle;
            int64_t periodNs = mRgbPeriodNs, latencyNs = mRgbLatencyNs;
            int retries = mRetries + 1;
            arm(retries);
            lock.unlock();
            ALOGW("No RGB events after enable, re-sending request (%d/%d)", retries, kMaxRetries);
            mInner->activate(handle, false).isOk();
            mInner->batch(handle, periodNs, latencyNs).isOk();
            mInner->activate(handle, true).isOk();
            lock.lock();
        }
    }

    ISensorsSubHal* mInner;
    std::atomic<int32_t> mRgbHandle = -1;
    std::atomic<bool> mRgbSeen = false;

    std::mutex mLock;
    std::condition_variable mCv;
    std::thread mWatchdog;
    bool mArmed = false;
    bool mRgbEnabled = false;
    int mRetries = 0;
    int64_t mRgbPeriodNs = 0;
    int64_t mRgbLatencyNs = 0;
    std::chrono::steady_clock::time_point mDeadline;
};

void ProxyCallback::postEvents(const std::vector<Event>& events, ScopedWakelock wakelock) {
    mParent->onEvents(events);
    mUpstream->postEvents(events, std::move(wakelock));
}

}  // namespace implementation
}  // namespace V2_1
}  // namespace sensors
}  // namespace hardware
}  // namespace android

using ::android::hardware::sensors::V2_1::implementation::GetSubHal;
using ::android::hardware::sensors::V2_1::implementation::ISensorsSubHal;
using ::android::hardware::sensors::V2_1::implementation::kInnerSubHal;
using ::android::hardware::sensors::V2_1::implementation::SensorsSubHal;

ISensorsSubHal* sensorsHalGetSubHal_2_1(uint32_t* version) {
    static ISensorsSubHal* subHal = [] () -> ISensorsSubHal* {
        void* lib = dlopen(kInnerSubHal, RTLD_NOW);
        auto getSubHal = lib ? reinterpret_cast<GetSubHal>(dlsym(lib, "sensorsHalGetSubHal_2_1"))
                             : nullptr;
        uint32_t innerVersion = 0;
        ISensorsSubHal* inner = getSubHal ? getSubHal(&innerVersion) : nullptr;
        if (!inner || innerVersion != SUB_HAL_2_1_VERSION) {
            ALOGE("Cannot load %s: %s", kInnerSubHal, lib ? "bad sub-HAL" : dlerror());
            return nullptr;
        }
        return new SensorsSubHal(inner);
    }();
    *version = SUB_HAL_2_1_VERSION;
    return subHal;
}
