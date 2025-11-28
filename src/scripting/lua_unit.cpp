/*
	Copyright (C) 2009 - 2024
	by Guillaume Melquiond <guillaume.melquiond@gmail.com>
	Part of the Battle for Wesnoth Project https://www.wesnoth.org/

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY.

	See the COPYING file for more details.
*/

#include "scripting/lua_unit.hpp"

#include "formatter.hpp"
#include "game_board.hpp"
#include "log.hpp"
#include "map/location.hpp"             // for map_location
#include "map/map.hpp"
#include "resources.hpp"
#include "scripting/lua_common.hpp"
#include "scripting/lua_unit_attacks.hpp"
#include "scripting/push_check.hpp"
#include "scripting/game_lua_kernel.hpp"
#include "units/unit.hpp"
#include "units/map.hpp"
#include "units/animation_component.hpp"
#include "game_version.hpp"
#include "deprecation.hpp"

#include "lua/wrapper_lauxlib.h"

static lg::log_domain log_scripting_lua("scripting/lua");
#define LOG_LUA LOG_STREAM(info, log_scripting_lua)
#define ERR_LUA LOG_STREAM(err, log_scripting_lua)

static const char getunitKey[] = "unit";
static const char ustatusKey[] = "unit status";
static const char unitvarKey[] = "unit variables";

lua_unit::~lua_unit()
{
}

unit* lua_unit::get() const
{
	if (c_ptr) return c_ptr;
	if (ptr) return ptr.get();
	if (side) {
		return resources::gameboard->get_team(side).recall_list().find_if_matches_underlying_id(uid).get();
	}
	unit_map::unit_iterator ui = resources::gameboard->units().find(uid);
	if (!ui.valid()) return nullptr;
	c_ptr = ui.get_shared_ptr().get(); //&*ui would not be legal, must get new shared_ptr by copy ctor because the unit_map itself is holding a boost shared pointer.
	return c_ptr;
}
unit_ptr lua_unit::get_shared() const
{
	if (ptr) return ptr;
	if (side) {
		return resources::gameboard->get_team(side).recall_list().find_if_matches_underlying_id(uid);
	}
	unit_map::unit_iterator ui = resources::gameboard->units().find(uid);
	if (!ui.valid()) return unit_ptr();
	return ui.get_shared_ptr(); //&*ui would not be legal, must get new shared_ptr by copy ctor because the unit_map itself is holding a boost shared pointer.
}

// Having this function here not only simplifies other code, it allows us to move
// pointers around from one structure to another.
// This makes bare pointer->map in particular about 2 orders of magnitude faster,
// as benchmarked from Lua code.
bool lua_unit::put_map(const map_location &loc)
{
	if (ptr) {
		auto [unit_it, success] = resources::gameboard->units().replace(loc, ptr);

		if(success) {
			ptr.reset();
			uid = unit_it->underlying_id();
		} else {
			ERR_LUA << "Could not move unit " << ptr->underlying_id() << " onto map location " << loc;
			return false;
		}
	} else if (side) { // recall list
		unit_ptr it = resources::gameboard->get_team(side).recall_list().extract_if_matches_underlying_id(uid);
		if (it) {
			side = 0;
			// uid may be changed by unit_map on insertion
			uid = resources::gameboard->units().replace(loc, it).first->underlying_id();
		} else {
			ERR_LUA << "Could not find unit " << uid << " on recall list of side " << side;
			return false;
		}
	} else { // on map
		unit_map::unit_iterator ui = resources::gameboard->units().find(uid);
		if (ui != resources::gameboard->units().end()) {
			map_location from = ui->get_location();
			if (from != loc) { // This check is redundant in current usage
				resources::gameboard->units().erase(loc);
				resources::gameboard->units().move(from, loc);
			}
			// No need to change our contents
		} else {
			ERR_LUA << "Could not find unit " << uid << " on the map";
			return false;
		}
	}
	return true;
}

bool luaW_isunit(lua_State* L, int index)
{
	return luaL_testudata(L, index,getunitKey) != nullptr;
}

enum {
	LU_OK,
	LU_NOT_UNIT,
	LU_NOT_ON_MAP,
	LU_NOT_VALID,
};

