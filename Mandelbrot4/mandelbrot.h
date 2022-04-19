#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <tuple>
#include <thread>
#include <omp.h>
#include <SDL_opengl.h>
#include <SDL.h>
#include <algorithm>
#define STB_IMAGE_IMPLEMENTATION

#include "stb_image.h"

#if _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif
#include "ComplexNumber.h"
#include "Gradient.h"
#include "Settings.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
using std::cout;
using std::endl;
using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::duration;
using std::chrono::milliseconds; 

using real = double;
using Complex = ComplexNumber<real>;


struct sRGBPixel
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

struct NovaDuckyRelax
{
	int symmetryMode;
	double bailout, power; // TODO: make exponents complex
	Complex absOffset, start, seed;
};

Complex get_complex_coord(real x_shifted, real y_shifted, const fractalPosition pos, real span, const renderSettings rendering)
{
	const double xRange = max(2.f * span * rendering.aspect, 2.f * span) / pos.magn;
	const double yRange = max(2.f * span / rendering.aspect, 2.f * span) / pos.magn;
	//real xSpan = span * rendering.imgWidth / rendering.imgHeight;
	//Complex center = pos.center;
	//Complex rot = pos.rotation;
	x_shifted = (x_shifted + 0.5 - 0.5*rendering.imgWidth) / rendering.imgWidth * xRange;
	y_shifted = (y_shifted + 0.5 - 0.5*rendering.imgHeight) / rendering.imgHeight * yRange;
	//real zx = center.real() - xSpan + 2 * x_shifted * xSpan / rendering.imgWidth;
	//real zy = center.imag() - span + 2 * y_shifted * span / rendering.imgHeight;
	return Complex(x_shifted, y_shifted) * pos.rotation + pos.center;
}


inline real sign(real v) { return (v >= 0) ? (real)1 : (real)-1; }

// Convert uniform distribution into triangle-shaped distribution
// From https://www.shadertoy.com/view/4t2SDh
inline real triDist(real v)
{
	if (v == 0.5) { return 0.; } // nerf the NaN at v = 0.5
	const real orig = v * 2 - 1;
	v = orig / std::sqrt(std::fabs(orig));
	v = v - sign(orig);

	return v;
}


template<int b>
real halton(int i)
{
	real f = 1.;
	real r = 0;
	const real inv_b = 1.f / (real)b;
	while (i > 0)
	{
		f = f * inv_b;
		r = r + f * (i % b);
		i = i/b;
		//cout << n1 << " " << r << " " << hn << "\n";
	}
	return r;
}


color aa_stress_test(Complex z0)
{
	float c = (float)z0.abs_squared();
	float cidx = 0.5f+0.5f*(float)cos(128*c);
	return volcano_under_a_glacier.get_color(cidx);
}


color manowar_iter_loop(const Complex z0, const float colorDensity, const float colorOffset, const int max_iter)
{
	int iter = 0;
	Complex z = z0;
	Complex z1 = z;
	Complex zold = z;
	real bailout = 128;
	while (z.abs_squared() <= bailout && iter < max_iter)
	{
		zold = z;
		z = zold * zold + z1 + z0;
		z1 = zold;
		iter++;
	}

	if (iter == max_iter) { return color(0, 0, 0); }

	real il = 1.f / log(2.f);
	real lp = log(log(bailout));
	real illoglog = log(log(z.cabs())) * il;
	real iterplus = (real)iter + lp * il - illoglog;
	real gradient_float_index = 0.05f * (float)iterplus;
	return uf_default.get_color(colorOffset + colorDensity * gradient_float_index);
}

color main_iter_loop2(const Complex z0, const colorSettings colors, const int max_iter)
{
	int iter = 0;
	Complex z = z0;
	real bailout = 128; // 1e10;
	int k = 0;
	real kmax = 0;
	Complex c(-0.1, 0);
	//Complex c2(-0.45664338566164, 0.55036858801806);
	Complex c2(0.0, -0.1);
	real dkmax = 111;
	while (z.abs_squared() <= bailout && iter < max_iter)
	{
		if (k < kmax)
		{
		z = z * z + z0; // MB
			k++;
		}
		else {
			z = z * z + c2 * z * z + z0;
			kmax = kmax + dkmax;
			k = 0;
		}
		iter++;
	}

	if (iter == max_iter) { return colors.insideColor; }

	//return uf_default.get_color(iter/100.f);
	real il = 1.f / log(2.f);
	real lp = log(log(bailout));
	real illoglog = log(log(z.cabs())) * il;
	real iterplus = (real)iter + lp * il - illoglog;
	real gradient_float_index = colors.colorOffset + colors.colorDensity * (0.05f * (float)iterplus);
	if (!colors.repeat_gradient)
		gradient_float_index = min(gradient_float_index, 1.f);
	// return uf_default.get_color(colorOffset + colorDensity * gradient_float_index);
	//return volcano_under_a_glacier.get_color(colorOffset + 1.f / 5.f * colorDensity * (0.05f + (float)iterplus) - 0.45f); // birth
	Gradient gradient = colors.gradient;
	return gradient.get_color(gradient_float_index);

	//return volcano_under_a_glacier.get_color(0.025f*(0.05f + (float)iterplus)+0.0f);
}

