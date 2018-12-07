#pragma once

#include "Vector3.h"
#include "Camera.h"
#include <vector>
#include "Light.h"

#define global_extern extern

#define SAMPLING
#define UNIFORM_SAMPLING
#define PATH_TRACING
#define BAD_SHADOWS


global_extern const int SCREEN_WIDTH;
global_extern const int SCREEN_HEIGHT;
global_extern const float ASPECT_RATIO;

global_extern const int MAX_RAY_DEPTH;
global_extern const int MAX_SAMPLES;

global_extern const Vector3 LIGHT_DIR;
global_extern const Vector3 LIGHT_POS;
global_extern const Vector3 LIGHT_POWER;
global_extern const Vector3 AMBIENT_LIGHT;

//Pos, normal, up, vFov, aspect ratio
global_extern Camera camera;


global_extern std::vector<Light> g_lights;

global_extern const Vector3 red_color;
global_extern const Vector3 blue_color;
global_extern const Vector3 green_color;
global_extern const Vector3 white_color;
