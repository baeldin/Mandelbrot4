#include <vector>
#include <math.h>
#include <iostream>

#include "euler.h"

using std::cout;

std::vector<double> calculate_spline_coefficients(std::vector<float> x, std::vector<float> y)
{
	// slopes, limit to 0 for extreme points
	float s1 = (y[1] >= y[0] && y[1] >= y[2]) ? 0 : (y[2] - y[0])/(x[2] - x[0]);
	float s2 = (y[2] >= y[1] && y[2] >= y[3]) ? 0 : (y[3] - y[1])/(x[3] - x[1]);
	cout << "Slopes: " << s1 << ", " << s2 << "\n";
	std::vector<double> M = {
		  pow(x[1], 3), pow(x[1], 2), x[1], 1,
		3*pow(x[1], 2),   2*x[1],        1, 0,
		  pow(x[2], 3), pow(x[2], 2), x[2], 1,
		3*pow(x[2], 2),   2*x[2],        1, 0};
	std::vector<double> b = {y[1], s1, y[2], s2};
	std::vector<double> coeffs = solve_linear_eqs(M, b);
	return coeffs;
}

int main()
{
	std::vector<float> x = {-1, 1, 1, 2};
	std::vector<float> y = {0, 1, 3, 2};
        std::vector<double> coeffs = calculate_spline_coefficients(x, y);
	cout << coeffs[0] << " " << coeffs[1] << " " << coeffs[2] << " " << coeffs[3] << "\n";
}

