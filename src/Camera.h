#pragma once
#include "Ray.h"
#include "Vector3.h"
#include "Mat4.h"

class Camera
{
public:
	Vector3 position;
	Vector3 normal;

	Vector3 screen_horizontal;
	Vector3 screen_vertical;
	Vector3 lower_left_corner;
	float lens_radius;
	Vector3 w,u,v;
	float time0, time1;

	Camera()= default;
	Camera(const Vector3& eye, const Vector3& target, const Vector3& up, float vFov, float aspect_ratio, float t0 = 0.f, float t1 = 0.f);
	Camera(const Vector3& eye, const Vector3& target, const Vector3& up, float vFov, float aspect_ratio, float aperture,
	       float focus_dist,  float t0 = 0.f, float t1= 0.f);
	// Camera(const Vector3& eye, const Vector3& target, const Vector3& up, float focal_length, float film_size,
	//        float aspect_ratio);

	Ray getRay(float x, float y) const;

	Mat4 getViewMatrix() const;
	Mat4 getViewMatrixInverse() const;
};
