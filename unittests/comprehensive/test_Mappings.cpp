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

#include <iostream>

#include <gtest/gtest.h>

#include "dart/collision/CollisionObject.hpp"
#include "dart/collision/Contact.hpp"
#include "dart/dynamics/BodyNode.hpp"
#include "dart/dynamics/RevoluteJoint.hpp"
#include "dart/dynamics/Skeleton.hpp"
#include "dart/math/Geometry.hpp"
#include "dart/neural/BackpropSnapshot.hpp"
#include "dart/neural/ConstrainedGroupGradientMatrices.hpp"
#include "dart/neural/DifferentiableContactConstraint.hpp"
#include "dart/neural/NeuralConstants.hpp"
#include "dart/neural/NeuralUtils.hpp"
#include "dart/neural/RestorableSnapshot.hpp"
#include "dart/simulation/World.hpp"

#include "GradientTestUtils.hpp"
#include "TestHelpers.hpp"
#include "stdio.h"

using namespace dart;
using namespace math;
using namespace dynamics;
using namespace simulation;
using namespace neural;

#define ALL_TESTS

void testWorldSpace(std::size_t numLinks)
{
  srand(42);

  // World
  WorldPtr world = World::create();
  world->setGravity(Eigen::Vector3d(0, -9.81, 0));

  SkeletonPtr arm = Skeleton::create("arm");

  std::pair<WeldJoint*, BodyNode*> rootJointPair
      = arm->createJointAndBodyNodePair<WeldJoint>(nullptr);
  Eigen::Isometry3d rootTransform = Eigen::Isometry3d::Identity();
  rootTransform.linear()
      = eulerXYZToMatrix(Eigen::Vector3d(90.0 * 3.14159 / 180.0, 0, 0));
  rootJointPair.first->setTransformFromParentBodyNode(rootTransform);

  BodyNode* parent = rootJointPair.second;

  for (std::size_t i = 0; i < numLinks; i++)
  {
    RevoluteJoint::Properties jointProps;
    jointProps.mName = "revolute_" + std::to_string(i);
    BodyNode::Properties bodyProps;
    bodyProps.mName = "arm_" + std::to_string(i);
    std::pair<RevoluteJoint*, BodyNode*> jointPair
        = arm->createJointAndBodyNodePair<RevoluteJoint>(
            parent, jointProps, bodyProps);
    if (parent != nullptr)
    {
      Eigen::Isometry3d armOffset = Eigen::Isometry3d::Identity();
      armOffset.translation() = Eigen::Vector3d(0, 1.0, 0);
      jointPair.first->setTransformFromParentBodyNode(armOffset);

      Eigen::Isometry3d bodyOffset = Eigen::Isometry3d::Identity();
      bodyOffset.translation() = Eigen::Vector3d(0, -1.0, 0);
      jointPair.first->setTransformFromChildBodyNode(bodyOffset);
    }
    jointPair.second->setMass(1.0);
    jointPair.first->setAxis(Eigen::Vector3d(1, 0, 0));
    parent = jointPair.second;
  }

  world->addSkeleton(arm);

  SkeletonPtr floor = Skeleton::create("floor");
  // std::pair<WeldJoint*, BodyNode*> floorJointPair =
  floor->createJointAndBodyNodePair<WeldJoint>(nullptr);
  world->addSkeleton(floor);

  EXPECT_TRUE(verifyWorldSpaceTransform(world));
  EXPECT_TRUE(verifyIKMapping(world));
}

#ifdef ALL_TESTS
TEST(GRADIENTS, WORLD_SPACE_5_LINK_ROBOT)
{
  testWorldSpace(5);
}
#endif

