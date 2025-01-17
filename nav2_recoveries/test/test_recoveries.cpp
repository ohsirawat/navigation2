// Copyright (c) 2019 Intel Corporation
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

#include <string>
#include <memory>
#include <chrono>
#include <iostream>
#include <future>

#include "gtest/gtest.h"
#include "rclcpp/rclcpp.hpp"

#include "rclcpp_action/rclcpp_action.hpp"
#include "nav2_recoveries/recovery.hpp"
#include "nav2_msgs/action/dummy_recovery.hpp"

using nav2_recoveries::Recovery;
using nav2_recoveries::Status;
using RecoveryAction = nav2_msgs::action::DummyRecovery;
using ClientGoalHandle = rclcpp_action::ClientGoalHandle<RecoveryAction>;

using namespace std::chrono_literals;

// A recovery for testing the base class

class DummyRecovery : public Recovery<RecoveryAction>
{
public:
  explicit DummyRecovery(rclcpp::Node::SharedPtr & node, std::shared_ptr<tf2_ros::Buffer> & tf)
  : Recovery<RecoveryAction>(node, "Recovery", tf),
    initialized_(false) {}

  ~DummyRecovery() {}

  Status onRun(const std::shared_ptr<const RecoveryAction::Goal> goal) override
  {
    // A normal recovery would catch the command and initialize
    initialized_ = false;
    command_ = goal->command.data;
    start_time_ = std::chrono::system_clock::now();

    // onRun method can have various possible outcomes (success, failure, cancelled)
    // The output is defined by the tester class on the command string.
    if (command_ == "Testing success" || command_ == "Testing failure on run") {
      initialized_ = true;
      return Status::SUCCEEDED;
    }

    return Status::FAILED;
  }

  Status onCycleUpdate() override
  {
    // A normal recovery would set the robot in motion in the first call
    // and check for robot states on subsequent calls to check if the movement
    // was completed.

    if (command_ != "Testing success" || !initialized_) {
      return Status::FAILED;
    }

    // Fake getting the robot state, calculate and send control output
    std::this_thread::sleep_for(2ms);

    // For testing, pretend the robot takes some fixed
    // amount of time to complete the motion.
    auto current_time = std::chrono::system_clock::now();
    auto motion_duration = 5s;

    if (current_time - start_time_ >= motion_duration) {
      // Movement was completed
      return Status::SUCCEEDED;
    }

    return Status::RUNNING;
  }

private:
  bool initialized_;
  std::string command_;
  std::chrono::system_clock::time_point start_time_;
};

// Define a test class to hold the context for the tests

class RecoveryTest : public ::testing::Test
{
protected:
  RecoveryTest() {}
  ~RecoveryTest() {}

  void SetUp() override
  {
    node_ = std::make_shared<rclcpp::Node>("RecoveryTestNode");
    auto tf_buffer = std::make_shared<tf2_ros::Buffer>(node_->get_clock());
    auto tf_listener = std::make_shared<tf2_ros::TransformListener>(*tf_buffer);
    node_->declare_parameter(
      "costmap_topic", rclcpp::ParameterValue(std::string("local_costmap/costmap_raw")));
    node_->declare_parameter(
      "footprint_topic", rclcpp::ParameterValue(std::string("local_costmap/published_footprint")));
    recovery_ = std::make_unique<DummyRecovery>(node_, tf_buffer);

    client_ = rclcpp_action::create_client<RecoveryAction>(node_, "Recovery");
  }

  void TearDown() override {}

  bool sendCommand(const std::string & command)
  {
    if (!client_->wait_for_action_server(4s)) {
      return false;
    }

    auto future_goal = getGoal(command);

    if (rclcpp::spin_until_future_complete(node_, future_goal) !=
      rclcpp::executor::FutureReturnCode::SUCCESS)
    {
      // failed sending the goal
      return false;
    }

    goal_handle_ = future_goal.get();

    if (!goal_handle_) {
      // goal was rejected by the action server
      return false;
    }

    return true;
  }

  std::shared_future<ClientGoalHandle::SharedPtr> getGoal(const std::string & command)
  {
    auto goal = RecoveryAction::Goal();
    goal.command.data = command;

    auto goal_options = rclcpp_action::Client<RecoveryAction>::SendGoalOptions();
    goal_options.result_callback = [](auto) {};

    return client_->async_send_goal(goal, goal_options);
  }

  Status getOutcome()
  {
    if (getResult().code == rclcpp_action::ResultCode::SUCCEEDED) {
      return Status::SUCCEEDED;
    }

    return Status::FAILED;
  }

  ClientGoalHandle::WrappedResult getResult()
  {
    auto future_result = goal_handle_->async_result();
    rclcpp::executor::FutureReturnCode frc;

    do {
      frc = rclcpp::spin_until_future_complete(node_, future_result);
    } while (frc != rclcpp::executor::FutureReturnCode::SUCCESS);

    return future_result.get();
  }

  std::shared_ptr<rclcpp::Node> node_;
  std::unique_ptr<DummyRecovery> recovery_;
  std::shared_ptr<rclcpp_action::Client<RecoveryAction>> client_;
  std::shared_ptr<rclcpp_action::ClientGoalHandle<RecoveryAction>> goal_handle_;
};

// Define the tests

TEST_F(RecoveryTest, testingSuccess)
{
  ASSERT_TRUE(sendCommand("Testing success"));
  EXPECT_EQ(getOutcome(), Status::SUCCEEDED);
}

TEST_F(RecoveryTest, testingFailureOnRun)
{
  ASSERT_TRUE(sendCommand("Testing failure on run"));
  EXPECT_EQ(getOutcome(), Status::FAILED);
}

TEST_F(RecoveryTest, testingFailureOnInit)
{
  ASSERT_TRUE(sendCommand("Testing failure on init"));
  EXPECT_EQ(getOutcome(), Status::FAILED);
}

TEST_F(RecoveryTest, testingSequentialFailures)
{
  ASSERT_TRUE(sendCommand("Testing failure on init"));
  EXPECT_EQ(getOutcome(), Status::FAILED);

  ASSERT_TRUE(sendCommand("Testing failure on run"));
  EXPECT_EQ(getOutcome(), Status::FAILED);
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);

  // initialize ROS
  rclcpp::init(0, nullptr);

  bool all_successful = RUN_ALL_TESTS();

  // shutdown ROS
  rclcpp::shutdown();

  return all_successful;
}
