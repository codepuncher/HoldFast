#pragma once

#include <cassert>
#include <string_view>

namespace QuickMap
{

	[[nodiscard]] inline std::string_view TrimWhitespace(std::string_view s)
	{
		const auto first = s.find_first_not_of(" \t\r\n");
		if (first == std::string_view::npos) {
			return {};
		}
		return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
	}

	[[nodiscard]] inline float ClampHoldDuration(float value, float defaultVal, float maxVal)
	{
		assert(defaultVal > 0.0F && defaultVal <= maxVal);
		if (value <= 0.0F) {
			return defaultVal;
		}
		if (value > maxVal) {
			return maxVal;
		}
		return value;
	}
}
