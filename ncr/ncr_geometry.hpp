/*
 * ncr_geometry - algorithms and data structures for geometry processing
 *
 * SPDX-FileCopyrightText: 2022-2023 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details.
 *
 */
#pragma once

#include <ncr/ncr_log.hpp>
#include <ncr/ncr_math.hpp>

namespace ncr {

#if 0

/*
 * NOTE: This file is currently a stub and not used. it might get filled with
 * working content when I need geometry routines that I cannot find in a way I
 * need them in other libraries.
 */

/*
 * pose_t - pose of a moving agent
 *
 * TODO: currently, this is not really supported. Move somewhere else
 */
template <std::size_t D>
struct pose_t {
	// location in D-dimensional space
	VecND<D, double> position;
	VecND<D, double> velocity;

	// rotation
	// TODO: change to some higher dimensional representation
	double theta;
};



// TODO: replace VecND with something sane

template <typename RealType = double>
struct HitRecord {
	int                id;
	VecND<3, RealType> hit;
};

template <typename RealType = double>
struct Ray {};

template <typename RealType = double>
struct Triangle {};

template <typename RealType = double>
struct Cylinder {};

template <typename RealType = double>
struct Plane {};

template <typename RealType = double>
struct Wall {};

template <typename RealType = double>
struct PolyWall {};

template <typename RealType = double>
struct AABB {};



template <typename RealType = double>
int intersect(Ray<RealType>&, Triangle<RealType>&, HitRecord<RealType>&)
{
	log_verbose(__PRETTY_FUNCTION__, "\n");
	return 0;
}

/*
template <typename RealType = double>
int intersect(Ray&, Cylinder&, HitRecord<RealType>&)
{
#if DEBUG && DEBUG_VERBOSE
	std::cout << __PRETTY_FUNCTION__ << std::endl;
#endif
	return 0;
}


template <typename RealType = double>
int intersect(Ray&, Plane&, HitRecord<RealType>&)
{
#if DEBUG && DEBUG_VERBOSE
	std::cout << __PRETTY_FUNCTION__ << std::endl;
#endif
	return 0;
}


template <typename RealType = double>
int intersect(Ray&, Wall&, HitRecord<RealType>&)
{
#if DEBUG && DEBUG_VERBOSE
	std::cout << __PRETTY_FUNCTION__ << std::endl;
#endif
	return 0;
}


template <typename RealType = double>
int intersect(Ray&, PolyWall&, HitRecord<RealType>&)
{
#if DEBUG && DEBUG_VERBOSE
	std::cout << __PRETTY_FUNCTION__ << std::endl;
#endif
	return 0;
}


template <typename RealType = double>
int intersect(AABB &, Ray &, HitRecord<RealType>&)
{
#if DEBUG && DEBUG_VERBOSE
	std::cout << __PRETTY_FUNCTION__ << std::endl;
#endif
	return 0;
}
*/


/* generic C interface declaration

void intersect_triangle();
void intersect_cylinder();
void intersect_plane();
void intersect_wall();
void intersect_polywall();
void intersett_aabb();

#define intersect_any(V) \
	_Generic((V), \
			Triangle: intersect_triangle, \
			Cylinder: intersect_cylinder, \
			Plane:    intersect_plane,    \
			Wall:     intersect_wall,     \
			PolyWall: intersect_polywall, \
			AABB:     intersect_aabb)(V)
*/



The following is working code, but disabled to prevent usage as this requires
some refactoring.



/**
 * struct Ray - A ray that can be traced through the world.
 *
 * In order to compute the distance to walls or other obstacles in the world, we
 * define a Ray here and implement a very rudimentary "ray tracer".
 *
 * We won't use acceleration structures ss we don't assume complex or a lot of
 * geometry in the scenes right now.
 */
struct Ray {
	Vec3f origin; // origin
	Vec3f dir; // direction
};


/**
 * get the exact information where an object was hit, and which object id
 */
struct HitRecord {
	int id;
	Vec3f hit;
};


/**
 * struct Intersectable - interface to intersect a primitive.
 *
 * All primitives have to implement this interface such that the ray tracer can
 * intersect the primitive with a ray.
 */
struct Intersectable {
	virtual ~Intersectable() {};
	virtual float intersect(const Ray &ray, HitRecord &hr) const = 0;
};


/*
// TODO
struct Sphere : Intersectable
{
	virtual float intersect(const Ray &ray, HitRecord &hr) const override;
};
*/


// TODO
// BezierCurve
// BezierSurface
// Cone (infinite length)
// GroundPlane / Surface (arbitrary heights, curvilinear motion)


/**
 * struct Triangle - a triangle
 */
struct Triangle : Intersectable {
	//! three vertex points of the triangle
	Vec3f A, B, C;

