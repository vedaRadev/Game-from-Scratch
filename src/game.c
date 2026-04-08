// NOTE(mal) ON 1D COORDINATE SYSTEM:
// World space: RHS +X Right, +Y Up, +Z Forward
// OGL-style camera space and projection matrix

#include <stdint.h>
#include <math.h>
#include "platform.h"
// NOTE(mal): Only on Linux, probably wrap this in a #if (something)
// Actually there should be no platform-specific anything in the game layer.
// All platform-specific behavior should be accessed via the interface that's passed
// to the game layer (at the moment, through the GameMemory param).
#include <stdlib.h> // for abs at the moment
#include <string.h>

#define PI 3.14159f
// TODO(mal): remove if unused, just added for fun
#define DEGREES_TO_RADIANS(deg) ((deg) * PI / 180.0f)

inline float degrees_to_radians(float degrees) {
	float result = degrees * PI / 180.0f;
	return result;
}

// TODO(mal): See chapter 4.10 SIMD/Vector Processing in "Game Engine Architecture" for how to
// implement many of our vector/matrix operations using SSE.
// TODO(mal): Some of the Mat4x4 and Mat3x3 ops are very similar in implementation. Is there a way
// to semantically compress that code?

typedef union Vec2 {
	float elements[2];
	struct { float x; float y; };
} Vec2;

typedef union Vec3 {
	float elements[3];
	struct { float x; float y; float z; };
} Vec3;

// a + b
Vec3 vec3_add(Vec3 a, Vec3 b) {
	Vec3 result = {};
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	result.z = a.z + b.z;
	return result;
}

// a - b
Vec3 vec3_sub(Vec3 a, Vec3 b) {
	Vec3 result = {};
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	result.z = a.z - b.z;
	return result;
}

// -v
Vec3 vec3_negate(Vec3 v) {
	Vec3 result = {};
	result.x = -v.x;
	result.y = -v.y;
	result.z = -v.z;
	return result;
}

// a dot b
float vec3_dot(Vec3 a, Vec3 b) {
	float result;
	result = a.x * b.x + a.y * b.y + a.z * b.z;
	return result;
}

// a x b
Vec3 vec3_cross(Vec3 a, Vec3 b) {
	Vec3 result = {};
	result.x = a.y * b.z - a.z * b.y;
	result.y = a.z * b.x - a.x * b.z;
	result.z = a.x * b.y - a.y * b.x;
	return result;
}

void vec3_normalize(Vec3 *v) {
	float magnitude = (float)sqrt((v->x * v->x) + (v->y * v->y) + (v->z * v->z));
	v->x /= magnitude;
	v->y /= magnitude;
	v->z /= magnitude;
}

Vec3 mult_vec3_scalar(Vec3 v, float s) {
	Vec3 result = {};
	result.x = v.x * s;
	result.y = v.y * s;
	result.z = v.z * s;
	return result;
}

// NOTE(mal): With homogeneous notation, vectors w = 0, points w = 1
typedef union Vec4 {
	float elements[4];
	struct {
		float x;
		float y;
		float z;
		float w;
	};
} Vec4;

typedef union Mat3x3 {
	float elements[9];
	float rows[3][3];
} Mat3x3;

// Let M be 3x3 matrix, V be a 3x1 vector
// Computes MV.
Vec3 mult_mat3x3_vec3(Mat3x3 m, Vec3 v) {
	Vec3 result = {};

	for (int i = 0; i < 3; i++) {
		float *m_row = m.rows[i];
		for (int j = 0; j < 3; j++) {
			float m_val = m_row[j];
			float v_val = v.elements[j];
			result.elements[i] += (m_val * v_val);
		}
	}

	return result;
}

// TODO(mal): should this be in-place?
// Computes the tranpose of the given matrix.
Mat3x3 mat3x3_transpose(Mat3x3 m) {
	Mat3x3 result = {};

	for (int r = 0; r < 3; r++) {
		for (int c = 0; c < 3; c++) {
			result.rows[c][r] = m.rows[r][c];
		}
	}

	return result;
}

// Let A be 4x4 matrix, B be 4x4 matrix.
// Computes AB.
Mat3x3 mult_mat3x3_mat3x3(Mat3x3 a, Mat3x3 b) {
	Mat3x3 result = {};
	Mat3x3 b_transpose = mat3x3_transpose(b);

	for (int r = 0; r < 3; r++) {
		float *a_row = a.rows[r];
		for (int c = 0; c < 3; c++) {
			float *b_col = b_transpose.rows[c];
			for (int i = 0; i < 3; i++) {
				result.rows[r][c] += (a_row[i] * b_col[i]);
			}
		}
	}

	return result;
}

Mat3x3 mat3x3_create_identity() {
	Mat3x3 result = {};
	result.rows[0][0] = 1;
	result.rows[1][1] = 1;
	result.rows[2][2] = 1;
	return result;
}

Mat3x3 mat3x3_create_rotation_x(float radians) {
	Mat3x3 result = {};

	float sin_theta = (float)sin(radians);
	float cos_theta = (float)cos(radians);

	result.rows[0][0] = 1;
	result.rows[0][1] = 0;
	result.rows[0][2] = 0;
	result.rows[1][0] = 0;
	result.rows[1][1] = cos_theta;
	result.rows[1][2] = -sin_theta;
	result.rows[2][0] = 0;
	result.rows[2][1] = sin_theta;
	result.rows[2][2] = cos_theta;

	return result;
}

Mat3x3 mat3x3_create_rotation_y(float radians) {
	Mat3x3 result = {};

	float sin_theta = (float)sin(radians);
	float cos_theta = (float)cos(radians);

	result.rows[0][0] = cos_theta;
	result.rows[0][1] = 0;
	result.rows[0][2] = sin_theta;
	result.rows[1][0] = 0;
	result.rows[1][1] = 1;
	result.rows[1][2] = 0;
	result.rows[2][0] = -sin_theta;
	result.rows[2][1] = 0;
	result.rows[2][2] = cos_theta;

	return result;
}

Mat3x3 mat3x3_create_rotation_z(float radians) {
	Mat3x3 result = {};

	float sin_theta = (float)sin(radians);
	float cos_theta = (float)cos(radians);

	result.rows[0][0] = cos_theta;
	result.rows[0][1] = -sin_theta;
	result.rows[0][2] = 0;
	result.rows[1][0] = sin_theta;
	result.rows[1][1] = cos_theta;
	result.rows[1][2] = 0;
	result.rows[2][0] = 0;
	result.rows[2][1] = 0;
	result.rows[2][2] = 1;

	return result;
}

typedef union Mat4x4 {
	float elements[16];
	float rows[4][4];
} Mat4x4;

