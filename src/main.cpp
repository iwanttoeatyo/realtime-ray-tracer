#include <iostream>
#include "SDL2/SDL.h"
#include "Random.h"
#include "Sphere.h"
#include "HitableList.h"
#include "Material.h"
#include "Box.h"
#include "PerformanceCounter.h"
#include "Polygon.h"
#include "XYRect.h"
#include "MovingSphere.h"
#include "Metal.h"
#include "BlinnPhong.h"
#include "Globals.h"

using std::cout;
using std::endl;

Uint32 rgba_to_uint32(Uint8 r, Uint8 g, Uint8 b, Uint8 a);
Uint32 vector3_to_uint32(const Vector3& color, float alpha = 1);
Vector3 ray_trace(const Ray& ray, Hitable* world, int depth);


Hitable* simpleLight();
Hitable* cornell_box();
Hitable* cornell_gloss();
Hitable* cornell_translucency();
Hitable* cornell_shadow();
Hitable* cornell_blur();
Hitable* cornell_dof();
Hitable* cornell_lambert();

void setupCornellWalls(Hitable** list, int& i);


int main(int argc, char** argv)
{
	Random::init();

	SDL_Init(SDL_INIT_EVERYTHING);
	SDL_Window* window = SDL_CreateWindow("RealTime Ray-tracer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	                                      SCREEN_WIDTH,
	                                      SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
	SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, SCREEN_WIDTH,
	                                         SCREEN_HEIGHT);

	Vector3* float_pixels = new Vector3[SCREEN_WIDTH * SCREEN_HEIGHT];
	memset(float_pixels, 0, sizeof(Vector3) * SCREEN_WIDTH * SCREEN_HEIGHT);

	Uint32* pixels = new Uint32[SCREEN_WIDTH * SCREEN_HEIGHT];
	memset(pixels, 0, sizeof(Uint32) * SCREEN_WIDTH * SCREEN_HEIGHT);

	SDL_SetRenderDrawColor(renderer, 50, 100, 50, 255);

	SDL_RenderClear(renderer);

	//Pos, normal, up, vFov, aspect ratio
	const float vFOV = 40;
	Vector3 eye(278, 278, -800);
	Vector3 target(278, 278, 0);

	camera = Camera(eye, target, {0, 1, 0}, vFOV, ASPECT_RATIO, 0, (eye - target).length() * 2, 0, 1);
	world = cornell_gloss();

	//camera = Camera({5, 2, 10}, {1, 1,-1}, {0, 1, 0}, vFOV, ASPECT_RATIO);
	//Hitable* world = simpleLight();

	int samples = 0;

	SDL_Event event;

	bool quit = false;
	while (!quit)
	{
		float blend = 1.0 / float(samples + 1);

#pragma omp parallel for
		for (int y = 0; y < SCREEN_HEIGHT; y++)
		{
			thread_local std::mt19937 gen(std::random_device{}());
			for (int x = 0; x < SCREEN_WIDTH; x++)
			{
				float fx = float(x), fy = float(y);
				float u = fx + 0.5f, v = fy + 0.5f;

				//Thread safe rand	
				u = Random::randf(gen, fx, fx + 1);
				v = Random::randf(gen, fy, fy + 1);

				u = u / float(SCREEN_WIDTH);
				v = v / float(SCREEN_HEIGHT);

				Ray ray = camera.getRay(u, v);
				//Ray ray = camera.getRay(u, v);
				Vector3 color = ray_trace(ray, world, 0);

				//color = {sqrtf(color.r), sqrtf(color.g), sqrtf(color.b)};

				//HDR + Gamma Correction Magic
				//https://www.slideshare.net/ozlael/hable-john-uncharted2-hdr-lighting  slide 140
				color -= 0.004f;
				color.clampMin(0);
				color = (color * (6.2f * color + 0.5f)) / (color * (6.2f * color + 1.7f) + 0.06f);

				float_pixels[(SCREEN_HEIGHT - y - 1) * SCREEN_WIDTH + x].mix(color, blend);

				pixels[(SCREEN_HEIGHT - y - 1) * SCREEN_WIDTH + x] = vector3_to_uint32(
					float_pixels[(SCREEN_HEIGHT - y - 1) * SCREEN_WIDTH + x]);
			}
		}

		//cout << "Sample " << samples++ << endl;


		SDL_UpdateTexture(texture, NULL, pixels, sizeof(Uint32) * SCREEN_WIDTH);
		SDL_RenderCopy(renderer, texture,NULL,NULL);
		SDL_RenderPresent(renderer);

		int last_x = mouse_x, last_y = mouse_y;

		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_QUIT:
				quit = true;
				break;
			case SDL_MOUSEMOTION:
				if (!mouse_down) break;
				SDL_GetMouseState(&mouse_x, &mouse_y);
				camera.processMouseMovement(mouse_x - last_x, mouse_y - last_y);
				last_x = mouse_x, last_y = mouse_y;
				memset(float_pixels, 0, sizeof(Vector3) * SCREEN_WIDTH * SCREEN_HEIGHT);
				samples = 0;
				break;
			case SDL_MOUSEBUTTONDOWN:
				SDL_GetMouseState(&mouse_x, &mouse_y);
				last_x = mouse_x, last_y = mouse_y;
				mouse_down = true;
				break;
			case SDL_MOUSEBUTTONUP:
				mouse_down = false;
				break;
			}
		}

		const Uint8* state = SDL_GetKeyboardState(NULL);
		if (state[SDL_SCANCODE_R])
		{
			memset(float_pixels, 0, sizeof(Vector3) * SCREEN_WIDTH * SCREEN_HEIGHT);
			samples = 0;
		}
	}

	SDL_DestroyWindow(window);
	SDL_Quit();

	return
		0;
}

