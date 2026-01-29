/*
	Copyright (C) 2003 - 2025
	by David White <dave@whitevine.net>
	Part of the Battle for Wesnoth Project https://www.wesnoth.org/

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY.

	See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "config.hpp"
#include "display_context.hpp"
#include "filter_context.hpp"
#include "game_config.hpp"
#include "game_data.hpp"
#include "log.hpp"
#include "map/map.hpp"
#include "side_filter.hpp"
#include "team.hpp"
#include "terrain/filter.hpp"
#include "tod_manager.hpp"
#include "units/unit.hpp"
#include "units/filter.hpp"
#include "variable.hpp"
#include "formula/callable_objects.hpp"
#include "formula/formula.hpp"
#include "formula/function_gamestate.hpp"
#include "scripting/game_lua_kernel.hpp"
#include "units/unit_alignments.hpp"

#include <boost/range/adaptor/transformed.hpp>

static lg::log_domain log_engine("engine");
#define ERR_NG LOG_STREAM(err, log_engine)
#define WRN_NG LOG_STREAM(warn, log_engine)

static lg::log_domain log_wml("wml");
#define ERR_WML LOG_STREAM(err, log_wml)

terrain_filter::~terrain_filter()
{
}

terrain_filter::terrain_filter(const vconfig& cfg, const filter_context* fc, const bool flat_tod)
	: cfg_(cfg)
	, fc_(fc)
	, cache_()
	, max_loop_(game_config::max_loop)
	, flat_(flat_tod)
{
}

terrain_filter::terrain_filter(const vconfig& cfg, const terrain_filter& original)
	: cfg_(cfg)
	, fc_(original.fc_)
	, cache_()
	, max_loop_(original.max_loop_)
	, flat_(original.flat_)
{
}

terrain_filter::terrain_filter(const terrain_filter& other)
	: xy_pred() // We should construct this too, since it has no datamembers use the default constructor.
	, cfg_(other.cfg_)
	, fc_(other.fc_)
	, cache_()
	, max_loop_(other.max_loop_)
	, flat_(other.flat_)
{
}

terrain_filter& terrain_filter::operator=(const terrain_filter& other)
{
	// Use copy constructor to make sure we are coherent
	if (this != &other) {
		this->~terrain_filter();
		new (this) terrain_filter(other) ;
	}
	return *this ;
}

terrain_filter::terrain_filter_cache::terrain_filter_cache()
	: parsed_terrain(nullptr)
	, adjacent_matches(nullptr)
	, adjacent_match_cache()
	, ufilter_()
{
}

bool terrain_filter::match_internal(const map_location& loc, const unit* ref_unit, const bool ignore_xy) const
{
	location_set temp{loc};
	match_internal(temp, ref_unit, ignore_xy);
	return temp.size() > 0;
}

void terrain_filter::match_internal(location_set& locs, const unit* ref_unit, const bool ignore_xy) const
{
	auto& context = fc_->get_disp_context();
	auto& map = context.map();

	location_set to_remove;
	for (auto& loc : locs) {
		if (!map.on_board_with_border(loc)) {
			to_remove.insert(loc);
		}
	}

	std::string lua_function = cfg_.expand_str(str_lua_function);
	if (!lua_function.empty() && fc_->get_lua_kernel()) {
		auto kernel = fc_->get_lua_kernel();
		auto func = lua_function.c_str();
		for (auto& loc : locs) {
			if (!kernel->run_filter(func, loc)) {
				to_remove.insert(loc);
			}
		}
	}

	//Filter Areas
	if (cfg_.has_attribute(str_area)) {
		auto& area = fc_->get_tod_man().get_area_by_id(cfg_.expand_str(str_area));
		for (auto& loc : locs) {
			if (area.count(loc) == 0) {
				to_remove.insert(loc);
			}
		}
	}

	if(cfg_.has_attribute(str_gives_income)) {
		bool gives_income = cfg_[str_gives_income].to_bool();
		for (auto& loc : locs) {
			if (gives_income != map.is_village(loc)) {
				to_remove.insert(loc);
			}
		}
	}

	if (cfg_.has_attribute(str_terrain)) {
		if(cache_.parsed_terrain == nullptr) {
			cache_.parsed_terrain.reset(new t_translation::ter_match(std::string_view(cfg_.expand_str(str_terrain))));
		}
		if(!cache_.parsed_terrain->is_empty) {
			for (auto& loc : locs) {
				const t_translation::terrain_code letter = map.get_terrain_info(loc).number();
				if (!t_translation::terrain_matches(letter, *cache_.parsed_terrain)) {
					to_remove.insert(loc);
				}
			}
		}
	}

	//Allow filtering on location ranges
	if (!ignore_xy) {
		std::string x = cfg_[str_x];
		std::string y = cfg_[str_y];
		for (auto& loc : locs) {
			if (!loc.matches_range(x, y)) {
				to_remove.insert(loc);
			}
		}
		//allow filtering by searching a stored variable of locations
		if (cfg_.has_attribute(str_find_in)) {
			if (const game_data * gd = fc_->get_game_data()) {
				try
				{
					variable_access_const vi = gd->get_variable_access_read(cfg_.expand_str(str_find_in));
					for (const config &cfg : vi.as_array()) {
						to_remove.insert(map_location(cfg, nullptr));
					}
				}
				catch (const invalid_variablename_exception&)
				{
					locs.clear();
					return;
				}
			}
		}
		if (cfg_.has_attribute(str_location_id)) {
			location_set matching_locs;
			for(const auto& id : utils::split(cfg_.expand_str(str_location_id))) {
				map_location test_loc = map.special_location(id);
				if(test_loc.valid()) {
					matching_locs.insert(test_loc);
				}
			}
			for (auto& loc : matching_locs) {
				if (!locs.count(loc)) {
					to_remove.insert(loc);
				}
			}
		}
	}

	//Allow filtering on unit
	if(cfg_.has_child(str_filter)) {
		if (!cache_.ufilter_) {
			cache_.ufilter_.reset(new unit_filter(cfg_.child(str_filter).make_safe()));
			cache_.ufilter_->set_use_flat_tod(flat_);
		}

		auto& units = context.units();
		for (auto& loc : locs) {
			const unit_map::const_iterator u = units.find(loc);
			if (!u.valid())
				to_remove.insert(loc);
			else if (!cache_.ufilter_->matches(*u, loc))
				to_remove.insert(loc);
		}
	}

	// Allow filtering on visibility to a side
	if (cfg_.has_child(str_filter_vision)) {
		const vconfig::child_list& vis_filt = cfg_.get_children(str_filter_vision);
		vconfig::child_list::const_iterator i, i_end = vis_filt.end();
		for (i = vis_filt.begin(); i != i_end; ++i) {
			bool visible = (*i)[str_visible].to_bool(true);
			bool respect_fog = (*i)[str_respect_fog].to_bool(true);

			side_filter ssf(*i, fc_);
			std::vector<int> sides = ssf.get_teams();

			for (auto& loc : locs) {
				bool found = false;
				for (const int side : sides) {
					const team &viewing_team = context.get_team(side);
					bool viewer_sees = respect_fog ? !viewing_team.fogged(loc) : !viewing_team.shrouded(loc);
					if (visible == viewer_sees) {
						found = true;
						break;
					}
				}
				if (!found) {
					to_remove.insert(loc);
				}
			}
		}
	}

	//Allow filtering on adjacent locations
	if(cfg_.has_child(str_filter_adjacent_location)) {
		const vconfig::child_list& adj_cfgs = cfg_.get_children(str_filter_adjacent_location);
		vconfig::child_list::const_iterator i, i_end, i_begin = adj_cfgs.begin();

		for (auto& loc : locs) {
			const auto adjacent = get_adjacent_tiles(loc);
			for (i = i_begin, i_end = adj_cfgs.end(); i != i_end; ++i) {
				int match_count = 0;
				vconfig::child_list::difference_type index = i - i_begin;
				std::vector<map_location::direction> dirs = (*i).has_attribute(str_adjacent)
					? map_location::parse_directions((*i)[str_adjacent]) : map_location::all_directions();
				std::vector<map_location::direction>::const_iterator j, j_end = dirs.end();
				for (j = dirs.begin(); j != j_end; ++j) {
					const map_location &adj = adjacent[static_cast<int>(*j)];
					if (fc_->get_disp_context().map().on_board(adj)) {
						if(cache_.adjacent_matches == nullptr) {
							while(index >= std::distance(cache_.adjacent_match_cache.begin(), cache_.adjacent_match_cache.end())) {
								const vconfig& adj_cfg = adj_cfgs[cache_.adjacent_match_cache.size()];
								std::pair<terrain_filter, std::map<map_location,bool>> amc_pair(
									terrain_filter(adj_cfg, *this),
									std::map<map_location,bool>());
								cache_.adjacent_match_cache.push_back(amc_pair);
							}
							terrain_filter &amc_filter = cache_.adjacent_match_cache[index].first;
							std::map<map_location,bool> &amc = cache_.adjacent_match_cache[index].second;
							std::map<map_location,bool>::iterator lookup = amc.find(adj);
							if(lookup == amc.end()) {
								if(amc_filter(adj)) {
									amc[adj] = true;
									++match_count;
								} else {
									amc[adj] = false;
								}
							} else if(lookup->second) {
								++match_count;
							}
						} else {
							assert(index < std::distance(cache_.adjacent_matches->begin(), cache_.adjacent_matches->end()));
							location_set &amc = (*cache_.adjacent_matches)[index];
							if(amc.find(adj) != amc.end()) {
								++match_count;
							}
						}
					}
				}
				static std::vector<std::pair<int,int>> default_counts = utils::parse_ranges_unsigned("1-6");
				const std::vector<std::pair<int,int>>& counts = (*i).has_attribute(str_count)
					? utils::parse_ranges_unsigned((*i)[str_count]) : default_counts;
				if(!in_ranges(match_count, counts)) {
					to_remove.insert(loc);
				}
			}
		}
	}

	std::string tod_type = cfg_.expand_str(str_time_of_day);
	std::string tod_id = cfg_.expand_str(str_time_of_day_id);
	if(!tod_type.empty()) {

		const std::vector<std::string>& vals = utils::split(tod_type);
		bool any_chaotic = std::find(vals.begin(),vals.end(), unit_alignments::chaotic) == vals.end();
		bool any_lawful = std::find(vals.begin(),vals.end(), unit_alignments::lawful) == vals.end();
		bool any_neutral_and_liminal = std::find(vals.begin(),vals.end(), unit_alignments::neutral) == vals.end()
							    	&& std::find(vals.begin(),vals.end(), unit_alignments::liminal) == vals.end();

		const std::vector<std::string>& id_vals = utils::split(tod_id);
		bool any_id_comma = std::find(tod_id.begin(),tod_id.end(),',') != tod_id.end();

		// creating a time_of_day is expensive, only do it if we will use it
		time_of_day tod;
		auto& tod_man = fc_->get_tod_man();
		auto& units = context.units();

		for (auto& loc : locs) {
			if(flat_) {
				tod = tod_man.get_time_of_day(loc);
			} else {
				tod = tod_man.get_illuminated_time_of_day(units, map, loc);
			}

			if (   (any_chaotic && tod.lawful_bonus<0)
				|| (any_lawful && tod.lawful_bonus>0)
				|| (any_neutral_and_liminal && tod.lawful_bonus==0)) {
				to_remove.insert(loc);
			}
			else {
				if(tod_id != tod.id) {
					if( any_id_comma
						&& std::search(tod_id.begin(),tod_id.end(),
						tod.id.begin(),tod.id.end()) != tod_id.end()) {
						if(std::find(id_vals.begin(),id_vals.end(),tod.id) == id_vals.end()) {
							to_remove.insert(loc);
						}
					} else {
						to_remove.insert(loc);
					}
				}
			}
		}
	}

	//allow filtering on owner (for villages)
	bool has_owner_side = cfg_.non_empty(str_owner_side);
	const vconfig& filter_owner = cfg_.child(str_filter_owner);
	if(!filter_owner.null()) {
		if(has_owner_side) {
			WRN_NG << "duplicate side information in a SLF, ignoring inline owner_side=";
		}

		side_filter ssf(filter_owner, fc_);
		std::vector<const team*> teams;
		for(const int side : ssf.get_teams()) {
			teams.push_back(&context.get_team(side));
		}

		for (auto& loc : locs) {
			if(!map.is_village(loc)) {
				to_remove.insert(loc);
			}
			else {
				if(teams.empty() && context.village_owner(loc) == 0) {
					continue;
				}

				bool found = false;
				for(auto team : teams) {
					if(team->owns_village(loc)) {
						found = true;
						break;
					}
				}
				if(!found)
					to_remove.insert(loc);
			}
		}
	}
	else if(has_owner_side) {
		const int side_num = cfg_[str_owner_side].to_int(0);
		for (auto& loc : locs) {
			if(context.village_owner(loc) != side_num) {
				to_remove.insert(loc);
			}
		}
	}

	if(cfg_.has_attribute(str_formula)) {
		wfl::gamestate_function_symbol_table symbols;
		const wfl::formula form(cfg_.expand_str(str_formula), &symbols);

		for (auto& loc : locs) {
			try {
				const wfl::terrain_callable main(context, loc);
				wfl::map_formula_callable callable(main.fake_ptr());
				if(ref_unit) {
					auto ref = std::make_shared<wfl::unit_callable>(*ref_unit);
					callable.add("teleport_unit", wfl::variant(ref));
					// It's not destroyed upon scope exit because the variant holds a reference
				}
				if(!form.evaluate(callable).as_bool()) {
					to_remove.insert(loc);
				}
			} catch(const wfl::formula_error& e) {
				lg::log_to_chat() << "Formula error in location filter: " << e.type << " at " << e.filename << ':' << e.line << ")\n";
				ERR_WML << "Formula error in location filter: " << e.type << " at " << e.filename << ':' << e.line << ")";
				// Formulae with syntax errors match nothing
				to_remove.insert(loc);
			}
		}
	}

	for (auto& loc : to_remove) {
		locs.erase(loc);
	}
}

class filter_with_unit : public xy_pred {
	const terrain_filter& filt_;
	const unit& ref_;
public:
	filter_with_unit(const terrain_filter& filt, const unit& ref) : filt_(filt), ref_(ref) {}
	bool operator()(const map_location& loc) const override {
		return filt_.match(loc, ref_);
	}
};

bool terrain_filter::match_impl(const map_location& loc, const unit* ref_unit) const
{
	if(cfg_[str_x] == str_recall && cfg_[str_y] == str_recall) {
		return !fc_->get_disp_context().map().on_board(loc);
	}
	location_set hexes;
	std::vector<map_location> loc_vec(1, loc);

	std::unique_ptr<scoped_wml_variable> ref_unit_var;
	if(ref_unit) {
		if(fc_->get_disp_context().map().on_board(ref_unit->get_location())) {
			ref_unit_var.reset(new scoped_xy_unit("teleport_unit", ref_unit->get_location(), fc_->get_disp_context().units()));
		} else {
			// Possible TODO: Support recall list units?
		}
	}

	//handle radius
	std::size_t radius = cfg_[str_radius].to_size_t(0);
	if(radius > max_loop_) {
		ERR_NG << "terrain_filter: radius greater than " << max_loop_
		<< ", restricting";
		radius = max_loop_;
	}
	if ( radius == 0 )
		hexes.insert(loc_vec.begin(), loc_vec.end());
	else if ( cfg_.has_child(str_filter_radius) ) {
		terrain_filter r_filter(cfg_.child(str_filter_radius), *this);
		if(ref_unit) {
			get_tiles_radius(fc_->get_disp_context().map(), loc_vec, radius, hexes, false, filter_with_unit(r_filter, *ref_unit));
		} else {
			get_tiles_radius(fc_->get_disp_context().map(), loc_vec, radius, hexes, false, r_filter);
		}
	} else {
		get_tiles_radius(fc_->get_disp_context().map(), loc_vec, radius, hexes);
	}

	std::size_t loop_count = 0;
	for(auto i = hexes.begin(); i != hexes.end(); ++i) {
		bool matches = match_internal(*i, ref_unit, false);

		// Handle [and], [or], and [not] with in-order precedence
		for(const auto& [key, filter] : cfg_.all_ordered()) {
			// Handle [and]
			if(key == str_and) {
				matches = matches && terrain_filter(filter, *this).match_impl(*i, ref_unit);
			}
			// Handle [or]
			else if(key == str_or) {
				matches = matches || terrain_filter(filter, *this).match_impl(*i, ref_unit);
			}
			// Handle [not]
			else if(key == str_not) {
				matches = matches && !terrain_filter(filter, *this).match_impl(*i, ref_unit);
			}
		}

		if(matches) {
			return true;
		}
		if(++loop_count > max_loop_) {
			location_set::const_iterator temp = i;
			if(++temp != hexes.end()) {
				ERR_NG << "terrain_filter: loop count greater than " << max_loop_
				<< ", aborting";
				break;
			}
		}
	}
	return false;
}
//using a class to be able to firen it in terrain_filter
class terrain_filterimpl
{
public:
	struct no_start_set_yet {};
	struct no_filter
	{
		bool operator()(const map_location&) const { return true; }
	};

	template<typename T, typename F1, typename F2, typename F3>
	static void filter_final(T&& src, location_set& dest, const terrain_filter&, const F1& f1, const F2& f2, const F3& f3)
	{
		for (map_location loc : src) {
			if (f1(loc) && f2(loc) && f3(loc)) {
				dest.insert(loc);
			}
		}
	}

	template<typename T, typename F1, typename F2>
	static void filter_special_loc(T&& src, location_set& dest, const terrain_filter& filter, const F1& f1, const F2& f2)
	{
		if (filter.cfg_.has_attribute(str_location_id)) {
			location_set matching_locs;
			for(const auto& id : utils::split(filter.cfg_[str_location_id])) {
				map_location test_loc = filter.fc_->get_disp_context().map().special_location(id);
				if(test_loc.valid()) {
					matching_locs.insert(test_loc);
				}
			}
			filter_final(src, dest, filter, f1, f2, [matching_locs](const map_location& loc) { return matching_locs.count(loc) > 0; });
		}
		else {
			filter_final(src, dest, filter, f1, f2, no_filter());
		}
	}

	template<typename T, typename F1>
	static void filter_area(T&& src, location_set& dest, const terrain_filter& filter, const F1& f1)
	{
		if (filter.cfg_.has_attribute(str_area)) {
			const location_set& area = filter.fc_->get_tod_man().get_area_by_id(filter.cfg_[str_area]);
			filter_special_loc(src, dest, filter, f1, [&area](const map_location& loc) { return area.find(loc) != area.end(); });
		}
		else {
			filter_special_loc(src, dest, filter, f1, no_filter());
		}
	}

	template<typename T>
	static void filter_xy(T&& src, location_set& dest, const terrain_filter& filter, bool with_border)
	{
		if (filter.cfg_.has_attribute(str_x) || filter.cfg_.has_attribute(str_y)) {
			std::vector<map_location> xy_vector = filter.fc_->get_disp_context().map().parse_location_range(filter.cfg_[str_x], filter.cfg_[str_y], with_border);
			filter_area(src, dest, filter, [&xy_vector](const map_location& loc) { return utils::contains(xy_vector, loc); });
		}
		else {
			filter_area(src, dest, filter, no_filter());
		}
	}
};
//using lambdas with boost transformed gives compile erros on gcc (it works on clang and msvc)
struct cfg_to_loc
{
	map_location operator()(const config& cfg) const { return map_location(cfg, nullptr); }
	typedef map_location result_type;
};
void terrain_filter::get_locs_impl(location_set& locs, const unit* ref_unit, bool with_border) const
{
	std::unique_ptr<scoped_wml_variable> ref_unit_var;
	if(ref_unit) {
		if(fc_->get_disp_context().map().on_board(ref_unit->get_location())) {
			ref_unit_var.reset(new scoped_xy_unit("teleport_unit", ref_unit->get_location(), fc_->get_disp_context().units()));
		} else {
			// Possible TODO: Support recall list units?
		}
	}

	location_set match_set;

	// See if the caller provided an override to with_border
	with_border = cfg_[str_include_borders].to_bool(with_border);

	if (cfg_.has_attribute(str_find_in)) {

		if (const game_data * gd = fc_->get_game_data()) {
			try
			{
				auto ar = gd->get_variable_access_read(cfg_[str_find_in]).as_array();
				terrain_filterimpl::filter_xy(ar | boost::adaptors::transformed(cfg_to_loc()), match_set, *this, with_border);
			}
			catch (const invalid_variablename_exception&)
			{
				//Do nothing
			}
		}
	}
	else if (cfg_.has_attribute(str_x) || cfg_.has_attribute(str_y)) {
		std::vector<map_location> xy_vector = fc_->get_disp_context().map().parse_location_range(cfg_[str_x], cfg_[str_y], with_border);
		terrain_filterimpl::filter_area(xy_vector, match_set, *this, terrain_filterimpl::no_filter());
	}
	else if (cfg_.has_attribute(str_area)) {
		const location_set& area = fc_->get_tod_man().get_area_by_id(cfg_[str_area]);
		terrain_filterimpl::filter_special_loc(area, match_set, *this, terrain_filterimpl::no_filter(), terrain_filterimpl::no_filter());
	}
	else if (cfg_.has_attribute(str_location_id)) {
		for(const auto& id : utils::split(cfg_[str_location_id])) {
			map_location test_loc = fc_->get_disp_context().map().special_location(id);
			if(test_loc.valid()) {
				match_set.insert(test_loc);
			}
		}
	}
	else if (cfg_[str_gives_income].to_bool()) {
		auto ar = fc_->get_disp_context().map().villages();
		terrain_filterimpl::filter_xy(ar, match_set, *this, with_border);
	}
	else {
		//consider all locations on the map
		int bs = fc_->get_disp_context().map().border_size();
		int w = with_border ? fc_->get_disp_context().map().w() + bs : fc_->get_disp_context().map().w();
		int h = with_border ? fc_->get_disp_context().map().h() + bs : fc_->get_disp_context().map().h();
		for (int x = with_border ? 0 - bs : 0; x < w; ++x) {
			for (int y = with_border ? 0 - bs : 0; y < h; ++y) {
				match_set.insert(map_location(x, y));
			}
		}
	}

	//handle location filter
	if(cfg_.has_child(str_filter_adjacent_location)) {
		if(cache_.adjacent_matches == nullptr) {
			cache_.adjacent_matches.reset(new std::vector<location_set>());
		}
		const vconfig::child_list& adj_cfgs = cfg_.get_children(str_filter_adjacent_location);
		for (unsigned i = 0; i < adj_cfgs.size(); ++i) {
			location_set adj_set;
			/* GCC-3.3 doesn't like operator[] so use at(), which has the same result */
			terrain_filter(adj_cfgs.at(i), *this).get_locations(adj_set, with_border);
			cache_.adjacent_matches->push_back(adj_set);
			if(i >= max_loop_ && i+1 < adj_cfgs.size()) {
				ERR_NG << "terrain_filter: loop count greater than " << max_loop_
				<< ", aborting";
				break;
			}
		}
	}

	match_internal(match_set, ref_unit, true);

	int ors_left = std::count_if(cfg_.ordered_begin(), cfg_.ordered_end(), [](const auto& val) { return val.first == "or"; });

	// Handle [and], [or], and [not] with in-order precedence
	for(const auto& [key, filter] : cfg_.all_ordered()) {
		//if there are no locations or [or] conditions left, go ahead and return empty
		if(match_set.empty() && ors_left <= 0) {
			return;
		}

		// Handle [and]
		if(key == str_and) {
			location_set intersect_hexes;
			terrain_filter(filter, *this).get_locations(intersect_hexes, with_border);
			location_set intersection;
			for (auto& loc : match_set) {
				if (intersect_hexes.count(loc) == 1) {
					intersection.insert(loc);
				}
			}
			match_set.swap(intersection);
		}
		// Handle [or]
		else if(key == str_or) {
			location_set union_hexes;
			terrain_filter(filter, *this).get_locations(union_hexes, with_border);
			//match_set.insert(union_hexes.begin(), union_hexes.end()); //doesn't compile on MSVC
			location_set::iterator insert_itor = union_hexes.begin();
			while(insert_itor != union_hexes.end()) {
				match_set.insert(*insert_itor++);
			}
			--ors_left;
		}
		// Handle [not]
		else if(key == str_not) {
			location_set removal_hexes;
			terrain_filter(filter, *this).get_locations(removal_hexes, with_border);
			for (auto& loc : removal_hexes) {
				match_set.erase(loc);
			}
		}
	}
	if(match_set.empty()) {
		return;
	}

	//handle radius
	std::size_t radius = cfg_[str_radius].to_size_t(0);
	if(radius > max_loop_) {
		ERR_NG << "terrain_filter: radius greater than " << max_loop_
		<< ", restricting";
		radius = max_loop_;
	}
	if(radius > 0) {
		std::vector<map_location> xy_vector (match_set.begin(), match_set.end());
		if(cfg_.has_child(str_filter_radius)) {
			terrain_filter r_filter(cfg_.child(str_filter_radius), *this);
			get_tiles_radius(fc_->get_disp_context().map(), xy_vector, radius, locs, with_border, r_filter);
		} else {
			get_tiles_radius(fc_->get_disp_context().map(), xy_vector, radius, locs, with_border);
		}
	} else {
		locs.insert(match_set.begin(), match_set.end());
	}
}

config terrain_filter::to_config() const
{
	return cfg_.get_config();
}
