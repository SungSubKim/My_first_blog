#pragma once
#ifndef __CIRCLE_H__
#define __CIRCLE_H__

struct circle_t
{
	vec3	center = vec3(0,22,0);		// 2D position for translation
	float	radius = 1.0f;		// radius
	float	theta = 0.0f;			// rotation angle
	vec4	color;				// RGBA color in [0,1]
	mat4	model_matrix;		// modeling transformation
	float r2 = 2.0f;
	float vel = 1.0f;

	// public functions
	void	update(float t);
};

inline std::vector<circle_t> create_circles()
{
	std::vector<circle_t> circles;
	//return circles;
	circle_t c;
	c = { vec3(0,22,0),4.0f,0.0f,vec4(1.0f,0.5f,0.5f,1.0f) };
	c.r2 = 0;
	circles.emplace_back(c);



	return circles;
}

inline void circle_t::update(float t)
{
	float c = cos(t), s = sin(t);
	mat4 scale_matrix =
	{
		radius, 0, 0, 0,
		0, radius, 0, 0,
		0, 0, radius, 0,
		0, 0, 0, 1
	};

	mat4 rotation_matrix =
	{
		c,-s, 0, 0,
		s, c, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	};

	mat4 translate_matrix =
	{
		1, 0, 0, center.x,//,center.x,
		0, 1, 0, center.y,//,center.y,
		0, 0, 1, center.z,
		0, 0, 0, 1
	};

	model_matrix = translate_matrix * rotation_matrix * scale_matrix;
}

#endif