inline Mat4x4 mat4x4_identity() {
	Mat4x4 result = {};
	result.rows[0][0] = 1;
	result.rows[1][1] = 1;
	result.rows[2][2] = 1;
	result.rows[3][3] = 1;
	return result;
}

void mat4x4_scale(Mat4x4 *m, float scale) {
	m->rows[0][0] *= scale;
	m->rows[1][1] *= scale;
	m->rows[2][2] *= scale;
	m->rows[3][3] *= scale;
}

// TODO(mal): should this be in-place?
// Computes the tranpose of the given matrix.
Mat4x4 mat4x4_transpose(Mat4x4 m) {
	Mat4x4 result = {};

	for (int r = 0; r < 4; r++) {
		for (int c = 0; c < 4; c++) {
			result.rows[c][r] = m.rows[r][c];
		}
	}

	return result;
}

// Let M be 4x4 matrix, V be a 4x1 vector
// Computes MV.
Vec4 mult_mat4x4_vec4(Mat4x4 m, Vec4 v) {
	Vec4 result = {};

	for (int i = 0; i < 4; i++) {
		float *m_row = m.rows[i];
		for (int j = 0; j < 4; j++) {
			float m_val = m_row[j];
			float v_val = v.elements[j];
			result.elements[i] += (m_val * v_val);
		}
	}

	return result;
}

// Let A be 4x4 matrix, B be 4x4 matrix.
// Computes AB.
Mat4x4 mult_mat4x4_mat4x4(Mat4x4 a, Mat4x4 b) {
	Mat4x4 result = {};
	Mat4x4 b_transpose = mat4x4_transpose(b);

	for (int r = 0; r < 4; r++) {
		float *a_row = a.rows[r];
		for (int c = 0; c < 4; c++) {
			float *b_col = b_transpose.rows[c];
			for (int i = 0; i < 4; i++) {
				result.rows[r][c] += (a_row[i] * b_col[i]);
			}
		}
	}

	return result;
}

// https://www.scratchapixel.com/lessons/mathematics-physics-for-computer-graphics/lookat-function/framing-lookat-function.html
// WARN(mal): Have not tested this yet. May or may not work with the way I have my world axes set
// up. Not sure yet.
Mat3x3 look_at(Vec3 from, Vec3 to, Vec3 up) {
	Vec3 forward = vec3_sub(from, to);
	vec3_normalize(&forward);
	Vec3 right = vec3_cross(up, forward);
	vec3_normalize(&right);
	up = vec3_cross(forward, right);

	Mat3x3 result = {
		.rows = {
			[0][0] = right.x,
			[0][1] = right.y,
			[0][2] = right.z,

			[1][0] = up.x,
			[1][1] = up.y,
			[1][2] = up.z,

			[2][0] = forward.x,
			[2][1] = forward.y,
			[2][2] = forward.z,
		}
	};

	return result;
}

// Draw a line using bresenham's line drawing algorithm
// NOTE(mal): This does NOT clip lines to the bounds of the pixel buffer. Caller beware!
void draw_line_2d(uint32_t *pixels, int width, int height, int x0, int y0, int x1, int y1) {
	int dx = abs(x1 - x0);
	int dy = -abs(y1 - y0);
	int sx = x0 < x1 ? 1 : -1;
	int sy = y0 < y1 ? 1 : -1;
	int err = dx + dy;

	// FIXME(mal): Sometimes an infinite loop happens here
	while (true) {
		if (x0 > 0 && x0 < width && y0 > 0 && y0 < height)
			pixels[x0 + y0 * width] = -1;

		if (x0 == x1 && y0 == y1) {
			break;
		}

		int e2 = err * 2;
		if (e2 >= dy) {
			err += dy;
			x0 += sx;
		}
		if (e2 <= dx) {
			err += dx;
			y0 += sy;
		}
	}
}

typedef struct Vertex {
	Vec4 position;
	uint32_t color;   // argb
	float tx_u, tx_v; // texture coordinates
} Vertex;

typedef struct Triangle3D {
	Vertex local_vertices[3]; // in local space
	Vec3   world_position;    // (x, y, z) in world space
 	Mat3x3 world_orientation; // degrees in world space
	float  scale;
} Triangle3D;

// A square in 3D space, constructed from two triangles
typedef struct Square3D {
	Vertex local_vertices[4];
	int    vertex_list[6];
	Vec3   world_position;    // (x, y, z) in world space
 	Mat3x3 world_orientation; // degrees in world space
	float  scale;
} Square3D;

typedef enum RenderRasterTileState {
	RENDER_RASTER_TILES_OFF,
	RENDER_RASTER_TILES_BELOW,
	RENDER_RASTER_TILES_ONTOP,
} RenderRasterTileState;

// TODO(mal): It will be critical in the future to introduce some memory allocators and start
// using them to store some of the data in here. For example, loaded texture data and whatnot.
// The backing stores of these allocators will be the rest of our GameMemory.storage excluding
// the GameState structure.
typedef struct GameState {
	// Triangle3D triangle;
	Square3D square;
	float rotation_y_degrees;
	Vec3 camera_world_position;
	Mat3x3 camera_world_orientation; // euler angles
	unsigned texture_width;
	unsigned texture_height;
	uint32_t *texture_pixels;
	bool render_wireframe;
	bool skip_rasterization;
	RenderRasterTileState render_raster_tile_state;
} GameState;

// http://www.paulbourke.net/dataformats/tga/
#pragma pack(push, 1)
typedef struct TGA_Header {
	char  idlength;
	char  colourmaptype;
	char  datatypecode;
	short colourmaporigin;
	short colourmaplength;
	char  colourmapdepth;
	short x_origin;
	short y_origin;
	short width;
	short height;
	char  bitsperpixel;
	char  imagedescriptor;
} TGA_Header;
#pragma pack(pop)

