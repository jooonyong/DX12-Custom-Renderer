#pragma once

#include "DirectXMath.h"

static float Angle = 0.0f;

class Camera
{
public:
	DirectX::XMMATRIX GetViewMatrix() const;
	DirectX::XMMATRIX GetProjectionMatrix() const;

	void MoveForward(float Distance);
	void MoveRight(float Distance);
    void MoveUp(float Distance);

    void AddRotation(float YawDelta, float PitchDelta);
    void UpdateDirection();

private:
    DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, -3.0f };
    DirectX::XMFLOAT3 Forward = { 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 Up = { 0.0f, 1.0f, 0.0f };
    DirectX::XMFLOAT3 Right = { 1.0f, 0.0f, 0.0f };

    float FovY = DirectX::XMConvertToRadians(60.0f);
    float AspectRatio = 16.0f / 9.0f;
    float NearZ = 0.1f;
    float FarZ = 100.0f;

    float Yaw = 0.0f;
    float Pitch = 0.0f;
};