static lua_unit* internal_get_unit(lua_State *L, int index, bool only_on_map, int& error)
{
	error = LU_OK;
	if(!luaW_isunit(L, index)) {
		error = LU_NOT_UNIT;
		return nullptr;
	}
	lua_unit* lu = static_cast<lua_unit*>(lua_touserdata(L, index));
	if(only_on_map && !lu->on_map()) {
		error = LU_NOT_ON_MAP;
	}
	if(!lu->get()) {
		error = LU_NOT_VALID;
	}
	return lu;
}

unit* luaW_tounit(lua_State *L, int index, bool only_on_map)
{
	int error;
	lua_unit* lu = internal_get_unit(L, index, only_on_map, error);
	if(error != LU_OK) {
		return nullptr;
	}
	return lu->get();
}

unit_ptr luaW_tounit_ptr(lua_State *L, int index, bool only_on_map)
{
	int error;
	lua_unit* lu = internal_get_unit(L, index, only_on_map, error);
	if(error != LU_OK) {
		return nullptr;
	}
	return lu->get_shared();
}

lua_unit* luaW_tounit_ref(lua_State *L, int index)
{
	int error;
	return internal_get_unit(L, index, false, error);
}

static void unit_show_error(lua_State *L, int index, int error)
{
	switch(error) {
		case LU_NOT_UNIT:
			luaW_type_error(L, index, "unit");
			break;
		case LU_NOT_VALID:
			luaL_argerror(L, index, "unit not found");
			break;
		case LU_NOT_ON_MAP:
			luaL_argerror(L, index, "unit not found on map");
			break;
	}
}

unit_ptr luaW_checkunit_ptr(lua_State *L, int index, bool only_on_map)
{
	int error;
	lua_unit* lu = internal_get_unit(L, index, only_on_map, error);
	unit_show_error(L, index, error);
	return lu->get_shared();
}

unit& luaW_checkunit(lua_State *L, int index, bool only_on_map)
{
	int error;
	lua_unit* lu = internal_get_unit(L, index, only_on_map, error);
	unit_show_error(L, index, error);
	return *lu->get();
}

lua_unit* luaW_checkunit_ref(lua_State *L, int index)
{
	int error;
	lua_unit* lu = internal_get_unit(L, index, false, error);
	unit_show_error(L, index, error);
	return lu;
}

void lua_unit::setmetatable(lua_State *L)
{
	luaL_setmetatable(L, getunitKey);
}

lua_unit* luaW_pushlocalunit(lua_State *L, unit& u)
{
	lua_unit* res = new(L) lua_unit(u);
	lua_unit::setmetatable(L);
	return res;
}

/**
 * Destroys a unit object before it is collected (__gc metamethod).
 */
static int impl_unit_collect(lua_State *L)
{
	lua_unit *u = static_cast<lua_unit *>(lua_touserdata(L, 1));
	u->lua_unit::~lua_unit();
	return 0;
}

/**
 * Checks two lua proxy units for equality. (__eq metamethod)
 */
static int impl_unit_equality(lua_State* L)
{
	unit& left = luaW_checkunit(L, 1);
	unit& right = luaW_checkunit(L, 2);
	const bool equal = left.underlying_id() == right.underlying_id();
	lua_pushboolean(L, equal);
	return 1;
}

/**
 * Turns a lua proxy unit to string. (__tostring metamethod)
 */
static int impl_unit_tostring(lua_State* L)
{
	const lua_unit* lu = luaW_tounit_ref(L, 1);
	unit &u = *lu->get();
	std::ostringstream str;

	str << "unit: <";
	if(!u.id().empty()) {
		str << u.id() << " ";
	} else {
		str << u.type_id() << " ";
	}
	if(int side = lu->on_recall_list()) {
		str << "at (side " << side << " recall list)";
	} else {
		str << "at (" << u.get_location() << ")";
	}
	str << '>';

	lua_push(L, str.str());
	return 1;
}

/**
 * Gets some data on a unit (__index metamethod).
 * - Arg 1: full userdata containing the unit id.
 * - Arg 2: string containing the name of the property.
 * - Ret 1: something containing the attribute.
 */
