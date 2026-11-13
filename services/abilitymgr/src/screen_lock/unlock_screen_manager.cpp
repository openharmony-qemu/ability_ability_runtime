/*
 * Copyright (c) 2024-2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "unlock_screen_manager.h"

#include <algorithm>

#include "hilog_tag_wrapper.h"
#include "in_process_call_wrapper.h"
#include "parameters.h"
#include "permission_verification.h"
#ifdef SUPPORT_POWER
#include "power_mgr_client.h"
#endif

#ifdef SUPPORT_GRAPHICS
#include "unlock_screen_callback.h"
#ifdef SUPPORT_SCREEN
#ifdef ABILITY_RUNTIME_SCREENLOCK_ENABLE
#include "screenlock_manager.h"
#include "screenlock_common.h"
#endif // ABILITY_RUNTIME_SCREENLOCK_ENABLE
#endif
#endif

namespace OHOS {
namespace AbilityRuntime {
namespace {
constexpr char DEVELOPER_MODE_STATE[] = "const.security.developermode.state";
constexpr char TIMEOUT_UNIT_TIME_RATIO[] = "persist.sys.abilityms.timeout_unit_time_ratio";
constexpr int32_t DEFAULT_UNLOCK_TIMEOUT_MS = 500;
constexpr int32_t SCALED_UNLOCK_UNIT_MS = 2500;
constexpr int32_t MAX_TIMEOUT_UNIT_RATIO = 20;
}

UnlockScreenManager::~UnlockScreenManager() {}

UnlockScreenManager::UnlockScreenManager() {}

UnlockScreenManager &UnlockScreenManager::GetInstance()
{
    static UnlockScreenManager instance;
    return instance;
}

bool UnlockScreenManager::UnlockScreen()
{
    bool isShellCall = AAFwk::PermissionVerification::GetInstance()->IsShellCall();
    bool isDeveloperMode = system::GetBoolParameter(DEVELOPER_MODE_STATE, false);
    if (!isShellCall || !isDeveloperMode) {
        return true;
    }

#ifdef SUPPORT_GRAPHICS
#ifdef SUPPORT_SCREEN
#ifdef ABILITY_RUNTIME_SCREENLOCK_ENABLE
    bool isScreenLocked = OHOS::ScreenLock::ScreenLockManager::GetInstance()->IsScreenLocked();
    bool isScreenSecured = OHOS::ScreenLock::ScreenLockManager::GetInstance()->GetSecure();
    TAG_LOGD(AAFwkTag::ABILITYMGR, "isScreenLocked: %{public}d, isScreenSecured: %{public}d",
        isScreenLocked, isScreenSecured);
    if (isScreenLocked && isScreenSecured) {
        return false;
    }
#endif // ABILITY_RUNTIME_SCREENLOCK_ENABLE
#endif
#endif

    TAG_LOGI(AAFwkTag::ABILITYMGR, "UnlockScreen begin");
#ifdef SUPPORT_POWER
    bool isScreenOn = PowerMgr::PowerMgrClient::GetInstance().IsScreenOn();
    TAG_LOGD(AAFwkTag::ABILITYMGR, "isScreenOn: %{public}d", isScreenOn);
    if (!isScreenOn) {
        PowerMgr::PowerMgrClient::GetInstance().WakeupDevice();
    }
#endif

#ifdef SUPPORT_GRAPHICS
#ifdef ABILITY_RUNTIME_SCREENLOCK_ENABLE
    if (isScreenLocked) {
        auto promise = std::make_shared<std::promise<bool>>();
        sptr<UnlockScreenCallback> listener = sptr<UnlockScreenCallback>::MakeSptr(promise);
        IN_PROCESS_CALL(OHOS::ScreenLock::ScreenLockManager::GetInstance()->Unlock(
            OHOS::ScreenLock::Action::UNLOCKSCREEN, listener));
        auto future = promise->get_future();
        int32_t ratio = std::clamp(
            system::GetIntParameter<int32_t>(TIMEOUT_UNIT_TIME_RATIO, 1), 1, MAX_TIMEOUT_UNIT_RATIO);
        int32_t timeoutMs = (ratio > 1) ? (SCALED_UNLOCK_UNIT_MS * ratio) : DEFAULT_UNLOCK_TIMEOUT_MS;
        std::future_status status = future.wait_for(std::chrono::milliseconds(timeoutMs));
        if (status == std::future_status::timeout) {
            TAG_LOGE(AAFwkTag::ABILITYMGR, "UnlockScreen timeout");
            return false;
        }
        TAG_LOGI(AAFwkTag::ABILITYMGR, "UnlockScreen end");
        return future.get();
    }
#endif // ABILITY_RUNTIME_SCREENLOCK_ENABLE
#endif
    TAG_LOGI(AAFwkTag::ABILITYMGR, "UnlockScreen end");
    return true;
}
} // namespace AbilityRuntime
} // namespace OHOS