#include <stdint.h>
#include <math.h> // TODO(ryan): remove this eventually, NO STANDARD LIBRARY
#include "platform.h"

// NOTE(ryan): With homogeneous notation, vectors w = 0, points w = 1
typedef struct Vec3 { float x; float y; float z; } Vec3;

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

// a * b
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

typedef struct Mat3x3 { float elements[9];  } Mat3x3;

typedef struct Mat4x4 {
    union {
        float elements[16];
        float rows[4][4];
    };
} Mat4x4;

// Let M be 4x4 matrix, V be a 4x1 vector
// Computes MV
Vec4 mult_mat4x4_vec4(Mat4x4 m, Vec4 v) {
    Vec4 result = {};

    for (int i = 0; i < 4; i++) {
        for (int m_row = 0; m_row < 4; m_row++) {
            for (int j = 0; j < 4; j++) {
                result.elements[i] += m.rows[m_row][j] * v.elements[j];
            }
        }
    }

    return result;
}

typedef struct GameState {
    Vec3 cam_world_pos;
    float cam_pitch;
    float cam_yaw;
    Mat4x4 perspective;
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

static const float PI = 3.14159f;

// OGL style perspective projection
// NOTE(ryan): In view space, we look down -z
static float vert_fov   = PI / 2;
static float near       = 0.0f;
static float far        = 0.0f;
static float top        = 0.0f;
static float bottom     = 0.0f;
static float left       = 0.0f;
static float right      = 0.0f;

EXPORT GAME_UPDATE_AND_RENDER_SIGNATURE(update_and_render) {
    GameState *game_state = (GameState *)memory->storage;
    if (!memory->is_initialized) {
        memory->is_initialized = true;

        game_state->cam_world_pos.x = 0.0f;
        game_state->cam_world_pos.y = 0.0f;
        game_state->cam_world_pos.z = 0.0f;
        game_state->cam_pitch = 0.0f;
        game_state->cam_yaw   = 0.0f;

        {
            float aspect_ratio = (float)offscreen_buffer->width / (float)offscreen_buffer->height;
            float c = 1.0f / tan(vert_fov / 2);

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
    }

    Vec3 camera_motion = {};
    if (input->keys_down[GAME_KEY_W]) camera_motion.z += 1.0f;
    if (input->keys_down[GAME_KEY_S]) camera_motion.z -= 1.0f;
    if (input->keys_down[GAME_KEY_A]) camera_motion.x -= 1.0f;
    if (input->keys_down[GAME_KEY_D]) camera_motion.x += 1.0f;

    if (input->keys_down[GAME_KEY_J]) game_state->cam_yaw += 1.0f;
    if (input->keys_down[GAME_KEY_L]) game_state->cam_yaw -= 1.0f;
    if (input->keys_down[GAME_KEY_I]) game_state->cam_pitch += 1.0f;
    if (input->keys_down[GAME_KEY_K]) game_state->cam_pitch -= 1.0f;

    uint8_t *row = (uint8_t *)offscreen_buffer->memory;
    for (int y = 0; y < offscreen_buffer->height; y++) {
        uint32_t *pixel = (uint32_t *)row;
        for (int x = 0; x < offscreen_buffer->width; x++) {
            *pixel = EDGE_COLOR;
            pixel++;
        }
        row += (offscreen_buffer->width * offscreen_buffer->bytes_per_pixel);
    }
}