static int impl_unit_get(lua_State *L)
{
	enum attribute_name {
		valid,
		x,
		y,
		side,
		id,
		type,
		image_mods,
		usage,
		ellipse,
		halo,
		hitpoints,
		max_hitpoints,
		experience,
		max_experience,
		recall_cost,
		moves,
		max_moves,
		max_attacks,
		attacks_left,
		vision,
		jamming,
		name,
		description,
		canrecruit,
		renamable,
		level,
		cost,
		extra_recruit,
		advances_to,
		animations,
		recall_filter,
		hidden,
		resting,
		role,
		race,
		gender,
		variation,
		undead_variation,
		zoc,
		facing,
		portrait,
		image,
		flag_rgb,
		modifications,
		resistance,
		underlying_id,
		__cfg,
		loc,
		__goto,
		alignment,
		upkeep,
		advancements,
		overlays,
		traits,
		abilities,
		status,
		variables,
		attacks,
		petrified
	};
	static const std::unordered_map<std::string, attribute_name> attributes {
		{"valid", valid},
		{"x", x},
		{"y", y},
		{"side", side},
		{"id", id},
		{"type", type},
		{"image_mods", image_mods},
		{"usage", usage},
		{"ellipse", ellipse},
		{"halo", halo},
		{"hitpoints", hitpoints},
		{"max_hitpoints", max_hitpoints},
		{"experience", experience},
		{"max_experience", max_experience},
		{"recall_cost", recall_cost},
		{"moves", moves},
		{"max_moves", max_moves},
		{"max_attacks", max_attacks},
		{"attacks_left", attacks_left},
		{"vision", vision},
		{"jamming", jamming},
		{"name", name},
		{"description", description},
		{"canrecruit", canrecruit},
		{"renamable", renamable},
		{"level", level},
		{"cost", cost},
		{"extra_recruit", extra_recruit},
		{"advances_to", advances_to},
		{"animations", animations},
		{"recall_filter", recall_filter},
		{"hidden", hidden},
		{"resting", resting},
		{"role", role},
		{"race", race},
		{"gender", gender},
		{"variation", variation},
		{"undead_variation", undead_variation},
		{"zoc", zoc},
		{"facing", facing},
		{"portrait", portrait},
		{"image", image},
		{"flag_rgb", flag_rgb},
		{"modifications", modifications},
		{"resistance", resistance},
		{"underlying_id", underlying_id},
		{"__cfg", __cfg},
		{"loc", loc},
		{"goto", __goto},
		{"alignment", alignment},
		{"upkeep", upkeep},
		{"advancements", advancements},
		{"overlays", overlays},
		{"traits", traits},
		{"abilities", abilities},
		{"status", status},
		{"variables", variables},
		{"attacks", attacks},
		{"petrified", petrified}
	};
	lua_unit *lu = static_cast<lua_unit *>(lua_touserdata(L, 1));
	size_t len;
	const char* s = luaL_checklstring(L, 2, &len);
	std::string m{s, len};
	const unit* pu = lu->get();

	auto it = attributes.find(m);
	if (it != attributes.end() && it->second == valid) {
		if(!pu) {
			return 0;
		}
		if(lu->on_map()) {
			lua_pushstring(L, "map");
		} else if(lu->on_recall_list()) {
			lua_pushstring(L, "recall");
		} else {
			lua_pushstring(L, "private");
		}
		return 1;
	}

	if(!pu) {
		return luaL_argerror(L, 1, "unknown unit");
	}

	if (it == attributes.end()) {
		// static std::vector<std::string> path{"wesnoth", "units", ""};
		// path[2] = m;
		// if(luaW_getglobal(L, path)) {
		static const char* path[3] = {"wesnoth", "units", nullptr};
		path[2] = m.c_str();
		if(lua_recursiveglobalrawget(L, path, 3)) {
			return 1;
		}
		else {
			return 0;
		}
	}

#define case_int_attrib(accessor) \
	lua_pushinteger(L, (accessor)); \
	break;
#define case_string_attrib(accessor) { \
	const std::string& str = (accessor); \
	lua_pushlstring(L, str.c_str(), str.length()); \
	break; }
