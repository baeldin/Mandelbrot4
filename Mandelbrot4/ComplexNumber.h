#pragma once
#include <cmath>
#include "color.h"

using real = double;
template <typename F>
class ComplexNumber
{
private:
	F x;
	F y;

public:
	ComplexNumber() : x(0), y(0) {}
	ComplexNumber(F realPart):
		x(realPart),
		y(0)
	{}
	ComplexNumber(F realPart, F imagPart) :
		x(realPart),
		y(imagPart)
	{}
	constexpr double real() { return x; }
	constexpr double imag() { return y; }
	//double square() { return ComplexNumber<F>(x * x - y * y, 2 * x * y); }
	const ComplexNumber abs_components() { return ComplexNumber(abs(x), abs(y)); }
	const ComplexNumber flip() { return ComplexNumber(y, x); }
	const ComplexNumber conj() { return ComplexNumber(x, -y); }
	constexpr double cabs() { return sqrt(x * x + y * y); }
	constexpr double abs_squared() { return x * x + y * y; }
	constexpr ComplexNumber operator+ (const ComplexNumber &z) {
		ComplexNumber z0(0, 0);
		z0.x = x + z.x;
		z0.y = y + z.y;
		return z0;
	}
	constexpr ComplexNumber operator- (const ComplexNumber &z) {
		ComplexNumber z0(0, 0);
		z0.x = x - z.x;
		z0.y = y - z.y;
		return z0;
	}
	constexpr ComplexNumber operator* (const ComplexNumber &z) {
		ComplexNumber z0(0, 0);
		z0.x = x * z.x - y * z.y;
		z0.y = x * z.y + y * z.x;
		return z0;
	}
	constexpr ComplexNumber operator* (const double v) {
		ComplexNumber z0(0, 0);
		z0.x = x * v;
		z0.y = x * v;
		return z0;
	}
};