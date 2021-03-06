#ifndef DART_NEURAL_IK_MAPPING_HPP_
#define DART_NEURAL_IK_MAPPING_HPP_

#include <memory>
#include <optional>
#include <string>

#include <Eigen/Dense>

#include "dart/dynamics/BodyNode.hpp"
#include "dart/dynamics/Skeleton.hpp"
#include "dart/neural/Mapping.hpp"

namespace dart {

namespace neural {

enum IKMappingEntryType
{
  NODE_SPATIAL,
  NODE_LINEAR,
  NODE_ANGULAR,
  COM
};

struct IKMappingEntry
{
  IKMappingEntryType type;
  std::string skelName;
  int bodyNodeOffset; // can be -1 for COM entries

  IKMappingEntry(IKMappingEntryType type, std::string skelName)
    : type(type), skelName(skelName), bodyNodeOffset(-1){};

  IKMappingEntry(IKMappingEntryType type, dynamics::BodyNode* node)
    : type(type),
      skelName(node->getSkeleton()->getName()),
      bodyNodeOffset(node->getIndexInSkeleton()){};
};

class IKMapping : public Mapping
{
public:
  IKMapping(std::shared_ptr<simulation::World> world);

  /// This adds the spatial (6D) coordinates of a body node to the list,
  /// increasing Dim size by 6
  void addSpatialBodyNode(dynamics::BodyNode* node);

  /// This adds the linear (3D) coordinates of a body node to the list,
  /// increasing Dim size by 3
  void addLinearBodyNode(dynamics::BodyNode* node);

  /// This adds the angular (3D) coordinates of a body node to the list,
  /// increasing Dim size by 3
  void addAngularBodyNode(dynamics::BodyNode* node);

  int getPosDim() override;
  int getVelDim() override;
  int getForceDim() override;
  int getMassDim() override;

  void setPositions(
      std::shared_ptr<simulation::World> world,
      const Eigen::Ref<Eigen::VectorXd>& positions) override;
  void setVelocities(
      std::shared_ptr<simulation::World> world,
      const Eigen::Ref<Eigen::VectorXd>& velocities) override;
  void setForces(
      std::shared_ptr<simulation::World> world,
      const Eigen::Ref<Eigen::VectorXd>& forces) override;
  void setMasses(
      std::shared_ptr<simulation::World> world,
      const Eigen::Ref<Eigen::VectorXd>& masses) override;

  void getPositionsInPlace(
      std::shared_ptr<simulation::World> world,
      /* OUT */ Eigen::Ref<Eigen::VectorXd> positions) override;
  void getVelocitiesInPlace(
      std::shared_ptr<simulation::World> world,
      /* OUT */ Eigen::Ref<Eigen::VectorXd> velocities) override;
  void getForcesInPlace(
      std::shared_ptr<simulation::World> world,
      /* OUT */ Eigen::Ref<Eigen::VectorXd> forces) override;
  void getMassesInPlace(
      std::shared_ptr<simulation::World> world,
      /* OUT */ Eigen::Ref<Eigen::VectorXd> masses) override;

  /// This gets a Jacobian relating the changes in the outer positions (the
  /// "mapped" positions) to inner positions (the "real" positions)
  Eigen::MatrixXd getMappedPosToRealPosJac(
      std::shared_ptr<simulation::World> world) override;

  /// This gets a Jacobian relating the changes in the inner positions (the
  /// "real" positions) to the corresponding outer positions (the "mapped"
  /// positions)
  Eigen::MatrixXd getRealPosToMappedPosJac(
      std::shared_ptr<simulation::World> world) override;

  /// This gets a Jacobian relating the changes in the inner velocities (the
  /// "real" velocities) to the corresponding outer positions (the "mapped"
  /// positions)
  Eigen::MatrixXd getRealVelToMappedPosJac(
      std::shared_ptr<simulation::World> world) override;