// v0 - first vertex, v1 - second vertex, p - test point
// Assumes a clockwise winding order.
// Returns the signed area of a parallelogram formed by vectors (v1 - v0) and (p - v0).
// The value is positive if p is on the right side of line (v1 - v0) and negative if on the left.
// The value is 0 if p is on the line (v1 - v0).
//
// NOTE(mal):
// The value returned by this function is the same as the 2D "cross product" of
// vectors (p - v0) and (v1 - v0) and the determinant of the 2D matrix formed by
// the same two vectors.
// NOTE(mal): Also see https://en.wikipedia.org/wiki/Shoelace_formula (found in
// https://jtsorlinis.github.io/rendering-tutorial/0)
// NOTE(mal): From "Conservative and Tiled Rasterization Using a Modified Triangle Setup"
// by Akenine-Moller T. and T. Aila, the edge function e(S) of a line PQ testing point S
// (For CCW ordering with +x right +y up):
//    Epq(S) = (-(Qy - Py), Qx - Px) dot (S - P) = n dot (S - P) = (n dot s) + c
// Expanding
//    Epq(S) = -(Qy - Py) * (Sx - Px) + (Qx - Px) * (Sy - Py)
//           = (Qx - Px) * (Sy - Py) - (Qy - Py) * (Sx - Px)
// However for our case where we have CW +Y down:
// The normal vector flips because it is a pseudovector (axial vector). The normal
// of our edge vector for CW +Y down is formed by rotating the edge vector -90
// degrees (basically negate the CCW +Y up normal).
// Thus we have
//    Epq = (Qy - Py, -(Qx - Px)) dot (S - P)
//        = (Qy - Py) * (Sx - Px) + -(Qx - Px) * (Sy - Py)
//        = (Qy - Py) * (Sx - Px) - (Qx - Px) * (Sy - Py)
// Which exactly matches our edge function below.
float edge_function(float v0x, float v0y, float v1x, float v1y, float px, float py) {
	float result = (v1y - v0y) * (px - v0x) - (v1x - v0x) * (py - v0y);
	return result;
}
// nx, ny - edge normal N
// sx, sy - test point S
// c - precomputed value = (-N dot edge_start_vertex)
float edge_function_2(float nx, float ny, float sx, float sy, float c) {
	float result = (nx * sx + ny * sy) + c;
	return result;
}

// FIXME(mal): Take input_capacity and output_capacity parameters to ensure we don't overflow buffers.
// NOTE(mal): Currently, this assumes that the input capacity and the output capacity are the same!
size_t clip_sutherland_hodgeman(int plane_index, int plane_sign, Vertex *input, size_t input_count, Vertex *output) {
	size_t output_count = 0;

	// Clip edge from start_vertex to end_vertex against the plane.
	// We will do this sequentially for each edge in the polygon.
	Vertex *start_vertex = &input[input_count - 1];
	float dist_start_vertex_to_plane = start_vertex->position.w + plane_sign * start_vertex->position.elements[plane_index];
	int   start_vertex_inside_plane  = dist_start_vertex_to_plane >= 0;
	for (int i = 0; i < input_count; i++) {
		Vertex *end_vertex = &input[i];
		float dist_end_vertex_to_plane = end_vertex->position.w + plane_sign * end_vertex->position.elements[plane_index];
		int   end_vertex_inside_plane  = dist_end_vertex_to_plane >= 0;

		// We only want to output vertices if at least one of endpoints is inside the plane.
		if (start_vertex_inside_plane || end_vertex_inside_plane) {
			if (start_vertex_inside_plane) {
				output[output_count] = *start_vertex;
				output_count++;
			}

			// If one of our vertices is outside the plane we have to generate a clip point at
			// the intersection of our edge and the plane.
			if (!(start_vertex_inside_plane && end_vertex_inside_plane)) {
				Vertex *clip_vertex = &output[output_count];
				output_count++;

				// It's important that we generate t in the same direction (inside plane to
				// outside plane) here for both cases, otherwise we can end up with cracks in
				// our geometry between two neighboring polygons due to floating point error.

				// TODO(mal): create interpolation functions for vec3, vec4

				if (end_vertex_inside_plane) {
					// end to start
					float t = dist_end_vertex_to_plane / (dist_end_vertex_to_plane - dist_start_vertex_to_plane);
					// NOTE(mal): Clamp to [0, 1] for the cases where a vertex lies directly on the
					// plane and floating point error causes the denominator to be veeeery close to
					// 0, meaning the computed distance explodes way past the bounds of our frustum.
					if (t < 0.0f) t = 0.0f;
					if (t > 1.0f) t = 1.0f;
					clip_vertex->position.x = end_vertex->position.x - t * (end_vertex->position.x - start_vertex->position.x);
					clip_vertex->position.y = end_vertex->position.y - t * (end_vertex->position.y - start_vertex->position.y);
					clip_vertex->position.z = end_vertex->position.z - t * (end_vertex->position.z - start_vertex->position.z);
					clip_vertex->position.w = end_vertex->position.w - t * (end_vertex->position.w - start_vertex->position.w);
					clip_vertex->color      = end_vertex->color - t * (end_vertex->color - start_vertex->color);
					clip_vertex->tx_u       = end_vertex->tx_u - t * (end_vertex->tx_u - start_vertex->tx_u);
					clip_vertex->tx_v       = end_vertex->tx_v - t * (end_vertex->tx_v - start_vertex->tx_v);
				} else {
					// start to end
					float t = dist_start_vertex_to_plane / (dist_start_vertex_to_plane - dist_end_vertex_to_plane);
					// NOTE(mal): Clamp to [0, 1] for the cases where a vertex lies directly on the
					// plane and floating point error causes the denominator to be veeeery close to
					// 0, meaning the computed distance explodes way past the bounds of our frustum.
					if (t < 0.0f) t = 0.0f;
					if (t > 1.0f) t = 1.0f;
					clip_vertex->position.x = start_vertex->position.x + t * (end_vertex->position.x - start_vertex->position.x);
					clip_vertex->position.y = start_vertex->position.y + t * (end_vertex->position.y - start_vertex->position.y);
					clip_vertex->position.z = start_vertex->position.z + t * (end_vertex->position.z - start_vertex->position.z);
					clip_vertex->position.w = start_vertex->position.w + t * (end_vertex->position.w - start_vertex->position.w);
					clip_vertex->color      = start_vertex->color + t * (end_vertex->color - start_vertex->color);
					clip_vertex->tx_u       = start_vertex->tx_u + t * (end_vertex->tx_u - start_vertex->tx_u);
					clip_vertex->tx_v       = start_vertex->tx_v + t * (end_vertex->tx_v - start_vertex->tx_v);
				}
			}
		}

		// The end_vertex is going to be the start_vertex of next iteration's edge
		start_vertex = end_vertex;
		dist_start_vertex_to_plane = dist_end_vertex_to_plane;
		start_vertex_inside_plane  = end_vertex_inside_plane;
	}

	return output_count;
}

