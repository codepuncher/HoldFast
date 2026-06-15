#pragma once

#include <algorithm>
#include <cassert>
#include <cctype>
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

	[[nodiscard]] inline bool CaseInsensitiveEqual(std::string_view a, std::string_view b)
	{
		return std::ranges::equal(
			a, b,
			[](unsigned char x, unsigned char y) { return std::tolower(x) == std::tolower(y); });
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
