// NOTE(mal) ON 3D COORDINATE SYSTEM:
// World space: RHS +X Right, +Y Up, +Z Forward
// OGL-style camera space and projection matrix

// FIXME's and TODO's
//
// Start using 4x4 matrices for all transforms instead of having separate 3x3
// rotations and vec3 translations.
//
// Review "Essential Math" 4.3.6 about application of affine transformations.
// Actually, need to review what the definition of an affine transformation is.
//
// Review the logic behind setting up perspective and ndc-to-screen matrices so
// that I can do it for any arbitrary coordinate system.
//
// Move all the matrix transformation setup stuff into initialization and find a
// way to only recompute when needed (e.g. window size changed).
// Maybe need to export a new function for the platform layer to load?
//
// Fiddle around with using quaternions for rotations and whatnot.
//
// Maybe introduce the following convention for all my math functions:
// - In-place modifications follow naming convention e.g.: vec3_normalized
// - Functions that do NOT modify in-pace follow convetion: vec3_to_normalized
// Example:
// void vec3_normalized(Vec3 *v) { /* ... */ } // in-place modification
// Vec3 vec3_to_normalized(const Vec3 *v) { /* ... */ } // no modification
// void vec3_add_vec3(Vec3 *v);
// void vec3_to_add_vec3(Vec3 a, Vec3 b); Iunno, workshop it

#include <stdint.h>
#include <math.h>
#include "platform.h"
// NOTE(mal): Only on Linux, probably wrap this in a #if (something)
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

typedef struct Vec2 {
	union {
		float elements[2];
		struct { float x; float y; };
	};
} Vec2;