inline Uint32 rgba_to_uint32(Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	auto result = r << 24 | g << 16 | b << 8 | a;
	return result;
}

Uint32 vector3_to_uint32(const Vector3& color, float alpha)
{
	int r = int(255.9 * color.r);
	r = std::clamp(r, 0, 255);
	int g = int(255.9 * color.g);
	g = std::clamp(g, 0, 255);
	int b = int(255.9 * color.b);
	b = std::clamp(b, 0, 255);
	int a = int(255.9 * alpha);
	a = std::clamp(a, 0, 255);
	auto result = r << 24 | g << 16 | b << 8 | a;
	return result;
}

Vector3 ray_trace(const Ray& ray, Hitable* world, int depth)
{
	HitRecord rec;
	Ray ray_out;
	ray_out.time = ray.time;
	Vector3 attenuation;

	if (world->hit(ray, 0.001f, FLT_MAX, rec))
	{
		Vector3 emitted = rec.mat_ptr->emitted(ray, rec);
		if (depth < MAX_RAY_DEPTH && rec.mat_ptr->scatter(ray, rec, attenuation, ray_out))
		{
			Vector3 color = emitted + attenuation * ray_trace(ray_out, world, depth + 1);
			//Whitted Ray tracing splits the rays into two for refract+reflect materials
			if (rec.mat_ptr->scatterTwo(ray, rec, attenuation, ray_out))
				color += attenuation * ray_trace(ray_out, world, depth + 1);
			return color;
		}
		else
			return emitted;
	}
	else
		return AMBIENT_LIGHT;
}


Hitable* cornell_lambert()
{
	Material* white = new Lambertian(new ConstantTexture(white_color));
	Material* red = new Lambertian(new ConstantTexture(red_color));
	Material* green = new Lambertian(new ConstantTexture(green_color));
	Material* blue = new Lambertian(new ConstantTexture(blue_color));

	Material* light = new DiffuseLight(new ConstantTexture({60, 60, 60}));

	g_lights.emplace_back(Vector3(50, 50, 0), Vector3(0, 0, 0), Vector3(1), Vector3(300));

	Hitable** list = new Hitable*[11];
	int i = 0;

	list[i++] = new FlipNormals(new YZRect(0, 555, 0, 555, 555, red));
	list[i++] = new YZRect(0, 555, 0, 555, 0, green);
	list[i++] = new FlipNormals(new XZRect(0, 555, 0, 555, 555, white));
	//floor
	list[i++] = new XZRect(0, 555, 0, 555, 0, white);
	list[i++] = new FlipNormals(new XYRect(0, 555, 0, 555, 555, white));
	//list[i++] = new XYRect(0, 555, 0, 555, 0, mirror);
	list[i++] = new XYRect(0, 50, 0, 50, 0, light);

	list[i++] = new Sphere({168, 388, 278}, 100, white);
	list[i++] = new Sphere({168, 168, 278}, 100, red);
	list[i++] = new Sphere({388, 388, 278}, 100, green);
	list[i++] = new Sphere({388, 168, 278}, 100, blue);

	return new HitableList(list, i);
}

