/*
	Copyright (C) 2006 - 2025
	by Joerg Hinrichs <joerg.hinrichs@alice-dsl.de>
	Copyright (C) 2003 by David White <dave@whitevine.net>
	Part of the Battle for Wesnoth Project https://www.wesnoth.org/

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY.

	See the COPYING file for more details.
*/

/**
 *  @file
 *  Container for multiplayer game-creation parameters.
 */

#include "log.hpp"
#include "mp_game_settings.hpp"
#include "serialization/string_utils.hpp"

static lg::log_domain log_engine("engine");
#define ERR_NG LOG_STREAM(err, log_engine)
#define WRN_NG LOG_STREAM(warn, log_engine)
#define LOG_NG LOG_STREAM(info, log_engine)
#define DBG_NG LOG_STREAM(debug, log_engine)

mp_game_settings::mp_game_settings() :
	name(),
	password(),
	hash(),
	mp_era_name(),
	mp_scenario(),
	mp_scenario_name(),
	mp_campaign(),
	side_users(),
	num_turns(0),
	village_gold(0),
	village_support(1),
	xp_modifier(100),
	mp_countdown_init_time(0),
	mp_countdown_reservoir_time(0),
	mp_countdown_turn_bonus(0),
	mp_countdown_action_bonus(0),
	mp_countdown(false),
	use_map_settings(false),
	random_start_time(false),
	fog_game(false),
	shroud_game(false),
	allow_observers(true),
	private_replay(false),
	shuffle_sides(false),
	saved_game(saved_game_mode::type::no),
	mode(random_faction_mode::type::independent),
	options(),
	addons()
{}

mp_game_settings::mp_game_settings(const config& cfg)
	: name(cfg[str_scenario].str())
	, password()
	, hash(cfg[str_hash].str())
	, mp_era_name(cfg[str_mp_era_name].str())
	, mp_scenario(cfg[str_mp_scenario].str())
	, mp_scenario_name(cfg[str_mp_scenario_name].str())
	, mp_campaign(cfg[str_mp_campaign].str())
	, side_users(utils::map_split(cfg[str_side_users]))
	, num_turns(cfg[str_mp_num_turns].to_int())
	, village_gold(cfg[str_mp_village_gold].to_int())
	, village_support(cfg[str_mp_village_support].to_int())
	, xp_modifier(cfg[str_experience_modifier].to_int(100))
	, mp_countdown_init_time(cfg[str_mp_countdown_init_time].to_int())
	, mp_countdown_reservoir_time(cfg[str_mp_countdown_reservoir_time].to_int())
	, mp_countdown_turn_bonus(cfg[str_mp_countdown_turn_bonus].to_int())
	, mp_countdown_action_bonus(cfg[str_mp_countdown_action_bonus].to_int())
	, mp_countdown(cfg[str_mp_countdown].to_bool())
	, use_map_settings(cfg[str_mp_use_map_settings].to_bool())
	, random_start_time(cfg[str_mp_random_start_time].to_bool())
	, fog_game(cfg[str_mp_fog].to_bool())
	, shroud_game(cfg[str_mp_shroud].to_bool())
	, allow_observers(cfg[str_observer].to_bool())
	, private_replay(cfg[str_private_replay].to_bool())
	, shuffle_sides(cfg[str_shuffle_sides].to_bool())
	, saved_game(saved_game_mode::get_enum(cfg[str_savegame].str()).value_or(saved_game_mode::type::no))
	, mode(random_faction_mode::get_enum(cfg[str_random_faction_mode].str()).value_or(random_faction_mode::type::independent))
	, options(cfg.child_or_empty("options"))
	, addons()
{
	for (const config & a : cfg.child_range(str_addon)) {
		if (!a[str_id].empty()) {
			addons.emplace(a[str_id].str(), addon_version_info(a));
		}
	}
}