#define case_tstring_attrib(accessor) \
	luaW_pushtstring(L, (accessor)); \
	break;
#define case_bool_attrib(accessor) \
	lua_pushboolean(L, (accessor)); \
	break;
#define case_cfg_attrib(accessor) {\
	config cfg; \
	{accessor;} \
	luaW_pushconfig(L, cfg); \
	break; }
#define case_vector_string_attrib(accessor) {\
	const std::vector<std::string>& vector = (accessor); \
	lua_createtable(L, vector.size(), 0); \
	int i = 1; \
	for (const std::string& s : vector) { \
		lua_pushlstring(L, s.c_str(), s.length()); \
		lua_rawseti(L, -2, i); \
		++i; \
	} \
	break; }

	const unit& u = *pu;
	switch (it->second) {
		case valid: // dummy, case already handled
			break;
		case x:
			case_int_attrib(u.get_location().wml_x());
		case y:
			case_int_attrib(u.get_location().wml_y());
		case side:
			case_int_attrib(u.side());
		case id:
			case_string_attrib(u.id());
		case type:
			case_string_attrib(u.type_id());
		case image_mods:
			case_string_attrib(u.effect_image_mods());
		case usage:
			case_string_attrib(u.usage());
		case ellipse:
			case_string_attrib(u.image_ellipse());
		case halo:
			case_string_attrib(u.image_halo());
		case hitpoints:
			case_int_attrib(u.hitpoints());
		case max_hitpoints:
			case_int_attrib(u.max_hitpoints());
		case experience:
			case_int_attrib(u.experience());
		case max_experience:
			case_int_attrib(u.max_experience());
		case recall_cost:
			case_int_attrib(u.recall_cost());
		case moves:
			case_int_attrib(u.movement_left());
		case max_moves:
			case_int_attrib(u.total_movement());
		case max_attacks:
			case_int_attrib(u.max_attacks());
		case attacks_left:
			case_int_attrib(u.attacks_left());
		case vision:
			case_int_attrib(u.vision());
		case jamming:
			case_int_attrib(u.jamming());
		case name:
			case_tstring_attrib(u.name());
		case description:
			case_tstring_attrib(u.unit_description());
		case canrecruit:
			case_bool_attrib(u.can_recruit());
		case renamable:
			case_bool_attrib(!u.unrenamable());
		case level:
			case_int_attrib(u.level());
		case cost:
			case_int_attrib(u.cost());
		case extra_recruit:
			case_vector_string_attrib(u.recruits());
		case advances_to:
			case_vector_string_attrib(u.advances_to());
		case animations:
			case_vector_string_attrib(u.anim_comp().get_flags());
		case recall_filter:
			case_cfg_attrib(cfg = u.recall_filter());
		case hidden:
			case_bool_attrib(u.get_hidden());
		case resting:
			case_bool_attrib(u.resting());
		case role:
			case_string_attrib(u.get_role());
		case race:
			case_string_attrib(u.race()->id());
		case gender:
			case_string_attrib(gender_string(u.gender()));
		case variation:
			case_string_attrib(u.variation());
		case undead_variation:
			case_string_attrib(u.undead_variation());
		case zoc:
			case_bool_attrib(u.get_emit_zoc());
		case facing:
			case_string_attrib(map_location::write_direction(u.facing()));
		case portrait:
			case_string_attrib(u.big_profile() == u.absolute_image()
				? u.absolute_image() + u.image_mods() + "~SCALE_SHARP(144,144)"
				: u.big_profile());
		case image:
			case_string_attrib(u.type().image());
		case flag_rgb:
			case_string_attrib(u.flag_rgb());
		case modifications:
			case_cfg_attrib(u.get_modifications());
		case resistance:
			if (!u.get_attr_changed(unit::UA_MOVEMENT_TYPE)) return 0;
			case_cfg_attrib(u.movement_type().get_resistances().write(cfg));
		case underlying_id:
			case_int_attrib(u.underlying_id());
		case __cfg:
			case_cfg_attrib(u.write(cfg); u.get_location().write(cfg));

		case loc: {
			luaW_pushlocation(L, u.get_location());
			break;
		}
		case __goto: {
			luaW_pushlocation(L, u.get_goto());
			break;
		}
		case alignment: {
			lua_push(L, unit_alignments::get_string(u.alignment()));
			break;
		}
		case upkeep: {
			unit::upkeep_t upkeep = u.upkeep_raw();

			// Need to keep these separate in order to ensure an int value is always used if applicable.
			if(int* v = utils::get_if<int>(&upkeep)) {
				lua_push(L, *v);
			} else {
				const std::string type = utils::visit(unit::upkeep_type_visitor{}, upkeep);
				lua_push(L, type);
			}

			break;
		}
		case advancements: {
			lua_push(L, u.modification_advancements());
			break;
		}
		case overlays: {
			lua_push(L, u.overlays());
			break;
		}
		case traits: {
			lua_push(L, u.get_traits_list());
			break;
		}
		case abilities: {
			lua_push(L, u.get_ability_list());
			break;
		}
		case status: {
			lua_createtable(L, 1, 0);
			lua_pushvalue(L, 1);
			lua_rawseti(L, -2, 1);
			luaL_setmetatable(L, ustatusKey);
			break;
		}
		case variables: {
			lua_createtable(L, 1, 0);
			lua_pushvalue(L, 1);
			lua_rawseti(L, -2, 1);
			luaL_setmetatable(L, unitvarKey);
			break;
		}
		case attacks: {
			push_unit_attacks_table(L, 1);
			break;
		}
		case petrified: {
			deprecated_message("(unit).petrified", DEP_LEVEL::INDEFINITE, {1,17,0}, "use (unit).status.petrified instead");
			lua_pushboolean(L, u.incapacitated());
			break;
		}
	}

	return 1;
}

