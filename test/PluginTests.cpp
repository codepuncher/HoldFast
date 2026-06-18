#include <catch2/catch_test_macros.hpp>
#include <limits>

#include "Config.h"
#include "Utils.h"

using HoldFast::ClampHoldDuration;
using HoldFast::TrimWhitespace;
using HoldFast::Config::ActionName;
using HoldFast::Config::ParseAction;

TEST_CASE("AsciiToLower lowercases A-Z only", "[utils]")
{
	using HoldFast::AsciiToLower;

	CHECK(AsciiToLower('A') == 'a');
	CHECK(AsciiToLower('Z') == 'z');
	CHECK(AsciiToLower('M') == 'm');
	CHECK(AsciiToLower('a') == 'a');
	CHECK(AsciiToLower('z') == 'z');
	CHECK(AsciiToLower('0') == '0');
	CHECK(AsciiToLower('!') == '!');
	CHECK(AsciiToLower(static_cast<unsigned char>(0xFF)) == static_cast<unsigned char>(0xFF));
}

TEST_CASE("CaseInsensitiveEqual matches ASCII case-insensitively", "[utils]")
{
	using HoldFast::CaseInsensitiveEqual;

	CHECK(CaseInsensitiveEqual("None", "none"));
	CHECK(CaseInsensitiveEqual("NONE", "None"));
	CHECK(CaseInsensitiveEqual("MCM", "mcm"));
	CHECK(CaseInsensitiveEqual("Map", "MAP"));
	CHECK(CaseInsensitiveEqual("", ""));
	CHECK_FALSE(CaseInsensitiveEqual("Map", "Mcm"));
	CHECK_FALSE(CaseInsensitiveEqual("None", ""));
	CHECK_FALSE(CaseInsensitiveEqual("abc", "abcd"));
}

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
	constexpr float kMin = 0.1F;
	constexpr float kDefault = 0.5F;
	constexpr float kMax = 5.0F;

	CHECK(ClampHoldDuration(1.0F, kDefault, kMin, kMax) == 1.0F);
	CHECK(ClampHoldDuration(2.5F, kDefault, kMin, kMax) == 2.5F);
	CHECK(ClampHoldDuration(5.0F, kDefault, kMin, kMax) == 5.0F);

	// At minimum boundary
	CHECK(ClampHoldDuration(0.1F, kDefault, kMin, kMax) == 0.1F);

	// Below minimum → default
	CHECK(ClampHoldDuration(0.05F, kDefault, kMin, kMax) == kDefault);
	CHECK(ClampHoldDuration(-1.0F, kDefault, kMin, kMax) == kDefault);
	CHECK(ClampHoldDuration(0.0F, kDefault, kMin, kMax) == kDefault);

	// Over max → cap at max
	CHECK(ClampHoldDuration(5.1F, kDefault, kMin, kMax) == kMax);
	CHECK(ClampHoldDuration(100.0F, kDefault, kMin, kMax) == kMax);

	// Non-finite → default
	CHECK(ClampHoldDuration(std::numeric_limits<float>::quiet_NaN(), kDefault, kMin, kMax) == kDefault);
	CHECK(ClampHoldDuration(std::numeric_limits<float>::infinity(), kDefault, kMin, kMax) == kDefault);
	CHECK(ClampHoldDuration(-std::numeric_limits<float>::infinity(), kDefault, kMin, kMax) == kDefault);
}

TEST_CASE("ParseAction accepts case-insensitive and trimmed values", "[config]")
{
	using Action = LongPressAction;

	CHECK(ParseAction("Map") == Action::kMap);
	CHECK(ParseAction("  map  ") == Action::kMap);
	CHECK(ParseAction("SyStEm") == Action::kSystem);
	CHECK(ParseAction("\tQuickSave\r\n") == Action::kQuickSave);
}

TEST_CASE("ParseAction supports favourites alias and invalid fallback", "[config]")
{
	using Action = LongPressAction;

	CHECK(ParseAction("Favorites") == Action::kFavorites);
	CHECK(ParseAction("Favourites") == Action::kFavorites);
	CHECK(ParseAction("not-an-action") == Action::kNone);
	CHECK(ParseAction("") == Action::kNone);
	CHECK(ParseAction("   ") == Action::kNone);

	// None in any casing/whitespace must parse to kNone without warning
	CHECK(ParseAction("None") == Action::kNone);
	CHECK(ParseAction("NONE") == Action::kNone);
	CHECK(ParseAction("  None  ") == Action::kNone);
}

TEST_CASE("ParseAction handles MCM action", "[config]")
{
	using Action = LongPressAction;

	CHECK(ParseAction("MCM") == Action::kMCM);
	CHECK(ParseAction("mcm") == Action::kMCM);
	CHECK(ParseAction("  Mcm  ") == Action::kMCM);
}

TEST_CASE("ActionName maps enum values and falls back to None", "[config]")
{
	using Action = LongPressAction;

	CHECK(std::string_view{ ActionName(Action::kMap) } == "Map");
	CHECK(std::string_view{ ActionName(Action::kMCM) } == "MCM");
	CHECK(std::string_view{ ActionName(Action::kQuickSave) } == "QuickSave");
	CHECK(std::string_view{ ActionName(Action::kCharacterSheet) } == "CharacterSheet");
	CHECK(std::string_view{ ActionName(static_cast<Action>(9999)) } == "None");
}