void setupCornellWalls(Hitable** list, int& i)
{
	//left walls
	list[i++] = new FlipNormals(new YZRect(0, 555, 0, 555, 555, red_matte));
	//right
	list[i++] = new YZRect(0, 555, 0, 555, 0, green_matte);
	//ceiling
	list[i++] = new FlipNormals(new XZRect(0, 555, 0, 555, 555, white_matte));
	//floor
	list[i++] = new XZRect(0, 555, 0, 555, 0, white_matte);
	//back
	list[i++] = new FlipNormals(new XYRect(0, 555, 0, 555, 555, white_matte));
}

Hitable* cornell_gloss()
{
	Hitable** list = new Hitable*[11];
	int i = 0;

	Material* light = new DiffuseLight(new ConstantTexture({1000, 600, 600}));
	g_lights.emplace_back(Vector3(25, 25, 0), Vector3(50, 50, 0), Vector3(1), Vector3(300));

	setupCornellWalls(list, i);
	list[i++] = new XYRect(0, 50, 0, 50, 0, light);

	list[i++] = new Sphere({168, 388, 278}, 100, white_gloss);
	list[i++] = new Sphere({168, 168, 278}, 100, red_gloss);
	list[i++] = new Sphere({388, 388, 278}, 100, green_gloss);
	list[i++] = new Sphere({388, 168, 278}, 100, blue_gloss);
	return new HitableList(list, i);
}

Hitable* cornell_dof()
{
	Material* whiteWall = new Lambertian(new ConstantTexture(white_color));
	Material* redWall = new Lambertian(new ConstantTexture(red_color));
	Material* greenWall = new Lambertian(new ConstantTexture(green_color));
	Material* light = new DiffuseLight(new ConstantTexture({60, 60, 60}));

	g_lights.emplace_back(Vector3(25, 25, 0), Vector3(0, 0, 0), Vector3(1), Vector3(300));

	Hitable** list = new Hitable*[11];
	int i = 0;

	Material* white = new BlinnPhong(white_color, Vector3(1), 64, false, 0.f);
	Material* red = new BlinnPhong(red_color, Vector3(1), 64, false, 0.f);
	Material* blue = new BlinnPhong(blue_color, Vector3(1), 64, false, 0.f);

	list[i++] = new FlipNormals(new YZRect(0, 555, 0, 555, 555, redWall));
	list[i++] = new YZRect(0, 555, 0, 555, 0, greenWall);
	list[i++] = new FlipNormals(new XZRect(0, 555, 0, 555, 555, whiteWall));
	//floor
	list[i++] = new XZRect(0, 555, 0, 555, 0, whiteWall);
	list[i++] = new FlipNormals(new XYRect(0, 555, 0, 555, 555, whiteWall));
	list[i++] = new XYRect(0, 50, 0, 50, 0, light);

	list[i++] = new Sphere({278, 278, 278}, 80, white);
	list[i++] = new Sphere({378, 278, 400}, 80, red);
	list[i++] = new Sphere({198, 278, 100}, 80, blue);

	return new HitableList(list, i);
}