	virtual float intersect(const Ray &ray, HitRecord &hr) const override;
};


/**
 * struct Plane - an infinite 2D surface in 3D euclidean space
 */
struct Plane : Intersectable {
	Vec3f origin;	// some point on the plane
	Vec3f normal;	// normal vector of the plane

	virtual float intersect(const Ray &ray, HitRecord &hr) const override;
};


struct Cylinder : Intersectable {
	Vec3f origin;	// origin
	Vec3f dir;	// "upright" direction of the cylinder
	float r;	// radius

	Cylinder(Vec3f o, Vec3f d, float radius);
	virtual ~Cylinder() {}
	virtual float intersect(const Ray &ray, HitRecord &hr) const override;
};


/**
 * struct Wall - a 50cm high wall from X0 to X1
 */
struct Wall : Intersectable {
	Wall(Vec2f X0, Vec2f X1);
	Wall(Vec3f X0, Vec3f X1, float height = 50._cm);
	virtual ~Wall() override {}
	virtual float intersect(const Ray &ray, HitRecord &hr) const override;
private:
	float _height;
	Vec3f _X0, _X1;
	Triangle _T0, _T1;
};


/**
 * struct AABox - axis aligned Box from X0 to X1.
 */
struct AABox : Intersectable {
	AABox(Vec3f X0, Vec3f X1, float height = 50._cm);
	virtual ~AABox() override {}
	virtual float intersect(const Ray &ray, HitRecord &hr) const override;
private:
	float _height;
	Vec3f _X0, _X1;
	Wall _north, _east, _south, _west;
};


/**
 * intersection with a triangle. ignores front/backside of triangle
 */
float Triangle::
intersect(const Ray &ray, HitRecord &hr) const
{
	auto inf = std::numeric_limits<float>::infinity();

	// Möller-Trumbore intersection

	// edges of the triangle
	auto e1 = B - A;
	auto e2 = C - A;

	// compute determinant
	auto P = ray.dir.cross(e2);
	auto det = e1.dot(P);
	// if det is near zero, ray is inside the triangle
	if (det == 0.f) return inf;
	// ignore negative determinante. this only means that the triangle was
	// hit on the backface - which is OK for us here
	auto inv_det = 1.f / det;

	// calculate distance from A to ray origin
	auto T = ray.origin - A;
	auto u = T.dot(P) * inv_det;
	// intersection lies outside of the triangle?
	if (u < 0.f || u > 1.f) return inf;

	auto Q = T.cross(e1);
	auto v = ray.dir.dot(Q) * inv_det;
	// intersection lies outside of triangle?
	if (v < 0.f || v > 1.f) return inf;

	// compute the distance.
	// NOTE that the distance needs to be positive, otherwise the triangle
	// would be hit walking backwards on the ray. however, as mentioned
	// above, we have ignored negative determinantes which means the
	// hit is still accepted even if the triangle is hit on the backside!
	auto dist = e2.dot(Q) * inv_det;
	if (dist > 0.f) {
		hr.hit = ray.origin + dist * ray.dir;
		return dist;
	}
	// no hit
	return inf;
}


Cylinder::Cylinder(Vec3f o, Vec3f d, float radius)
: origin(o), dir(d), r(radius)
{}

Wall::Wall(Vec2f X0, Vec2f X1)
: Wall(Vec3f{X0[0], X0[1], 0.0}, Vec3f{X1[0], X1[1], 0.0})
{ }


Wall::Wall(Vec3f X0, Vec3f X1, float height)
: _height(height), _X0(X0), _X1(X1)
{
	// Initialize the two triangles
	_T0.A = X0;
	_T0.B = X1;
	_T0.C = {X1[0], X1[1], _height};

	_T1.A = X0;
	_T1.B = {X1[0], X1[1], _height};
	_T1.C = {X0[0], X0[1], _height};
}

float Wall::
intersect(const Ray &ray, HitRecord &hr) const
{
	auto inf = std::numeric_limits<float>::infinity();
	HitRecord hr0;
	HitRecord hr1;

	float t0 = _T0.intersect(ray, hr0);
	float t1 = _T1.intersect(ray, hr1);
	if (t0 == inf && t1 == inf) return inf;
	else if (t0 < t1) {
		hr.hit = hr0.hit;
		return t0;
	}
	else {
		hr.hit = hr1.hit;
		return t1;
	}
}

float Plane::
intersect(const Ray &ray, HitRecord &hr) const
{
	// return the distance to the intersection
	auto dt = ray.dir.dot(normal);

	// if dt is zero, the ray is parallel to the plane
	if (dt == 0.0) return std::numeric_limits<float>::infinity();
	else {
		float dist = (origin - ray.origin).dot(normal) / dt;
		hr.hit = ray.origin + dist * ray.dir;
		return dist;
	}
}


float Cylinder::
intersect(const Ray &ray, HitRecord &hr) const
{
	// TODO: check if correct

	auto dp = ray.origin - origin;
	auto E = ray.dir - ray.dir.dot(dir) * dir;
	auto D = dp - dp.dot(dir) * dir;

	auto A = E.dot(E);
	auto B = 2*(E.dot(D));
	auto C = D.dot(D) - r*r;

	auto q = B*B - 4*A*C;
	if (q < 0.0) {
		return std::numeric_limits<float>::infinity();
	}
	if (A == 0.0) {
		return std::numeric_limits<float>::infinity();
	}

	q = std::sqrt(q);
	auto t0 = (-B + q) / (2.0 * A);
	auto t1 = (-B - q) / (2.0 * A);

	if (t0 < 0.0 && t1 < 0.0) {
		return std::numeric_limits<float>::infinity();
	}
	if (t0 < 0.0 && t1 >= 0.0) {
		hr.hit = ray.origin + t1 * ray.dir;
		return t1;
	}
	if (t1 < 0.0 && t0 >= 0.0) {
		hr.hit = ray.origin + t0 * ray.dir;
		return t0;
	}
	if (t1 > t0) {
		hr.hit = ray.origin + t0 * ray.dir;
		return t0;
	}
	hr.hit = ray.origin + t1 * ray.dir;
	return t1;
}


AABox::AABox(Vec3f X0, Vec3f X1, float height)
: _height(height), _X0(X0), _X1(X1)
, _north({X0[0], X1[1], 0.0f}, X1, _height)
, _east(X1, {X1[0], X0[1], 0.0f}, _height)
, _south({X1[0], X0[1], 0.0f}, X0, _height)
, _west(X0, {X0[0], X1[1], 0.0f}, _height)
{ }


float AABox::
intersect(const Ray &ray, HitRecord &hr) const
{
	auto inf = std::numeric_limits<float>::infinity();
	float dist_min = inf;
	float dist = inf;

	for (auto wall: {_north, _east, _south, _west}) {
		HitRecord local_hr;
		dist = wall.intersect(ray, local_hr);
		if (dist < dist_min) {
			dist_min = dist;
			hr.hit = local_hr.hit;
		}
	}

	return dist_min;
}


#endif

} // ncr::