/******************************************************************************

This test sets up a configuration that looks something like this:

   O-----O
   |     |
   |     |
   O

It's a robot arm, with a rotating base, and 3 links. Each link is of unit
length. This is supposed to make it easier to reason about geometry Jacobians.
*/
void testSimple3Link()
{
  // World
  WorldPtr world = World::create();
  world->setGravity(Eigen::Vector3d(0, -9.81, 0));

  SkeletonPtr arm = Skeleton::create("arm");

  std::pair<RevoluteJoint*, BodyNode*> zerothJointPair
      = arm->createJointAndBodyNodePair<RevoluteJoint>(nullptr);
  zerothJointPair.first->setAxis(Eigen::Vector3d::UnitZ());
  Eigen::Isometry3d zerothOffset = Eigen::Isometry3d::Identity();
  zerothOffset.translation() = Eigen::Vector3d(0, 1.0, 0);
  zerothJointPair.first->setTransformFromChildBodyNode(zerothOffset);

  std::pair<RevoluteJoint*, BodyNode*> firstJointPair
      = arm->createJointAndBodyNodePair<RevoluteJoint>(zerothJointPair.second);
  firstJointPair.first->setAxis(Eigen::Vector3d::UnitZ());
  Eigen::Isometry3d firstOffset = Eigen::Isometry3d::Identity();
  firstOffset.translation() = Eigen::Vector3d(1.0, 0.0, 0);
  firstJointPair.first->setTransformFromChildBodyNode(firstOffset);

  std::pair<RevoluteJoint*, BodyNode*> secondJointPair
      = arm->createJointAndBodyNodePair<RevoluteJoint>(firstJointPair.second);
  secondJointPair.first->setAxis(Eigen::Vector3d::UnitZ());
  Eigen::Isometry3d secondOffset = Eigen::Isometry3d::Identity();
  secondOffset.translation() = Eigen::Vector3d(0, -1.0, 0);
  secondJointPair.first->setTransformFromChildBodyNode(secondOffset);

  Eigen::MatrixXd expectedJac = Eigen::MatrixXd::Zero(9, 3);
  // clang-format off
  expectedJac <<
      /* Body 1 X */ 1, 0, 0, 
      /* Body 1 Y */ 0, 0, 0, 
      /* Body 1 Z */ 0, 0, 0,
      /* Body 2 X */ 1, 0, 0,
      /* Body 2 Y */ -1, -1, 0,
      /* Body 2 Z */ 0, 0, 0,
      /* Body 3 X */ 0, -1, -1,
      /* Body 3 Y */ -1, -1, 0,
      /* Body 3 Z */ 0, 0, 0;
  // clang-format on
  Eigen::MatrixXd analyticalJac
      = jointPosToWorldLinearJacobian(arm, arm->getBodyNodes());

  if (!equals(analyticalJac, expectedJac, 1e-5))
  {
    std::cout << "Expected: \n"
              << expectedJac << "\nAnalytical: \n"
              << analyticalJac << "\nDiff: \n"
              << (expectedJac - analyticalJac) << "\n";
  }
  EXPECT_TRUE(equals(analyticalJac, expectedJac));

  world->addSkeleton(arm);

  EXPECT_TRUE(verifyLinearJacobian(
      world, Eigen::Vector3d::Zero(), Eigen::Vector3d::Zero()));
  EXPECT_TRUE(verifySpatialJacobian(
      world, Eigen::Vector3d::Zero(), Eigen::Vector3d::Zero()));
  EXPECT_TRUE(verifyIKMapping(world));
}

#ifdef ALL_TESTS
TEST(GRADIENTS, WORLD_SPACE_SIMPLE_LINK)
{
  testSimple3Link();
}
#endif

void testWorldSpaceWithBoxes(int jointType)
{
  // World
  WorldPtr world = World::create();
  world->setGravity(Eigen::Vector3d(0, -9.81, 0));

  SkeletonPtr boxes = Skeleton::create("boxes");

  // We set up the root so that it's off by 90 deg on the x-axis, establishing
  // a non-world root frame

  std::pair<WeldJoint*, BodyNode*> rootJointPair
      = boxes->createJointAndBodyNodePair<WeldJoint>(nullptr);
  WeldJoint* rootJoint = rootJointPair.first;
  BodyNode* rootBody = rootJointPair.second;
  Eigen::Isometry3d rootTransform = Eigen::Isometry3d::Identity();
  rootTransform.linear()
      = eulerXYZToMatrix(Eigen::Vector3d(90.0 * M_PI / 180.0, 0, 0));
  rootJoint->setTransformFromParentBodyNode(rootTransform);

  // Then we add a box, on a free transform from the root. This is now a
  // different frame than the world, by 90 deg on the x axis.

  Eigen::Isometry3d fromChild = Eigen::Isometry3d::Identity();
  fromChild.translation() = Eigen::Vector3d::Ones();
  if (jointType == 0)
  {
    std::pair<TranslationalJoint*, BodyNode*> boxJointPair
        = boxes->createJointAndBodyNodePair<TranslationalJoint>(rootBody);
    boxJointPair.first->setTransformFromChildBodyNode(fromChild);
  }
  else if (jointType == 1)
  {
    std::pair<BallJoint*, BodyNode*> boxJointPair
        = boxes->createJointAndBodyNodePair<BallJoint>(rootBody);
    boxJointPair.first->setTransformFromChildBodyNode(fromChild);
  }
  else if (jointType == 2)
  {
    std::pair<FreeJoint*, BodyNode*> boxJointPair
        = boxes->createJointAndBodyNodePair<FreeJoint>(rootBody);
    boxJointPair.first->setTransformFromChildBodyNode(fromChild);
  }
  boxes->setPositions(Eigen::VectorXd::Zero(boxes->getNumDofs()));

  world->addSkeleton(boxes);

  EXPECT_TRUE(verifyWorldSpaceTransform(world));
  EXPECT_TRUE(verifyIKMapping(world));
}

#ifdef ALL_TESTS
TEST(GRADIENTS, WORLD_SPACE_BOXES_TRANSLATION_JOINT)
{
  testWorldSpaceWithBoxes(0);
}
#endif

#ifdef ALL_TESTS
TEST(GRADIENTS, WORLD_SPACE_BOXES_BALL_JOINT)
{
  testWorldSpaceWithBoxes(1);
}
#endif

#ifdef ALL_TESTS
TEST(GRADIENTS, WORLD_SPACE_BOXES_FREE_JOINT)
{
  testWorldSpaceWithBoxes(2);
}
#endif