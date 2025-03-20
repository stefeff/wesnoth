/*
	Copyright (C) 2014 - 2025
	by Mark de Wever <koraq@xs4all.nl>
	Part of the Battle for Wesnoth Project https://www.wesnoth.org/

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY.

	See the COPYING file for more details.
*/

#include "sdl/point.hpp"
#include "sdl/rect.hpp"

#include <cmath>
#include <algorithm>
#include <ostream>

#ifdef __SSE2__
#include <immintrin.h>

#ifdef __SSE4_1__
#define mm_blendv_epi8 _mm_blendv_epi8
#define mm_min_epi32 _mm_min_epi32
#define mm_max_epi32 _mm_max_epi32
#else

static inline __m128i mm_blendv_epi8(__m128i a, __m128i b, __m128i mask)
{
	return _mm_or_si128(_mm_andnot_si128(mask, a), _mm_and_si128(mask, b));
}

static inline __m128i mm_min_epi32(__m128i a, __m128i b)
{
	return mm_blendv_epi8(a,  b, _mm_cmplt_epi32(b, a));
}

static inline __m128i mm_max_epi32(__m128i a, __m128i b)
{
	return mm_blendv_epi8(a,  b, _mm_cmpgt_epi32(b, a));
}

#endif
#endif

std::ostream& operator<<(std::ostream& s, const SDL_Rect& r)
{
	s << '[' << r.x << ',' << r.y << '|' << r.w << ',' << r.h << ']';
	return s;
}

bool rect::empty() const
{
	return SDL_RectEmpty(this);
}

bool rect::contains(int x, int y) const
{
	point p{x, y};
	return SDL_PointInRect(&p, this) != SDL_FALSE;
}

bool rect::contains(const point& point) const
{
	return SDL_PointInRect(&point, this) != SDL_FALSE;
}

bool rect::contains(const rect& r) const
{
#ifdef __SSE2__
	auto lhs_left_bottom = _mm_loadu_si128(reinterpret_cast<const __m128i*>(this));
	auto lhs_size = _mm_bsrli_si128(lhs_left_bottom, 8);
	auto rhs_left_bottom = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&r));
	auto rhs_size = _mm_bsrli_si128(rhs_left_bottom, 8);

	auto lhs_right_top = _mm_add_epi32(lhs_left_bottom, lhs_size);
	auto rhs_right_top = _mm_add_epi32(rhs_left_bottom, rhs_size);

	auto is_left_bottom = _mm_cmplt_epi32(rhs_left_bottom, lhs_left_bottom);
	auto is_right_top = _mm_cmpgt_epi32(rhs_right_top, lhs_right_top);
	auto any_outside = _mm_or_si128(is_left_bottom, is_right_top);
	return _mm_cvtsi128_si64x(any_outside) == 0;
#else
	if(this->x > r.x) return false;
	if(this->y > r.y) return false;
	if(this->x + this->w < r.x + r.w) return false;
	if(this->y + this->h < r.y + r.h) return false;
	return true;
#endif
}

bool rect::overlaps(const rect& r) const
{
#ifdef __SSE2__
	auto lhs_left_bottom = _mm_loadu_si128(reinterpret_cast<const __m128i*>(this));
	auto lhs_size = _mm_bsrli_si128(lhs_left_bottom, 8);
	auto rhs_left_bottom = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&r));
	auto rhs_size = _mm_bsrli_si128(rhs_left_bottom, 8);

	auto right_distance = _mm_sub_epi32(rhs_left_bottom, lhs_left_bottom);
	auto left_distance = _mm_sub_epi32(lhs_left_bottom, rhs_left_bottom);
	auto is_left = _mm_cmplt_epi32(right_distance, _mm_setzero_si128());

	auto right_overlap = _mm_cmplt_epi32(right_distance, lhs_size);
	auto left_overlap = _mm_cmplt_epi32(left_distance, rhs_size);
	auto overlap = mm_blendv_epi8(right_overlap, left_overlap, is_left);
	return _mm_cvtsi128_si64x(overlap) == -1ll;
#else
	return SDL_HasIntersection(this, &r);
#endif
}

