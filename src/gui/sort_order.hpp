/*
	Copyright (C) 2008 - 2025
	Part of the Battle for Wesnoth Project https://www.wesnoth.org/

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY.

	See the COPYING file for more details.
*/

#pragma once

#include "enum_base.hpp"
#include "config_strings.hpp"

struct sort_order_defines
{
	static constexpr const utils::interned_string& none = str_none;
	static constexpr const utils::interned_string& ascending = str_ascending;
	static constexpr const utils::interned_string& descending = str_descending;

	ENUM_AND_ARRAY(none, ascending, descending)
};
using sort_order = string_enums::enum_base<sort_order_defines>;
