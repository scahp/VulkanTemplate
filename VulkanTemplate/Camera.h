#include "Matrix.h"

namespace jCameraUtil
{
	Matrix CreateViewMatrix(const Vector& pos, const Vector& target, const Vector& up);

	Matrix CreatePerspectiveMatrix(float width, float height, float fov, float farDist, float nearDist);
	Matrix CreatePerspectiveMatrixFarAtInfinity(float width, float height, float fov, float nearDist);

	Matrix CreateOrthogonalMatrix(float width, float height, float farDist, float nearDist);
	Matrix CreateOrthogonalMatrix(float left, float right, float top, float bottom, float farDist, float nearDist);
}