config mp_game_settings::to_config() const
{
	config cfg;

	cfg[str_scenario] = name;
	cfg[str_hash] = hash;
	cfg[str_mp_era_name] = mp_era_name;
	cfg[str_mp_scenario] = mp_scenario;
	cfg[str_mp_scenario_name] = mp_scenario_name;
	cfg[str_mp_campaign] = mp_campaign;
	cfg[str_side_users] = utils::join_map(side_users);
	cfg[str_experience_modifier] = xp_modifier;
	cfg[str_mp_countdown] = mp_countdown;
	cfg[str_mp_countdown_init_time] = mp_countdown_init_time;
	cfg[str_mp_countdown_turn_bonus] = mp_countdown_turn_bonus;
	cfg[str_mp_countdown_reservoir_time] = mp_countdown_reservoir_time;
	cfg[str_mp_countdown_action_bonus] = mp_countdown_action_bonus;
	cfg[str_mp_num_turns] = num_turns;
	cfg[str_mp_village_gold] = village_gold;
	cfg[str_mp_village_support] = village_support;
	cfg[str_mp_fog] = fog_game;
	cfg[str_mp_shroud] = shroud_game;
	cfg[str_mp_use_map_settings] = use_map_settings;
	cfg[str_mp_random_start_time] = random_start_time;
	cfg[str_observer] = allow_observers;
	cfg[str_private_replay] = private_replay;
	cfg[str_shuffle_sides] = shuffle_sides;
	cfg[str_random_faction_mode] = random_faction_mode::get_string(mode);
	cfg[str_savegame] = saved_game_mode::get_string(saved_game);
	cfg.add_child(str_options, options);

	for(auto& p : addons) {
		config & c = cfg.add_child(str_addon);
		p.second.write(c);
		c[str_id] = p.first;
	}

	return cfg;
}

mp_game_settings::addon_version_info::addon_version_info(const config & cfg)
	: version()
	, min_version()
	, name(cfg[str_name])
	, required(cfg[str_required].to_bool(false))
	, content()
{
	if (!cfg[str_version].empty()) {
		version = cfg[str_version].str();
	}
	if (!cfg[str_min_version].empty()) {
		min_version = cfg[str_min_version].str();
	}
	for(const auto& child : cfg.child_range(str_content)) {
		content.emplace_back(addon_content{ child[str_id].str(), child[str_name].str(), child[str_type].str() });
	}
}

void mp_game_settings::addon_version_info::write(config & cfg) const {
	if (version) {
		cfg[str_version] = *version;
	}
	if (min_version) {
		cfg[str_min_version] = *min_version;
	}

	cfg[str_name]	= name;
	cfg[str_required] = required;
	for(const auto& item : content) {
		config& c = cfg.add_child(str_content);
		c[str_id] = item.id;
		c[str_name] = item.name;
		c[str_type] = item.type;
	}
}

void mp_game_settings::update_addon_requirements(const config & cfg) {
	if (cfg[str_id].empty()) {
		WRN_NG << "Tried to add add-on metadata to a game, missing mandatory id field... skipping.\n" << cfg.debug();
		return;
	}

	mp_game_settings::addon_version_info new_data(cfg);

	// if the add-on doesn't require all players have it, then min_version is irrelevant
	if(!new_data.required) {
		new_data.min_version = {};
	}
	// else if it is required and no min_version was explicitly specified, default the min_version to the add-on's version
	else if(new_data.required && !new_data.min_version) {
		new_data.min_version = new_data.version;
	}

	std::map<std::string, addon_version_info>::iterator it = addons.find(cfg[str_id].str());
	// Check if this add-on already has an entry as a dependency for this scenario. If so, try to reconcile their version info,
	// by taking the larger of the min versions. The version should be the same for all WML from the same add-on...
	if (it != addons.end()) {
		addon_version_info& addon = it->second;

		// an add-on can contain multiple types of content
		// for example, an era and a scenario
		for(const auto& item : new_data.content) {
			addon.content.emplace_back(addon_content{ item.id, item.name, item.type });
		}

		if(addon.version != new_data.version) {
			ERR_NG << "Addon version data mismatch! Not all local WML has same version of the addon: '" << cfg[str_id].str() << "'.";
		}

		if(new_data.required) {
			addon.required = true;

			// if the existing entry for the add-on didn't have a min_version or had a lower min_version, update it to this min_version
			if (!addon.min_version || *new_data.min_version > *addon.min_version) {
				addon.min_version = new_data.min_version;
			}
		}
	} else {
		// Didn't find this addon-id in the map, so make a new entry.
		addons.emplace(cfg[str_id].str(), new_data);
	}
}
