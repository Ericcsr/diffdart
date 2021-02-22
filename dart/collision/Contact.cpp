/*
 * Copyright (c) 2011-2019, The DART development contributors
 * All rights reserved.
 *
 * The list of contributors can be found at:
 *   https://github.com/dartsim/dart/blob/master/LICENSE
 *
 * This file is provided under the following "BSD-style" License:
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *   CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 *   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 *   USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 *   AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *   ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *   POSSIBILITY OF SUCH DAMAGE.
 */

#include "dart/collision/Contact.hpp"

namespace dart {
namespace collision {

//==============================================================================
Contact::Contact()
  : point(Eigen::Vector3d::Zero()),
    normal(Eigen::Vector3d::Zero()),
    force(Eigen::Vector3d::Zero()),
    collisionObject1(nullptr),
    collisionObject2(nullptr),
    penetrationDepth(0),
    triID1(0),
    triID2(0),
    userData(nullptr),
    lcpResult(0),
    type(ContactType::UNSUPPORTED),
    // Poison all of the gradient related metadata to make it easier to detect
    // using uninitialized values
    edgeAClosestPoint(Eigen::Vector3d::Constant(NAN)),
    edgeAFixedPoint(Eigen::Vector3d::Constant(NAN)),
    edgeADir(Eigen::Vector3d::Constant(NAN)),
    edgeBClosestPoint(Eigen::Vector3d::Constant(NAN)),
    edgeBFixedPoint(Eigen::Vector3d::Constant(NAN)),
    edgeBDir(Eigen::Vector3d::Constant(NAN)),
    sphereCenter(Eigen::Vector3d::Constant(NAN)),
    sphereRadius(NAN),
    pipeDir(Eigen::Vector3d::Constant(NAN)),
    pipeClosestPoint(Eigen::Vector3d::Constant(NAN)),
    pipeFixedPoint(Eigen::Vector3d::Constant(NAN)),
    pipeRadius(NAN),
    vertexPoint(Eigen::Vector3d::Constant(NAN)),
    face1Locked(false),
    face1Normal(Eigen::Vector3d::Constant(NAN)),
    face2Locked(false),
    face2Normal(Eigen::Vector3d::Constant(NAN)),
    face3Locked(false),
    face3Normal(Eigen::Vector3d::Constant(NAN)),
    centerA(Eigen::Vector3d::Constant(NAN)),
    radiusA(NAN),
    centerB(Eigen::Vector3d::Constant(NAN)),
    radiusB(NAN)
{
  // TODO(MXG): Consider using NaN instead of zero for uninitialized quantities
  // Do nothing
}

//==============================================================================
bool Contact::isZeroNormal(const Eigen::Vector3d& normal)
{
  if (normal.squaredNorm() < getNormalEpsilonSquared())
    return true;
  else
    return false;
}

//==============================================================================
bool Contact::isNonZeroNormal(const Eigen::Vector3d& normal)
{
  return !isZeroNormal(normal);
}

} // namespace collision
} // namespace dart
