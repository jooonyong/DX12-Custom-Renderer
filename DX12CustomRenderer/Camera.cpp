#include "Camera.h"
#include <iostream>
#include <algorithm>

DirectX::XMMATRIX Camera::GetViewMatrix() const
{
	DirectX::XMVECTOR CameraPosition = DirectX::XMVectorSet(Position.x, Position.y, Position.z, 1.0f);
	DirectX::XMVECTOR Direction = DirectX::XMLoadFloat3(&Forward);
	DirectX::XMVECTOR UpDirection = DirectX::XMVectorSet(Up.x, Up.y, Up.z, 0.0f);
	DirectX::XMMATRIX View = DirectX::XMMatrixLookToLH(CameraPosition, Direction, UpDirection);

	return View;
}

DirectX::XMMATRIX Camera::GetProjectionMatrix() const
{
	DirectX::XMMATRIX Projection = DirectX::XMMatrixPerspectiveFovLH(FovY, AspectRatio, NearZ, FarZ);
	return Projection;
}

void Camera::MoveForward(float Distance)
{
	DirectX::XMVECTOR ForwardVector;
	DirectX::XMVECTOR PositionVector;

	ForwardVector = DirectX::XMLoadFloat3(&Forward);
	PositionVector = DirectX::XMLoadFloat3(&Position);

	PositionVector = DirectX::XMVectorAdd(PositionVector, DirectX::XMVectorScale(ForwardVector, Distance));
	DirectX::XMStoreFloat3(&Position, PositionVector);
}

void Camera::MoveRight(float Distance)
{
	DirectX::XMVECTOR ForwardVector = DirectX::XMLoadFloat3(&Forward);
	DirectX::XMVECTOR PositionVector = DirectX::XMLoadFloat3(&Position);
	DirectX::XMVECTOR UpVector = DirectX::XMLoadFloat3(&Up);
	DirectX::XMVECTOR RightVector = DirectX::XMLoadFloat3(&Right);

	PositionVector = DirectX::XMVectorAdd(PositionVector, DirectX::XMVectorScale(RightVector, Distance));

	DirectX::XMStoreFloat3(&Position, PositionVector);
}

void Camera::MoveUp(float Distance)
{
	DirectX::XMVECTOR PositionVector = DirectX::XMLoadFloat3(&Position);
	DirectX::XMVECTOR UpVector = DirectX::XMLoadFloat3(&Up);
	
	PositionVector = DirectX::XMVectorAdd(PositionVector, DirectX::XMVectorScale(UpVector, Distance));
	DirectX::XMStoreFloat3(&Position, PositionVector);
}

void Camera::AddRotation(float YawDelta, float PitchDelta)
{
	const float MaxPitch = DirectX::XMConvertToRadians(89.0f);
	
	Yaw += YawDelta;
	Pitch += PitchDelta;
	Pitch = std::clamp(Pitch, -MaxPitch, MaxPitch);

	UpdateDirection();
}

void Camera::UpdateDirection()
{
	DirectX::XMFLOAT3 NewForward;

	//cosf(float)가 cos(double)보다 빠를수있음
	NewForward.x = cosf(Pitch) * sinf(Yaw);
	NewForward.y = sinf(Pitch);
	NewForward.z = cosf(Pitch) * cosf(Yaw);

	//forward 수정
	DirectX::XMVECTOR NewForwardVector = DirectX::XMLoadFloat3(&NewForward);
	DirectX::XMStoreFloat3(&Forward, DirectX::XMVector3Normalize(NewForwardVector));
	
	//right 수정
	const DirectX::XMVECTOR WorldUp = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	DirectX::XMVECTOR NewRightVector = DirectX::XMVector3Cross(WorldUp, DirectX::XMVector3Normalize(NewForwardVector));
	NewRightVector = DirectX::XMVector3Normalize(NewRightVector);
	DirectX::XMStoreFloat3(&Right, NewRightVector);

	//up수정
	DirectX::XMVECTOR NewUp = DirectX::XMVector3Cross(NewForwardVector, NewRightVector);
	NewUp = DirectX::XMVector3Normalize(NewUp);
	DirectX::XMStoreFloat3(&Up, NewUp);
}
