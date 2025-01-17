#!/usr/bin/env python3

# Copyright (c) 2018 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import sys

from launch import LaunchDescription
from launch import LaunchService
from launch.actions import ExecuteProcess

import launch_ros.actions

from launch_testing.legacy import LaunchTestService


def main(argv=sys.argv[1:]):
    testExecutable = os.getenv('TEST_EXECUTABLE')

    run_navfn = launch_ros.actions.Node(
        package='nav2_navfn_planner',
        node_executable='navfn_planner',
        output='screen')
    run_lifecycle_manager = launch_ros.actions.Node(
        package='nav2_lifecycle_manager',
        node_executable='lifecycle_manager',
        node_name='lifecycle_manager',
        output='screen',
        parameters=[{'node_names': ['navfn_planner']}, {'autostart': True}])
    ld = LaunchDescription([run_navfn, run_lifecycle_manager])

    test1_action = ExecuteProcess(
        cmd=[testExecutable],
        name='test_planner_node',
        output='screen'
    )

    lts = LaunchTestService()
    lts.add_test_action(ld, test1_action)
    ls = LaunchService(argv=argv)
    ls.include_launch_description(ld)
    return lts.run(ls)


if __name__ == '__main__':
    sys.exit(main())