typedef struct Vec3 {
	union {
		float elements[3];
		struct { float x; float y; float z; };
	};
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
typedef struct Vec4 {
	union {
		float elements[4];
		struct {
			float x;
			float y;
			float z;
			float w;
		};
	};
} Vec4;

typedef struct Mat3x3 {
	union {
		float elements[9];
		float rows[3][3];
	};
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

typedef struct Mat4x4 {
	union {
		float elements[16];
		float rows[4][4];
	};
} Mat4x4;

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

typedef struct Point { int x; int y; } Point;
size_t bresenham(Point *points, size_t points_len, int x0, int y0, int x1, int y1) {
	int dx = abs(x1 - x0);
	int dy = -abs(y1 - y0);
	int sx = x0 < x1 ? 1 : -1;
	int sy = y0 < y1 ? 1 : -1;
	int err = dx + dy;

	size_t point_index = 0;
	size_t generated = 0;
	while (point_index < points_len) {
		Point p = { .x = x0, .y = y0 };
		points[point_index] = p;
		point_index++;
		generated++;

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

	return generated;
}

typedef struct Vertex {
	Vec3 position;
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

// NOTE(mal): Vec3 but we're only checking x and y coords. Maybe it's best to just pass the
// components in directlry?
float edge_function(Vec3 v0, Vec3 v1, Vec3 p) {
	float result = (p.x - v0.x) * (v1.y - v0.y) - (p.y - v0.y) * (v1.x - v0.x);
	return result;
}

EXPORT void game_init(GameMemory *memory, int initial_width, int initial_height) {
	ASSERT(memory->debug_platform_read_entire_file);
	ASSERT(memory->debug_platform_free_entire_file);

	GameState *game_state = (GameState *)memory->storage;

	// 0: Square bottom left
	game_state->square.local_vertices[0] = (Vertex){
		.position = { .x = -1, .y = -1 },
		.tx_u = 0.0f, .tx_v = 1.0f,
	};
	// 1: Square top left
	game_state->square.local_vertices[1] = (Vertex){
		.position = { .x = -1, .y = 1 },
		.tx_u = 0.0f, .tx_v = 0.0f,
	};
	// 2: Square bottom right
	game_state->square.local_vertices[2] = (Vertex){
		.position = { .x = 1, .y = -1 },
		.tx_u = 1.0f, .tx_v = 1.0f,
	};
	// 3: Square top right
	game_state->square.local_vertices[3] = (Vertex){
		.position = { .x = 1, .y = 1 },
		.tx_u = 1.0f, .tx_v = 0.0f,
	};

	// Defining the two triangles making up the square in CW-order
	// LEFT: 
	game_state->square.vertex_list[0] = 0;
	game_state->square.vertex_list[1] = 1;
	game_state->square.vertex_list[2] = 2;
	// RIGHT
	game_state->square.vertex_list[3] = 2;
	game_state->square.vertex_list[4] = 1;
	game_state->square.vertex_list[5] = 3;

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
	float far = 100.0f;
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
	Mat4x4 perspective = {};
	// NOTE(mal): We account for the fact here that pixels aren't necessarily square and that the
	// horizontal FOV may differ from the vertical FOV.
	perspective.rows[0][0] = d / aspect_ratio;
	perspective.rows[0][1] = 0;
	perspective.rows[0][2] = 0;
	perspective.rows[0][3] = 0;

	perspective.rows[1][0] = 0;
	perspective.rows[1][1] = d;
	perspective.rows[1][2] = 0;
	perspective.rows[1][3] = 0;

	perspective.rows[2][0] = 0;
	perspective.rows[2][1] = 0;
	// Preserving and remapping depth values from [-near, -far] (because we look down -z in RHS view
	// space) to be [-1, 1].
	// NOTE(mal): That this does NOT clip or cull vertices! If a vertex depth < near or depth > far
	// then it just gets assigned an NDC Z value of -1 or 1, respectively.
	perspective.rows[2][2] = -((far + near) / (far - near));
	perspective.rows[2][3] = -((2.0f * far * near) / (far - near));

	perspective.rows[3][0] = 0;
	perspective.rows[3][1] = 0;
	perspective.rows[3][2] = -1;
	perspective.rows[3][3] = 0;

	// NDC ranges from [-1, -1] to [1, 1]
	// We want to map to our screen, which is from [0, 0] to [width, height], and where +y is down
	Mat4x4 ndc_to_screen = {};

	float half_width = offscreen_buffer->width / 2.0f;
	float half_height = offscreen_buffer->height / 2.0f;
	float screen_offset_x = 0.0f;
	float screen_offset_y = 0.0f;

	ndc_to_screen.rows[0][0] = half_width;
	ndc_to_screen.rows[0][1] = 0;
	ndc_to_screen.rows[0][2] = 0;
	ndc_to_screen.rows[0][3] = half_width + screen_offset_x;

	ndc_to_screen.rows[1][0] = 0;
	ndc_to_screen.rows[1][1] = -half_height;
	ndc_to_screen.rows[1][2] = 0;
	ndc_to_screen.rows[1][3] = (half_height + screen_offset_y);

	// z is a special case since we want to use it for depth testing later.
	// We want it to range from [0, depth] where depth usually = 1.
	float depth = 1.0f;
	float half_depth = depth / 2;
	ndc_to_screen.rows[2][0] = 0;
	ndc_to_screen.rows[2][1] = 0;
	ndc_to_screen.rows[2][2] = half_depth;
	ndc_to_screen.rows[2][3] = half_depth;

	ndc_to_screen.rows[3][0] = 0;
	ndc_to_screen.rows[3][1] = 0;
	ndc_to_screen.rows[3][2] = 0;
	ndc_to_screen.rows[3][3] = 1;

	// NOTE(mal): NOT optimized!
	// TODO(mal): Optimization: only loop over pixels within the AABB of the triangle.
	uint32_t *pixels = (uint32_t *)offscreen_buffer->memory;
	// Clear to black
	memset(pixels, 0, offscreen_buffer->height * offscreen_buffer->width * offscreen_buffer->bytes_per_pixel);

	Vertex *vertices = game_state->square.local_vertices;
	int num_vertex_indices = 6;
	ASSERT(num_vertex_indices % 3 == 0);
	for (int vertex_list_offset = 0; vertex_list_offset < num_vertex_indices; vertex_list_offset += 3) {
		// Making copies because I'll be modifying them in-place during the loop iteration.
		// Not sure if this is the best way to do things but I'll figure that out later.
		Vertex v0 = vertices[game_state->square.vertex_list[vertex_list_offset]];
		Vertex v1 = vertices[game_state->square.vertex_list[vertex_list_offset + 1]];
		Vertex v2 = vertices[game_state->square.vertex_list[vertex_list_offset + 2]];

		// Scaling
		v0.position = mult_vec3_scalar(v0.position, game_state->square.scale);
		v1.position = mult_vec3_scalar(v1.position, game_state->square.scale);
		v2.position = mult_vec3_scalar(v2.position, game_state->square.scale);
		// Rotating
		Mat3x3 triangle_y_rot = mat3x3_create_rotation_y(DEGREES_TO_RADIANS(game_state->rotation_y_degrees));
		v0.position = mult_mat3x3_vec3(triangle_y_rot, v0.position);
		v1.position = mult_mat3x3_vec3(triangle_y_rot, v1.position);
		v2.position = mult_mat3x3_vec3(triangle_y_rot, v2.position);
		// Translating
		v0.position = vec3_add(v0.position, game_state->square.world_position);
		v1.position = vec3_add(v1.position, game_state->square.world_position);
		v2.position = vec3_add(v2.position, game_state->square.world_position);

		Vertex *verts[3] = { &v0, &v1, &v2 };
		float reciprocal_depth[3];
		for (int i = 0; i < 3; i++) {
			Vec4 homogeneous_vert_pos = { .x = verts[i]->position.x, .y = verts[i]->position.y, .z = verts[i]->position.z, .w = 1 };
			Vec4 view_space_vert = mult_mat4x4_vec4(world_to_camera, homogeneous_vert_pos);
			Vec4 perspective_vert = mult_mat4x4_vec4(perspective, view_space_vert);
			// NOTE(mal): If our perspective projection is set up right, then the resulting point
			// should be in a space which is our viewing frustum transformed into a unit cube.
			// From RTR 4.7.2 p98: "The perspective transform in any form followed by clipping and
			// homogenization (division by w) results in normalized device coordinates.
			// TODO(mal): Clip, then perform perspective divide?
			// NOTE(mal): After perspective projection and befoe perspective divide, the w
			// component IS our view-space Z (depth) coordinate!
			float reciprocal_w = 1.0f / perspective_vert.w;
			reciprocal_depth[i] = reciprocal_w;
			perspective_vert.x *= reciprocal_w;
			perspective_vert.y *= reciprocal_w;
			perspective_vert.z *= reciprocal_w;
			perspective_vert.w *= reciprocal_w;
			Vec4 screen_space_vert = mult_mat4x4_vec4(ndc_to_screen, perspective_vert);
			verts[i]->position = (Vec3){ .x = screen_space_vert.x, .y = screen_space_vert.y, .z = screen_space_vert.z };
		}

		// NOTE(mal): because we have now essentially mirrored our triangle across
		// the X axis the winding order of our vertices has technically changed, so
		// we need to feed the vertices in backward!
		// TODO(mal): Instead of copying or changing to Vertex *vs[3], just hard-code which vertices
		// we're using directly?
		Vertex vs[3] = { v2, v1, v0 };
		float reciprocal_depth_2[3] = { reciprocal_depth[2], reciprocal_depth[1], reciprocal_depth[0] };
		float area = edge_function(vs[0].position, vs[1].position, vs[2].position);
		float reciprocal_area = 1 / area;
		// Compute the AABB of the triangle so that we don't have to loop over the entire buffer
		// every time regardless of the size of the triangle.
		// TODO(mal): Maybe create to_vec3_max(Vec3 a, Vec3 b) and to_vec3_min(Vec3 a, Vec3 b)
		// functions and call those here instead?
		int xmax =
			vs[0].position.x > vs[1].position.x
			? (vs[0].position.x > vs[2].position.x ? vs[0].position.x : vs[2].position.x)
			: (vs[1].position.x > vs[2].position.x ? vs[1].position.x : vs[2].position.x);
		int xmin =
			vs[0].position.x < vs[1].position.x
			? (vs[0].position.x < vs[2].position.x ? vs[0].position.x : vs[2].position.x)
			: (vs[1].position.x < vs[2].position.x ? vs[1].position.x : vs[2].position.x);
		int ymax =
			vs[0].position.y > vs[1].position.y
			? (vs[0].position.y > vs[2].position.y ? vs[0].position.y : vs[2].position.y)
			: (vs[1].position.y > vs[2].position.y ? vs[1].position.y : vs[2].position.y);
		int ymin =
			vs[0].position.y < vs[1].position.y
			? (vs[0].position.y < vs[2].position.y ? vs[0].position.y : vs[2].position.y)
			: (vs[1].position.y < vs[2].position.y ? vs[1].position.y : vs[2].position.y);

		// TODO(mal): Do actual vertex clipping and get rid of this
		ymin = ymin < 0 ? 0 : ymin;
		ymax = ymax > offscreen_buffer->height ? offscreen_buffer->height : ymax;
		xmin = xmin < 0 ? 0 : xmin;
		xmax = xmax > offscreen_buffer->width ? offscreen_buffer->width : xmax;

		// NOTE(mal): Avoiding per-pixel edge function computation. This should be possible because the
		// edge function is linear. Thus,
		// E(x + 1, y) = E(x, y) + dY
		// E(x, y + 1) = E(x, y) - dX
		// https://www.cs.drexel.edu/~deb39/Classes/Papers/comp175-06-pineda.pdf
		// Taking the algorithm for stepping from here: https://www.youtube.com/watch?v=k5wtuKWmV48
		// at chapter "Avoiding Computing the Edge Function Per-Pixel".
		//
		// Prime our linear stepping with weights using point at topleft of the triangle's bounding box.
		Vec3 p = { .x = (float)xmin + 0.5f, .y = (float)ymin + 0.5f }; // test pixel center
		float w0_row = edge_function(vs[1].position, vs[2].position, p);
		float w1_row = edge_function(vs[2].position, vs[0].position, p);
		float w2_row = edge_function(vs[0].position, vs[1].position, p);

		// Constant weight deltas for horizontal and vertical steps
		//
		// From my edge function, given how I set it up:
		// (px - v0x) * (v1y - v0y) - (py - v0y) * (v1x - v0x)
		// (px*v1y - px*v0y -v0x*v1y + v0x*v0y) - (py*v1x - py*v0x - v0y*v1x + v0y*v0x)
		// px*v1y - px*v0y - v0x*v1y + v0x*v0y - py*v1x + py*v0x + v0y*v1x - v0y*v0x
		// px*v1y - px*v0y - v0x*v1y - py*v1x + py*v0x + v0y*v1x
		// px(v1y - v0y) - v0x*v1y + py(-v1x + v0x) + v0y*v1x
		// px(v1y - v0y) + py(v0x - v1x) - v0x*v1y + v0y*v1x
		// The term multiplying px is the column delta and the term multiplying py is the row delta.
		float d_w0_col = (vs[2].position.y - vs[1].position.y);
		float d_w1_col = (vs[0].position.y - vs[2].position.y);
		float d_w2_col = (vs[1].position.y - vs[0].position.y);
		float d_w0_row = (vs[1].position.x - vs[2].position.x);
		float d_w1_row = (vs[2].position.x - vs[0].position.x);
		float d_w2_row = (vs[0].position.x - vs[1].position.x);

		for (int row = ymin; row < ymax; row++) {
			float w0 = w0_row;
			float w1 = w1_row;
			float w2 = w2_row;
			for (int col = xmin; col < xmax; col++) {
				if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
					// HACK(mal): poor man's clipping to avoid crashing
					// TODO(mal): Get rid of this once clipping is actually added.
					int has_negative_depth = 0;
					for (int i = 0; i < 3; i++) {
						if (reciprocal_depth_2[i] < 0) {
							has_negative_depth = 1;
							break;
						}
					}
					if (has_negative_depth) {
						continue;
					}

					// If p was inside the triangle_vertices, compute its barycentric coordinates for vertex
					// attribute interpolation.

					// TODO(mal): Rename some of this stuff. Names are taken from Realtime
					// Rendering. See p1000 for perspective-correct barycentric interpolation.
					// I believe here we're essentially foreshortening our barycentric coordinates.
					float f0 = w0 * reciprocal_depth_2[0];
					float f1 = w1 * reciprocal_depth_2[1];
					float f2 = w2 * reciprocal_depth_2[2];
					float perspective_reciprocal_area = 1.0f / (f0 + f1 + f2);
					float tx_u = (f0 * vs[0].tx_u + f1 * vs[1].tx_u + f2 * vs[2].tx_u) * perspective_reciprocal_area;
					float tx_v = (f0 * vs[0].tx_v + f1 * vs[1].tx_v + f2 * vs[2].tx_v) * perspective_reciprocal_area;
					
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

EXPORT void game_update(GameMemory *memory, GameInput *input) {
	GameState *game_state = (GameState *)memory->storage;

	// TODO(mal): Local and world space (2D) have Y pointing up but screen space has Y pointing down.
	// Need to include a transformation step that flips the direction of our Y axis!

	game_state->camera_world_orientation = mat3x3_create_rotation_y(DEGREES_TO_RADIANS(0.0f));
	game_state->square.scale = 10.0f;
	game_state->square.world_position = (Vec3){ .z = game_state->square.scale * 2.0f };

	// Defining the two triangles making up the square in CW-order
	
	// LEFT: 
	game_state->square.vertex_list[0] = 0;
	game_state->square.vertex_list[1] = 1;
	game_state->square.vertex_list[2] = 2;
	// RIGHT
	game_state->square.vertex_list[3] = 2;
	game_state->square.vertex_list[4] = 1;
	game_state->square.vertex_list[5] = 3;

	// Rotate
	const float rot_speed = 2.0f;

	game_state->rotation_y_degrees += rot_speed;

	// if (input->keys_down[GAME_KEY_J]) game_state->rotation_y_degrees += rot_speed;
	// if (input->keys_down[GAME_KEY_L]) game_state->rotation_y_degrees -= rot_speed;
	if (game_state->rotation_y_degrees > 180.0f) game_state->rotation_y_degrees -= 360.0f;
	if (game_state->rotation_y_degrees < 180.0f) game_state->rotation_y_degrees += 360.0f;

	if (input->keys_down[GAME_KEY_W]) game_state->camera_world_position.z += 1.0f;
	if (input->keys_down[GAME_KEY_S]) game_state->camera_world_position.z -= 1.0f;
}
