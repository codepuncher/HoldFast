#include <catch2/catch_test_macros.hpp>
#include <limits>

#include "Utils.h"

using QuickMap::ClampHoldDuration;
using QuickMap::TrimWhitespace;

TEST_CASE("TrimWhitespace removes leading and trailing whitespace", "[utils]")
{
	CHECK(TrimWhitespace("  hello  ") == "hello");
	CHECK(TrimWhitespace("\t hello\t") == "hello");
	CHECK(TrimWhitespace("hello") == "hello");
	CHECK(TrimWhitespace("  ") == "");
	CHECK(TrimWhitespace("") == "");
	CHECK(TrimWhitespace("  a b c  ") == "a b c");
	CHECK(TrimWhitespace("hello\r\n") == "hello");
	CHECK(TrimWhitespace("\r\n  hello  \r\n") == "hello");
}

TEST_CASE("ClampHoldDuration clamps and validates values", "[utils]")
{
	constexpr float kDefault = 0.5F;
	constexpr float kMax = 5.0F;

	CHECK(ClampHoldDuration(1.0F, kDefault, kMax) == 1.0F);
	CHECK(ClampHoldDuration(2.5F, kDefault, kMax) == 2.5F);
	CHECK(ClampHoldDuration(5.0F, kDefault, kMax) == 5.0F);

	// Below-zero and zero → default
	CHECK(ClampHoldDuration(-1.0F, kDefault, kMax) == kDefault);
	CHECK(ClampHoldDuration(0.0F, kDefault, kMax) == kDefault);

	// Over max → cap at max
	CHECK(ClampHoldDuration(5.1F, kDefault, kMax) == kMax);
	CHECK(ClampHoldDuration(100.0F, kDefault, kMax) == kMax);

	// Non-finite → default
	CHECK(ClampHoldDuration(std::numeric_limits<float>::quiet_NaN(), kDefault, kMax) == kDefault);
	CHECK(ClampHoldDuration(std::numeric_limits<float>::infinity(), kDefault, kMax) == kDefault);
	CHECK(ClampHoldDuration(-std::numeric_limits<float>::infinity(), kDefault, kMax) == kDefault);
}
