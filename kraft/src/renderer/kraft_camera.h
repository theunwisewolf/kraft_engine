#pragma once

#include "core/kraft_core.h"
#include "core/kraft_math.h"

namespace kraft {

enum CameraProjectionType { Perspective, Orthographic };

struct Camera {
    Vec3f Position;

    union {
        struct {
            f32 Pitch;
            f32 Yaw;
            f32 Roll;
        };

        Vec3f Euler;
    };

    CameraProjectionType ProjectionType;
    Mat4f ProjectionMatrix; // Computed matrix
    Mat4f ViewMatrix; // Computed matrix
    bool Dirty = true;
    f32 FOVRadians = kraft::Radians(45.0f);

    // The target the camera is supposed to look at
    Vec3f Target;

    // The direction of the camera to the target
    Vec3f Direction;

    // Direction vectors
    Vec3f Up;
    Vec3f Right;
    Vec3f Front;

    Camera();

    void Reset();
    // void SetPosition(Vec3f position);
    // void SetRotation(Vec3f rotation);
    void SetOrthographicProjection(u32 Width, u32 Height, f32 NearClip, f32 FarClip);
    void SetPerspectiveProjection(f32 FOVRadians, u32 Width, u32 Height, f32 NearClip, f32 FarClip);
    void SetPerspectiveProjection(f32 FOVRadians, f32 AspectRatio, f32 NearClip, f32 FarClip);

    KRAFT_INLINE Mat4f GetViewMatrix() {
        if (this->ProjectionType == CameraProjectionType::Orthographic) {
            // this->ViewMatrix = Mat4f(Identity);
            // this->ViewMatrix = kraft::LookAt(this->Position, this->Position + this->Front, this->Up);
            this->ViewMatrix = TranslationMatrix(this->Position);
        } else {
            this->ViewMatrix = LookAt(this->Position, this->Position + this->Front, this->Up);
        }

        return this->ViewMatrix;
        // UpdateViewMatrix();
        // return this->ViewMatrix;
    }

    void UpdateVectors() {
        Vec3f front;
        front.x = Cos(Radians(Yaw)) * Cos(Radians(Pitch));
        front.y = Sin(Radians(Pitch));
        front.z = Sin(Radians(Yaw)) * Cos(Radians(Pitch));
        this->Front = Normalize(front);
        this->Right = Normalize(Cross(this->Front, {0.0f, 1.0f, 0.0f}));
        this->Up = Normalize(Cross(this->Right, this->Front));
    }
};

// A very simple 2d camera
struct Camera2D {
    vec2 position = {};
    vec2 target_position = {};
    f32 zoom = 1.0f;
    f32 target_zoom = 1.0f;
    f32 move_speed = 500.0f;
    f32 smoothing = 12.0f; // higher = snappier, lower = smoother
    f32 zoom_smoothing = 8.0f;
    f32 min_zoom = 0.1f;
    f32 max_zoom = 5.0f;
    f32 zoom_step = 0.1f; // zoom factor per scroll tick
    f32 scroll_accumulator = 0.0f;
    bool panning = false;
    MousePosition last_mouse_pos = {};

    // Screen dimensions
    f32 screen_width = 0.0f;
    f32 screen_height = 0.0f;

    void Init(f32 width, f32 height) {
        position = {0.0f, 0.0f};
        target_position = {0.0f, 0.0f};
        zoom = 1.0f;
        target_zoom = 1.0f;
        scroll_accumulator = 0.0f;
        panning = false;
        last_mouse_pos = {};
        screen_width = width;
        screen_height = height;
        EventSystem::Listen(EVENT_TYPE_SCROLL, this, OnScroll);
    }

    void Shutdown() {
        EventSystem::Unlisten(EVENT_TYPE_SCROLL, this, OnScroll);
    }

    mat4 GetViewMatrix() {
        return TranslationMatrix(vec3{-position.x, -position.y, 0.0f}) * ScaleMatrix(vec3{zoom, zoom, 1.0f});
    }

    // Convert screen pixel coords (origin top-left, Y down) to world coords
    vec2 ScreenToWorld(i32 screen_x, i32 screen_y) {
        f32 world_x = (f32)screen_x / zoom + position.x;
        f32 world_y = (screen_height - (f32)screen_y) / zoom + position.y;
        return {world_x, world_y};
    }

    // Convert world coords to screen pixel coords (origin top-left, Y down)
    vec2 WorldToScreen(f32 world_x, f32 world_y) {
        f32 screen_x = (world_x - position.x) * zoom;
        f32 screen_y = screen_height - (world_y - position.y) * zoom;
        return {screen_x, screen_y};
    }

    void Update(f32 delta_time) {
        vec2 move_dir = Vec2fZero;
        if (InputSystem::IsKeyDown(KEY_W))
            move_dir.y += 1.0f;
        if (InputSystem::IsKeyDown(KEY_S))
            move_dir.y -= 1.0f;
        if (InputSystem::IsKeyDown(KEY_A))
            move_dir.x -= 1.0f;
        if (InputSystem::IsKeyDown(KEY_D))
            move_dir.x += 1.0f;

        if (LengthSquared(move_dir) > 0.001f) {
            move_dir = Normalize(move_dir);
            // Faster movement when zoomed out
            target_position += move_dir * (move_speed / zoom) * delta_time;
        }

        // Panning
        MousePosition mouse_pos = InputSystem::GetMousePosition();
        if (InputSystem::IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) || InputSystem::IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            if (!panning) {
                panning = true;
                last_mouse_pos = mouse_pos;
            } else {
                f32 dx = (f32)(mouse_pos.x - last_mouse_pos.x);
                f32 dy = (f32)(mouse_pos.y - last_mouse_pos.y);
                // Mouse Y is top-down, world Y is bottom-up
                target_position.x -= dx / zoom;
                target_position.y += dy / zoom;
                last_mouse_pos = mouse_pos;
            }
        } else {
            panning = false;
        }

        if (scroll_accumulator != 0.0f) {
            // World pos under cursor before zoom change
            vec2 world_before = ScreenToWorld(mouse_pos.x, mouse_pos.y);

            if (scroll_accumulator > 0.0f) {
                target_zoom *= (1.0f + zoom_step);
            } else {
                target_zoom *= (1.0f - zoom_step);
            }
            target_zoom = math::Clamp(target_zoom, min_zoom, max_zoom);

            // Apply zoom instantly so cursor-point stays exactly fixed
            zoom = target_zoom;

            vec2 world_after = ScreenToWorld(mouse_pos.x, mouse_pos.y);

            // Adjust both current and target position so there's no smoothing drift
            vec2 correction = world_before - world_after;
            position += correction;
            target_position += correction;

            scroll_accumulator = 0.0f;
        }

        f32 t_move = 1.0f - Exp(-smoothing * delta_time);
        f32 t_zoom = 1.0f - Exp(-zoom_smoothing * delta_time);
        position = Lerp(position, target_position, t_move);
        zoom = Lerp(zoom, target_zoom, t_zoom);
    }

    static bool OnScroll(EventType type, void* sender, void* listener, EventData event_data) {
        Camera2D* self = (Camera2D*)listener;
        f64 y = event_data.Float64Value[1];
        self->scroll_accumulator += (f32)y;
        return false;
    }
};

} // namespace kraft