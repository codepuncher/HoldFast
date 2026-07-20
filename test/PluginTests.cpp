#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <vector>

#include "BindingResolver.h"
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

namespace
{
	/**
	 * Gameplay-context gamepad rows from a custom controlmap that dual-binds keys.
	 * 0x0010 = Start, 0x0020 = Back, 0x0100 = LB, 0x1000 = A, 0x0200 = RB.
	 */
	const std::vector<HoldFast::RawMapping> kCustomControlmap{
		{ "Ready Weapon", 0x0004, 0x0100 },
		{ "Tween Menu", 0x0010, 0x0100 },
		{ "Sneak", 0x0040, 0x0100 },
		{ "Favorites", 0x0020, 0x0000 },
		{ "Wait", 0x0008, 0x0100 },
		{ "Journal", 0x0010, 0x0000 },
		{ "Quick Inventory", 0x0200, 0x1000 },
	};
}

TEST_CASE("ResolveBinding resolves solo event for a dual-bound key", "[binding]")
{
	using HoldFast::ResolveBinding;

	// Start is both "Journal" (solo) and the terminal of LB+Start ("Tween Menu").
	const auto resolved = ResolveBinding(0x0010, kCustomControlmap);
	CHECK(resolved.soloEvent == "Journal");
	REQUIRE(resolved.comboModifiers.size() == 1);
	CHECK(resolved.comboModifiers[0] == 0x0100);
}

TEST_CASE("ResolveBinding resolves solo-only and combo-only keys", "[binding]")
{
	using HoldFast::ResolveBinding;

	const auto back = ResolveBinding(0x0020, kCustomControlmap);
	CHECK(back.soloEvent == "Favorites");
	CHECK(back.comboModifiers.empty());

	const auto rb = ResolveBinding(0x0200, kCustomControlmap);
	CHECK(rb.soloEvent.empty());
	REQUIRE(rb.comboModifiers.size() == 1);
	CHECK(rb.comboModifiers[0] == 0x1000);
}

TEST_CASE("ResolveBinding returns empty for an unbound key", "[binding]")
{
	using HoldFast::ResolveBinding;

	const auto resolved = ResolveBinding(0x4000, kCustomControlmap);
	CHECK(resolved.soloEvent.empty());
	CHECK(resolved.comboModifiers.empty());
}

TEST_CASE("ResolveBinding keeps first solo event and deduplicates modifiers", "[binding]")
{
	using HoldFast::ResolveBinding;

	const std::vector<HoldFast::RawMapping> mappings{
		{ "First", 0x0010, 0x0000 },
		{ "ComboA", 0x0010, 0x0100 },
		{ "Second", 0x0010, 0x0000 },
		{ "ComboA repeat", 0x0010, 0x0100 },
		{ "ComboB", 0x0010, 0x1000 },
	};
	const auto resolved = ResolveBinding(0x0010, mappings);
	CHECK(resolved.soloEvent == "First");
	REQUIRE(resolved.comboModifiers.size() == 2);
	CHECK(resolved.comboModifiers[0] == 0x0100);
	CHECK(resolved.comboModifiers[1] == 0x1000);
}

TEST_CASE("IsUsedAsModifier detects keys used as combo modifiers", "[binding]")
{
	using HoldFast::IsUsedAsModifier;

	CHECK(IsUsedAsModifier(0x0100, kCustomControlmap));
	CHECK(IsUsedAsModifier(0x1000, kCustomControlmap));
	CHECK_FALSE(IsUsedAsModifier(0x0010, kCustomControlmap));
	CHECK_FALSE(IsUsedAsModifier(0x0020, kCustomControlmap));
}