// Our normals assume a CW winding order in a coordinate system where +Y is down.
// i.e. our normals point inward to our triangles when in screen space.
// For some reason I couldn't get the standard top-left rule working to not produce gaps, 
// but this version where we nudge "nudge the bottom and right edges over by a pixel to
// allow inclusion of pixels that might be along a shared edge there" seems to work.
// This may cause slight overdraw if surfaces are transparent. Might have to visit later.
// FIXME(mal): If overdraw on shared edges of transparent polygons, will have to fix this
// properly by using the true top-left rule. Apparently for the true top-left rule to work,
// the edge function will have to evaluate to exactly 0.0f for shared pixel edges which only
// holds when shared vertices are bit-identical. If there's any floating point drift then
// adding a bias of -1 to non-top-or-right edges will create gaps due to underdrawing.
float bottom_right_bias(float triangle_edge_nx, float triangle_edge_ny) {
	bool is_bottom = (triangle_edge_nx == 0.0f) && (triangle_edge_ny < 0.0f);
	bool is_right  = triangle_edge_nx < 0.0f;
	float result   = (is_bottom || is_right) ? 1.0f : 0.0f;
	return result;
}

EXPORT void game_init(GameMemory *memory, int initial_width, int initial_height) {
	ASSERT(memory->debug_platform_read_entire_file);
	ASSERT(memory->debug_platform_free_entire_file);

	GameState *game_state = (GameState *)memory->storage;

	// Square in CW winding order
	// 0: Square bottom left
	game_state->square.local_vertices[0] = (Vertex){
		.position = { .x = -1, .y = -1, .w = 1 },
		.color = -1,
		.tx_u = 0.0f, .tx_v = 1.0f,
	};
	// 1: Square top left
	game_state->square.local_vertices[1] = (Vertex){
		.position = { .x = -1, .y = 1, .w = 1 },
		.color = -1,
		.tx_u = 0.0f, .tx_v = 0.0f,
	};
	// 2: Square top right
	game_state->square.local_vertices[2] = (Vertex){
		.position = { .x = 1, .y = 1, .w = 1 },
		.color = -1,
		.tx_u = 1.0f, .tx_v = 0.0f,
	};
	// 3: Square bottom right
	game_state->square.local_vertices[3] = (Vertex){
		.position = { .x = 1, .y = -1, .w = 1 },
		.color = -1,
		.tx_u = 1.0f, .tx_v = 1.0f,
	};
	
	game_state->square.world_position = (Vec3){ .x = 0.0f, .y = 0.0f, .z = 5.0f };
	game_state->square.scale = 75.0f;

	game_state->camera_world_position = (Vec3){0};
	game_state->camera_world_orientation = (Mat3x3){0};

	char *tga_data = (char *)memory->debug_platform_read_entire_file("../testtexture.tga");
	TGA_Header *tga_header = (TGA_Header *)tga_data;
	ASSERT(tga_header->bitsperpixel == 32);
	ASSERT_MSG(tga_header->datatypecode == 2, "Test texture TGA is not RGB!");
	game_state->texture_height = tga_header->height;
	game_state->texture_width  = tga_header->width;
	game_state->texture_pixels = (uint32_t *)(tga_data + sizeof(TGA_Header));

	game_state->render_wireframe = 0;
}

