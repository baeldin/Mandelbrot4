#pragma once

struct color {
	float r, g, b;
	color() : r(0), g(0), b(0) {};
	color(float v_) {
		r = v_;
		g = v_;
		b = v_;
	}
	color(float r_, float g_, float b_) {
		r = r_;
		g = g_;
		b = b_;
	}
	void operator=(const color& c) {
		r = c.r;
		g = c.g;
		b = c.b;
	}
	color operator+(const color& c) const {
		color result;
		result.r = r + c.r;
		result.g = g + c.g;
		result.b = b + c.b;
		return result;
	}
	color operator*(const float s) const {
		color result;
		result.r = r * s;
		result.g = g * s;
		result.b = b * s;
		return result;
	}
};