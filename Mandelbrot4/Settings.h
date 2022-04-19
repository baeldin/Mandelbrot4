#pragma once
#include "Gradient.h"
#include "ComplexNumber.h"

struct fractalPosition
{
	ComplexNumber<double> center;
	ComplexNumber<double> rotation;
	double magn;
	int maxIter;
};

struct renderSettings
{
	int imgWidth, imgHeight, maxPasses;
	float aspect;
};

struct colorSettings
{
	double colorDensity = 1.f;
	double colorOffset = 0.f;
	color insideColor = color(0,0,0);
	Gradient gradient = uf_default;
	bool repeat_gradient = true;
};