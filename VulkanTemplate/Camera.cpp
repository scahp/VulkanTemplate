#include <pch.h>
#include "Camera.h"

Matrix jCameraUtil::CreateViewMatrix(const Vector& pos, const Vector& target, const Vector& up)
{
	const auto zAxis = (target - pos).GetNormalize();
	auto yAxis = (up - pos).GetNormalize();
	const auto xAxis = zAxis.CrossProduct(yAxis).GetNormalize();
	yAxis = xAxis.CrossProduct(zAxis).GetNormalize();

	Matrix InvRot{ IdentityType };
	InvRot.m[0][0] = xAxis.x;
	InvRot.m[0][1] = xAxis.y;
	InvRot.m[0][2] = xAxis.z;
	InvRot.m[1][0] = yAxis.x;
	InvRot.m[1][1] = yAxis.y;
	InvRot.m[1][2] = yAxis.z;
	InvRot.m[2][0] = -zAxis.x;
	InvRot.m[2][1] = -zAxis.y;
	InvRot.m[2][2] = -zAxis.z;

	// auto InvPos = Matrix::MakeTranslate(-pos.x, -pos.y, -pos.z);
	// return InvRot * InvPos;

	auto InvPos = Vector4(-pos.x, -pos.y, -pos.z, 1.0);
	InvRot.m[0][3] = InvRot.GetRow(0).DotProduct(InvPos);
	InvRot.m[1][3] = InvRot.GetRow(1).DotProduct(InvPos);
	InvRot.m[2][3] = InvRot.GetRow(2).DotProduct(InvPos);
	return InvRot;
}

Matrix jCameraUtil::CreatePerspectiveMatrix(float width, float height, float fov, float farDist, float nearDist)
{
	const float F = 1.0f / tanf(fov / 2.0f);
	const float farSubNear = (farDist - nearDist);

	Matrix projMat;
	projMat.m[0][0] = F * (height / width); projMat.m[0][1] = 0.0f;      projMat.m[0][2] = 0.0f;									projMat.m[0][3] = 0.0f;
	projMat.m[1][0] = 0.0f;					projMat.m[1][1] = F;         projMat.m[1][2] = 0.0f;									projMat.m[1][3] = 0.0f;
	projMat.m[2][0] = 0.0f;					projMat.m[2][1] = 0.0f;      projMat.m[2][2] = -(farDist + nearDist) / farSubNear;		projMat.m[2][3] = -(2.0f * nearDist * farDist) / farSubNear;
	projMat.m[3][0] = 0.0f;					projMat.m[3][1] = 0.0f;      projMat.m[3][2] = -1.0f;									projMat.m[3][3] = 0.0f;
	return projMat;
}

Matrix jCameraUtil::CreatePerspectiveMatrixFarAtInfinity(float width, float height, float fov, float nearDist)
{
	const float F = 1.0f / tanf(fov / 2.0f);

	Matrix projMat;
	projMat.m[0][0] = F * (height / width); projMat.m[0][1] = 0.0f;      projMat.m[0][2] = 0.0f;                      projMat.m[0][3] = 0.0f;
	projMat.m[1][0] = 0.0f;					projMat.m[1][1] = F;         projMat.m[1][2] = 0.0f;                      projMat.m[1][3] = 0.0f;
	projMat.m[2][0] = 0.0f;					projMat.m[2][1] = 0.0f;      projMat.m[2][2] = -1.0f;                     projMat.m[2][3] = -(2.0f * nearDist);
	projMat.m[3][0] = 0.0f;					projMat.m[3][1] = 0.0f;      projMat.m[3][2] = -1.0f;                     projMat.m[3][3] = 0.0f;
	return projMat;
}

Matrix jCameraUtil::CreateOrthogonalMatrix(float width, float height, float farDist, float nearDist)
{
	const float farSubNear = (farDist - nearDist);
	const float halfWidth = width * 0.5f;
	const float halfHeight = height * 0.5f;

	Matrix projMat;
	projMat.m[0][0] = 1.0f / halfWidth;     projMat.m[0][1] = 0.0f;                  projMat.m[0][2] = 0.0f;                      projMat.m[0][3] = 0.0f;
	projMat.m[1][0] = 0.0f;					projMat.m[1][1] = 1.0f / halfHeight;     projMat.m[1][2] = 0.0f;                      projMat.m[1][3] = 0.0f;
	projMat.m[2][0] = 0.0f;					projMat.m[2][1] = 0.0f;                  projMat.m[2][2] = -2.0f / farSubNear;        projMat.m[2][3] = -(farDist + nearDist) / farSubNear;
	projMat.m[3][0] = 0.0f;					projMat.m[3][1] = 0.0f;                  projMat.m[3][2] = 0.0f;                      projMat.m[3][3] = 1.0f;
	return projMat;
}

Matrix jCameraUtil::CreateOrthogonalMatrix(float left, float right, float top, float bottom, float farDist, float nearDist)
{
	const float fsn = (farDist - nearDist);
	const float rsl = (right - left);
	const float tsb = (top - bottom);

	Matrix projMat;
	projMat.m[0][0] = 2.0f / rsl;		 projMat.m[0][1] = 0.0f;                  projMat.m[0][2] = 0.0f;                   projMat.m[0][3] = -(right + left) / rsl;
	projMat.m[1][0] = 0.0f;              projMat.m[1][1] = 2.0f / tsb;		      projMat.m[1][2] = 0.0f;                   projMat.m[1][3] = -(top + bottom) / tsb;
	projMat.m[2][0] = 0.0f;              projMat.m[2][1] = 0.0f;                  projMat.m[2][2] = -2.0f / fsn;			projMat.m[2][3] = -(farDist + nearDist) / fsn;
	projMat.m[3][0] = 0.0f;              projMat.m[3][1] = 0.0f;                  projMat.m[3][2] = 0.0f;                   projMat.m[3][3] = 1.0f;
	return projMat;
}