EXPORT void game_render(GameMemory *memory, GameOffscreenBuffer *offscreen_buffer) {
	GameState *game_state = (GameState *)memory->storage;

	Mat4x4 world_to_opengl_coordinates = {
		.rows = {
			[0][0] = 1,
			[1][1] = 1,
			[2][2] = -1, // flip z so that +z points out of screen
			[3][3] = 1,
		}
	};

	//////////////////////////////
	// CREATING CAMERA_TO_WORLD
	//////////////////////////////
	// TODO(mal): Once I swich to using Mat4x4 transforms instead of separate orientations and
	// position structures, create a mat4x4_inverse function that can calculate what I'm doing below
	// more efficiently!
	// NOTE(mal): The inverse of an orthogonal matrix (which rotation matrices are), is the same as
	// its transpose! (An orthogonal matrix is a square matrix whose rows and coumns are orthonormal
	// vectors e.g. perpendicual to each other and have a length of one).
	// Compute orientation inverse R^-1
	Mat3x3 world_to_camera_orientation = mat3x3_transpose(game_state->camera_world_orientation);
	// Compute translation inverse t^-1 = -R^-1 * t
	Vec3 world_to_camera_translation = vec3_negate(mult_mat3x3_vec3(world_to_camera_orientation, game_state->camera_world_position));
	// NOTE(mal): See "Essential Math" 4.3.6 on p136 for why we compute the above this way.
	Mat4x4 world_to_camera = {0};
	// Copying orientation
	for (int r = 0; r < 3; r++) {
		for (int c = 0; c < 3; c++) {
			world_to_camera.rows[r][c] = world_to_camera_orientation.rows[r][c];
		}
	}
	// Copying translation
	world_to_camera.rows[0][3] = world_to_camera_translation.x;
	world_to_camera.rows[1][3] = world_to_camera_translation.y;
	world_to_camera.rows[2][3] = world_to_camera_translation.z;
	world_to_camera.rows[3][3] = 1; // For homogeneous coordinates

	// Translate from world RHS (X+ right, Y+ up, Z+ into) to OpenGL-style RHS
	world_to_camera = mult_mat4x4_mat4x4(world_to_opengl_coordinates, world_to_camera);

	float vert_fov = DEGREES_TO_RADIANS(90.0f);
	float near = 1.0f;
	float far = 200.0f;
	float aspect_ratio = (float)offscreen_buffer->width / (float)offscreen_buffer->height;
	// NOTE(mal): d here is the distance from view origin to the plane onto which we're projecting
	// R3 points down to R2. Its value is really somewhat arbitrary since any point along the view
	// space z axis will project to the same point on our 2D image plane (given that d > 0... d < 0
	// will result in a flipped image). However, setting it to the cotangent of half the vertical
	// fov does mean that any point which intersects at the top/bottom of our view frustum will map
	// the Y component to 1/-1, which is very convenient for mapping into a unit cube (NDC space)
	// which we'll use for clipping and eventually conversion to screen space. It just means that we
	// don't have to do an extra division later to normalize our coordinates to be in the range [-1, 1].
	// See Essential Math 7.3.5 p246 for this derivation of d.
	// Another insight: we are solving for d such that the half height of our 2D image plane is 1
	// unit. This is why we divide our vert_fov in half -- because we're just considering one half of
	// our Y frustum. Also recall that coordinates in NDC space are in range [-1, 1].
	float d = (float)(1.0f / tan(vert_fov / 2.0f));

	// NOTE(mal): A pure form of the perspective projection matrix just performs a division of x and
	// y coordinates by z (using perspective divide with w). More useful forms actually will also
	// scale x,y into NDC range via d and the aspect ratio, and will also remap Z into NDC depth
	// range via the near and far plane values.
	// NOTE(mal): If we were not using d and aspect_ratio we would do X component NDC mapping using
	// left and right values of the frustum planes similar to how we're using near and far.

	// Set up OpenGL-style perspective transform matrix
	Mat4x4 perspective = {
		.rows = {
			// NOTE(mal): We account for the fact here that pixels aren't necessarily square and that the
			// horizontal FOV may differ from the vertical FOV.
			{ d / aspect_ratio, 0, 0,                              0,                                    },
			{ 0,                d, 0,                              0,                                    },
			// Map depth [-near, -far] (because we look down -z in RHS view space) to [-1, 1].
			// NOTE(mal): That this does NOT clip or cull vertices! If a vertex depth < near or depth > far
			// then it just gets assigned an NDC Z value of -1 or 1, respectively.
			{ 0,                0, -((far + near) / (far - near)), -((2.0f * far * near) / (far - near)) },
			{ 0,                0, -1,                             0                                     },
		}
	};

	// NDC ranges from [-1, -1] to [1, 1]
	// We want to map to our screen, which is from [0, 0] to [width, height], and where +y is down

	float half_width = offscreen_buffer->width / 2.0f;
	float half_height = offscreen_buffer->height / 2.0f;
	float screen_offset_x = 0.0f;
	float screen_offset_y = 0.0f;
	// z is a special case since we want to use it for depth testing later.
	// We want it to range from [0, depth] where depth usually = 1.
	float depth = 1.0f;
	float half_depth = depth / 2;
	Mat4x4 ndc_to_screen = {
		.rows = {
			{ half_width, 0,            0,          half_width + screen_offset_x  },
			{ 0,          -half_height, 0,          half_height + screen_offset_y },
			{ 0,          0,            half_depth, half_depth                    },
			{ 0,          0,            0,          1                             },
		}
	};

	uint32_t *pixels = (uint32_t *)offscreen_buffer->memory;

	// NOTE(mal): MUST be powers of 2!
	const int RASTER_TILE_WIDTH  = 16;
	const int RASTER_TILE_HEIGHT = 16;
	#define CLEAR_COLOR 0x00000000
	#define RASTER_TILE_COLOR 0x00440011
	for (int r = 0; r < offscreen_buffer->height; r++) {
		const int row_offset = r * offscreen_buffer->width;
		for (int c = 0; c < offscreen_buffer->width; c++) {
			uint32_t *pixel = &pixels[c + row_offset];
			*pixel = CLEAR_COLOR;
			if (game_state->render_raster_tile_state == RENDER_RASTER_TILES_BELOW) {
				if (!(c & (RASTER_TILE_WIDTH - 1)) || !(r & (RASTER_TILE_HEIGHT - 1))) {
					*pixel = RASTER_TILE_COLOR;
				}
			}
		}
	}

	// FIXME(mal): Just construct/use/whatever a 4x4 transformation matrix for local_to_world
	Vertex transformed_vertices[4]; // for the quad, will need to use dynamic mem management for
									// actual stuff
	{
		float r = DEGREES_TO_RADIANS(game_state->rotation_y_degrees);
		float sin_r = sin(r), cos_r = cos(r);
		for (int v_i = 0; v_i < 4; v_i++) {
			Vertex v = game_state->square.local_vertices[v_i];

			// Scaling
			for (int i = 0; i < 3; i++) v.position.elements[i] *= game_state->square.scale;
			// Rotating about Y axis
			float x = v.position.x, z = v.position.z;
			v.position.x = z*sin_r + x*cos_r;
			v.position.z = z*cos_r - x*sin_r;
			// Translating
			for (int i = 0; i < 3; i++) v.position.elements[i] += game_state->square.world_position.elements[i];
			// world --> view --> homogeneous clip
			v.position = mult_mat4x4_vec4(perspective, mult_mat4x4_vec4(world_to_camera, v.position));

			transformed_vertices[v_i] = v;
		}
	}

	//////////////////////////////
	// BEGIN CLIPPING
	//////////////////////////////
	// Feed the entire polygon through the clipping pipeline rather
	// than clipping individual triangles within the polygon.
	// This will ensure that positions of vertices that create shared
	// edges in the output triangle fan will always be identical, thus
	// ensuring that no under- or over-draw will occur (provided our
	// edge constant bias is set up properly).

	// NOTE(mal): I believe that each plane can add at most one generated vertex, which means
	// that if we start with 4 and clip against 6 frustum planes then we will end up with at
	// most 10 vertices (TODO figure out number of triangles generated with 10 vertices in triangle
	// fan configuration)
	Vertex clip_buffer_a[10] = {
		transformed_vertices[0],
		transformed_vertices[1],
		transformed_vertices[2],
		transformed_vertices[3]
	};
	Vertex clip_buffer_b[10];
	Vertex *input = clip_buffer_a;
	Vertex *output = clip_buffer_b;
	size_t input_count, output_count;
	#define SWAP_POINTERS(Type, a, b) {\
		Type *tmp = (a);\
		(a) = (b);\
		(b) = tmp;\
	}

	// clip against the six frustum planes
	// NOTE(mal): See "Essential Math" 7.4.3 and 7.4.4 about clipping

	// clip +x
	input_count  = 4;
	output_count = clip_sutherland_hodgeman(0, -1, input, input_count, output);
	// clip -x
	SWAP_POINTERS(Vertex, input, output);
	input_count  = output_count;
	output_count = clip_sutherland_hodgeman(0, +1, input, input_count, output);
	// clip +y
	SWAP_POINTERS(Vertex, input, output);
	input_count  = output_count;
	output_count = clip_sutherland_hodgeman(1, -1, input, input_count, output);
	// clip -y
	SWAP_POINTERS(Vertex, input, output);
	input_count  = output_count;
	output_count = clip_sutherland_hodgeman(1, +1, input, input_count, output);
	// clip +z
	SWAP_POINTERS(Vertex, input, output);
	input_count  = output_count;
	output_count = clip_sutherland_hodgeman(2, -1, input, input_count, output);
	// clip -z
	SWAP_POINTERS(Vertex, input, output);
	input_count  = output_count;
	output_count = clip_sutherland_hodgeman(2, +1, input, input_count, output);

	Vertex *clipped_vertices     = output;
	size_t  clipped_vertex_count = output_count;

	//////////////////////////////
	// END CLIPPING
	//////////////////////////////

	// Take all clipped vertices from homogeneous clip space --> NDC --> screen
	// Retain depth values for each vertex.
	// TODO(mal): Should we just store the depth on the vertex itself?
	float outer_reciprocal_depth[clipped_vertex_count];
	for (int i = 0; i < clipped_vertex_count; i++) {
		// NOTE(mal): After perspective projection and before perspective divide, the w
		// component IS our view-space Z (depth) coordinate!
		float reciprocal_w = 1.0f / clipped_vertices[i].position.w;
		outer_reciprocal_depth[i] = reciprocal_w;
		// perspective divide: homogeneous clip --> NDC
		clipped_vertices[i].position.x *= reciprocal_w;
		clipped_vertices[i].position.y *= reciprocal_w;
		clipped_vertices[i].position.z *= reciprocal_w;
		clipped_vertices[i].position.w *= reciprocal_w;
		// NDC --> screen
		clipped_vertices[i].position = mult_mat4x4_vec4(ndc_to_screen, clipped_vertices[i].position);
	}

	size_t triangle_fan_center_index = 0;
	for (int i = 2; i < clipped_vertex_count; i++) {
		// Grab our triangle from the fan generated by clipping. Also fix the winding order that
		// we're about to screw up by transforming from homogenous clip --> NDC --> screen.
		// TODO(mal): Stop copying, but then also need to make sure that we don't update the
		// same vertices over and over again with perspective divides!
		// OR maybe just change the edge function to assume CCW order instead of CW? Then we
		// don't have to change the order of our vertices here.
		Vertex triangle[3] = { clipped_vertices[i], clipped_vertices[i - 1], clipped_vertices[triangle_fan_center_index] };
		float reciprocal_depth[3] = { outer_reciprocal_depth[i], outer_reciprocal_depth[i - 1], outer_reciprocal_depth[triangle_fan_center_index] };

		// Compute the AABB of the triangle so that we don't have to loop over the entire buffer
		// every time regardless of the size of the triangle.
		// TODO(mal): Maybe create to_vec3_max(Vec3 a, Vec3 b) and to_vec3_min(Vec3 a, Vec3 b)
		// functions and call those here instead?
		int triangle_xmax =
			triangle[0].position.x > triangle[1].position.x
			? (triangle[0].position.x > triangle[2].position.x ? triangle[0].position.x : triangle[2].position.x)
			: (triangle[1].position.x > triangle[2].position.x ? triangle[1].position.x : triangle[2].position.x);
		int triangle_xmin =
			triangle[0].position.x < triangle[1].position.x
			? (triangle[0].position.x < triangle[2].position.x ? triangle[0].position.x : triangle[2].position.x)
			: (triangle[1].position.x < triangle[2].position.x ? triangle[1].position.x : triangle[2].position.x);
		int triangle_ymax =
			triangle[0].position.y > triangle[1].position.y
			? (triangle[0].position.y > triangle[2].position.y ? triangle[0].position.y : triangle[2].position.y)
			: (triangle[1].position.y > triangle[2].position.y ? triangle[1].position.y : triangle[2].position.y);
		int triangle_ymin =
			triangle[0].position.y < triangle[1].position.y
			? (triangle[0].position.y < triangle[2].position.y ? triangle[0].position.y : triangle[2].position.y)
			: (triangle[1].position.y < triangle[2].position.y ? triangle[1].position.y : triangle[2].position.y);

		// if (triangle_xmin < 0) triangle_xmin = 0;
		// if (triangle_xmax > offscreen_buffer->width) triangle_xmax = offscreen_buffer->width;
		// if (triangle_ymin < 0) triangle_ymin = 0;
		// if (triangle_ymax > offscreen_buffer->height) triangle_ymax = offscreen_buffer->height;

		ASSERT(triangle_xmin >= 0);
		ASSERT(triangle_xmax <= offscreen_buffer->width);
		ASSERT(triangle_ymin >= 0);
		ASSERT(triangle_ymax <= offscreen_buffer->height);

		// NOTE(mal): Does NOT account for winding order so at the moment we always render even if
		// the triange is facing away from us.
		if (game_state->render_wireframe) {
			draw_line_2d(
				pixels, offscreen_buffer->width, offscreen_buffer->height,
				triangle[0].position.x, triangle[0].position.y, triangle[1].position.x, triangle[1].position.y
			);
			draw_line_2d(
				pixels, offscreen_buffer->width, offscreen_buffer->height,
				triangle[1].position.x, triangle[1].position.y, triangle[2].position.x, triangle[2].position.y
			);
			draw_line_2d(
				pixels, offscreen_buffer->width, offscreen_buffer->height,
				triangle[2].position.x, triangle[2].position.y, triangle[0].position.x, triangle[0].position.y
			);
		}

		if (game_state->skip_rasterization) {
			continue;
		}

		//////////////////////////////
		// TRIANGLE SETUP
		//////////////////////////////

		// Compute the topleft points of the tiles at the extremities
		// (the topleft-most tile and the bottomright-most tile) of our
		// triangle's AABB.
		int bottomleft_tile_bottomleft_x = triangle_xmin - (triangle_xmin & (RASTER_TILE_WIDTH  - 1));
		int bottomleft_tile_bottomleft_y = triangle_ymin - (triangle_ymin & (RASTER_TILE_HEIGHT - 1));
		int topright_tile_bottomleft_x   = triangle_xmax - (triangle_xmax & (RASTER_TILE_WIDTH  - 1));
		int topright_tile_bottomleft_y   = triangle_ymax - (triangle_ymax & (RASTER_TILE_HEIGHT - 1));

		// TODO(mal): Set up for tiled rasterization
		// https://fileadmin.cs.lth.se/graphics/research/papers/2005/cr/conservative.pdf
		// Also see "Realtime Rendering" p996

		// Setting up per-edge values
		// NOTE(mal): The barycentric weight for a given vertex in the triangle is related to
		// the area of the subtriangle defined by the test point p and the edge OPPOSITE the
		// given vertex.
		//
		// NOTE(mal): Avoiding per-pixel edge function computation. This should be possible because the
		// edge function is linear. Thus,
		// E(x + 1, y) = E(x, y) + dY
		// E(x, y + 1) = E(x, y) - dX
		// https://www.cs.drexel.edu/~deb39/Classes/Papers/comp175-06-pineda.pdf
		// Taking the algorithm for stepping from here: https://www.youtube.com/watch?v=k5wtuKWmV48
		// at chapter "Avoiding Computing the Edge Function Per-Pixel".

		// edge v0 to v1
		float v0v1_nx =  (triangle[1].position.y - triangle[0].position.y);
		float v0v1_ny = -(triangle[1].position.x - triangle[0].position.x);
		float v0v1_c =
			// -N dot edge_start_vertex
			  (-v0v1_nx * triangle[0].position.x)
			+ (-v0v1_ny * triangle[0].position.y)
			+ bottom_right_bias(v0v1_nx, v0v1_ny);
		float d_w2_col = v0v1_nx;
		float d_w2_row = v0v1_ny;
		Vec2 tile_offset_most_inside_v0v1 = {
			.x = v0v1_nx < 0.0f ? 0.0f : RASTER_TILE_WIDTH,
			.y = v0v1_ny < 0.0f ? 0.0f : RASTER_TILE_HEIGHT
		};
		Vec2 tile_offset_most_outside_v0v1 = {
			.x = v0v1_nx < 0.0f ? RASTER_TILE_WIDTH : 0.0f,
			.y = v0v1_ny < 0.0f ? RASTER_TILE_HEIGHT : 0.0f,
		};

		// edge v1 to v2
		float v1v2_nx =  (triangle[2].position.y - triangle[1].position.y);
		float v1v2_ny = -(triangle[2].position.x - triangle[1].position.x);
		float v1v2_c =
			  (-v1v2_nx * triangle[1].position.x)
			+ (-v1v2_ny * triangle[1].position.y)
			+ bottom_right_bias(v1v2_nx, v1v2_ny);
		float d_w0_col = v1v2_nx;
		float d_w0_row = v1v2_ny;
		Vec2 tile_offset_most_inside_v1v2 = {
			.x = v1v2_nx < 0.0f ? 0.0f : RASTER_TILE_WIDTH,
			.y = v1v2_ny < 0.0f ? 0.0f : RASTER_TILE_HEIGHT
		};
		Vec2 tile_offset_most_outside_v1v2 = {
			.x = v1v2_nx < 0.0f ? RASTER_TILE_WIDTH : 0.0f,
			.y = v1v2_ny < 0.0f ? RASTER_TILE_HEIGHT : 0.0f,
		};

		// edge v2 to v0
		float v2v0_nx =  (triangle[0].position.y - triangle[2].position.y);
		float v2v0_ny = -(triangle[0].position.x - triangle[2].position.x);
		float v2v0_c =
			  (-v2v0_nx * triangle[2].position.x)
			+ (-v2v0_ny * triangle[2].position.y)
			+ bottom_right_bias(v2v0_nx, v2v0_ny);
		float d_w1_col = v2v0_nx;
		float d_w1_row = v2v0_ny;
		Vec2 tile_offset_most_inside_v2v0 = {
			.x = v2v0_nx < 0.0f ? 0.0f : RASTER_TILE_WIDTH,
			.y = v2v0_ny < 0.0f ? 0.0f : RASTER_TILE_HEIGHT
		};
		Vec2 tile_offset_most_outside_v2v0 = {
			.x = v2v0_nx < 0.0f ? RASTER_TILE_WIDTH : 0.0f,
			.y = v2v0_ny < 0.0f ? RASTER_TILE_HEIGHT : 0.0f,
		};

		//////////////////////////////
		// RASTERIZATION (TILED)
		//////////////////////////////
		// TODO(mal): For future optimization, might be able to add another layer of tiling and
		// then apply vectorization.
		// See https://www.cs.cmu.edu/afs/cs/academic/class/15869-f11/www/readings/abrash09_lrbrast.pdf
		//     ^^^ Michael Abrash on the Larabee rasterizer.
		// It also has a great (implicit) explanation of what our barycentric weight deltas
		// actually are. (If I understand right, those values are just the amounts that the
		// edge function changes when stepping by some amount in the given direction (i.e.
		// +row, +col)).

		for (
			int tile_min_y = bottomleft_tile_bottomleft_y;
			tile_min_y <= topright_tile_bottomleft_y;
			tile_min_y += RASTER_TILE_HEIGHT
		)
		{
			for (
				int tile_min_x = bottomleft_tile_bottomleft_x;
				tile_min_x <= topright_tile_bottomleft_x;
				tile_min_x += RASTER_TILE_WIDTH
			)
			{

				bool is_tile_fully_outside_v0v1 = edge_function_2(
					v0v1_nx, v0v1_ny,
					tile_min_x + tile_offset_most_inside_v0v1.x,
					tile_min_y + tile_offset_most_inside_v0v1.y,
					v0v1_c
				) < 0.0f;
				bool is_tile_fully_outside_v1v2 = edge_function_2(
					v1v2_nx, v1v2_ny,
					tile_min_x + tile_offset_most_inside_v1v2.x,
					tile_min_y + tile_offset_most_inside_v1v2.y,
					v1v2_c
				) < 0.0f;
				bool is_tile_fully_outside_v2v0 = edge_function_2(
					v2v0_nx, v2v0_ny,
					tile_min_x + tile_offset_most_inside_v2v0.x,
					tile_min_y + tile_offset_most_inside_v2v0.y,
					v2v0_c
				) < 0.0f;
				bool is_tile_fully_outside_triangle =
					   is_tile_fully_outside_v0v1
					|| is_tile_fully_outside_v1v2
					|| is_tile_fully_outside_v2v0;
				if (is_tile_fully_outside_triangle) {
					continue;
				}

				// A tile is fully inside an edge if its most outside vertex is inside the edge.
				bool is_tile_fully_inside_v0v1 = edge_function_2(
					v0v1_nx, v0v1_ny,
					tile_min_x + tile_offset_most_outside_v0v1.x,
					tile_min_y + tile_offset_most_outside_v0v1.y,
					v0v1_c
				) >= 0.0f;
				bool is_tile_fully_inside_v1v2 = edge_function_2(
					v1v2_nx, v1v2_ny,
					tile_min_x + tile_offset_most_outside_v1v2.x,
					tile_min_y + tile_offset_most_outside_v1v2.y,
					v1v2_c
				) >= 0.0f;
				bool is_tile_fully_inside_v2v0 = edge_function_2(
					v2v0_nx, v2v0_ny,
					tile_min_x + tile_offset_most_outside_v2v0.x,
					tile_min_y + tile_offset_most_outside_v2v0.y,
					v2v0_c
				) >= 0.0f;
				bool is_tile_fully_inside_triangle =
					   is_tile_fully_inside_v0v1
					&& is_tile_fully_inside_v1v2
					&& is_tile_fully_inside_v2v0;

				float w0_row = edge_function_2(
					v1v2_nx, v1v2_ny,
					tile_min_x + 0.5f, tile_min_y + 0.5f,
					v1v2_c
				);
				float w1_row = edge_function_2(
					v2v0_nx, v2v0_ny,
					tile_min_x + 0.5f, tile_min_y + 0.5f,
					v2v0_c
				);
				float w2_row = edge_function_2(
					v0v1_nx, v0v1_ny,
					tile_min_x + 0.5f, tile_min_y + 0.5f,
					v0v1_c
				);

				int tile_max_x = tile_min_x + RASTER_TILE_WIDTH;
				if (tile_max_x >= offscreen_buffer->width) tile_max_x = offscreen_buffer->width;
				int tile_max_y = tile_min_y + RASTER_TILE_HEIGHT;
				if (tile_max_y >= offscreen_buffer->height) tile_max_y = offscreen_buffer->height;

				// Loop over the pixels in the tile
				for (int row = tile_min_y; row < tile_max_y; row++) {
					float w0 = w0_row;
					float w1 = w1_row;
					float w2 = w2_row;
					for (int col = tile_min_x; col < tile_max_x; col++) {
						// TODO(mal): two separate loops:
						// - if tile fully inside triangle, no weight check
						// - if partially inside, weight check
						if (is_tile_fully_inside_triangle || (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f)) {
							// TODO(mal): Rename some of this stuff. Names are taken from Realtime
							// Rendering. See p1000 for perspective-correct barycentric interpolation.
							// I believe here we're essentially foreshortening our barycentric coordinates.
							float f0 = w0 * reciprocal_depth[0];
							float f1 = w1 * reciprocal_depth[1];
							float f2 = w2 * reciprocal_depth[2];
							float perspective_reciprocal_area = 1.0f / (f0 + f1 + f2);

							// TEXTURING
							float tx_u = (f0 * triangle[0].tx_u + f1 * triangle[1].tx_u + f2 * triangle[2].tx_u) * perspective_reciprocal_area;
							float tx_v = (f0 * triangle[0].tx_v + f1 * triangle[1].tx_v + f2 * triangle[2].tx_v) * perspective_reciprocal_area;
							unsigned tx_x = (unsigned)(tx_u * game_state->texture_width);
							unsigned tx_y = (unsigned)(tx_v * game_state->texture_height);
							unsigned texel_index = tx_x + tx_y * game_state->texture_width;
							// FIXME(mal): Need to detect machine's endianness and extract the bits
							// properly. Check the 32-bit color format of TGA (or any other texture
							// file we may load). I believe it's BGRA.
							uint32_t texel_tga_color = game_state->texture_pixels[texel_index];
							uint8_t  texel_red       = (texel_tga_color & 0x00FF0000) >> 16;
							uint8_t  texel_green     = (texel_tga_color & 0x0000FF00) >> 8;
							uint8_t  texel_blue      = texel_tga_color & 0x000000FF;
							uint32_t texel_color     = (texel_red << 16) | (texel_green << 8) | texel_blue;
							pixels[col + row * offscreen_buffer->width] = texel_color;

							// #define U32_R8(x) (((x) & (0xFF << 16)) >> 16)
							// #define U32_G8(x) (((x) & (0xFF << 8)) >> 8)
							// #define U32_B8(x) ((x) & 0xFF)
							// uint8_t color_red   = (f0 * U32_R8(vs[0].color) + f1 * U32_R8(vs[1].color) + f2 * U32_R8(vs[2].color)) * perspective_reciprocal_area;
							// uint8_t color_green = (f0 * U32_G8(vs[0].color) + f1 * U32_G8(vs[1].color) + f2 * U32_G8(vs[2].color)) * perspective_reciprocal_area;
							// uint8_t color_blue  = (f0 * U32_B8(vs[0].color) + f1 * U32_B8(vs[1].color) + f2 * U32_B8(vs[2].color)) * perspective_reciprocal_area;
							// pixels[col + row * offscreen_buffer->width] =
							// 	color_red << 16
							// 	| color_green << 8
							// 	| color_blue;

						}

						w0 += d_w0_col;
						w1 += d_w1_col;
						w2 += d_w2_col;
					}

					w0_row += d_w0_row;
					w1_row += d_w1_row;
					w2_row += d_w2_row;
				}
			}
		}
	}

	if (game_state->render_raster_tile_state == RENDER_RASTER_TILES_ONTOP) {
		// NOTE(mal): MUST be powers of 2!
		for (int r = 0; r < offscreen_buffer->height; r++) {
			const int row_offset = r * offscreen_buffer->width;
			for (int c = 0; c < offscreen_buffer->width; c++) {
				uint32_t *pixel = &pixels[c + row_offset];
				if (!(c & (RASTER_TILE_WIDTH - 1)) || !(r & (RASTER_TILE_HEIGHT - 1))) {
					*pixel = RASTER_TILE_COLOR;
				}
			}
		}
	}

}

