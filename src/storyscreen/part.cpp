/*
	Copyright (C) 2009 - 2025
	by Iris Morelle <shadowm2006@gmail.com>
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
 * @file
 * Storyscreen parts and floating images representation.
 */

#include "storyscreen/part.hpp"

#include "config.hpp"
#include "variable.hpp"

namespace storyscreen
{
floating_image::floating_image(const config& cfg)
	: file_(cfg[str_file])
	, x_(cfg[str_x].to_int())
	, y_(cfg[str_y].to_int())
	, delay_(cfg[str_delay].to_int())
	, resize_with_background_(cfg[str_resize_with_background].to_bool())
	, centered_(cfg[str_centered].to_bool())
{
}

background_layer::background_layer()
	: scale_horizontally_(true)
	, scale_vertically_(true)
	, tile_horizontally_(false)
	, tile_vertically_(false)
	, keep_aspect_ratio_(true)
	, is_base_layer_(false)
	, image_file_()
{
}

background_layer::background_layer(const config& cfg)
	: scale_horizontally_(true)
	, scale_vertically_(true)
	, tile_horizontally_(false)
	, tile_vertically_(false)
	, keep_aspect_ratio_(true)
	, is_base_layer_(false)
	, image_file_()
{
	if(cfg.has_attribute(str_image)) {
		image_file_ = cfg[str_image].str();
	}

	if(cfg.has_attribute(str_scale)) {
		scale_vertically_ = cfg[str_scale].to_bool(true);
		scale_horizontally_ = cfg[str_scale].to_bool(true);
	} else {
		if(cfg.has_attribute(str_scale_vertically)) {
			scale_vertically_ = cfg[str_scale_vertically].to_bool(true);
		}

		if(cfg.has_attribute(str_scale_horizontally)) {
			scale_horizontally_ = cfg[str_scale_horizontally].to_bool(true);
		}
	}

	if(cfg.has_attribute(str_tile)) {
		tile_vertically_ = cfg[str_tile].to_bool(false);
		tile_horizontally_ = cfg[str_tile].to_bool(false);
	} else {
		if(cfg.has_attribute(str_tile_vertically)) {
			tile_vertically_ = cfg[str_tile_vertically].to_bool(false);
		}

		if(cfg.has_attribute(str_tile_horizontally)) {
			tile_horizontally_ = cfg[str_tile_horizontally].to_bool(false);
		}
	}

	if(cfg.has_attribute(str_keep_aspect_ratio)) {
		keep_aspect_ratio_ = cfg[str_keep_aspect_ratio].to_bool(true);
	}

	if(cfg.has_attribute(str_base_layer)) {
		is_base_layer_ = cfg[str_base_layer].to_bool(false);
	}
}

part::part(const vconfig& part_cfg)
	: show_title_()
	, text_()
	, text_title_()
	, text_block_loc_(part::BLOCK_BOTTOM)
	, text_alignment_("left")
	, title_alignment_("left")
	, music_()
	, sound_()
	, background_layers_()
	, floating_images_()
{
	resolve_wml(part_cfg);
}

part::BLOCK_LOCATION part::string_tblock_loc(const std::string& s)
{
	if(s.empty() != true) {
		if(s == "top") {
			return part::BLOCK_TOP;
		} else if(s == "middle") {
			return part::BLOCK_MIDDLE;
		}
	}

	return part::BLOCK_BOTTOM;
}

void part::resolve_wml(const vconfig& cfg)
{
	if(cfg.null()) {
		return;
	}

	// Converts shortcut syntax to members of [background_layer]
	background_layer bl;

	if(cfg.has_attribute(str_background)) {
		bl.set_file(cfg[str_background].str());
	}

	if(cfg.has_attribute(str_scale_background)) {
		bl.set_scale_horizontally(cfg[str_scale_background].to_bool(true));
		bl.set_scale_vertically(cfg[str_scale_background].to_bool(true));
	} else {
		if(cfg.has_attribute(str_scale_background_vertically)) {
			bl.set_scale_vertically(cfg[str_scale_background_vertically].to_bool(true));
		}

		if(cfg.has_attribute(str_scale_background_horizontally)) {
			bl.set_scale_horizontally(cfg[str_scale_background_horizontally].to_bool(true));
		}
	}

	if(cfg.has_attribute(str_tile_background)) {
		bl.set_tile_horizontally(cfg[str_tile_background].to_bool(false));
		bl.set_tile_vertically(cfg[str_tile_background].to_bool(false));
	} else {
		if(cfg.has_attribute(str_tile_background_vertically)) {
			bl.set_tile_vertically(cfg[str_tile_background_vertically].to_bool(false));
		}

		if(cfg.has_attribute(str_tile_background_horizontally)) {
			bl.set_tile_vertically(cfg[str_tile_background_horizontally].to_bool(false));
		}
	}

	if(cfg.has_attribute(str_keep_aspect_ratio)) {
		bl.set_keep_aspect_ratio(cfg[str_keep_aspect_ratio].to_bool(true));
	}

	background_layers_.push_back(bl);

	if(cfg.has_attribute(str_show_title)) {
		show_title_ = cfg[str_show_title].to_bool();
	}

	if(cfg.has_attribute(str_story)) {
		text_ = cfg[str_story].str();
	}

	if(cfg.has_attribute(str_title)) {
		text_title_ = cfg[str_title].str();
		if(!cfg.has_attribute(str_show_title)) {
			show_title_ = true;
		}
	}

	if(cfg.has_attribute(str_text_layout)) {
		text_block_loc_ = string_tblock_loc(cfg[str_text_layout]);
	}

	if(cfg.has_attribute(str_text_alignment)) {
		text_alignment_ = cfg[str_text_alignment].str();
	}

	const auto decode_hposition = [](const std::string& pos_str) {
		if(pos_str == "left") {
			return 0;
		} else if (pos_str == "center") {
			return 50;
		} else if (pos_str == "right") {
			return 100;
		} else {
			return 0;
		}
	};

	const auto decode_vposition = [&loc = text_block_loc_](const std::string& pos_str) {
		// The ternary checks avoid part text and title text overlaping.
		if(pos_str == "top") {
			return loc == BLOCK_TOP ? 50 : 0;
		} else if (pos_str == "middle") {
			return loc == BLOCK_MIDDLE ? 0 : 50;
		} else if (pos_str == "bottom") {
			return loc == BLOCK_BOTTOM ? 50 : 100;
		} else {
			return 0;
		}
	};

	if(cfg.has_attribute("title_position")) {
		if(cfg[str_title_position] == "centered") {
			title_perc_pos_ = {50, 50};
		} else {
			const auto vals = utils::split(cfg[str_title_position]);
			switch(vals.size()) {
			case 0:
				// No values provided. Default to top-left.
				title_perc_pos_ = {0, 0};
				break;

			case 1:
				// Singe value, could be either horizontal or vertical.
				title_perc_pos_ = {decode_hposition(vals[0]), decode_vposition(vals[0])};
				break;

			default:
				// Separate horizontal and vertical values.
				title_perc_pos_ = {decode_hposition(vals[0]), decode_vposition(vals[1])};
				break;
			}
		}
	}

	if(cfg.has_attribute("title_alignment")) {
		title_alignment_ = cfg[str_title_alignment].str();
	}

	if(cfg.has_attribute(str_music)) {
		music_ = cfg[str_music].str();
	}

	if(cfg.has_attribute(str_sound)) {
		sound_ = cfg[str_sound].str();
	}

	if(cfg.has_attribute(str_voice)) {
		voice_ = cfg[str_voice].str();
	}

	// Inherited
	story_parser::resolve_wml(cfg);
}

bool part::resolve_wml_helper(const std::string& key, const vconfig& node)
{
	bool found = false;

	// [background_layer]
	if(key == "background_layer") {
		background_layers_.push_back(node.get_parsed_config());
		found = true;
	}
	// [image]
	else if(key == "image") {
		floating_images_.push_back(node.get_parsed_config());
		found = true;
	}

	return found;
}

} // end namespace storyscreen
