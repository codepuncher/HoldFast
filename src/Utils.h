#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <string_view>

namespace HoldFast
{

	// Returns a view into `s`, so the caller must keep that buffer alive for as long
	// as the returned view is used (do not pass a temporary std::string).
	[[nodiscard]] inline std::string_view TrimWhitespace(std::string_view s)
	{
		const auto first = s.find_first_not_of(" \t\r\n");
		if (first == std::string_view::npos) {
			return {};
		}
		return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
	}

	[[nodiscard]] inline constexpr unsigned char AsciiToLower(unsigned char c) noexcept
	{
		return (c >= 'A' && c <= 'Z') ? static_cast<unsigned char>(c + ('a' - 'A')) : c;
	}

	[[nodiscard]] inline bool CaseInsensitiveEqual(std::string_view a, std::string_view b)
	{
		return std::ranges::equal(
			a, b,
			[](unsigned char x, unsigned char y) { return AsciiToLower(x) == AsciiToLower(y); });
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