//color main_iter_loop3(const Complex z0, const float colorDensity, const float colorOffset, const int max_iter, NovaDuckyRelax novaParams)
//{
//	Complex z = novaParams.start;
//	if (novaParams.symmetryMode == 0)
//	{
//		Complex z_offset = z - novaParams.absOffset;
//		z = novaParams.absOffset + z_offset.abs_components();
//	}
//	else if (novaParams.symmetryMode == 1)
//	{
//		Complex z_offset = z - novaParams.absOffset;
//		Complex z = z_offset + Complex(z_offset.real(), abs(z_offset.imag()));
//	}
//	while (z.abs_squared() > novaParams.bailout)
//	{
//		z = (z * z * z) / (z * z * 3);
//		z = z * z0 + novaParams.seed;
//	}
//}

//color main_iter_loop(const Complex z0, const int max_iter)
//{
//	int iter = 0;
//	Complex z = z0;
//	real bailout = 1e10;
//	while (z.abs_squared() <= bailout && iter < max_iter)
//	{
//		z = z * z + z0;
//		iter++;
//	}
//
//	//if (iter == max_iter) { return color(0,0,0); }
//
//	real il = 1. / log(2);
//	real lp = log(log(bailout));
//	real illoglog = log(log(z.cabs())) * il;
//	real iterplus = iter + lp * il - illoglog;
//	return volcano_under_a_glacier.get_color(0.000015f*(0.05 + iterplus)+0.4);

//}
color get_color(int iter)
{
	return color(
		0.5f + 0.5f * (float)cos(0.2 * iter),
		0.5f + 0.5f * (float)cos(0.2 * iter),
		0.5f + 0.5f * (float)cos(0.2 * iter));
}


float linear2srgb(float c)
{
	return (c < 0.0031308f) ? 12.92f * c : 1.055f * std::pow(c, 1 / 2.5f) - 0.055f;
}

// Hash function by Thomas Wang: https://burtleburtle.net/bob/hash/integer.html
inline uint32_t hash(uint32_t x)
{
	x = (x ^ 12345391) * 2654435769;
	x ^= (x << 6) ^ (x >> 26);
	x *= 2654435769;
	x += (x << 5) ^ (x >> 12);

	return x;
}


inline real uintToUnitReal(uint32_t v)
{
#if USE_DOUBLE
	constexpr double uint32_double_scale = 1.0 / (1ull << 32);
	return v * uint32_double_scale;
#else
	// Trick from MTGP: generate an uniformly distributed single precision number in [1,2) and subtract 1
	union
	{
		uint32_t u;
		float f;
	} x;
	x.u = (v >> 9) | 0x3f800000u;
	return x.f - 1.0f;
#endif
}


inline real wrap1r(real u, real v) { return (u + v < 1) ? u + v : u + v - 1; }

void save_to_png(std::vector<float>* image_data, int imgWidth, const int imgHeight, char fnam[50])
{
	// write to file
	//const char* fnam = "test5.png";
	//const float sample_fac = 1.f / n_passes;
	std::vector<uint8_t> image_sRGB(3 * imgWidth * imgHeight);
	for (unsigned int i = 0; i < 3 * imgWidth * imgHeight; i++) {
		//color pixel = image_data[i]; // *sample_fac;
		image_sRGB[i] = uint8_t(((*image_data)[i]) * 255);
	}
	stbi_write_png(fnam, imgWidth, imgHeight, 3, &image_sRGB[0], imgWidth * 3);
	
}

void simple_func(const int wtf_some_value, int* counter)
{
	for (int i = 0; i < 5; i++) {
		cout << "Hello from simple func, you sent me " << wtf_some_value << "\n";
		*counter = i;
		Sleep(1000);
	}
	*counter = 9999;
}

//void calc_mb(real centerX, real centerY, double magn, const int max_iter, const float colorDensity, const float colorOffset, const int max_passes,
//	int* current_passes, std::vector<float>* out_vec_img_f, int* out_width, int* out_height, std::mutex m)
	//GLuint* out_texture, 
//void calc_mb_noargs(float* out_image_float, int* out_width, int* out_height, real centerX=0, real centerY=0, double magn=1, const int max_iter=250)

