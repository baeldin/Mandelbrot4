#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <tuple>
#include <thread>
#include <SDL_opengl.h>
#include <SDL.h>
#define STB_IMAGE_IMPLEMENTATION

#include "stb_image.h"

#if _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif
#include "ComplexNumber.h"
#include "Gradient.h"
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


Complex get_complex_coord(real i_shifted, real j_shifted, Complex center, real span, int imgWidth, int imgHeight)
{
	real xSpan = span * imgWidth / imgHeight;
	real zx = center.real() - xSpan + 2 * i_shifted * xSpan / imgWidth;
	real zy = center.imag() - span + 2 * j_shifted * span / imgHeight;
	return Complex(zx, zy);
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

color main_iter_loop2(const Complex z0, const float colorDensity, const float colorOffset, const int max_iter)
{
	int iter = 0;
	Complex z = z0;
	real bailout = 1e10;
	//int k = 0;
	//real kmax = 0;
	//Complex c(0, -0.1);
	//real dkmax = 111;
	while (z.abs_squared() <= bailout && iter < max_iter)
	{
	//	if (k < kmax)
	//	{
		z = z * z + z0; // MB
	//		k++;
	//	}
	//	else {
	//		z = z * z + c * z * z + z0;
	//		kmax = kmax + dkmax;
	//		k = 0;
	//	}
		iter++;
	}

	if (iter == max_iter) { return color(0,0,0); }

	real il = 1. / log(2);
	real lp = log(log(bailout));
	real illoglog = log(log(z.cabs())) * il;
	real iterplus = iter + lp * il - illoglog;
	return volcano_under_a_glacier.get_color(colorOffset + 5.f * colorDensity * (0.05f + (float)iterplus) - 0.45f); // birth
	//return volcano_under_a_glacier.get_color(0.000006f * (0.05f + (float)iterplus) + 0.0f);
	//return volcano_under_a_glacier.get_color(0.025f*(0.05f + (float)iterplus)+0.0f);

}



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

void save_to_png(const float *image_data, int imgWidth, const int imgHeight, const int n_passes)
{
	// write to file
	const char* fnam = "test5.png";
	const float sample_fac = 1.f / n_passes;
	std::vector<sRGBPixel> image_sRGB(imgWidth * imgHeight);
	for (unsigned int i = 0; i < imgWidth * imgHeight; i++) {
		color pixel = image_data[i] * sample_fac;
		image_sRGB[i] = { uint8_t((pixel.r) * 255), uint8_t((pixel.g) * 255), uint8_t((pixel.b) * 255) };
	}
	stbi_write_png("test4.png", imgWidth, imgHeight, 3, &image_sRGB[0], imgWidth * 3);
	
}

void simple_func(const int wtf_some_value)
{
	cout << "Hello from simple func, you sent me " << wtf_some_value << "\n";
}

void calc_mb(real centerX, real centerY, double magn, const int max_iter, const float colorDensity, const float colorOffset, const int max_passes,
	GLuint* out_texture, int* out_width, int* out_height)
	//std::vector<float*> out_vec_img_f,
//void calc_mb_noargs(float* out_image_float, int* out_width, int* out_height, real centerX=0, real centerY=0, double magn=1, const int max_iter=250)

{
#if _WIN32
	SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
#endif
#if _DEBUG
	const int num_threads = 1;
#else
	const int num_threads = (int)std::thread::hardware_concurrency();
#endif
	//constexpr int imgMult = 80;
	int imgWidth = *out_width;
	int imgHeight = *out_height;
	// constexpr int imgWidth = 3;
	// constexpr int imgHeight = 3;
	//const Complex center(-0.77, 0.125);
	//constexpr int max_iter = 11250;
	//constexpr real magn = 350.;

	//const Complex center(-0.12210986062462065f, 0.64954319871203645f);
	//constexpr int max_iter = 120000;
	//constexpr real magn = 12715934.;
	const Complex center(centerX, centerY);
	// constexpr int max_iter = 131000;
	//constexpr real magn = magn;
	//const Complex center(-0.48044823219619325f, 0.5326565590281079f);
	//constexpr int max_iter = 131000;
	//constexpr real magn = 4101418.5;
	//const Complex center(5,5);
	//constexpr int max_iter = 10;
	//constexpr real magn = 0.1;

	real span = 1.5;
	span = span / magn;


	const real sample_fac = 1.f / max_passes;
	color pixel_color(0);
	std::vector<color> image(imgHeight * imgWidth, color(0));
	auto t1 = high_resolution_clock::now();
	// real x_offset = 0.f;
	// real y_offset = 0.f;
	std::ofstream fout("samples_256_tent.txt");



	for (int pass = 0; pass < max_passes; pass++)
	{
		//cout << "Pass > 0, doing offset\n";
		//real x_offset = pass*sample_fac - 0.5;
		//real y_offset = halton(2, pass) - 0.5;
		//cout << sample_fac << " " << x_offset << " " << y_offset << "\n";
		#pragma omp parallel for
		for (int y = 0; y < imgHeight; y++) 
		{
			for (int x = 0; x < imgWidth; x++) 
			{
				unsigned int pixel_idx = y * imgWidth + x;
				const real hash_random = uintToUnitReal(hash(pass * imgWidth * imgHeight + pixel_idx)); // Use pixel idx to randomise Halton sequence
				real x_offset = triDist(wrap1r(halton<2>(pass), hash_random));
				real y_offset = triDist(wrap1r(halton<3>(pass), hash_random));
				real x_shifted = (real)x + x_offset;
				real y_shifted = (real)y + y_offset;
				if (x == 0 && y == 0)
				{
					fout << x_offset << " " << y_offset << " " << hash_random << endl;
					//cout << x_offset << " " << y_offset << "\n";
				}
				Complex z = get_complex_coord(x_shifted, y_shifted, center, span, imgWidth, imgHeight);
				// color pixel_color = aa_stress_test(z);
				pixel_color = main_iter_loop2(z, colorDensity, colorOffset, max_iter);
				image[pixel_idx] = image[pixel_idx] + pixel_color;
			}
		}
		cout << "Pass " << pass << " complete.\n";
	}


	auto t2 = high_resolution_clock::now();
	/* Getting number of milliseconds as an integer. */
	auto ms_int = duration_cast<milliseconds>(t2 - t1);
	int duration = ms_int.count();
	std::cout << ms_int.count() << "ms\n";
	fout.close();

	std::vector<sRGBPixel> image_sRGB(imgWidth * imgHeight);
	static float * image_float = new float[imgWidth * imgHeight * 3];
	//std::vector<float> vec_img_f(imgWidth * imgHeight * 3);
	//std::vector<uint8_t> fimage(imgWidth * imgHeight * 3);

	// write to file
	{
		for (unsigned int i = 0; i < imgWidth * imgHeight; i++) {
			color pixel = image[i] * sample_fac;
			//image_sRGB[i] = { uint8_t((pixel.r) * 255), uint8_t((pixel.g) * 255), uint8_t((pixel.b) * 255) };
			//fimage[3 * i + 0] = uint8_t((pixel.r) * 255);
			//fimage[3 * i + 1] = uint8_t((pixel.g) * 255);
			//fimage[3 * i + 2] = uint8_t((pixel.b) * 255);
			image_float[3 * i + 0] = pixel.r;
			image_float[3 * i + 1] = pixel.g;
			image_float[3 * i + 2] = pixel.b;
			//vec_img_f[3 * i + 0] = pixel.r;
			//vec_img_f[3 * i + 1] = pixel.g;
			//vec_img_f[3 * i + 2] = pixel.b;
		}
		//stbi_write_png("test4.png", imgWidth, imgHeight, 3, &image_sRGB[0], imgWidth * 3);
	}


	// MAKE TEXTURE FROM IMAGE DATA
	GLuint texture;
	// create texture to hold image data for rendering
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	// Setup filtering parameters for display
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same

	// Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, imgWidth, imgHeight, 0, GL_RGB, GL_FLOAT, vec_img_f.data());
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, imgWidth, imgHeight, 0, GL_RGB, GL_FLOAT, &image_float[0]);

	// debug printing, such good, much wow!
	//GLenum err;
	//while((err = glGetError()) != GL_NO_ERROR){
	//	std::cout << err;
	//}

	//out_vec_img_f = vec_img_f;
	*out_texture = texture;
	*out_width = imgWidth;
	*out_height = imgHeight;
}
