/*
	Copyright (C) 2003 - 2024
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

#include "time_of_day.hpp"

#include "config.hpp"
#include "gettext.hpp"

std::ostream& operator<<(std::ostream& s, const tod_color& c)
{
	s << c.r << "," << c.g << "," << c.b;
	return s;
}

time_of_day::time_of_day(const config& cfg)
	: lawful_bonus(cfg[str_lawful_bonus])
	, bonus_modified(0)
	, image(cfg[str_image])
	, name(cfg[str_name].t_str())
	, description(cfg[str_description].t_str())
	, id(cfg[str_id])
	, image_mask(cfg[str_mask])
	, color(cfg[str_red], cfg[str_green], cfg[str_blue])
	, sounds(cfg[str_sound])
{
}

time_of_day::time_of_day()
	: lawful_bonus(0)
	, bonus_modified(0)
	, image()
	, name(N_("Stub Time of Day"))
	, description(N_("This Time of Day is only a Stub!"))
	, id("nulltod")
	, image_mask()
	, color(0, 0, 0)
	, sounds()
{
}

void time_of_day::write(config& cfg, std::string textdomain) const
{
	cfg[str_lawful_bonus] = lawful_bonus;
	cfg[str_red] = color.r;
	cfg[str_green] = color.g;
	cfg[str_blue] = color.b;
	cfg[str_image] = image;
	cfg[str_name] = textdomain.empty() ? name : t_string(name, textdomain);
	cfg[str_id] = id;

	// Optional keys
	cfg[str_description].write_if_not_empty(textdomain.empty() ? description : t_string(description, textdomain));
	cfg[str_mask].write_if_not_empty(image_mask);
	cfg[str_sound].write_if_not_empty(sounds);
}

void time_of_day::parse_times(const config& cfg, std::vector<time_of_day>& times)
{
	for(const config& t : cfg.child_range(str_time)) {
		times.emplace_back(t);
	}
}