Hitable* cornell_blur()
{
	Material* whiteWall = new Lambertian(new ConstantTexture(white_color));
	Material* redWall = new Lambertian(new ConstantTexture(red_color));
	Material* greenWall = new Lambertian(new ConstantTexture(green_color));
	Material* light = new DiffuseLight(new ConstantTexture({60, 60, 60}));

	g_lights.emplace_back(Vector3(25, 25, 0), Vector3(0, 0, 0), Vector3(1), Vector3(300));

	Hitable** list = new Hitable*[11];
	int i = 0;

	Material* white = new BlinnPhong(white_color, Vector3(1), 64, false, 0.f);
	Material* red = new BlinnPhong(red_color, Vector3(1), 64, false, 0.f);
	Material* blue = new BlinnPhong(blue_color, Vector3(1), 64, false, 0.f);

	list[i++] = new FlipNormals(new YZRect(0, 555, 0, 555, 555, redWall));
	list[i++] = new YZRect(0, 555, 0, 555, 0, greenWall);
	list[i++] = new FlipNormals(new XZRect(0, 555, 0, 555, 555, whiteWall));
	//floor
	list[i++] = new XZRect(0, 555, 0, 555, 0, whiteWall);
	list[i++] = new FlipNormals(new XYRect(0, 555, 0, 555, 555, whiteWall));
	list[i++] = new XYRect(0, 50, 0, 50, 0, light);
#if 0
	list[i++] = new MovingSphere({238,140,238},{288,80,188},0,1,80, white);
	list[i++] = new MovingSphere({428,120,228},{428,80,228},0,1,80, red);
	list[i++] = new MovingSphere({128,120,228},{128,80,328},0,1,80, blue);
#else
	list[i++] = new Sphere({288, 80, 188}, 80, white);
	list[i++] = new Sphere({428, 80, 228}, 80, red);
	list[i++] = new Sphere({128, 80, 328}, 80, blue);
#endif
	return new HitableList(list, i);
}


Hitable* cornell_shadow()
{
	Material* checker = new Lambertian(new CheckerTexture(new ConstantTexture({0.12f, 0.45f, 0.15f}),
	                                                      new ConstantTexture({0.73f, 0.73f, 0.73f}), 0.1f));
	Material* whiteWall = new Lambertian(new ConstantTexture(white_color));
	Material* redWall = new Lambertian(new ConstantTexture(red_color));
	Material* greenWall = new Lambertian(new ConstantTexture(green_color));
	Material* light = new DiffuseLight(new ConstantTexture({60, 60, 60}));

	g_lights.emplace_back(Vector3(25, 25, 0), Vector3(0, 0, 0), Vector3(1), Vector3(300));

	Hitable** list = new Hitable*[11];
	int i = 0;

	Material* white = new BlinnPhong(white_color, Vector3(1), 64, false, 0.f);
	Material* red = new BlinnPhong(red_color, Vector3(1), 64, false, 0.f);

	list[i++] = new FlipNormals(new YZRect(0, 555, 0, 555, 555, redWall));
	list[i++] = new YZRect(0, 555, 0, 555, 0, greenWall);
	list[i++] = new FlipNormals(new XZRect(0, 555, 0, 555, 555, whiteWall));
	//floor
	list[i++] = new XZRect(0, 555, 0, 555, 0, whiteWall);
	list[i++] = new FlipNormals(new XYRect(0, 555, 0, 555, 555, checker));
	list[i++] = new XYRect(0, 50, 0, 50, 0, light);


	list[i++] = new Sphere({308, 188, 278}, 80, red);
	list[i++] = new Box({178, 100, 178}, {50, 200, 50}, white);
	return new HitableList(list, i);
}

Hitable* cornell_translucency()
{
	Material* checker = new Lambertian(new CheckerTexture(new ConstantTexture({0.12f, 0.45f, 0.15f}),
	                                                      new ConstantTexture({0.73f, 0.73f, 0.73f}), 0.1f));
	Material* whiteWall = new Lambertian(new ConstantTexture(white_color));
	Material* redWall = new Lambertian(new ConstantTexture(red_color));
	Material* greenWall = new Lambertian(new ConstantTexture(green_color));
	Material* light = new DiffuseLight(new ConstantTexture({60, 60, 60}));

	float blur = 0.0f;
	float ref = 1.5f;
	Material* white = new Dialectric(white_color, ref, blur);
	Material* red = new Dialectric(red_color, ref, blur);
	Material* green = new Dialectric(green_color, ref, blur);
	Material* blue = new Dialectric(blue_color, ref, blur);

	g_lights.emplace_back(Vector3(50, 50, 0), Vector3(0, 0, 0), Vector3(1), Vector3(300));

	Hitable** list = new Hitable*[11];
	int i = 0;

	list[i++] = new FlipNormals(new YZRect(0, 555, 0, 555, 555, redWall));
	list[i++] = new YZRect(0, 555, 0, 555, 0, greenWall);
	list[i++] = new FlipNormals(new XZRect(0, 555, 0, 555, 555, whiteWall));
	//floor
	list[i++] = new XZRect(0, 555, 0, 555, 0, whiteWall);
	list[i++] = new FlipNormals(new XYRect(0, 555, 0, 555, 555, checker));
	list[i++] = new XYRect(0, 50, 0, 50, 0, light);

	list[i++] = new Sphere({168, 388, 278}, 100, white);
	list[i++] = new Sphere({168, 168, 278}, 100, red);
	list[i++] = new Sphere({388, 388, 278}, 100, green);
	list[i++] = new Sphere({388, 168, 278}, 100, blue);

	return new HitableList(list, i);
}

