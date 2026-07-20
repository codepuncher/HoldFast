#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace HoldFast
{
	/**
	 * Mirrors RE::ControlMap::UserEventMapping without the SKSE dependency. modifier == 0
	 * means unmodified; otherwise modifier is the combo's held-first button.
	 */
	struct RawMapping
	{
		std::string   eventID;
		std::uint16_t inputKey{};
		std::uint16_t modifier{};
	};

	struct ResolvedBinding
	{
		std::string                soloEvent;
		std::vector<std::uint16_t> comboModifiers;
	};

	/**
	 * Unlike ControlMap::GetUserEventName, a key that is both solo-bound and a combo
	 * terminal still resolves its solo event.
	 */
	[[nodiscard]] inline ResolvedBinding ResolveBinding(std::uint16_t keyCode, std::span<const RawMapping> mappings)
	{
		ResolvedBinding resolved;
		for (const auto& mapping : mappings) {
			if (mapping.inputKey != keyCode) {
				continue;
			}
			if (mapping.modifier == 0) {
				if (resolved.soloEvent.empty()) {
					resolved.soloEvent = mapping.eventID;
				}
				continue;
			}
			if (std::ranges::find(resolved.comboModifiers, mapping.modifier) == resolved.comboModifiers.end()) {
				resolved.comboModifiers.push_back(mapping.modifier);
			}
		}
		return resolved;
	}

	/**
	 * HoldFast can't pass a combo through when it also tracks the modifier button itself,
	 * since it must own that button's Down to detect holds. Callers only warn about this.
	 */
	[[nodiscard]] inline bool IsUsedAsModifier(std::uint16_t keyCode, std::span<const RawMapping> mappings)
	{
		return std::ranges::any_of(mappings, [keyCode](const RawMapping& mapping) {
			return mapping.modifier == keyCode;
		});
	}
}