void calc_mb_onearg(const fractalPosition pos, const colorSettings cs, const renderSettings rendering,
	int* current_passes, std::vector<float>* out_vec_img_f, float* progress, std::mutex*)
{
	//std::vector<float> out_fec_img_f(3 * 1280 * 720);
	//std::mutex m;
//void calc_mb_fewargs(int* current_passes, std::vector<float>* out_vec_img_f, int* out_width, int* out_height, std::mutex m)
	//real centerX = 0.;
	//real centerY = 0.;
	//double magn = 1.;
	//const int max_iter = 250;
	//const float colorDensity = 0.1;
	//const float colorOffset = 0.;
	//const int max_passes = 16;
	
#if _WIN32
	SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
#endif
#if _DEBUG
	const int num_threads = 1;
#else
	const int num_threads = (int)std::thread::hardware_concurrency();
#endif
	omp_set_num_threads(num_threads);
	//constexpr int imgMult = 80;
	//int imgWidth = out_width;
	//int imgHeight = out_height;
	// constexpr int imgWidth = 3;
	// constexpr int imgHeight = 3;
	//const Complex center(-0.77, 0.125);
	//constexpr int max_iter = 11250;
	//constexpr real magn = 350.;

	//const Complex center(-0.12210986062462065f, 0.64954319871203645f);
	//constexpr int max_iter = 120000;
	//constexpr real magn = 12715934.;
	// constexpr int max_iter = 131000;
	//constexpr real magn = magn;
	//const Complex center(-0.48044823219619325f, 0.5326565590281079f);
	//constexpr int max_iter = 131000;
	//constexpr real magn = 4101418.5;
	//const Complex center(5,5);
	//constexpr int max_iter = 10;
	//constexpr real magn = 0.1;

	real span = 1.5;
	span = span / pos.magn;


	const real sample_fac = 1.f / rendering.maxPasses;
	auto t1 = high_resolution_clock::now();
	std::atomic<int> pixel_count = 0;
	int max_pixel_count = rendering.maxPasses * rendering.imgWidth * rendering.imgHeight;
	for (int pass = 0; pass < rendering.maxPasses; pass++)
	{
		float pass_fac = 1.f / ((float)pass + 1);
		float pass_fac2 = (float)pass / ((float)pass + 1);
		#pragma omp parallel for schedule(dynamic)
		for (int y = 0; y < rendering.imgHeight; y++) 
		{
			for (int x = 0; x < rendering.imgWidth; x++) 
			{
				unsigned int pixel_idx = (rendering.imgHeight - 1 - y) * rendering.imgWidth + x;
				const real hash_random = uintToUnitReal(hash(pass * rendering.imgWidth * rendering.imgHeight + pixel_idx)); // Use pixel idx and pass to randomise Halton sequence
				real x_offset = min(rendering.maxPasses - 1, 1) * triDist(wrap1r(halton<2>(pass), hash_random));
				real y_offset = min(rendering.maxPasses - 1, 1) * triDist(wrap1r(halton<3>(pass), hash_random));
				real x_shifted = (real)x + x_offset;
				real y_shifted = (real)y + y_offset;
				//if (x == 0 && y == 0)
				//{
				//	fout << x_offset << " " << y_offset << " " << hash_random << endl;
				//	//cout << x_offset << " " << y_offset << "\n";
				//}
				Complex z = get_complex_coord(x_shifted, y_shifted, pos, span, rendering);
				// color pixel_color = aa_stress_test(z);
				color pixel_color = main_iter_loop2(z, cs, pos.maxIter);
				// color pixel_color = manowar_iter_loop(z, colorDensity, colorOffset, max_iter);
				(*out_vec_img_f)[3 * pixel_idx + 0] = pass_fac2 * (*out_vec_img_f)[3 * pixel_idx + 0] + pass_fac * pixel_color.r;
				(*out_vec_img_f)[3 * pixel_idx + 1] = pass_fac2 * (*out_vec_img_f)[3 * pixel_idx + 1] + pass_fac * pixel_color.g;
				(*out_vec_img_f)[3 * pixel_idx + 2] = pass_fac2 * (*out_vec_img_f)[3 * pixel_idx + 2] + pass_fac * pixel_color.b;
				//vec_img_f[3 * pixel_idx + 0] = pass_fac2 * vec_img_f[3 * pixel_idx + 0] + pass_fac * pixel_color.r;
				//vec_img_f[3 * pixel_idx + 1] = pass_fac2 * vec_img_f[3 * pixel_idx + 1] + pass_fac * pixel_color.g;
				//vec_img_f[3 * pixel_idx + 2] = pass_fac2 * vec_img_f[3 * pixel_idx + 2] + pass_fac * pixel_color.b;
			}
			pixel_count += rendering.imgWidth;
			*progress = (float)pixel_count / (float)max_pixel_count;
		}
		cout << "Pass " << pass << " complete.\n";
	}

	auto t2 = high_resolution_clock::now();
	/* Getting number of milliseconds as an integer. */
	auto ms_int = duration_cast<milliseconds>(t2 - t1);
	int duration = ms_int.count();
	std::cout << ms_int.count() << "ms\n";

	// before exiting the function, set passes to a number that makes it obvious that it's done, no matter what happened before
	*current_passes = 9999999;
}
