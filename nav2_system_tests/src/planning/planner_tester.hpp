// Copyright (c) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License. Reserved.

#ifndef PLANNING__PLANNER_TESTER_HPP_
#define PLANNING__PLANNER_TESTER_HPP_

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "nav2_msgs/action/compute_path_to_pose.hpp"
#include "nav_msgs/msg/occupancy_grid.hpp"
#include "nav2_msgs/msg/costmap.hpp"
#include "nav2_msgs/srv/get_costmap.hpp"
#include "visualization_msgs/msg/marker.hpp"
#include "nav2_util/costmap.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "geometry_msgs/msg/transform_stamped.hpp"
#include "tf2_msgs/msg/tf_message.hpp"

namespace nav2_system_tests
{

enum class TaskStatus : int8_t
{
  SUCCEEDED = 1,
  FAILED = 2,
  RUNNING = 3,
};

class PlannerTester : public rclcpp::Node, public ::testing::Test
{
public:
  using ComputePathToPoseCommand = geometry_msgs::msg::PoseStamped;
  using ComputePathToPoseResult = nav2_msgs::msg::Path;

  PlannerTester();
  ~PlannerTester();

  // Loads the provided map and and generates a costmap from it.
  void loadDefaultMap();

  // Alternatively, use a preloaded 10x10 costmap
  void loadSimpleCostmap(const nav2_util::TestCostmap & testCostmapType);

  // Sends the request to the planner and gets the result.
  // Uses the user provided robot position and goal.
  // A map should be loaded before calling this method.
  // Success criteria is a collision free path.
  // TODO(orduno): #443 Assuming a robot the size of a costmap cell
  bool plannerTest(
    const geometry_msgs::msg::Point & robot_position,
    const ComputePathToPoseCommand & goal,
    ComputePathToPoseResult & path);

  // Sends the request to the planner and gets the result.
  // Uses the default map or preloaded costmaps.
  // Success criteria is a collision free path and a deviation to a
  // reference path smaller than a tolerance.
  bool defaultPlannerTest(
    ComputePathToPoseResult & path,
    const double deviation_tolerance = 1.0);

  bool defaultPlannerRandomTests(
    const unsigned int number_tests,
    const float acceptable_fail_ratio);

  // Sends a cancel command to the Planner
  bool sendCancel();

private:
  void setCostmap();

  void startRobotPoseProvider();
  void startCostmapServer();

  TaskStatus sendRequest(
    const ComputePathToPoseCommand & goal,
    ComputePathToPoseResult & path
  );

  bool isCollisionFree(const ComputePathToPoseResult & path);

  bool isWithinTolerance(
    const geometry_msgs::msg::Point & robot_position,
    const ComputePathToPoseCommand & goal,
    const ComputePathToPoseResult & path) const;

  bool isWithinTolerance(
    const geometry_msgs::msg::Point & robot_position,
    const ComputePathToPoseCommand & goal,
    const ComputePathToPoseResult & path,
    const double deviationTolerance,
    const ComputePathToPoseResult & reference_path) const;

  void printPath(const ComputePathToPoseResult & path) const;

  // The static map
  std::shared_ptr<nav_msgs::msg::OccupancyGrid> map_;

  // The costmap representation of the static map
  std::unique_ptr<nav2_util::Costmap> costmap_;

  // The interface to the global planner
  std::shared_ptr<rclcpp_action::Client<nav2_msgs::action::ComputePathToPose>> planner_client_;
  std::string plannerName_;
  void waitForPlanner();

  // The tester must provide the costmap service
  rclcpp::Service<nav2_msgs::srv::GetCostmap>::SharedPtr costmap_server_;

  // The tester must provide the robot pose through a transform
  rclcpp::Publisher<tf2_msgs::msg::TFMessage>::SharedPtr transform_publisher_;

  void updateRobotPosition(const geometry_msgs::msg::Point & position);

  // Occupancy grid publisher for visualization
  rclcpp::Publisher<nav_msgs::msg::OccupancyGrid>::SharedPtr map_pub_;
  rclcpp::TimerBase::SharedPtr map_timer_;
  rclcpp::WallRate map_publish_rate_;
  void mapCallback();

  bool map_set_;
  bool costmap_set_;
  bool using_fake_costmap_;
  bool costmap_server_running_;

  // Parameters of the costmap
  bool trinary_costmap_;
  bool track_unknown_space_;
  int lethal_threshold_;
  int unknown_cost_value_;
  nav2_util::TestCostmap testCostmapType_;

  // A thread for spinning the ROS node
  void spinThread();
  std::thread * spin_thread_;
  rclcpp::executors::SingleThreadedExecutor executor_;
};

}  // namespace nav2_system_tests

#endif  // PLANNING__PLANNER_TESTER_HPP_
