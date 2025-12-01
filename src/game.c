#include <stdint.h>
#include <math.h> // TODO(ryan): remove this eventually, NO STANDARD LIBRARY
#include "platform.h"

#define PI 3.14159f

inline float degrees_to_radians(float degrees) {
    float result = degrees * PI / 180.0f;
    return result;
}

// TODO(ryan): See chapter 4.10 SIMD/Vector Processing in "Game Engine Architecture" for how to
// implement many of our vector/matrix operations using SSE.
// TODO(ryan): Some of the Mat4x4 and Mat3x3 ops are very similar in implementation. Is there a way
// to semantically compress that code?

typedef struct Vec3 {
    union {
        float elements[3];
        struct {
            float x;
            float y;
            float z;
        };
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

// NOTE(ryan): With homogeneous notation, vectors w = 0, points w = 1
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

// TODO(ryan): should this be in-place?
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

// TODO(ryan): should this be in-place?
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

// 3D STUFF
/*
typedef struct GameState {
    Vec3 camera_world_position;
    Mat3x3 camera_world_rotation;
    Mat4x4 perspective;
    Mat4x4 ndc_to_screen;
} GameState;

typedef struct Vertex {
    Vec3 position;  // model-space position
    uint32_t color; 
} Vertex;

// NOTE(ryan): From "Real Time Rendering" (page 703) -- for max efficiency, the
// order of vertices in the vertex buffer should match the order in which they
// are accessed by the index buffer.
// TODO(ryan): Make adjustments based on the above information.

//    4-------5
//   /|      /|
//  / |     / |
// 0--+----1  |
// |  |    |  |
// |  7----+--6
// | /     | /
// 3-------2/
static Vertex cube_vertices[8] = {
    { .position = { .x = -1.0f, .y =  1.0f, .z =  1.0f } }, // 0
    { .position = { .x =  1.0f, .y =  1.0f, .z =  1.0f } }, // 1
    { .position = { .x =  1.0f, .y = -1.0f, .z =  1.0f } }, // 2
    { .position = { .x = -1.0f, .y = -1.0f, .z =  1.0f } }, // 3
    { .position = { .x = -1.0f, .y =  1.0f, .z = -1.0f } }, // 4
    { .position = { .x =  1.0f, .y =  1.0f, .z = -1.0f } }, // 5
    { .position = { .x =  1.0f, .y = -1.0f, .z = -1.0f } }, // 6
    { .position = { .x = -1.0f, .y = -1.0f, .z = -1.0f } }, // 7
};

// Indexes into vertices.
// HOW TO DRAW:
// Grab in groups of four.
// Draw lines between adjacent indices in subset, then between the last and first.
// e.g. front face edges: (0 -> 1) (1 -> 2) (2 -> 3) (3 -> 0)
static size_t cube_edges[] = {
    // Front face
    0, 1, 2, 3,
    // Left face
    0, 4, 7, 3,
    // Right face
    1, 5, 6, 2,
    // Back face
    4, 5, 6, 7,
    // Top face
    0, 1, 5, 4,
    // Bottom face
    3, 7, 6, 2,
};

// NOTE(ryan): Unused for now.
// TODO(ryan): Eventually worry about triangle winding order!
static size_t cube_triangles[] = {
    // Front face
    0, 1, 3,
    2, 3, 1,

    // Left face
    4, 0, 7,
    7, 6, 0,

    // Back face
    4, 5, 7,
    6, 7, 5,

    // Right face
    1, 5, 2,
    6, 2, 5,

    // Top face
    4, 5, 0,
    1, 0, 5,

    // Bottom face
    7, 6, 3,
    2, 3, 6,
};

static uint32_t BACKGROUND_COLOR = 0x00000000;
static uint32_t EDGE_COLOR       = 0x00FFFFFF;

// OGL style perspective projection
// NOTE(ryan): In view space, we look down -z
static float vert_fov   = PI / 2.0f;
static float near       = 1.0f;
static float far        = 10.0f;
// static float top        = 0.0f;
// static float bottom     = 0.0f;
// static float left       = 0.0f;
// static float right      = 0.0f;

// Draw a line between points (x0, y0) and (x1, y1) inclusive. Scratch memory is
// needed for generation of intermediate points.
void draw_line(
    GameOffscreenBuffer *offscreen_buffer,
    Point *scratch, size_t scratch_mem_size,
    int x0, int y0,
    int x1, int y1,
    uint32_t color
)
{
    size_t num_points = 0;
    num_points = bresenham(scratch, scratch_mem_size, x0, y0, x1, y1);
    uint32_t *pixels = (uint32_t *)offscreen_buffer->memory;
    for (int p_i = 0; p_i < num_points; p_i++) {
        Point p = scratch[p_i];
        if ((p.x >= 0 && p.x < offscreen_buffer->width) &&
            (p.y >= 0 && p.y < offscreen_buffer->height))
        {
            pixels[p.x + p.y * offscreen_buffer->width] = color;
        }
    }
}

EXPORT GAME_UPDATE_AND_RENDER_SIGNATURE(update_and_render) {
    GameState *game_state = (GameState *)memory->storage;

    if (!memory->is_initialized) {
        memory->is_initialized = true;

        game_state->camera_world_position.x = 0.0f;
        game_state->camera_world_position.y = 0.0f;
        game_state->camera_world_position.z = 0.0f;
        game_state->camera_world_rotation = mat3x3_create_identity();

        {
            float aspect_ratio = (float)offscreen_buffer->width / (float)offscreen_buffer->height;
            float c = (float)(1.0 / tan(vert_fov / 2.0f));

            // Set up OpenGL-style perspective transform matrix

            game_state->perspective.rows[0][0] = c / aspect_ratio;
            game_state->perspective.rows[0][1] = 0;
            game_state->perspective.rows[0][2] = 0;
            game_state->perspective.rows[0][3] = 0;

            game_state->perspective.rows[1][0] = 0;
            game_state->perspective.rows[1][1] = c;
            game_state->perspective.rows[1][2] = 0;
            game_state->perspective.rows[1][3] = 0;

            game_state->perspective.rows[2][0] = 0;
            game_state->perspective.rows[2][1] = 0;
            game_state->perspective.rows[2][2] = -((far + near) / (far - near));
            game_state->perspective.rows[2][3] = -((2 * far * near) / (far - near));

            game_state->perspective.rows[3][0] = 0;
            game_state->perspective.rows[3][1] = 0;
            game_state->perspective.rows[3][2] = -1;
            game_state->perspective.rows[3][3] = 0;
        }

        {
            // NDC ranges from [-1, -1] to [1, 1]
            // We want to map to our screen, which is from [0, 0] to [width, height], and where +y is down

            float half_width = offscreen_buffer->width / 2.0f;
            float half_height = offscreen_buffer->height / 2.0f;
            float screen_offset_x = 0.0f;
            float screen_offset_y = 0.0f;

            game_state->ndc_to_screen.rows[0][0] = half_width;
            game_state->ndc_to_screen.rows[0][1] = 0;
            game_state->ndc_to_screen.rows[0][2] = 0;
            game_state->ndc_to_screen.rows[0][3] = half_width + screen_offset_x;

            game_state->ndc_to_screen.rows[1][0] = 0;
            game_state->ndc_to_screen.rows[1][1] = -half_height;
            game_state->ndc_to_screen.rows[1][2] = 0;
            game_state->ndc_to_screen.rows[1][3] = half_height + screen_offset_y;

            // z is a special case since we want to use it for depth testing later.
            // We want it to range from [0, depth] where depth usually = 1.
            float depth = 1.0f;
            float half_depth = depth / 2;
            game_state->ndc_to_screen.rows[2][0] = 0;
            game_state->ndc_to_screen.rows[2][1] = 0;
            game_state->ndc_to_screen.rows[2][2] = half_depth;
            game_state->ndc_to_screen.rows[2][3] = half_depth;

            game_state->ndc_to_screen.rows[3][0] = 0;
            game_state->ndc_to_screen.rows[3][1] = 0;
            game_state->ndc_to_screen.rows[3][2] = 0;
            game_state->ndc_to_screen.rows[3][3] = 1;
        }
    }

    Vec3 camera_local_motion = {};
    if (input->keys_down[GAME_KEY_W]) { camera_local_motion.z += 0.1f; }
    if (input->keys_down[GAME_KEY_S]) { camera_local_motion.z -= 0.1f; }
    // FIXME(ryan): A and D are backward! I think the camera is set up
    // incorrectly. One of its vectors must be flipped.
    // NOTE(ryan): Temporarily account for this by changing the sign of the ops.
    if (input->keys_down[GAME_KEY_A]) { camera_local_motion.x += 0.1f; }
    if (input->keys_down[GAME_KEY_D]) { camera_local_motion.x -= 0.1f; }

    float camera_yaw = 0.0f;
    float camera_pitch = 0.0f;
    if (input->keys_down[GAME_KEY_J]) { camera_yaw += 2.0f; }
    if (input->keys_down[GAME_KEY_L]) { camera_yaw -= 2.0f; }
    if (input->keys_down[GAME_KEY_I]) { camera_pitch += 2.0f; }
    if (input->keys_down[GAME_KEY_K]) { camera_pitch -= 2.0f; }

    Mat3x3 rot_x = mat3x3_create_rotation_x(degrees_to_radians(camera_pitch));
    Mat3x3 rot_y = mat3x3_create_rotation_y(degrees_to_radians(camera_yaw));
    game_state->camera_world_rotation = mult_mat3x3_mat3x3(game_state->camera_world_rotation, mult_mat3x3_mat3x3(rot_y, rot_x));
    game_state->camera_world_position = vec3_add(game_state->camera_world_position, mult_mat3x3_vec3(game_state->camera_world_rotation, camera_local_motion));

    Mat3x3 camera_world_rotation_transpose = mat3x3_transpose(game_state->camera_world_rotation);
    Vec3 camera_translation_inverse = vec3_negate(mult_mat3x3_vec3(camera_world_rotation_transpose, game_state->camera_world_position));
    Mat4x4 world_to_view = {};
    // Copy inverse camera rotation to upper 3x3
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            world_to_view.rows[r][c] = camera_world_rotation_transpose.rows[r][c];
        }
    }
    // Set inverse camera translation
    world_to_view.rows[0][3] = camera_translation_inverse.x;
    world_to_view.rows[1][3] = camera_translation_inverse.y;
    world_to_view.rows[2][3] = camera_translation_inverse.z;
    // Set that one last bit for the translation homogeneous coords
    world_to_view.rows[3][3] = 1;

    Vec4 screen_space_cube_verts[8] = {};
    for (int i = 0; i < 8; i++) {
        Vec3 v = cube_vertices[i];
        Vec4 homogeneous_cube_vertex = { .x = v.x, .y = v.y, .z = v.z, .w = 1 };
        Vec4 view_space_vert = mult_mat4x4_vec4(world_to_view, homogeneous_cube_vertex);
        Vec4 perspective_vert = mult_mat4x4_vec4(game_state->perspective, view_space_vert);
        perspective_vert.x /= perspective_vert.w;
        perspective_vert.y /= perspective_vert.w;
        perspective_vert.z /= perspective_vert.w;
        perspective_vert.w /= perspective_vert.w;
        Vec4 screen_space_vert = mult_mat4x4_vec4(game_state->ndc_to_screen, perspective_vert);
        screen_space_cube_verts[i] = screen_space_vert;
    }

    // Clear to background
    uint8_t *row = (uint8_t *)offscreen_buffer->memory;
    for (int y = 0; y < offscreen_buffer->height; y++) {
        uint32_t *pixel = (uint32_t *)row;
        for (int x = 0; x < offscreen_buffer->width; x++) {
            *pixel = BACKGROUND_COLOR;
            pixel++;
        }
        row += (offscreen_buffer->width * offscreen_buffer->bytes_per_pixel);
    }

    // Draw cube edges
    #define SCRATCH_LEN 256u
    Point scratch[SCRATCH_LEN];
    for (int i = 0; i < 24; i += 4) {
        size_t v_index_0 = cube_edges[i];
        int v0x = (int)screen_space_cube_verts[v_index_0].x;
        int v0y = (int)screen_space_cube_verts[v_index_0].y;
        float v0z = screen_space_cube_verts[v_index_0].z;

        size_t v_index_1 = cube_edges[i + 1];
        int v1x = (int)screen_space_cube_verts[v_index_1].x;
        int v1y = (int)screen_space_cube_verts[v_index_1].y;
        float v1z = screen_space_cube_verts[v_index_1].z;

        size_t v_index_2 = cube_edges[i + 2];
        int v2x = (int)screen_space_cube_verts[v_index_2].x;
        int v2y = (int)screen_space_cube_verts[v_index_2].y;
        float v2z = screen_space_cube_verts[v_index_2].z;

        size_t v_index_3 = cube_edges[i + 3];
        int v3x = (int)screen_space_cube_verts[v_index_3].x;
        int v3y = (int)screen_space_cube_verts[v_index_3].y;
        float v3z = screen_space_cube_verts[v_index_3].z;

        // NOTE(ryan): Super rudimentary clipping to ensure we don't draw stuff that's behind the camera.
        if (v0z > near && v1z > near) draw_line(offscreen_buffer, scratch, SCRATCH_LEN, v0x, v0y, v1x, v1y, EDGE_COLOR);
        if (v1z > near && v2z > near) draw_line(offscreen_buffer, scratch, SCRATCH_LEN, v1x, v1y, v2x, v2y, EDGE_COLOR);
        if (v2z > near && v3z > near) draw_line(offscreen_buffer, scratch, SCRATCH_LEN, v2x, v2y, v3x, v3y, EDGE_COLOR);
        if (v3z > near && v0z > near) draw_line(offscreen_buffer, scratch, SCRATCH_LEN, v3x, v3y, v0x, v0y, EDGE_COLOR);
    }

}
*/

typedef struct Vertex {
    float coordinates[2]; // x, y
    uint32_t color;       // argb
} Vertex;

typedef struct GameState {
    Vertex triangle[3];
} GameState;

// v0 - first vertex, v1 - second vertex, p - test point
// Assumes a clockwise winding order.
// Returns the signed area of a parallelogram formed by vectors (v1 - v0) and (p - v0).
// The value is positive if p is on the right side of line (v1 - v0) and negative if on the left.
// The value is 0 if p is on the line (v1 - v0).
//
// NOTE(ryan):
// The value returned by this function is the same as the 2D "cross product" of
// vectors (p - v0) and (v1 - v0) and the determinant of the 2D matrix formed by
// the same two vectors.
float edge_function(float v0[2], float v1[2], float p[2]) {
    float result = (p[0] - v0[0]) * (v1[1] - v0[1]) - (p[1] - v0[1]) * (v1[0] - v0[0]);
    return result;
}

EXPORT GAME_UPDATE_AND_RENDER_SIGNATURE(update_and_render) {
    GameState *game_state = (GameState *)memory->storage;
    if (!memory->is_initialized) {
        memory->is_initialized = true;

        Vertex v0 = { .coordinates = { 0, (float)offscreen_buffer->height }, .color = 0x00FF0000 };
        game_state->triangle[0] = v0;

        Vertex v1 = { .coordinates = { (float)offscreen_buffer->width / 2, 0 }, .color = 0x0000FF00 };
        game_state->triangle[1] = v1;

        Vertex v2 = { .coordinates = { (float)offscreen_buffer->width, (float)offscreen_buffer->height }, .color = 0x000000FF };
        game_state->triangle[2] = v2;
    }

    // NOTE(ryan): NOT optimized!
    // TODO(ryan): Optimization: only loop over pixels within the AABB of the triangle.
    uint32_t *pixels = (uint32_t *)offscreen_buffer->memory;
    Vertex *triangle = game_state->triangle;
    float area = edge_function(triangle[0].coordinates, triangle[1].coordinates, triangle[2].coordinates);
    for (int row = 0; row < offscreen_buffer->height; row++) {
        for (int col = 0; col < offscreen_buffer->width; col++) {
            // float p[2] = { (float)col + 0.5f, (float)row + 0.5f }; // testing pixel _centers_, hence the +0.5
            float p[2] = { (float)col, (float)row }; 
            float w0   = edge_function(triangle[1].coordinates, triangle[2].coordinates, p);
            float w1   = edge_function(triangle[2].coordinates, triangle[0].coordinates, p);
            float w2   = edge_function(triangle[0].coordinates, triangle[1].coordinates, p);
            // FIXME(ryan): have to check for negative right now because we're working with +Y is
            // down in our coordinate system. Therefore, the winding order should be inverted
            // because right now we're technically passing arguments to the edge function in CCW
            // winding order.
            // TODO(ryan): maybe we should somehow use a +Y is up coordinate system then flip for
            // the final draw to the screen buffer?
            if (w0 <= 0 && w1 <= 0 && w2 <= 0) {
                // If p was inside the triangle, compute its barycentric coordinates for vertex
                // attribute interpolation.
                w0 /= area;
                w1 /= area;
                w2 /= area;

                // Interpolate the color based on the three triangle vertices
                float red   = w0 * ((triangle[0].color >> 16) & 0xFF) + w1 * ((triangle[1].color >> 16) & 0xFF) + w2 * ((triangle[2].color >> 16) & 0xFF);
                float green = w0 * ((triangle[0].color >> 8)  & 0xFF) + w1 * ((triangle[1].color >> 8)  & 0xFF) + w2 * ((triangle[2].color >> 8) & 0xFF);
                float blue  = w0 *  (triangle[0].color        & 0xFF) + w1 *  (triangle[1].color        & 0xFF) + w2 *  (triangle[2].color & 0xFF);

                uint8_t red_byte = (uint8_t)red;
                uint8_t blue_byte = (uint8_t)blue;
                uint8_t green_byte = (uint8_t)green;
                uint32_t color = (red_byte << 16) | (blue_byte << 8) | (green_byte);
                pixels[col + row * offscreen_buffer->width] = color;
            }
        }
    }
}

