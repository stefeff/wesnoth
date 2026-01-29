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

/**
 *  @file
 *  Definitions related to theme-support.
 */

#pragma once

#include "color.hpp"
#include "config.hpp"
#include "generic_event.hpp"
#include "global.hpp"
#include "sdl/rect.hpp"

#include <memory>

class game_config_view;

struct _rect { std::size_t x1,y1,x2,y2; };

struct theme_info
{
	std::string id;
	t_string name;
	t_string description;
};

template<class Object>
class area_lookup
{
private:

	struct entry
	{
		const rect* area;
		const Object* value;
		int start;

		bool operator<(const entry& rhs) const {
			return start < rhs.start;
		}
		bool operator<(int rhs) const {
			return start < rhs;
		}
	};

	friend inline bool operator<(int lhs, const entry& rhs) {
		return lhs < rhs.start;
	}

public:

	area_lookup(const std::vector<Object>& objects)
		: objects_{ &objects }
		, valid_{ false }
	{}

	void invalidate() { valid_ = false; }

	class result_iterator
	{
	public:
		const Object& operator*() const {
			return *current_;
		}
		result_iterator& operator++() {
			advance();
			return *this;
		}
		bool operator==(const result_iterator& rhs) const {
			return current_ == rhs.current_;
		}
		bool operator!=(const result_iterator& rhs) const {
			return !operator==(rhs);
		}

	private:

		friend class area_lookup::result_range;
		using iterator = typename std::vector<entry>::const_iterator;

		result_iterator(const rect& region)
			: region_{ region }
			, current_{ nullptr }
		{}
		result_iterator(
			  const rect& region
			, iterator current_x
			, iterator last_x
			, iterator current_y
			, iterator last_y
			, iterator current_large
			, iterator last_large)
			: region_{ region }
			, current_{ nullptr }
			, current_x_{ current_x }
			, last_x_{ last_x }
			, current_y_{ current_y }
			, last_y_{ last_y }
			, current_large_{ current_large }
			, last_large_{ last_large }
		{
			advance();
		}

		bool is_match(const entry& e) const {
			return region_.overlaps(*e.area);
		}

		bool advance(iterator& first, iterator& last) {
			if (first != last) {
				if (first->value != current_ && is_match(*first)) {
					current_ = first->value;
					return true;
				}

				while (++first != last) {
					if (is_match(*first)) {
						current_ = first->value;
						return true;
					}
				}
			}

			return false;
		}

		void advance() {
			if (   advance(current_x_, last_x_)
			    || advance(current_y_, last_y_)
			    || advance(current_large_, last_large_)) {
				return;
			}

			current_ = nullptr;
		}

		const rect& region_;
		const Object* current_;

		iterator current_x_;
		iterator last_x_;
		iterator current_y_;
		iterator last_y_;
		iterator current_large_;
		iterator last_large_;
	};

	class result_range
	{
	public:

		result_iterator begin() {
			return { region_, current_x_, last_x_, current_y_, last_y_, current_large_, last_large_ };
		}

		result_iterator end() {
			return { region_ };
		}

	private:

		friend class area_lookup;
		using iterator = typename std::vector<entry>::const_iterator;

		result_range(
			  const rect& region
			, iterator current_x
			, iterator last_x
			, iterator current_y
			, iterator last_y
			, iterator current_large
			, iterator last_large)
			: region_{ region }
			, current_x_{ current_x }
			, last_x_{ last_x }
			, current_y_{ current_y }
			, last_y_{ last_y }
			, current_large_{ current_large }
			, last_large_{ last_large }
		{
		}

		rect region_;

		iterator current_x_;
		iterator last_x_;
		iterator current_y_;
		iterator last_y_;
		iterator current_large_;
		iterator last_large_;
	};

	result_range find(const rect& region, const rect& game_canvas) const {
		fill(game_canvas);

		return {
			region,
			std::lower_bound(small_by_x_.begin(), small_by_x_.end(), region.x - 72),
			std::upper_bound(small_by_x_.begin(), small_by_x_.end(), region.x + region.w),
			std::lower_bound(small_by_y_.begin(), small_by_y_.end(), region.y - 72),
			std::upper_bound(small_by_y_.begin(), small_by_y_.end(), region.y + region.h),
			large_.begin(), large_.end()
		};
	}

private:

