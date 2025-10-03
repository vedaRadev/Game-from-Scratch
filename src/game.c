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

typedef struct GameState {
    Vec3 cam_world_pos;
    float cam_pitch;
    float cam_yaw;
    Mat4x4 perspective;
    Mat4x4 ndc_to_screen;
} GameState;

//    4-------5
//   /|      /|
//  / |     / |
// 0--+----1  |
// |  |    |  |
// |  7----+--6
// | /     | /
// 3-------2/
static Vec3 cube_vertices[8] = {
    { .x = -1.0f, .y =  1.0f, .z =  1.0f }, // 0
    { .x =  1.0f, .y =  1.0f, .z =  1.0f }, // 1
    { .x =  1.0f, .y = -1.0f, .z =  1.0f }, // 2
    { .x = -1.0f, .y = -1.0f, .z =  1.0f }, // 3
    { .x = -1.0f, .y =  1.0f, .z = -1.0f }, // 4
    { .x =  1.0f, .y =  1.0f, .z = -1.0f }, // 5
    { .x =  1.0f, .y = -1.0f, .z = -1.0f }, // 6
    { .x = -1.0f, .y = -1.0f, .z = -1.0f }, // 7
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

EXPORT GAME_UPDATE_AND_RENDER_SIGNATURE(update_and_render) {
    GameState *game_state = (GameState *)memory->storage;

    if (!memory->is_initialized) {
        memory->is_initialized = true;

        game_state->cam_world_pos.x = 0.0f;
        game_state->cam_world_pos.y = 0.0f;
        game_state->cam_world_pos.z = 0.0f;
        game_state->cam_pitch = 0.0f;
        game_state->cam_yaw   = 0.0f;
    }

    // TODO(ryan): move back into game state init!
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

    // TODO(ryan): move back into game state init!
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

    Vec3 cam_motion = {};
    if (input->keys_down[GAME_KEY_W]) cam_motion.z += 0.1f;
    if (input->keys_down[GAME_KEY_S]) cam_motion.z -= 0.1f;
    if (input->keys_down[GAME_KEY_A]) cam_motion.x -= 0.1f;
    if (input->keys_down[GAME_KEY_D]) cam_motion.x += 0.1f;

    if (input->keys_down[GAME_KEY_J]) game_state->cam_yaw   += 2.0f;
    if (input->keys_down[GAME_KEY_L]) game_state->cam_yaw   -= 2.0f;
    if (input->keys_down[GAME_KEY_I]) game_state->cam_pitch += 2.0f;
    if (input->keys_down[GAME_KEY_K]) game_state->cam_pitch -= 2.0f;

    float cam_yaw_rad = degrees_to_radians(game_state->cam_yaw);
    float cam_yaw_cos = (float)cos(cam_yaw_rad);
    float cam_yaw_sin = (float)sin(cam_yaw_rad);
    float cam_pitch_rad = degrees_to_radians(game_state->cam_pitch);
    float cam_pitch_cos = (float)cos(cam_pitch_rad);
    float cam_pitch_sin = (float)sin(cam_pitch_rad);
    Mat3x3 cam_rotation = {};
    // If Rx is a rotation about the x axis and Ry is a rotation about the y axis,
    // then the upper 3x3 of the cam_transform is RxRy (rotate y first, then x)
    cam_rotation.rows[0][0] = cam_yaw_cos; cam_rotation.rows[0][1] = 0;
    cam_rotation.rows[0][2] = cam_yaw_sin;
    cam_rotation.rows[1][0] = cam_pitch_sin * cam_yaw_sin;
    cam_rotation.rows[1][1] = cam_pitch_cos;
    cam_rotation.rows[1][2] = cam_pitch_sin * -cam_yaw_cos;
    cam_rotation.rows[2][0] = -cam_pitch_cos * cam_yaw_sin;
    cam_rotation.rows[2][1] = cam_pitch_sin;
    cam_rotation.rows[2][2] = cam_pitch_cos * cam_yaw_cos;

    game_state->cam_world_pos = vec3_add(game_state->cam_world_pos, mult_mat3x3_vec3(cam_rotation, cam_motion));

    Mat3x3 cam_rotation_transpose = mat3x3_transpose(cam_rotation);
    Vec3 cam_translation_inverse = vec3_negate(mult_mat3x3_vec3(cam_rotation_transpose, game_state->cam_world_pos));
    Mat4x4 world_to_view = {};
    // Copy inverse cam rotation to upper 3x3
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            world_to_view.rows[r][c] = cam_rotation_transpose.rows[r][c];
        }
    }
    // Set inverse cam translation
    world_to_view.rows[0][3] = cam_translation_inverse.x;
    world_to_view.rows[1][3] = cam_translation_inverse.y;
    world_to_view.rows[2][3] = cam_translation_inverse.z;
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
    uint32_t *pixels = (uint32_t *)offscreen_buffer->memory;
    for (int i = 0; i < 24; i += 4) {
        size_t v_index_0 = cube_edges[i];
        int v0x = (int)screen_space_cube_verts[v_index_0].x;
        int v0y = (int)screen_space_cube_verts[v_index_0].y;

        size_t v_index_1 = cube_edges[i + 1];
        int v1x = (int)screen_space_cube_verts[v_index_1].x;
        int v1y = (int)screen_space_cube_verts[v_index_1].y;

        size_t v_index_2 = cube_edges[i + 2];
        int v2x = (int)screen_space_cube_verts[v_index_2].x;
        int v2y = (int)screen_space_cube_verts[v_index_2].y;

        size_t v_index_3 = cube_edges[i + 3];
        int v3x = (int)screen_space_cube_verts[v_index_3].x;
        int v3y = (int)screen_space_cube_verts[v_index_3].y;

        Point scratch[256];
        size_t num_points = 0;

        // v0 -> v1
        num_points = bresenham(scratch, 256, v0x, v0y, v1x, v1y);
        for (int p_i = 0; p_i < num_points; p_i++) {
            Point p = scratch[p_i];
            if (
                p.x >= 0 && p.x < offscreen_buffer->width &&
                p.y >= 0 && p.y < offscreen_buffer->height
            )
            {
                pixels[p.x + p.y * offscreen_buffer->width] = EDGE_COLOR;
            }
        }

        // v1 -> v2
        num_points = bresenham(scratch, 256, v1x, v1y, v2x, v2y);
        for (int p_i = 0; p_i < num_points; p_i++) {
            Point p = scratch[p_i];
            if (
                p.x >= 0 && p.x < offscreen_buffer->width &&
                p.y >= 0 && p.y < offscreen_buffer->height
            )
            {
                pixels[p.x + p.y * offscreen_buffer->width] = EDGE_COLOR;
            }
        }

        // v2 -> v3
        num_points = bresenham(scratch, 256, v2x, v2y, v3x, v3y);
        for (int p_i = 0; p_i < num_points; p_i++) {
            Point p = scratch[p_i];
            if (
                p.x >= 0 && p.x < offscreen_buffer->width &&
                p.y >= 0 && p.y < offscreen_buffer->height
            )
            {
                pixels[p.x + p.y * offscreen_buffer->width] = EDGE_COLOR;
            }
        }

        // v3 -> v0
        num_points = bresenham(scratch, 256, v3x, v3y, v0x, v0y);
        for (int p_i = 0; p_i < num_points; p_i++) {
            Point p = scratch[p_i];
            if (
                p.x >= 0 && p.x < offscreen_buffer->width &&
                p.y >= 0 && p.y < offscreen_buffer->height
            )
            {
                pixels[p.x + p.y * offscreen_buffer->width] = EDGE_COLOR;
            }
        }
    }

    // // Draw vertices
    // uint32_t *pixels = (uint32_t *)offscreen_buffer->memory;
    // for (int i = 0; i < 8; i++) {
    //     int vx = (int)screen_space_cube_verts[i].x;
    //     int vy = (int)screen_space_cube_verts[i].y;;
    //     if (vx >= 0 && vx < offscreen_buffer->width && vy >= 0 && vy < offscreen_buffer->height) {
    //         pixels[vx + vy * offscreen_buffer->width] = EDGE_COLOR;
    //     }
    // }

}

