#pragma once

#include <cmath>
#include <cstring>
#include <array>
#include <algorithm>

namespace Aetherion::Core::Math
{
    inline void Mat4Identity(float out[16])
    {
        std::memset(out, 0, sizeof(float) * 16);
        out[0] = 1.0f;
        out[5] = 1.0f;
        out[10] = 1.0f;
        out[15] = 1.0f;
    }

    inline void Mat4Mul(float out[16], const float a[16], const float b[16])
    {
        float r[16];
        for (int c = 0; c < 4; ++c)
        {
            for (int rIdx = 0; rIdx < 4; ++rIdx)
            {
                r[c * 4 + rIdx] = a[0 * 4 + rIdx] * b[c * 4 + 0] + a[1 * 4 + rIdx] * b[c * 4 + 1] +
                                  a[2 * 4 + rIdx] * b[c * 4 + 2] + a[3 * 4 + rIdx] * b[c * 4 + 3];
            }
        }
        std::memcpy(out, r, sizeof(r));
    }

    inline void Mat4RotationX(float out[16], float radians)
    {
        Mat4Identity(out);
        const float c = std::cos(radians);
        const float s = std::sin(radians);
        out[5] = c;
        out[9] = -s;
        out[6] = s;
        out[10] = c;
    }

    inline void Mat4RotationY(float out[16], float radians)
    {
        Mat4Identity(out);
        const float c = std::cos(radians);
        const float s = std::sin(radians);
        out[0] = c;
        out[8] = s;
        out[2] = -s;
        out[10] = c;
    }

    inline void Mat4RotationZ(float out[16], float radians)
    {
        Mat4Identity(out);
        const float c = std::cos(radians);
        const float s = std::sin(radians);
        out[0] = c;
        out[4] = -s;
        out[1] = s;
        out[5] = c;
    }

    inline void Mat4Translation(float out[16], float x, float y, float z)
    {
        Mat4Identity(out);
        out[12] = x;
        out[13] = y;
        out[14] = z;
    }

    inline void Mat4Scale(float out[16], float x, float y, float z)
    {
        Mat4Identity(out);
        out[0] = x;
        out[5] = y;
        out[10] = z;
    }

    inline void Mat4Compose(float out[16],
                     float tx, float ty, float tz,
                     float rx, float ry, float rz, // radians
                     float sx, float sy, float sz)
    {
        const float cx = std::cos(rx);
        const float sx_sin = std::sin(rx);
        const float cy = std::cos(ry);
        const float sy_sin = std::sin(ry);
        const float cz = std::cos(rz);
        const float sz_sin = std::sin(rz);

        const float cxsy = cx * sy_sin;
        const float sxsy = sx_sin * sy_sin;

        out[0] = (cy * cz) * sx;
        out[4] = (cz * sxsy - cx * sz_sin) * sy;
        out[8] = (cxsy * cz + sx_sin * sz_sin) * sz;
        out[12] = tx;

        out[1] = (cy * sz_sin) * sx;
        out[5] = (cx * cz + sxsy * sz_sin) * sy;
        out[9] = (-cz * sx_sin + cxsy * sz_sin) * sz;
        out[13] = ty;

        out[2] = (-sy_sin) * sx;
        out[6] = (cy * sx_sin) * sy;
        out[10] = (cx * cy) * sz;
        out[14] = tz;

        out[3] = 0.0f;
        out[7] = 0.0f;
        out[11] = 0.0f;
        out[15] = 1.0f;
    }

    inline void Vec3Normalize(float v[3])
    {
        const float lenSq = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
        if (lenSq <= 0.0f) return;
        const float invLen = 1.0f / std::sqrt(lenSq);
        v[0] *= invLen; v[1] *= invLen; v[2] *= invLen;
    }

    inline void Vec3Cross(float out[3], const float a[3], const float b[3])
    {
        out[0] = a[1] * b[2] - a[2] * b[1];
        out[1] = a[2] * b[0] - a[0] * b[2];
        out[2] = a[0] * b[1] - a[1] * b[0];
    }

    inline float Vec3Dot(const float a[3], const float b[3])
    {
        return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    }
}