  /// This gets a Jacobian relating the changes in the outer velocity (the
  /// "mapped" velocity) to inner velocity (the "real" velocity)
  Eigen::MatrixXd getMappedVelToRealVelJac(
      std::shared_ptr<simulation::World> world) override;

  /// This gets a Jacobian relating the changes in the inner velocity (the
  /// "real" velocity) to the corresponding outer velocity (the "mapped"
  /// velocity)
  Eigen::MatrixXd getRealVelToMappedVelJac(
      std::shared_ptr<simulation::World> world) override;

  /// This gets a Jacobian relating the changes in the inner position (the
  /// "real" position) to the corresponding outer velocity (the "mapped"
  /// velocity)
  Eigen::MatrixXd getRealPosToMappedVelJac(
      std::shared_ptr<simulation::World> world) override;

  /// This gets a Jacobian relating the changes in the outer force (the
  /// "mapped" force) to inner force (the "real" force)
  Eigen::MatrixXd getMappedForceToRealForceJac(
      std::shared_ptr<simulation::World> world) override;

  /// This gets a Jacobian relating the changes in the inner force (the
  /// "real" force) to the corresponding outer force (the "mapped"
  /// force)
  Eigen::MatrixXd getRealForceToMappedForceJac(
      std::shared_ptr<simulation::World> world) override;

  /// This gets a Jacobian relating the changes in the outer mass (the
  /// "mapped" mass) to inner mass (the "real" mass)
  Eigen::MatrixXd getMappedMassToRealMassJac(
      std::shared_ptr<simulation::World> world) override;

  /// This gets a Jacobian relating the changes in the inner mass (the
  /// "real" mass) to the corresponding outer mass (the "mapped"
  /// mass)
  Eigen::MatrixXd getRealMassToMappedMassJac(
      std::shared_ptr<simulation::World> world) override;

  Eigen::VectorXd getPositionLowerLimits(
      std::shared_ptr<simulation::World> world) override;
  Eigen::VectorXd getPositionUpperLimits(
      std::shared_ptr<simulation::World> world) override;
  Eigen::VectorXd getVelocityLowerLimits(
      std::shared_ptr<simulation::World> world) override;
  Eigen::VectorXd getVelocityUpperLimits(
      std::shared_ptr<simulation::World> world) override;
  Eigen::VectorXd getForceLowerLimits(
      std::shared_ptr<simulation::World> world) override;
  Eigen::VectorXd getForceUpperLimits(
      std::shared_ptr<simulation::World> world) override;
  Eigen::VectorXd getMassLowerLimits(
      std::shared_ptr<simulation::World> world) override;
  Eigen::VectorXd getMassUpperLimits(
      std::shared_ptr<simulation::World> world) override;

protected:
  /// This returns the number of dimensions that the IK mapping represents.
  int getDim();

  /// Computes a Jacobian that transforms changes in joint angle to changes in
  /// IK body positions (expressed in log space).
  Eigen::MatrixXd getPosJacobian(std::shared_ptr<simulation::World> world);

  /// Computes the pseudo-inverse of the pos Jacobian
  Eigen::MatrixXd getPosJacobianInverse(
      std::shared_ptr<simulation::World> world);

  /// Computes a Jacobian that transforms changes in joint vel to changes in
  /// IK body vels (expressed in log space).
  Eigen::MatrixXd getVelJacobian(std::shared_ptr<simulation::World> world);

  /// Computes the pseudo-inverse of the vel Jacobian
  Eigen::MatrixXd getVelJacobianInverse(
      std::shared_ptr<simulation::World> world);

  /// Computes a Jacobian of J(x)*vel wrt pos
  Eigen::MatrixXd getJacobianOfJacVelWrtPosition(
      std::shared_ptr<simulation::World> world);

  /// The brute force version of getJacobianOfJacVelWrtPosition()
  Eigen::MatrixXd bruteForceJacobianOfJacVelWrtPosition(
      std::shared_ptr<simulation::World> world);

  std::vector<IKMappingEntry> mEntries;

  int mMassDim;
};

} // namespace neural
} // namespace dart

#endif