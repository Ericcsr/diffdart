#ifndef DART_TRAJECTORY_ROLLOUT_HPP_
#define DART_TRAJECTORY_ROLLOUT_HPP_

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <Eigen/Dense>

#include "dart/neural/Mapping.hpp"
#include "dart/proto/TrajectoryRollout.pb.h"

namespace dart {

namespace simulation {
class World;
}

namespace trajectory {

class Problem;
class TrajectoryRolloutReal;
class TrajectoryRolloutRef;
class TrajectoryRolloutConstRef;

class TrajectoryRollout
{
public:
  virtual ~TrajectoryRollout();

  virtual const std::string& getRepresentationMapping() const = 0;
  virtual const std::vector<std::string>& getMappings() const = 0;

  virtual Eigen::Ref<Eigen::MatrixXd> getPoses(
      const std::string& mapping = "identity")
      = 0;
  virtual Eigen::Ref<Eigen::MatrixXd> getVels(
      const std::string& mapping = "identity")
      = 0;
  virtual Eigen::Ref<Eigen::MatrixXd> getForces(
      const std::string& mapping = "identity")
      = 0;
  virtual Eigen::Ref<Eigen::VectorXd> getMasses() = 0;

  virtual const Eigen::Ref<const Eigen::MatrixXd> getPosesConst(
      const std::string& mapping = "identity") const = 0;
  virtual const Eigen::Ref<const Eigen::MatrixXd> getVelsConst(
      const std::string& mapping = "identity") const = 0;
  virtual const Eigen::Ref<const Eigen::MatrixXd> getForcesConst(
      const std::string& mapping = "identity") const = 0;
  virtual const Eigen::Ref<const Eigen::VectorXd> getMassesConst() const = 0;

  virtual const std::unordered_map<std::string, Eigen::MatrixXd>&
  getMetadataMap() const = 0;
  virtual Eigen::MatrixXd getMetadata(const std::string& key) const = 0;
  virtual void setMetadata(const std::string& key, Eigen::MatrixXd value) = 0;

  /// This returns a trajectory rollout ref, corresponding to a slice
  /// of this trajectory rollout
  TrajectoryRolloutRef slice(int start, int len);

  /// This returns a trajectory rollout ref, corresponding to a slice
  /// of this trajectory rollout
  const TrajectoryRolloutConstRef sliceConst(int start, int len) const;

  /// This returns a copy of the trajectory rollout
  TrajectoryRollout* copy() const;

  /// This formats the rollout as JSON, which can be sent to the frontend to be
  /// parsed and displayed.
  std::string toJson(std::shared_ptr<simulation::World> world) const;

  /// This writes us out to a protobuf
  void serialize(proto::TrajectoryRollout& proto) const;

  /// This decodes a protobuf
  static TrajectoryRolloutReal deserialize(
      const proto::TrajectoryRollout& proto);

  /// This creates a rollout from forces over time
  static TrajectoryRolloutReal fromForces(
      std::shared_ptr<simulation::World> world,
      Eigen::VectorXd startPos,
      Eigen::VectorXd startVel,
      std::vector<Eigen::VectorXd> forces);

  /// This creates a rollout from poses over time
  static TrajectoryRolloutReal fromPoses(
      std::shared_ptr<simulation::World> world,
      std::vector<Eigen::VectorXd> poses);
};

class TrajectoryRolloutReal : public TrajectoryRollout
{
public:
  /// Fresh copy constructior
  TrajectoryRolloutReal(
      const std::unordered_map<std::string, std::shared_ptr<neural::Mapping>>
          mappings,
      int steps,
      std::string representationMapping,
      int massDim,
      const std::unordered_map<std::string, Eigen::MatrixXd> metadata);

  /// Create a fresh trajector rollout for a shot
  TrajectoryRolloutReal(Problem* shot);

  /// Deep copy constructor
  TrajectoryRolloutReal(const TrajectoryRollout* copy);

  /// Raw constructor
  TrajectoryRolloutReal(
      std::string representationMapping,
      const std::unordered_map<std::string, Eigen::MatrixXd> pos,
      const std::unordered_map<std::string, Eigen::MatrixXd> vel,
      const std::unordered_map<std::string, Eigen::MatrixXd> force,
      const Eigen::VectorXd mass,
      const std::unordered_map<std::string, Eigen::MatrixXd> metadata);

