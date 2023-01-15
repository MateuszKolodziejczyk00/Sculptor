#pragma once

#pragma warning(push)
#pragma warning(disable: 5054)

#include "Eigen/Geometry"

#pragma warning(pop)

namespace spt::math
{

using namespace Eigen;

using Vector2u = Matrix<uint32_t, 2, 1>;
using Vector3u = Matrix<uint32_t, 3, 1>;
using Vector4u = Matrix<uint32_t, 4, 1>;

using AlignedBox2u = AlignedBox<uint32_t, 2>;
using AlignedBox3u = AlignedBox<uint32_t, 3>;
using AlignedBox4u = AlignedBox<uint32_t, 4>;

}