/**
 * Sets some data on a unit (__newindex metamethod).
 * - Arg 1: full userdata containing the unit id.
 * - Arg 2: string containing the name of the property.
 * - Arg 3: something containing the attribute.
 */
static int impl_unit_set(lua_State *L)
{
	lua_unit *lu = static_cast<lua_unit *>(lua_touserdata(L, 1));
	char const *m = luaL_checkstring(L, 2);
	unit* pu = lu->get();
	if (!pu) return luaL_argerror(L, 1, "unknown unit");
	unit &u = *pu;

	// Find the corresponding attribute.
	//modify_int_attrib_check_range("side", u.set_side(value), 1, static_cast<int>(teams().size())); TODO: Figure out if this is a good idea, to refer to teams() and make this depend on having a gamestate
	modify_int_attrib("side", u.set_side(value));
	modify_int_attrib("moves", u.set_movement(value));
	modify_int_attrib("max_moves", u.set_total_movement(value));
	modify_int_attrib("max_attacks", u.set_max_attacks(value));
	modify_int_attrib("hitpoints", u.set_hitpoints(value));
	modify_int_attrib("max_hitpoints", u.set_max_hitpoints(value));
	modify_int_attrib("experience", u.set_experience(value));
	modify_int_attrib("max_experience", u.set_max_experience(value));
	modify_int_attrib("recall_cost", u.set_recall_cost(value));
	modify_int_attrib("attacks_left", u.set_attacks(value));
	modify_int_attrib("level", u.set_level(value));
	modify_bool_attrib("resting", u.set_resting(value));
	modify_tstring_attrib("name", u.set_name(value));
	modify_tstring_attrib("description", u.set_unit_description(value));
	modify_string_attrib("portrait", u.set_big_profile(value));
	modify_string_attrib("role", u.set_role(value));
	modify_string_attrib("facing", u.set_facing(map_location::parse_direction(value)));
	modify_string_attrib("usage", u.set_usage(value));
	modify_string_attrib("undead_variation", u.set_undead_variation(value));
	modify_string_attrib("ellipse", u.set_image_ellipse(value));
	modify_string_attrib("halo", u.set_image_halo(value));
	modify_bool_attrib("hidden", u.set_hidden(value));
	modify_bool_attrib("zoc", u.set_emit_zoc(value));
	modify_bool_attrib("canrecruit", u.set_can_recruit(value));
	modify_bool_attrib("renamable", u.set_unrenamable(!value));
	modify_cfg_attrib("recall_filter", u.set_recall_filter(cfg));

	modify_vector_string_attrib("extra_recruit", u.set_recruits(value));
	modify_vector_string_attrib("advances_to", u.set_advances_to(value));
	if(strcmp(m, "alignment") == 0) {
		u.set_alignment(lua_enum_check<unit_alignments>(L, 3));
		return 0;
	}

	if(strcmp(m, "advancements") == 0) {
		u.set_advancements(lua_check<std::vector<config>>(L, 3));
		return 0;
	}

	if(strcmp(m, "upkeep") == 0) {
		if(lua_isnumber(L, 3)) {
			u.set_upkeep(static_cast<int>(luaL_checkinteger(L, 3)));
			return 0;
		}
		const char* v = luaL_checkstring(L, 3);
		if((strcmp(v, "loyal") == 0) || (strcmp(v, "free") == 0)) {
			u.set_upkeep(unit::upkeep_loyal());
		} else if(strcmp(v, "full") == 0) {
			u.set_upkeep(unit::upkeep_full());
		} else {
			std::string err_msg = "unknown upkeep value of unit: ";
			err_msg += v;
			return luaL_argerror(L, 2, err_msg.c_str());
		}
		return 0;
	}

	if(!lu->on_map()) {
		map_location loc = u.get_location();
		modify_int_attrib("x", loc.set_wml_x(value); u.set_location(loc));
		modify_int_attrib("y", loc.set_wml_y(value); u.set_location(loc));
		modify_string_attrib("id", u.set_id(value));
		if(strcmp(m, "loc") == 0) {
			luaW_tolocation(L, 3, loc);
			u.set_location(loc);
			return 0;
		}
	} else {
		const bool is_key_x = strcmp(m, "x") == 0;
		const bool is_key_y = strcmp(m, "y") == 0;
		const bool is_loc_key = strcmp(m, "loc") == 0;

		// Handle moving an on-map unit
		if(is_key_x || is_key_y || is_loc_key) {
			game_board* gb = resources::gameboard;

			if(!gb) {
				return 0;
			}

			map_location src = u.get_location();
			map_location dst = src;

			if(is_key_x) {
				dst.set_wml_x(luaL_checkinteger(L, 3));
			} else if(is_key_y) {
				dst.set_wml_y(luaL_checkinteger(L, 3));
			} else {
				dst = luaW_checklocation(L, 3);
			}

			// TODO: could probably be relegated to a helper function.
			if(src != dst) {
				// If the dst isn't on the map, the unit will be clobbered. Guard against that.
				if(!gb->map().on_board(dst)) {
					std::string err_msg = formatter() << "destination hex not on map (excluding border): " << dst;
					return luaL_argerror(L, 2, err_msg.c_str());
				}

				auto [unit_iterator, success] = gb->units().move(src, dst);

				if(success) {
					unit_iterator->anim_comp().set_standing();
				}
			}

			return 0;
		}
	}

	if(strcmp(m, "goto") == 0) {
		u.set_goto(luaW_checklocation(L, 3));
		return 0;
	}

	std::string err_msg = "unknown modifiable property of unit: ";
	err_msg += m;
	return luaL_argerror(L, 2, err_msg.c_str());
}