rect rect::minimal_cover(const rect& other) const
{
	rect result;
#ifdef __SSE2__
	auto lhs = _mm_loadu_si128(reinterpret_cast<const __m128i*>(this));
	auto rhs = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&other));

	auto lhs_valid = _mm_cmpgt_epi32(lhs, _mm_setzero_si128());
	lhs_valid = _mm_shuffle_epi32(_mm_and_si128(lhs_valid, _mm_bsrli_si128(lhs_valid, 4)), 0xaa);
	auto rhs_valid = _mm_cmpgt_epi32(rhs, _mm_setzero_si128());
	rhs_valid = _mm_shuffle_epi32(_mm_and_si128(rhs_valid, _mm_bsrli_si128(rhs_valid, 4)), 0xaa);

	auto lhs_left_bottom = mm_blendv_epi8(rhs, lhs, lhs_valid);
	auto rhs_left_bottom = mm_blendv_epi8(lhs_left_bottom, rhs, rhs_valid);
	auto lhs_size = _mm_bsrli_si128(lhs_left_bottom, 8);
	auto rhs_size = _mm_bsrli_si128(rhs_left_bottom, 8);

	auto lhs_right_top = _mm_add_epi32(lhs_left_bottom, lhs_size);
	auto rhs_right_top = _mm_add_epi32(rhs_left_bottom, rhs_size);

	auto left_bottom = mm_min_epi32(lhs_left_bottom, rhs_left_bottom);
	auto right_top = mm_max_epi32(lhs_right_top, rhs_right_top);
	auto size = _mm_sub_epi32(right_top, left_bottom);

	auto cover = _mm_or_si128(_mm_move_epi64(left_bottom), _mm_bslli_si128(size, 8));
	_mm_storeu_si128(reinterpret_cast<__m128i*>(&result), cover);
#else
	SDL_UnionRect(this, &other, &result);
#endif
	return result;
}

rect& rect::expand_to_cover(const rect& other)
{
	*this = minimal_cover(other);
	return *this;
}

rect rect::intersect(const rect& other) const
{
	rect result;
#ifdef __SSE2__
	auto lhs_left_bottom = _mm_loadu_si128(reinterpret_cast<const __m128i*>(this));
	auto lhs_size = _mm_bsrli_si128(lhs_left_bottom, 8);
	auto rhs_left_bottom = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&other));
	auto rhs_size = _mm_bsrli_si128(rhs_left_bottom, 8);

	auto lhs_right_top = _mm_add_epi32(lhs_left_bottom, lhs_size);
	auto rhs_right_top = _mm_add_epi32(rhs_left_bottom, rhs_size);

	auto left_bottom = mm_max_epi32(lhs_left_bottom, rhs_left_bottom);
	auto right_top = mm_min_epi32(lhs_right_top, rhs_right_top);

	auto size = _mm_sub_epi32(right_top, left_bottom);
	auto insertsect = _mm_or_si128(_mm_move_epi64(left_bottom), _mm_bslli_si128(size, 8));

	auto valid = _mm_cmplt_epi32(left_bottom, right_top);
	valid = _mm_shuffle_epi32(_mm_and_si128(valid, _mm_bsrli_si128(valid, 4)), 0);
	insertsect = _mm_and_si128(valid, insertsect);

	_mm_storeu_si128(reinterpret_cast<__m128i*>(&result), insertsect);
#else
	if(!SDL_IntersectRect(this, &other, &result)) {
		return rect();
	}
#endif
	return result;
}

void rect::clip(const rect& other)
{
	*this = this->intersect(other);
}

void rect::shift(const point& other)
{
	this->x += other.x;
	this->y += other.y;
}

rect rect::shifted_by(int x, int y) const
{
	rect res = *this;
	res.x += x;
	res.y += y;
	return res;
}

rect rect::shifted_by(const point& other) const
{
	return shifted_by(other.x, other.y);
}

point rect::point_at(double x, double y) const
{
	return {
		static_cast<int>(this->x + std::round(this->w * std::clamp(x, 0.0, 1.0))),
		static_cast<int>(this->y + std::round(this->h * std::clamp(y, 0.0, 1.0)))
	};
}

std::ostream& operator<<(std::ostream& s, const rect& r)
{
	s << '[' << r.x << ',' << r.y << '|' << r.w << ',' << r.h << ']';
	return s;
}