EXPORT void game_update(GameMemory *memory, GameInput *input) {
	GameState *game_state = (GameState *)memory->storage;

	// TODO(mal): Local and world space (2D) have Y pointing up but screen space has Y pointing down.
	// Need to include a transformation step that flips the direction of our Y axis!

	game_state->camera_world_orientation = mat3x3_create_rotation_y(DEGREES_TO_RADIANS(0.0f));
	game_state->square.scale = 10.0f;
	game_state->square.world_position = (Vec3){ .z = game_state->square.scale * 2.0f };

	// Rotate
	const float rot_speed = 1.0f;

	if (input->keys[GAME_KEY_J].is_down) game_state->rotation_y_degrees += rot_speed;
	if (input->keys[GAME_KEY_L].is_down) game_state->rotation_y_degrees -= rot_speed;

	if (game_state->rotation_y_degrees > 180.0f) game_state->rotation_y_degrees -= 360.0f;
	if (game_state->rotation_y_degrees < -180.0f) game_state->rotation_y_degrees += 360.0f;

	const float move_speed = 1.0f;

	if (input->keys[GAME_KEY_W].is_down) game_state->camera_world_position.z += move_speed;
	if (input->keys[GAME_KEY_S].is_down) game_state->camera_world_position.z -= move_speed;
	if (input->keys[GAME_KEY_A].is_down) game_state->camera_world_position.x -= move_speed;
	if (input->keys[GAME_KEY_D].is_down) game_state->camera_world_position.x += move_speed;

	#define PRESSED_THIS_FRAME(game_key) input->keys[(game_key)].is_down && !input->keys[(game_key)].was_down
	if (PRESSED_THIS_FRAME(GAME_KEY_F1)) {
		game_state->skip_rasterization = !game_state->skip_rasterization;
	}
	if (PRESSED_THIS_FRAME(GAME_KEY_F2)) {
		game_state->render_wireframe = !game_state->render_wireframe;
	}
	if (PRESSED_THIS_FRAME(GAME_KEY_F3)) {
		RenderRasterTileState next_state;
		switch (game_state->render_raster_tile_state) {
			case RENDER_RASTER_TILES_OFF:
				next_state = RENDER_RASTER_TILES_BELOW;
				break;
			case RENDER_RASTER_TILES_BELOW:
				next_state = RENDER_RASTER_TILES_ONTOP;
				break;
			case RENDER_RASTER_TILES_ONTOP:
				next_state = RENDER_RASTER_TILES_OFF;
				break;
		}
		game_state->render_raster_tile_state = next_state;
	}
}
