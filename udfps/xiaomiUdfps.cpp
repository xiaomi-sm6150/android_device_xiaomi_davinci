/*
 * Copyright (C) 2022 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "libxiaomiudfps"

#include <android-base/unique_fd.h>
#include <log/log.h>
#include <poll.h>
#include <hidl/Status.h>
#include <thread>

#include "fingerprint.h"

using ::android::hardware::Return;
using ::android::hardware::Void;

#define COMMAND_NIT 10
#define PARAM_NIT_FOD 1
#define PARAM_NIT_NONE 0

#define FOD_STATUS_ON 1
#define FOD_STATUS_OFF -1

#define TOUCH_DEV_PATH "/dev/xiaomi-touch"
#define TOUCH_FOD_ENABLE 10
#define TOUCH_MAGIC 0x5400
#define TOUCH_IOC_SETMODE TOUCH_MAGIC + 0

#define FOD_UI_PATH "/sys/devices/platform/soc/soc:qcom,dsi-display/fod_ui"

static bool readBool(int fd) {
    char c;
    int rc;

    rc = lseek(fd, 0, SEEK_SET);
    if (rc) {
        ALOGE("failed to seek fd, err: %d", rc);
        return false;
    }

    rc = read(fd, &c, sizeof(char));
    if (rc != 1) {
        ALOGE("failed to read bool from fd, err: %d", rc);
        return false;
    }

    return c != '0';
}

android::base::unique_fd fod_ui_fd_;
android::base::unique_fd touch_fd_;

void udfpsPollThread(fingerprint_device_t* device) {
    struct pollfd fodUiPoll = {
            .fd = fod_ui_fd_.get(),
            .events = POLLERR | POLLPRI,
            .revents = 0,
    };

    while (true) {
        int rc = poll(&fodUiPoll, 1, -1);
        if (rc < 0) {
            ALOGE("%s: Failed to poll fd, err: %d", __func__, rc);
            continue;
        }

        bool status = readBool(fod_ui_fd_.get());

        device->extCmd(device, COMMAND_NIT, status ? PARAM_NIT_FOD : PARAM_NIT_NONE);

        int arg[2] = {TOUCH_FOD_ENABLE, status ? FOD_STATUS_ON : FOD_STATUS_OFF};
        ioctl(touch_fd_.get(), TOUCH_IOC_SETMODE, &arg);
    }
}


extern "C" void xiaomiUdfpsInit(fingerprint_device_t* device) {
    fod_ui_fd_ = android::base::unique_fd(open(FOD_UI_PATH, O_RDONLY));
    if (fod_ui_fd_.get() == -1) {
        ALOGE("%s: Failed to open %s", __func__, FOD_UI_PATH);
        return;
    }

    touch_fd_ = android::base::unique_fd(open(TOUCH_DEV_PATH, O_RDWR));
    if (touch_fd_.get() == -1) {
        ALOGE("%s: Failed to open %s", __func__, TOUCH_DEV_PATH);
        return;
    }

    std::thread thread(udfpsPollThread, device);
    thread.detach();
}

extern "C" void xiaomiOnFingerDown(fingerprint_device_t* /*device*/, uint32_t /*x*/, uint32_t /*y*/,
                                float /*minor*/, float /*major*/) {
}

extern "C" void xiaomiOnFingerUp(fingerprint_device_t* /*device*/) {
}
