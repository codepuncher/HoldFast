#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace MCMNavigator
{
	bool IsMCMOpen();

	bool IsAnyModOpen();

	void NavigateToTarget(const std::string& modName) noexcept;

	// Debounced — safe to call every frame.
	void TryCacheFromOpenMCM();

	std::vector<std::string> GetCachedModNames();

	// Reads SKI_ConfigManager's _modNames from SKI_ConfigManagerInstance.
	// Must be called on the game thread. No-op if SkyUI is not installed or script not yet bound.
	void CacheModListFromPapyrus();

	// Async. Safe to call from any thread. Schedules CacheModListFromPapyrus at most once;
	// subsequent calls are no-ops regardless of whether caching succeeded.
	// TryCacheFromOpenMCM handles retries when the Journal is open.
	void EnsureCachePopulated();
}
