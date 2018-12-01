#pragma once

#include "HitRecord.h"
#include "Vector3.h"
#include "Random.h"
#include "Ray.h"

inline Vector3 reflect(const Vector3& v, const Vector3& n);
inline bool refract(const Vector3& v, const Vector3& n, float ni_over_nt, Vector3& refracted);
inline float schlick(float cosine, float ref_idx);

class Material
{
public:
	virtual ~Material() = default;
	virtual bool scatter(const Ray& ray_in, const HitRecord& rec, Vector3& attenuation,
	                     Ray& scattered_ray_out) const = 0;
};

class Lambertian : public Material
{
public:
	Vector3 albedo;

	Lambertian(const Vector3& a) : albedo(a)
	{
	}

	bool scatter(const Ray& ray_in, const HitRecord& rec, Vector3& attenuation, Ray& scattered_ray_out) const override
	{
#ifdef SAMPLING
		const Vector3 out_direction = rec.normal + Random::random_in_unit_sphere();
#else
		const Vector3 out_direction = rec.normal;
#endif
		scattered_ray_out = Ray(rec.position, out_direction);
		attenuation = albedo;
		return true;
	}
};


class Metal : public Material
{
public:
	Vector3 albedo;
	float fuzz;

	Metal(const Vector3& a, float f) : albedo(a)
	{
		if (f < 1) fuzz = f;
		else fuzz = 1;
	}

	bool scatter(const Ray& ray_in, const HitRecord& rec, Vector3& attenuation, Ray& scattered_ray_out) const override
	{
		Vector3 reflected = reflect(ray_in.direction.getNormalized(), rec.normal);
#ifdef SAMPLING
		scattered_ray_out = Ray(rec.position, reflected + fuzz * Random::random_in_unit_sphere());
#else
		scattered_ray_out = Ray(rec.position, reflected);
#endif
		attenuation = albedo;
		return (scattered_ray_out.direction.dot(rec.normal) > 0);
	}
};


class Dialectric : public Material
{
public:
	float ref_idx;

	Dialectric(float ri) : ref_idx(ri)
	{
	}

	bool scatter(const Ray& ray_in, const HitRecord& rec, Vector3& attenuation, Ray& scattered_ray_out) const override
	{
		Vector3 outward_normal;
		Vector3 reflected = reflect(ray_in.direction, rec.normal);
		float ni_over_nt;
		attenuation = Vector3::ONE;
		Vector3 refracted;
		float reflect_prob;
		float cosine;

		if (ray_in.direction.dot(rec.normal) > 0.0f)
		{
			outward_normal = -rec.normal;
			ni_over_nt = ref_idx;
			cosine = ref_idx * ray_in.direction.dot(rec.normal) / ray_in.direction.length();
		}
		else
		{
			outward_normal = rec.normal;
			ni_over_nt = 1.0f / ref_idx;
			cosine = -ray_in.direction.dot(rec.normal) / ray_in.direction.length();
		}

		if (refract(ray_in.direction, outward_normal, ni_over_nt, refracted))
			reflect_prob = schlick(cosine, ref_idx);
		else
			reflect_prob = 1.0;

#ifdef SAMPLING
		if (Random::randf(0, 1) < reflect_prob)
			scattered_ray_out = Ray(rec.position, reflected);
		else
#endif
			scattered_ray_out = Ray(rec.position, refracted);

		return true;
	}
};

//Reflect Equation
Vector3 reflect(const Vector3& v, const Vector3& n)
{
	return v - 2 * v.dot(n) * n;
}

bool refract(const Vector3& v, const Vector3& n, float ni_over_nt, Vector3& refracted)
{
	Vector3 uv = v.getNormalized();
	float dt = uv.dot(n);
	float discriminant = 1.0f - ni_over_nt * ni_over_nt * (1.0f - dt * dt);
	if (discriminant > 0.0f)
	{
		refracted = ni_over_nt * (uv - n * dt) - n * sqrtf(discriminant);

		//OpenGL Formula - v,n must be normalized
		//refracted = ni_over_nt * uv - (ni_over_nt * dt + sqrtf(discriminant)) * n.getNormalized();
		return true;
	}
	return false;
}

float schlick(float cosine, float ref_idx)
{
	float r0 = (1.0f - ref_idx) / (1.0f + ref_idx);
	r0 = r0 * r0;
	return r0 + (1.0f - r0) * pow((1.0f - cosine), 5.0f);
}