	void fill(const rect& game_canvas) const {
		if (!valid_ || game_canvas != game_canvas_) {
			small_by_x_.clear();
			small_by_y_.clear();
			large_.clear();

			for (auto& object : *objects_) {
				auto& area = object.location(game_canvas);
				if (area.w <= SMALL_THRESHOLD) {
					small_by_x_.push_back({ &area, &object, area.x });
				}
				else if (area.h <= SMALL_THRESHOLD) {
					small_by_y_.push_back({ &area, &object, area.y });
				}
				else {
					large_.push_back({ &area, &object, 0 });
				}
			}

			std::sort(small_by_x_.begin(), small_by_x_.end());
			std::sort(small_by_y_.begin(), small_by_y_.end());

			valid_ = true;
			game_canvas_ = game_canvas;
		}
	}

	const std::vector<Object>* objects_;
	mutable rect game_canvas_;
	mutable bool valid_;

	constexpr static int SMALL_THRESHOLD = 72;
	mutable std::vector<entry> small_by_x_;
	mutable std::vector<entry> small_by_y_;
	mutable std::vector<entry> large_;
};

class theme
{

	class object
	{
	public:
		object();
		object(std::size_t sw, std::size_t sh, const config& cfg);
		virtual ~object() { }

		const rect& location(const rect& screen) const
		{
			if (screen != last_screen_) [[unlikely]] {
				update_location(screen);
			}
			return relative_loc_;
		}
		const rect& get_location() const { return loc_; }
		const std::string& get_id() const { return id_; }

		// This supports relocating of theme elements ingame.
		// It is needed for [change] tags in theme WML.
		void modify_location(const _rect& rect);
		void modify_location(const std::string& rect_str, rect rect_ref);

		// All on-screen objects have 'anchoring' in the x and y dimensions.
		// 'fixed' means that they have fixed co-ordinates and don't move.
		// 'top anchored' means they are anchored to the top (or left) side
		// of the screen - the top (or left) edge stays a constant distance
		// from the top of the screen.
		// 'bottom anchored' is the inverse of top anchored.
		// 'proportional' means the location and dimensions change
		// proportionally to the screen size.
		enum ANCHORING { FIXED, TOP_ANCHORED, PROPORTIONAL, BOTTOM_ANCHORED };

	private:
		bool location_modified_;
		std::string id_;
		rect loc_;
		mutable rect relative_loc_;
		mutable rect last_screen_;

		ANCHORING xanchor_, yanchor_;
		std::size_t spec_width_, spec_height_;

		static ANCHORING read_anchor(const std::string& str);

		void update_location(const rect& screen) const;
	};

	struct border_t
	{

		border_t();
		border_t(const config& cfg);

		double size;

		std::string background_image;
		std::string tile_image;

		bool show_border;
	};

public:

	class label : public object
	{
	public:
		label();
		explicit label(std::size_t sw, std::size_t sh, const config& cfg);

		const std::string& text() const { return text_; }
		void set_text(const std::string& text) { text_ = text; }
		const std::string& icon() const { return icon_; }

		bool empty() const { return text_.empty() && icon_.empty(); }

		std::size_t font_size() const { return font_; }
		color_t font_rgb() const { return font_rgb_; }
		bool font_rgb_set() const { return font_rgb_set_; }
	private:
		std::string text_, icon_;
		std::size_t font_;
		bool font_rgb_set_;
		color_t font_rgb_;
	};

	class status_item : public object
	{
	public:

		explicit status_item(std::size_t sw, std::size_t sh, const config& cfg);

		const std::string& prefix() const { return prefix_; }
		const std::string& postfix() const { return postfix_; }

		// If the item has a label associated with it, Show where the label is
		const label* get_label() const { return label_.empty() ? nullptr : &label_; }

		std::size_t font_size() const { return font_; }
		color_t font_rgb() const { return font_rgb_; }
		bool font_rgb_set() const { return font_rgb_set_; }

	private:
		std::string prefix_, postfix_;
		label label_;
		std::size_t font_;
		bool font_rgb_set_;
		color_t font_rgb_;
	};

	class panel : public object
	{
	public:
		explicit panel(std::size_t sw, std::size_t sh, const config& cfg);

		const std::string& image() const { return image_; }

	private:
		std::string image_;
	};

	class action : public object
	{
	public:
		action();
		explicit action(std::size_t sw, std::size_t sh, const config& cfg);

		bool is_context() const  { return context_; }

		const std::string& title() const { return title_; }

		const std::string tooltip(std::size_t index) const;

		const std::string& type() const { return type_; }

		const std::string& image() const { return image_; }

		const std::string& overlay() const { return overlay_; }

		const std::vector<std::string>& items() const { return items_; }