  const std::string& getRepresentationMapping() const override;
  const std::vector<std::string>& getMappings() const override;
  Eigen::Ref<Eigen::MatrixXd> getPoses(
      const std::string& mapping = "identity") override;
  Eigen::Ref<Eigen::MatrixXd> getVels(
      const std::string& mapping = "identity") override;
  Eigen::Ref<Eigen::MatrixXd> getForces(
      const std::string& mapping = "identity") override;
  Eigen::Ref<Eigen::VectorXd> getMasses() override;
  const Eigen::Ref<const Eigen::MatrixXd> getPosesConst(
      const std::string& mapping = "identity") const override;
  const Eigen::Ref<const Eigen::MatrixXd> getVelsConst(
      const std::string& mapping = "identity") const override;
  const Eigen::Ref<const Eigen::MatrixXd> getForcesConst(
      const std::string& mapping = "identity") const override;
  const Eigen::Ref<const Eigen::VectorXd> getMassesConst() const override;

  virtual const std::unordered_map<std::string, Eigen::MatrixXd>&
  getMetadataMap() const override;
  virtual Eigen::MatrixXd getMetadata(const std::string& key) const override;
  virtual void setMetadata(
      const std::string& key, Eigen::MatrixXd value) override;

protected:
  std::unordered_map<std::string, Eigen::MatrixXd> mPoses;
  std::unordered_map<std::string, Eigen::MatrixXd> mVels;
  std::unordered_map<std::string, Eigen::MatrixXd> mForces;
  Eigen::VectorXd mMasses;
  std::unordered_map<std::string, Eigen::MatrixXd> mMetadata;
  std::string mRepresentationMapping;
  std::vector<std::string> mMappings;
};

class TrajectoryRolloutRef : public TrajectoryRollout
{
public:
  /// Slice constructor
  TrajectoryRolloutRef(TrajectoryRollout* toSlice, int start, int len);

  const std::string& getRepresentationMapping() const override;
  const std::vector<std::string>& getMappings() const override;
  Eigen::Ref<Eigen::MatrixXd> getPoses(
      const std::string& mapping = "identity") override;
  Eigen::Ref<Eigen::MatrixXd> getVels(
      const std::string& mapping = "identity") override;
  Eigen::Ref<Eigen::MatrixXd> getForces(
      const std::string& mapping = "identity") override;
  Eigen::Ref<Eigen::VectorXd> getMasses() override;
  const Eigen::Ref<const Eigen::MatrixXd> getPosesConst(
      const std::string& mapping = "identity") const override;
  const Eigen::Ref<const Eigen::MatrixXd> getVelsConst(
      const std::string& mapping = "identity") const override;
  const Eigen::Ref<const Eigen::MatrixXd> getForcesConst(
      const std::string& mapping = "identity") const override;
  const Eigen::Ref<const Eigen::VectorXd> getMassesConst() const override;

  virtual const std::unordered_map<std::string, Eigen::MatrixXd>&
  getMetadataMap() const override;
  virtual Eigen::MatrixXd getMetadata(const std::string& key) const override;
  virtual void setMetadata(
      const std::string& key, Eigen::MatrixXd value) override;

protected:
  TrajectoryRollout* mToSlice;
  int mStart;
  int mLen;
};

class TrajectoryRolloutConstRef : public TrajectoryRollout
{
public:
  /// Slice constructor
  TrajectoryRolloutConstRef(
      const TrajectoryRollout* toSlice, int start, int len);

  const std::string& getRepresentationMapping() const override;
  const std::vector<std::string>& getMappings() const override;
  Eigen::Ref<Eigen::MatrixXd> getPoses(
      const std::string& mapping = "identity") override;
  Eigen::Ref<Eigen::MatrixXd> getVels(
      const std::string& mapping = "identity") override;
  Eigen::Ref<Eigen::MatrixXd> getForces(
      const std::string& mapping = "identity") override;
  Eigen::Ref<Eigen::VectorXd> getMasses() override;
  const Eigen::Ref<const Eigen::MatrixXd> getPosesConst(
      const std::string& mapping = "identity") const override;
  const Eigen::Ref<const Eigen::MatrixXd> getVelsConst(
      const std::string& mapping = "identity") const override;
  const Eigen::Ref<const Eigen::MatrixXd> getForcesConst(
      const std::string& mapping = "identity") const override;
  const Eigen::Ref<const Eigen::VectorXd> getMassesConst() const override;

  virtual const std::unordered_map<std::string, Eigen::MatrixXd>&
  getMetadataMap() const override;
  virtual Eigen::MatrixXd getMetadata(const std::string& key) const override;
  virtual void setMetadata(
      const std::string& key, Eigen::MatrixXd value) override;

protected:
  const TrajectoryRollout* mToSlice;
  int mStart;
  int mLen;
};

} // namespace trajectory
} // namespace dart

#endif