#pragma once

#include "kraft_camera.h"

#include <core/kraft_math.h>
#include <core/kraft_input.h>
#include <core/kraft_events.h>

namespace kraft {

struct DebugCamera {
    Camera camera;

    Vec3f target_position = Vec3f{0.0f, 0.0f, -5.0f};
    f32 target_pitch = 0.0f;
    f32 target_yaw = -90.0f;

    // Settings
    f32 move_speed = 10.0f;
    f32 look_sensitivity = 0.1f;
    f32 scroll_speed = 3.0f;
    f32 pan_speed = 0.01f;
    f32 smoothing = 15.0f;
    f32 boost_multiplier = 3.0f;

    // Internal
    f32 scroll_accumulator = 0.0f;
    bool looking = false;
    bool panning = false;

    void Init(Vec3f position, f32 pitch = 0.0f, f32 yaw = -90.0f) {
        target_position = position;
        target_pitch = pitch;
        target_yaw = yaw;

        camera.Position = position;
        camera.Pitch = pitch;
        camera.Yaw = yaw;
        camera.UpdateVectors();

        EventSystem::Listen(EVENT_TYPE_SCROLL, this, OnScroll);
    }

    void Shutdown() {
        EventSystem::Unlisten(EVENT_TYPE_SCROLL, this, OnScroll);
    }

    // Call once per frame. Returns the camera ready to use.
    Camera* Update(f32 dt, f32 aspect_ratio) {
        HandleInput(dt);
        Smooth(dt);

        camera.SetPerspectiveProjection(camera.FOVRadians, aspect_ratio, 0.1f, 1000.0f);
        camera.UpdateVectors();

        return &camera;
    }

private:
    static bool OnScroll(EventType type, void* sender, void* listener, EventData event_data) {
        DebugCamera* self = (DebugCamera*)listener;
        f64 y = event_data.Float64Value[1];
        self->scroll_accumulator += (f32)y;
        return false;
    }

    void HandleInput(f32 dt) {
        f32 speed = move_speed;
        if (InputSystem::IsKeyDown(Keys::KEY_LEFT_SHIFT) || InputSystem::IsKeyDown(Keys::KEY_RIGHT_SHIFT)) {
            speed *= boost_multiplier;
        }

        // Look around
        looking = InputSystem::IsMouseButtonDown(MouseButtons::MOUSE_BUTTON_RIGHT);
        if (looking) {
            MousePosition current = InputSystem::GetMousePosition();
            MousePosition previous = InputSystem::GetPreviousMousePosition();
            f32 dx = (f32)(current.x - previous.x);
            f32 dy = (f32)(current.y - previous.y);

            target_yaw += dx * look_sensitivity;
            target_pitch -= dy * look_sensitivity;
            target_pitch = math::Clamp(target_pitch, -89.0f, 89.0f);
        }

        // Panning
        panning = InputSystem::IsMouseButtonDown(MouseButtons::MOUSE_BUTTON_MIDDLE);
        if (panning) {
            MousePosition current = InputSystem::GetMousePosition();
            MousePosition previous = InputSystem::GetPreviousMousePosition();
            f32 dx = (f32)(current.x - previous.x);
            f32 dy = (f32)(current.y - previous.y);

            target_position = target_position - camera.Right * dx * pan_speed * speed;
            target_position = target_position + camera.Up * dy * pan_speed * speed;
        }

        // Movement
        if (looking) {
            Vec3f move_dir = Vec3f{0.0f, 0.0f, 0.0f};

            if (InputSystem::IsKeyDown(Keys::KEY_W))
                move_dir = move_dir + camera.Front;
            if (InputSystem::IsKeyDown(Keys::KEY_S))
                move_dir = move_dir - camera.Front;
            if (InputSystem::IsKeyDown(Keys::KEY_A))
                move_dir = move_dir - camera.Right;
            if (InputSystem::IsKeyDown(Keys::KEY_D))
                move_dir = move_dir + camera.Right;
            if (InputSystem::IsKeyDown(Keys::KEY_E))
                move_dir = move_dir + Vec3f{0.0f, 1.0f, 0.0f};
            if (InputSystem::IsKeyDown(Keys::KEY_Q))
                move_dir = move_dir - Vec3f{0.0f, 1.0f, 0.0f};

            if (LengthSquared(move_dir) > 0.001f) {
                move_dir = Normalize(move_dir);
                target_position = target_position + move_dir * speed * dt;
            }
        }

        // Zooming
        if (scroll_accumulator != 0.0f) {
            target_position = target_position + camera.Front * scroll_accumulator * scroll_speed;
            scroll_accumulator = 0.0f;
        }
    }

    void Smooth(f32 dt) {
        // Frame-rate independent exponential interpolation
        // t = 1 - exp(-smoothing * dt) gives consistent behavior at any framerate
        f32 t = 1.0f - Exp(-smoothing * dt);

        camera.Position = Lerp(camera.Position, target_position, t);
        camera.Pitch = Lerp(camera.Pitch, target_pitch, t);
        camera.Yaw = Lerp(camera.Yaw, target_yaw, t);
    }
};

} // namespace kraft