Hitable* simpleLight()
{
	Texture* t = new ConstantTexture({1, 0, 0});
	Texture* checker = new CheckerTexture(new ConstantTexture({.2f, .3f, .1f}), new ConstantTexture({.9f, .9f, .9f}),
	                                      10);
	Hitable** list = new Hitable*[4];

	list[0] = new Sphere({0, -1000, 0}, 1000, new Lambertian(checker));
	list[1] = new Sphere({0, 1, 2}, 3, new Lambertian(t));
	list[2] = new Sphere({0, 7, 0}, 2, new DiffuseLight(new ConstantTexture({4, 4, 4})));
	list[3] = new XYRect(3, 5, 1, 3, -2, new DiffuseLight(new ConstantTexture({4, 4, 4})));

	g_lights.emplace_back(Vector3(4, 2, -2), Vector3(2, 2, 0), Vector3(1), Vector3(4));

	return new HitableList(list, 4);
}

Hitable* cornell_box()
{
	Material* white = new Lambertian(new ConstantTexture({0.73f, 0.73f, 0.73f}));
	Material* red = new Lambertian(new ConstantTexture({0.65f, 0.05f, 0.05f}));
	Material* green = new Lambertian(new ConstantTexture({0.12f, 0.45f, 0.15f}));
	Material* light = new DiffuseLight(new ConstantTexture({15, 15, 15}));
	Material* light2 = new DiffuseLight(new ConstantTexture({2, 2, 2}));
	Material* checker = new Lambertian(new CheckerTexture(new ConstantTexture({0.12f, 0.45f, 0.15f}),
	                                                      new ConstantTexture({0.73f, 0.73f, 0.73f}), 0.1f));

	Material* mirror = new Metal(white_color, 0.3f);
	Material* metal = new Metal(white_color, 0.3f);
	Material* dialectric = new Dialectric(white_color, 1.5);

	Material* blinn = new BlinnPhong(red_color, Vector3(1), 128, false, 1.f);

	Hitable** list = new Hitable*[11];
	int i = 0;


	g_lights.emplace_back(Vector3((213 + 343) / 2, 524, (227 + 332) / 2), Vector3(342 - 213, 0, 332 - 227), Vector3(1),
	                      Vector3(300));


	list[i++] = new FlipNormals(new YZRect(0, 555, 0, 555, 555, red));
	list[i++] = new YZRect(0, 555, 0, 555, 0, green);
	list[i++] = new XZRect(213, 343, 227, 332, 554, light);
	//list[i++] = new XZRect(0, 555, 0, 555, 554, light2);
	list[i++] = new FlipNormals(new XZRect(0, 555, 0, 555, 555, white));
	//floor
	list[i++] = new XZRect(0, 555, 0, 555, 0, white);
	list[i++] = new FlipNormals(new XYRect(0, 555, 0, 555, 555, white));
	//list[i++] = new XYRect(0, 555, 0, 555, 0, mirror);

	list[i++] = new Sphere({450, 80, 250}, 75, dialectric);
#ifdef PATH_TRACING
	list[i++] = new MovingSphere({105, 80, 250}, {105, 160, 250}, 0.f, 1.f, 75, red);
#else
	list[i++] = new Sphere({105, 80, 250}, 75, blinn);
#endif
	list[i++] = new Box({475, 75, 450}, {100, 150, 100}, checker);
	list[i++] = new Sphere({278, 20, 278}, 80, metal);

	//return new BVHNode(list, i, 0, 0);
	return new HitableList(list, i);
}
