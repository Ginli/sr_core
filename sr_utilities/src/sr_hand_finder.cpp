/*
* File:  sf_hand_finder.cpp
* Author: Vahid Aminzadeh <vahid@shadowrobot.com>
* Copyright 2015 Shadow Robot Company Ltd.
*
* This program is free software: you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the Free
* Software Foundation, either version 2 of the License, or (at your option)
* any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program.  If not, see <http://www.gnu.org/licenses/>.
*
* @brief see README.md
*/

#include "sr_utilities/sr_hand_finder.hpp"
#include "urdf_parser/urdf_parser.h"
#include <ros/package.h>

namespace shadow_robot
{
  const std::vector<std::string> SrHandFinder::joint_names_ = {"FFJ1", "FFJ2", "FFJ3", "FFJ4", "MFJ1", "MFJ2", "MFJ3", "MFJ4",
                                              "RFJ1", "RFJ2", "RFJ3", "RFJ4", "LFJ1", "LFJ2", "LFJ3", "LFJ4",
                                              "LFJ5", "THJ1", "THJ2", "THJ3", "THJ4", "THJ5", "WRJ1", "WRJ2"};

  const std::vector<std::string> SrHandFinder::get_default_joints()
  {
    return SrHandFinder::joint_names_;
  }

  HandConfig SrHandFinder::get_hand_parameters()
  {
    return hand_config_;
  }

  SrHandFinder::SrHandFinder()
  {
    if (ros::param::has("hand"))
    {
      ros::param::get("hand/mapping", hand_config_.mapping_);
      ros::param::get("hand/joint_prefix", hand_config_.joint_prefix_);

      for (const auto &pair : hand_config_.mapping_)
      {
        ROS_INFO_STREAM("detected hands are \n" << "hand serial:" << pair.first << " hand_id:" << pair.second);
      }

      generate_joints_with_prefix();
      generate_calibration_path();
      generate_hand_controller_tuning_path();
    } else {
      ROS_ERROR_STREAM("No hand found");
    }
  }

  void SrHandFinder::generate_joints_with_prefix()
  {
    if (ros::param::has("robot_description"))
    {
      std::set<std::string> hand_joints;
      for (const auto &hand : hand_config_.joint_prefix_) {
        for (const auto &default_joint_name : joint_names_) {
          hand_joints.insert(hand.second + default_joint_name);
        }
      }

      std::string robot_description;
      ros::param::get("robot_description", robot_description);
      const auto hand_urdf = urdf::parseURDF(robot_description);
      for (const auto &hand : hand_config_.joint_prefix_) {
        std::set<std::string> joints_tmp;
        for (const auto &joint : hand_urdf->joints_) {
          if (urdf::Joint::FIXED != joint.second->type) {
            const std::string joint_name = joint.first;
            if (std::none_of(hand_config_.joint_prefix_.begin(), hand_config_.joint_prefix_.end(),
                             [&joint_name](const std::pair<std::string, std::string> &item) {
                               return 0 == joint_name.find(item.second); })) {
              ROS_DEBUG_STREAM("Joint " + joint_name + "has invalid prefix");
            } else if (0 == joint_name.find(hand.second)) {
              joints_tmp.insert(joint_name);
            }
          }
        }
        std::set_intersection(joints_tmp.begin(), joints_tmp.end(), hand_joints.begin(), hand_joints.end(),
                              std::back_inserter(joints_[hand_config_.mapping_[hand.first]]));
      }
    } else {
      ROS_WARN_STREAM("No robot_description found on parameter server. Joint names are loaded for 5 finger hand");

      for (const auto &hand : hand_config_.mapping_) {
        const std::string hand_mapping = hand.second;
        for (const auto &default_joint_name : joint_names_) {
          joints_[hand_mapping].push_back(hand_config_.joint_prefix_[hand.first] + default_joint_name);
        }
      }
    }
  }

  void SrHandFinder::generate_calibration_path()
  {
    std::string ethercat_path = ros::package::getPath("sr_ethercat_hand_config");
    for (std::map<std::string, std::string>::const_iterator mapping_iter = hand_config_.mapping_.begin();
         mapping_iter != hand_config_.mapping_.end(); ++mapping_iter)
    {
      calibration_path_[mapping_iter->second] = ethercat_path + "/calibrations/"
                                                + mapping_iter->second + "/" + "calibration.yaml";
    }
  }

  void SrHandFinder::generate_hand_controller_tuning_path()
  {
    std::string ethercat_path = ros::package::getPath("sr_ethercat_hand_config");
    for (std::map<std::string, std::string>::const_iterator mapping_iter = hand_config_.mapping_.begin();
         mapping_iter != hand_config_.mapping_.end(); ++mapping_iter)
    {
      hand_controller_tuning_.friction_compensation_[mapping_iter->second] =
              ethercat_path + "/controls/" + "friction_compensation.yaml";
      hand_controller_tuning_.motor_control_[mapping_iter->second] =
              ethercat_path + "/controls/motors/" + mapping_iter->second + "/motor_board_effort_controllers.yaml";
      std::string host_path(ethercat_path + "/controls/host/" + mapping_iter->second + "/");
      hand_controller_tuning_.host_control_[mapping_iter->second].push_back(
              host_path + "sr_edc_calibration_controllers.yaml");
      hand_controller_tuning_.host_control_[mapping_iter->second].push_back(
              host_path + "sr_edc_joint_velocity_controllers_PWM.yaml");
      hand_controller_tuning_.host_control_[mapping_iter->second].push_back(
              host_path + "sr_edc_effort_controllers_PWM.yaml");
      hand_controller_tuning_.host_control_[mapping_iter->second].push_back(
              host_path + "sr_edc_joint_velocity_controllers.yaml");
      hand_controller_tuning_.host_control_[mapping_iter->second].push_back(
              host_path + "sr_edc_effort_controllers.yaml");

      hand_controller_tuning_.host_control_[mapping_iter->second].push_back(
              host_path + "sr_edc_mixed_position_velocity_joint_controllers_PWM.yaml");
      hand_controller_tuning_.host_control_[mapping_iter->second].push_back(
              host_path + "sr_edc_joint_position_controllers_PWM.yaml");
      hand_controller_tuning_.host_control_[mapping_iter->second].push_back(
              host_path + "sr_edc_mixed_position_velocity_joint_controllers.yaml");
      hand_controller_tuning_.host_control_[mapping_iter->second].push_back(
              host_path + "sr_edc_joint_position_controllers.yaml");
    }
  }

  std::map<std::string, std::vector<std::string>> SrHandFinder::get_joints()
  {
    return joints_;
  }

  std::map<std::string, std::string> SrHandFinder::get_calibration_path()
  {
    return calibration_path_;
  }

  HandControllerTuning SrHandFinder::get_hand_controller_tuning()
  {
    return hand_controller_tuning_;
  }
} /* namespace shadow_robot */