/**
 * Gets the status of a unit (__index metamethod).
 * - Arg 1: table containing the userdata containing the unit id.
 * - Arg 2: string containing the name of the status.
 * - Ret 1: boolean.
 */
static int impl_unit_status_get(lua_State *L)
{
	if(!lua_istable(L, 1)) {
		return luaW_type_error(L, 1, "unit status");
	}
	lua_rawgeti(L, 1, 1);
	const unit* u = luaW_tounit(L, -1);
	if(!u) {
		return luaL_argerror(L, 1, "unknown unit");
	}
	char const *m = luaL_checkstring(L, 2);
	lua_pushboolean(L, u->get_state(m));
	return 1;
}

/**
 * Sets the status of a unit (__newindex metamethod).
 * - Arg 1: table containing the userdata containing the unit id.
 * - Arg 2: string containing the name of the status.
 * - Arg 3: boolean.
 */
static int impl_unit_status_set(lua_State *L)
{
	if(!lua_istable(L, 1)) {
		return luaW_type_error(L, 1, "unit status");
	}
	lua_rawgeti(L, 1, 1);
	unit* u = luaW_tounit(L, -1);
	if(!u) {
		return luaL_argerror(L, 1, "unknown unit");
	}
	char const *m = luaL_checkstring(L, 2);
	u->set_state(m, luaW_toboolean(L, 3));
	return 0;
}

