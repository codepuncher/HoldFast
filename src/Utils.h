#pragma once

#include <cassert>
#include <cmath>
#include <string_view>

namespace HoldFast
{

	[[nodiscard]] inline std::string_view TrimWhitespace(std::string_view s)
	{
		const auto first = s.find_first_not_of(" \t\r\n");
		if (first == std::string_view::npos) {
			return {};
		}
		return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
	}

	[[nodiscard]] inline float ClampHoldDuration(float value, float defaultVal, float minVal, float maxVal)
	{
		assert(minVal > 0.0F && defaultVal >= minVal && defaultVal <= maxVal);
		if (!std::isfinite(value) || value < minVal) {
			return defaultVal;
		}
		if (value > maxVal) {
			return maxVal;
		}
		return value;
	}
}