		void set_title(const std::string& new_title) { title_ = new_title; }
	private:
		bool context_, auto_tooltip_, tooltip_name_prepend_;
		std::string title_, tooltip_, image_, overlay_,  type_;
		std::vector<std::string> items_;
	};

	class slider : public object
	{
	public:
		slider();
		explicit slider(std::size_t sw, std::size_t sh, const config& cfg);

		const std::string& title() const { return title_; }

		const std::string& tooltip() const { return tooltip_; }

		const std::string& image() const { return image_; }

		const std::string& overlay() const { return overlay_; }

		bool black_line() const { return black_line_; }

		void set_title(const std::string& new_title) { title_ = new_title; }
	private:
		std::string title_, tooltip_, image_, overlay_;
		bool black_line_;
	};

	class menu : public object
	{
	public:
		menu();
		explicit menu(std::size_t sw, std::size_t sh, const config& cfg);

		bool is_button() const { return button_; }

		bool is_context() const  { return context_; }

		const std::string& title() const { return title_; }

		const std::string& tooltip() const { return tooltip_; }

		const std::string& image() const { return image_; }

		const std::string& overlay() const { return overlay_; }

		const std::vector<config>& items() const { return items_; }

		void set_title(const std::string& new_title) { title_ = new_title; }
	private:
		bool button_;
		bool context_;
		std::string title_, tooltip_, image_, overlay_;
		std::vector<config> items_;
	};

	explicit theme(const config& cfg, const rect& screen);
	theme(const theme&) = delete;
	theme& operator=(const theme&) = delete;
	theme& operator=(theme&&) noexcept;

	bool set_resolution(const rect& screen);
	void modify(const config &cfg);

	auto panels(const rect& region, const rect& game_canvas) const {
		return panels_lookup_.find(region, game_canvas);
	}
	auto labels(const rect& region, const rect& game_canvas) const {
		return labels_lookup_.find(region, game_canvas);
	}
	const std::vector<menu>& menus() const { return menus_; }
	const std::vector<slider>& sliders() const { return sliders_; }
	const std::vector<action>& actions() const { return actions_; }

	const menu* context_menu() const
		{ return context_.is_context() ? &context_ : nullptr; }

	//refresh_title2 changes the title of a menu entry, identified by id.
	//If no menu entry is found, an empty menu object is returned.
	object* refresh_title(const std::string& id, const std::string& new_title);
	object* refresh_title2(const std::string& id, const std::string& title_tag);
	void modify_label(const std::string& id, const std::string& text);

	const status_item* get_status_item(const std::string& item) const;
	const menu *get_menu_item(const std::string &key) const;
	const action* get_action_item(const std::string &key) const;

	const rect& main_map_location(const rect& screen) const
		{ return main_map_.location(screen); }
	const rect& mini_map_location(const rect& screen) const
		{ return mini_map_.location(screen); }
	const rect& unit_image_location(const rect& screen) const
		{ return unit_image_.location(screen); }
	const rect& palette_location(const rect& screen) const
		{ return palette_.location(screen); }

	const border_t& border() const { return border_; }

	events::generic_event& theme_reset_event() { return theme_reset_event_; }

private:
	theme::object& find_element(const std::string& id);
	void add_object(std::size_t sw, std::size_t sh, const config& cfg);
	void remove_object(const std::string& id);
	void set_object_location(theme::object& element, const std::string& rect_str, std::string ref_id);

	//notify observers that the theme has been rebuilt completely
	//atm this is used for replay_controller to add replay controls to the standard theme
	events::generic_event theme_reset_event_;

	std::string cur_theme;
	config cfg_;
	std::vector<panel> panels_;
	area_lookup<panel> panels_lookup_;
	std::vector<label> labels_;
	area_lookup<label> labels_lookup_;
	std::vector<menu> menus_;
	std::vector<action> actions_;
	std::vector<slider> sliders_;

	menu context_;
	action action_context_;

	std::map<std::string, std::unique_ptr<status_item>> status_;

	object main_map_, mini_map_, unit_image_, palette_;

	border_t border_;

	rect screen_dimensions_;
	std::size_t cur_spec_width_, cur_spec_height_;

	static inline std::map<std::string, config> known_themes{};

public:
	/** Copies the theme configs from the main game config. */
	static void set_known_themes(const game_config_view* cfg);

	/** Returns the saved config for the theme with the given ID. */
	NOT_DANGLING static const config& get_theme_config(const std::string& id);

	/** Returns minimal info about saved themes, optionally including hidden ones. */
	static std::vector<theme_info> get_basic_theme_info(bool include_hidden = false);
};