/**
 * Gets the variable of a unit (__index metamethod).
 * - Arg 1: table containing the userdata containing the unit id.
 * - Arg 2: string containing the name of the status.
 * - Ret 1: boolean.
 */
static int impl_unit_variables_get(lua_State *L)
{
	if(!lua_istable(L, 1)) {
		return luaW_type_error(L, 1, "unit variables");
	}
	lua_rawgeti(L, 1, 1);
	const unit* u = luaW_tounit(L, -1);
	if(!u) {
		return luaL_argerror(L, 2, "unknown unit");
	}
	char const *m = luaL_checkstring(L, 2);
	return_cfgref_attrib("__cfg", u->variables());

	variable_access_const v(m, u->variables());
	return luaW_pushvariable(L, v) ? 1 : 0;
}

/**
 * Sets the variable of a unit (__newindex metamethod).
 * - Arg 1: table containing the userdata containing the unit id.
 * - Arg 2: string containing the name of the status.
 * - Arg 3: scalar.
 */
static int impl_unit_variables_set(lua_State *L)
{
	if(!lua_istable(L, 1)) {
		return luaW_type_error(L, 1, "unit variables");
	}
	lua_rawgeti(L, 1, 1);
	unit* u = luaW_tounit(L, -1);
	if(!u) {
		return luaL_argerror(L, 2, "unknown unit");
	}
	char const *m = luaL_checkstring(L, 2);
	modify_cfg_attrib("__cfg", u->variables() = cfg);
	config& vars = u->variables();
	if(lua_isnoneornil(L, 3)) {
		try {
			variable_access_throw(m, vars).clear(false);
		} catch(const invalid_variablename_exception&) {
		}
		return 0;
	}
	variable_access_create v(m, vars);
	luaW_checkvariable(L, v, 3);
	return 0;
}

namespace lua_units {
	std::string register_metatables(lua_State* L)
	{
		std::ostringstream cmd_out;

		// Create the getunit metatable.
		cmd_out << "Adding getunit metatable...\n";

		luaL_newmetatable(L, getunitKey);
		lua_pushcfunction(L, impl_unit_collect);
		lua_setfield(L, -2, "__gc");
		lua_pushcfunction(L, impl_unit_equality);
		lua_setfield(L, -2, "__eq");
		lua_pushcfunction(L, impl_unit_tostring);
		lua_setfield(L, -2, "__tostring");
		lua_pushcfunction(L, impl_unit_get);
		lua_setfield(L, -2, "__index");
		lua_pushcfunction(L, impl_unit_set);
		lua_setfield(L, -2, "__newindex");
		lua_pushstring(L, "unit");
		lua_setfield(L, -2, "__metatable");

		// Create the unit status metatable.
		cmd_out << "Adding unit status metatable...\n";

		luaL_newmetatable(L, ustatusKey);
		lua_pushcfunction(L, impl_unit_status_get);
		lua_setfield(L, -2, "__index");
		lua_pushcfunction(L, impl_unit_status_set);
		lua_setfield(L, -2, "__newindex");
		lua_pushstring(L, "unit status");
		lua_setfield(L, -2, "__metatable");

		// Create the unit variables metatable.
		cmd_out << "Adding unit variables metatable...\n";

		luaL_newmetatable(L, unitvarKey);
		lua_pushcfunction(L, impl_unit_variables_get);
		lua_setfield(L, -2, "__index");
		lua_pushcfunction(L, impl_unit_variables_set);
		lua_setfield(L, -2, "__newindex");
		lua_pushstring(L, "unit variables");
		lua_setfield(L, -2, "__metatable");

		return cmd_out.str();
	}
}
