/*
    Lightmetrica - A modern, research-oriented renderer

    Copyright (c) 2015 Hisanari Otsu

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#pragma once

#include <lightmetrica/macros.h>
#include <lightmetrica/math.h>
#include <lightmetrica/bound.h>
#include <lightmetrica/spectrum.h>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/unordered_map.hpp>

LM_NAMESPACE_BEGIN

// --------------------------------------------------------------------------------

#pragma region Serialization support for math functions
template <typename Archive> auto serialize(Archive& ar, Vec2& v) { ar(v.x, v.y); }
template <typename Archive> auto serialize(Archive& ar, Vec3& v) { ar(v.x, v.y, v.z); }
template <typename Archive> auto serialize(Archive& ar, Vec4& v) { ar(v.x, v.y, v.z, v.w); }
template <typename Archive> auto serialize(Archive& ar, Mat2& m) { ar(m.v_[0], m.v_[1]); }
template <typename Archive> auto serialize(Archive& ar, Mat3& m) { ar(m.v_[0], m.v_[1], m.v_[2]); }
template <typename Archive> auto serialize(Archive& ar, Mat4& m) { ar(m.v_[0], m.v_[1], m.v_[2], m.v_[3]); }
template <typename Archive> auto serialize(Archive& ar, Bound& b) { ar(b.min, b.max); }
template <typename Archive> auto serialize(Archive& ar, SphereBound& b) { ar(b.center, b.radius); }
#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Serialization support for spectrum class
template <typename Archive> auto serialize(Archive& ar, SPD& s) { ar(s.v); }
#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Serializable internal types
struct SerializablePrimitive
{
    std::string id;
    int index;
    Mat4 transform;
    Mat3 normalTransform;
    
    // Pointer to assets are serialized by asset index
    int meshID;
    int bsdfID;
    int emitterID;
    int lightID;
    int sensorID;

    template <typename Archive>
    auto serialize(Archive& ar) -> void
    {
        ar(id, index, transform, normalTransform, meshID, bsdfID, emitterID, lightID, sensorID);
    }
};
#pragma endregion

LM_NAMESPACE_END
