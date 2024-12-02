// Copyright (c) 2021 Franka Emika GmbH
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
// limitations under the License.





#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <unistd.h>
#include <thread>
#include <chrono>         

#include "riemannian_motion_policy/user_input_server.hpp"

#include <rclcpp/rclcpp.hpp>
#include "rclcpp/subscription.hpp"

#include <Eigen/Dense>
#include <Eigen/Eigen>

#include <controller_interface/controller_interface.hpp>

#include <franka/model.h>
#include <franka/robot.h>
#include <franka/robot_state.h>

#include "franka_hardware/franka_hardware_interface.hpp"
#include <franka_hardware/model.hpp>

#include "franka_msgs/msg/franka_robot_state.hpp"
#include "franka_msgs/msg/errors.hpp"
#include "franka_msgs/srv/set_load.hpp"
#include "messages_fr3/srv/set_pose.hpp"
#include "messages_fr3/msg/tau_gravity.hpp"
#include "messages_fr3/msg/jacobian.hpp"
#include "messages_fr3/msg/transformation_matrix.hpp"
#include "messages_fr3/msg/gravity_force_vector.hpp"
#include "franka_semantic_components/franka_robot_model.hpp"
#include "franka_semantic_components/franka_robot_state.hpp"
#include "messages_fr3/msg/acceleration.hpp"
#include "geometry_msgs/msg/pose.hpp"
#define IDENTITY Eigen::MatrixXd::Identity(6, 6)

using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;
using Vector7d = Eigen::Matrix<double, 7, 1>;

namespace riemannian_motion_policy {

class RiemannianMotionPolicy : public controller_interface::ControllerInterface {
public:
  [[nodiscard]] controller_interface::InterfaceConfiguration command_interface_configuration()
      const override;

  [[nodiscard]] controller_interface::InterfaceConfiguration state_interface_configuration()
      const override;

  controller_interface::return_type update(const rclcpp::Time& time,
                                           const rclcpp::Duration& period) override;
  controller_interface::CallbackReturn on_init() override;

  controller_interface::CallbackReturn on_configure(
      const rclcpp_lifecycle::State& previous_state) override;

  controller_interface::CallbackReturn on_activate(
      const rclcpp_lifecycle::State& previous_state) override;

  controller_interface::CallbackReturn on_deactivate(
      const rclcpp_lifecycle::State& previous_state) override;

  void setPose(const std::shared_ptr<messages_fr3::srv::SetPose::Request> request, 
    std::shared_ptr<messages_fr3::srv::SetPose::Response> response);
      

 private:
    //Nodes
    rclcpp::Subscription<franka_msgs::msg::FrankaRobotState>::SharedPtr franka_state_subscriber = nullptr;
    rclcpp::Subscription<geometry_msgs::msg::Pose>::SharedPtr desired_pose_sub;
    rclcpp::Service<messages_fr3::srv::SetPose>::SharedPtr pose_srv_;


    //Functions
    void topic_callback(const std::shared_ptr<franka_msgs::msg::FrankaRobotState> msg);
    void jointStateCallback(const sensor_msgs::msg::JointState::SharedPtr msg);
    void desiredJointStateCallback(const sensor_msgs::msg::JointState::SharedPtr msg);
    void reference_pose_callback(const geometry_msgs::msg::Pose::SharedPtr msg);
    void rmp_joint_limit_avoidance();
    void calculateRMP_global(const Eigen::MatrixXd& A_obs_tilde, 
                         const Eigen::VectorXd& f_obs_tilde);
    void get_ddq();
    void updateJointStates();
    void update_stiffness_and_references();
    void arrayToMatrix(const std::array<double, 6>& inputArray, Eigen::Matrix<double, 6, 1>& resultMatrix);
    void arrayToMatrix(const std::array<double, 7>& inputArray, Eigen::Matrix<double, 7, 1>& resultMatrix);
    void calculate_tau_friction();
    void calculate_residual_torque(const Eigen::Map<Eigen::Matrix<double, 7, 1>>& coriolis, const Eigen::Map<Eigen::Matrix<double, 7, 1>>& gravity_force_vector);
    void calculateRTOB();
    void calculate_tau_gravity(const Eigen::Map<Eigen::Matrix<double, 7, 1>>& coriolis, const Eigen::Map<Eigen::Matrix<double, 7, 1>>& gravity_force_vector, const Eigen::Matrix<double, 6, 7>& jacobian);
    
    Eigen::Matrix<double, 7, 1> saturateTorqueRate(const Eigen::Matrix<double, 7, 1>& tau_d_calculated, const Eigen::Matrix<double, 7, 1>& tau_J_d);  
    std::array<double, 6> convertToStdArray(const geometry_msgs::msg::WrenchStamped& wrench);
    Eigen::VectorXd calculate_f_obstacle(const Eigen::VectorXd& d_obs,
                                       const Eigen::VectorXd& d_obs_prev);
    Eigen::MatrixXd calculate_A_obstacle(const Eigen::VectorXd& d_obs,
                                      const Eigen::VectorXd& f_obs);
    Eigen::Vector3d calculateNearestPointOnSphere(const Eigen::Vector3d& position,
                                                                      const Eigen::Vector3d& sphereCenter, 
                                                                      double radius); 
    //State vectors and matrices
    Eigen::Matrix<double, 7, 7> M;
    std::array<double, 7> q_subscribed;
    std::array<double, 7> tau_J_d = {0,0,0,0,0,0,0};
    std::array<double, 6> O_F_ext_hat_K = {0,0,0,0,0,0};
    Eigen::Matrix<double, 7, 1> q_subscribed_M;
    Eigen::Matrix<double, 6, 7> jacobian;
    Eigen::Matrix<double, 7, 1> tau_J_d_M = Eigen::MatrixXd::Zero(7, 1);
    Eigen::Matrix<double, 6, 1> O_F_ext_hat_K_M = Eigen::MatrixXd::Zero(6,1);
    Eigen::Matrix<double, 7, 1> q_;
    Eigen::Matrix<double, 7, 1> dq_;
    Eigen::Matrix<double, 7, 1> dq_prev_;
    Eigen::Matrix<double, 7, 1> q_prev;
    Eigen::Matrix<double, 7, 1> dq_filtered;
    Eigen::Matrix<double, 7, 1> dq_filtered_gravity;
    Eigen::MatrixXd jacobian_transpose_pinv;  
    Eigen::MatrixXd jacobian_pinv;
    //initialize jacobian array wiht all zeros 42x1
    std::array<double, 42> jacobian_array;
    std::array<double,42> jacobian_prev_;
    std::array<double,42> jacobian_endeffector;
    std::array<double,42> dJ;
    // control input
    int control_mode; // either position control or velocity control
    Eigen::Matrix<double, 7, 1> tau_impedance; // impedance torque
    Eigen::Matrix<double, 7, 1> tau_impedance_filtered = Eigen::MatrixXd::Zero(7,1); // impedance torque filtered
    Eigen::Matrix<double, 7, 1> tau_friction;
    Eigen::Matrix<double, 7, 1> tau_threshold;  //Creating and filtering a "fake" tau_impedance with own weights, optimized for friction compensation
    bool friction_ = true; // set if friciton compensation should be turned on
    Eigen::MatrixXd N; // nullspace projection matrix
    // friction compensation observer
    Eigen::Matrix<double, 7, 1> dz = Eigen::MatrixXd::Zero(7,1);
    Eigen::Matrix<double, 7, 1> z = Eigen::MatrixXd::Zero(7,1);
    Eigen::Matrix<double, 7, 1> f = Eigen::MatrixXd::Zero(7,1);
    Eigen::Matrix<double, 7, 1> g = Eigen::MatrixXd::Zero(7,1);
    Eigen::Matrix<double, 6,6> K_friction = IDENTITY; //impedance stiffness term for friction compensation
    Eigen::Matrix<double, 6,6> D_friction = IDENTITY; //impedance damping term for friction compensation
    const Eigen::Matrix<double, 7, 1> sigma_0 = (Eigen::VectorXd(7) << 76.95, 37.94, 71.07, 44.02, 21.32, 21.83, 53).finished();
    const Eigen::Matrix<double, 7, 1> sigma_1 = (Eigen::VectorXd(7) << 0.056, 0.06, 0.064, 0.073, 0.1, 0.0755, 0.000678).finished();
    //friction compensation model paramers (coulomb, viscous, stribeck)
    Eigen::Matrix<double, 7, 1> dq_s = (Eigen::VectorXd(7) << 0, 0, 0, 0.0001, 0, 0, 0.05).finished();
    Eigen::Matrix<double, 7, 1> static_friction = (Eigen::VectorXd(7) << 1.025412896, 1.259913793, 0.8380147058, 1.005214968, 1.2928, 0.41525, 0.5341655).finished(); 
    Eigen::Matrix<double, 7, 1> offset_friction = (Eigen::VectorXd(7) << -0.05, -0.70, -0.07, -0.13, -0.1025, 0.103, -0.02).finished();
    Eigen::Matrix<double, 7, 1> coulomb_friction = (Eigen::VectorXd(7) << 1.025412896, 1.259913793, 0.8380147058, 0.96, 1.2928, 0.41525, 0.5341655).finished();
    Eigen::Matrix<double, 7, 1> beta = (Eigen::VectorXd(7) << 1.18, 0, 0.55, 0.87, 0.935, 0.54, 0.45).finished();//component b of linear friction model (a + b*dq)
    
    //gravity compensation
    Eigen::Matrix<double, 7, 1> tau_gravity_filtered = Eigen::MatrixXd::Zero(7,1);
    Eigen::Matrix<double, 7, 1> tau_gravity_filtered_prev = Eigen::MatrixXd::Zero(7,1);
    Eigen::Matrix<double, 7, 1> tau_gravity = Eigen::MatrixXd::Zero(7,1);
    Eigen::Matrix<double, 7, 1> gravity_force = Eigen::MatrixXd::Zero(7,1);
    Eigen::Matrix<double, 7, 1> tau_gravity_ana = Eigen::MatrixXd::Zero(7,1);
    Eigen::Matrix<double, 7, 1> tau_gravity_error = Eigen::MatrixXd::Zero(7,1);
    Eigen::Matrix<double, 7, 1> residual = Eigen::MatrixXd::Zero(7,1);
    Eigen::Matrix<double, 7, 1> residual_prev = Eigen::MatrixXd::Zero(7,1);
    Eigen::Matrix<double, 7, 1> tau_J;
    Eigen::Matrix<double, 7, 7> M_prev= Eigen::MatrixXd::Zero(7,7); // Previous mass matrix for numerical differentiation
    Eigen::Matrix<double, 7, 1> I_tau= Eigen::MatrixXd::Zero(7,1); // Integral of torque over time
    Eigen::Matrix<double, 7, 1> I_tau_prev= Eigen::MatrixXd::Zero(7,1);
    Eigen::Matrix<double, 7, 1> tau_commanded;  // To store commanded torques
    Eigen::Matrix<double, 7, 7> kp;  // Diagonal matrix for kp
    Eigen::Matrix<double, 7, 7> kd;  // Diagonal matrix for kd
    
    Eigen::Matrix<double, 7, 7> K_0; //gain for residual torque observer
    double filter_gain = 0.1; //filter gain for residual torque observer
    bool gravity_ = true; // set if gravity compensation should be turned on
    double m_estimated = 0.0; // estimated mass of the load
    Eigen::Matrix<double, 3, 1> r_CoM_estimated = Eigen::MatrixXd::Zero(3,1); // estimated center of mass of the load
    Eigen::Matrix<double, 7, 1> tau_load = Eigen::MatrixXd::Zero(7,1); // load torque
    Eigen::Matrix<double, 6, 1> wrench_load; // load wrench

    //RMP Parameters
    Eigen::Matrix<double, 7, 1> sigma_u = Eigen::MatrixXd::Zero(7,1);
    Eigen::Matrix<double, 7, 1> alpha_u = Eigen::MatrixXd::Zero(7,1);
    Eigen::Matrix<double, 7, 1> d_ii = Eigen::MatrixXd::Zero(7,1);
    Eigen::Matrix<double, 7, 1> d_ii_tilde = Eigen::MatrixXd::Zero(7,1);
    Eigen::Matrix<double, 7, 7> D_sigma = Eigen::MatrixXd::Zero(7,7);
    double c_alpha = 1.0;
    double c = 0.1;
    double gamma_p = 15;
    double gamma_d = 7.7;
    const Eigen::Matrix<double, 7, 1> q_0= (Eigen::VectorXd(7) << 0.109, -0.189, -0.104, -2.09, -0.0289, 1.90, 0.0189).finished();
    const Eigen::Matrix<double, 7, 1> q_lower_limit = (Eigen::VectorXd(7) << -2.89725, -1.8326,-2.89725, -3.0718, -2.87979, 0.436332, -3.05433).finished();
    const Eigen::Matrix<double, 7, 1> q_upper_limit = (Eigen::VectorXd(7) << 2.89725,1.8326, -2.89725, -0.122173, 2.87979, 4.62512, 3.05433).finished();
    Eigen::Matrix<double, 6, 7> jacobian_tilde = Eigen::MatrixXd::Zero(6,7);
    Eigen::Matrix<double, 7, 1> h_joint_limits = Eigen::MatrixXd::Zero(7,1);
    Eigen::Matrix<double, 6, 1> x_dd_des = Eigen::MatrixXd::Zero(6,1);
    Eigen::Matrix<double, 6, 1> s = Eigen::MatrixXd::Zero(6,1);
    Eigen::Matrix<double, 6, 1> pose_endeffector = Eigen::MatrixXd::Zero(6,1);
    Eigen::Matrix<double, 7, 1> u = Eigen::MatrixXd::Zero(7,1);
    
    double lambda_RMP = 0.01;
    Eigen::Matrix<double, 7, 1> ddq_;
    Eigen::Matrix<double, 7, 1> tau_RMP;
    Eigen::Matrix<double, 6, 6> K_RMP =  (Eigen::MatrixXd(6,6) << 250,   0,   0,   0,   0,   0,
                                                                0, 250,   0,   0,   0,   0,
                                                                0,   0, 250,   0,   0,   0,  
                                                                0,   0,   0, 20,   0,   0,
                                                                0,   0,   0,   0, 20,   0,
                                                                0,   0,   0,   0,   0,  10).finished();

    Eigen::Matrix<double, 6, 6> D_RMP =  (Eigen::MatrixXd(6,6) <<  30,   0,   0,   0,   0,   0,
                                                                0,  30,   0,   0,   0,   0,
                                                                0,   0,  30,   0,   0,   0,  
                                                                0,   0,   0,   9,   0,   0,
                                                                0,   0,   0,   0,   9,   0,
                                                                0,   0,   0,   0,   0,   6).finished();
    //RMP Obstacle Avoidance Parameters
    
    Eigen::Vector3d d_obs1 ;
    Eigen::Vector3d d_obs_prev1 ;
    Eigen::Matrix<double, 3, 7> Jp = Eigen::MatrixXd::Zero(3,1);
    
    Eigen::VectorXd f_obs_tilde1 = Eigen::VectorXd::Zero(6); 
    Eigen::Matrix<double, 6, 6> A_obs_tilde1 = Eigen::MatrixXd::Identity(6,6);
  
    Eigen::Vector3d sphere_center = (Eigen::VectorXd(3) << 0.3, 0.0, 0.5).finished();
    double sphere_radius = 0.0;
    
    Eigen::VectorXd f_tot = Eigen::VectorXd::Zero(6);  // Correct dynamic-size initialization
    Eigen::MatrixXd A_tot = Eigen::MatrixXd::Zero(6,6);  // Dynamic-size matrix
    Eigen::MatrixXd A_tot_pinv;





    //Robot parameters
    const int num_joints = 7;
    const std::string state_interface_name_{"robot_state"};
    const std::string robot_name_{"fr3"};
    const std::string k_robot_state_interface_name{"robot_state"};
    const std::string k_robot_model_interface_name{"robot_model"};
    franka_hardware::FrankaHardwareInterface interfaceClass;
    std::unique_ptr<franka_semantic_components::FrankaRobotModel> franka_robot_model_;
    const double delta_tau_max_{1.0};
    const double dt = 0.001;
                
    //Impedance control variables              
    Eigen::Matrix<double, 6, 6> Lambda = IDENTITY;                                           // operational space mass matrix
    Eigen::Matrix<double, 6, 6> Sm = IDENTITY;                                               // task space selection matrix for positions and rotation
    Eigen::Matrix<double, 6, 6> Sf = Eigen::MatrixXd::Zero(6, 6);                            // task space selection matrix for forces
    Eigen::Matrix<double, 6, 6> K =  (Eigen::MatrixXd(6,6) << 250,   0,   0,   0,   0,   0,
                                                                0, 250,   0,   0,   0,   0,
                                                                0,   0, 250,   0,   0,   0,  // impedance stiffness term
                                                                0,   0,   0, 130,   0,   0,
                                                                0,   0,   0,   0, 130,   0,
                                                                0,   0,   0,   0,   0,  10).finished();

    Eigen::Matrix<double, 6, 6> D =  (Eigen::MatrixXd(6,6) <<  35,   0,   0,   0,   0,   0,
                                                                0,  35,   0,   0,   0,   0,
                                                                0,   0,  35,   0,   0,   0,  // impedance damping term
                                                                0,   0,   0,   25,   0,   0,
                                                                0,   0,   0,   0,   25,   0,
                                                                0,   0,   0,   0,   0,   6).finished();

    // Eigen::Matrix<double, 6, 6> K =  (Eigen::MatrixXd(6,6) << 250,   0,   0,   0,   0,   0,
    //                                                             0, 250,   0,   0,   0,   0,
    //                                                             0,   0, 250,   0,   0,   0,  // impedance stiffness term
    //                                                             0,   0,   0,  80,   0,   0,
    //                                                             0,   0,   0,   0,  80,   0,
    //                                                             0,   0,   0,   0,   0,  10).finished();

    // Eigen::Matrix<double, 6, 6> D =  (Eigen::MatrixXd(6,6) <<  30,   0,   0,   0,   0,   0,
    //                                                             0,  30,   0,   0,   0,   0,
    //                                                             0,   0,  30,   0,   0,   0,  // impedance damping term
    //                                                             0,   0,   0,  18,   0,   0,
    //                                                             0,   0,   0,   0,  18,   0,
    //                                                             0,   0,   0,   0,   0,   9).finished();
    Eigen::Matrix<double, 6, 6> Theta = IDENTITY;
    Eigen::Matrix<double, 6, 6> T = (Eigen::MatrixXd(6,6) <<       1,   0,   0,   0,   0,   0,
                                                                   0,   1,   0,   0,   0,   0,
                                                                   0,   0,   2.5,   0,   0,   0,  // Inertia term
                                                                   0,   0,   0,   1,   0,   0,
                                                                   0,   0,   0,   0,   1,   0,
                                                                   0,   0,   0,   0,   0,   2.5).finished();                                               // impedance inertia term

    Eigen::Matrix<double, 6, 6> cartesian_stiffness_target_;                                 // impedance damping term
    Eigen::Matrix<double, 6, 6> cartesian_damping_target_;                                   // impedance damping term
    Eigen::Matrix<double, 6, 6> cartesian_inertia_target_;                                   // impedance damping term
    Eigen::Vector3d position_d_target_ = {0.5, 0.0, 0.5};
    Eigen::Vector3d rotation_d_target_ = {M_PI, 0.0, 0.0};
    Eigen::Quaterniond orientation_d_target_;
    Eigen::Vector3d position_d_;
    Eigen::Quaterniond orientation_d_; 
    Eigen::Matrix<double, 6, 1> F_impedance;  
    Eigen::Matrix<double, 6, 1> F_contact_des = Eigen::MatrixXd::Zero(6, 1);                 // desired contact force
    Eigen::Matrix<double, 6, 1> F_contact_target = Eigen::MatrixXd::Zero(6, 1);              // desired contact force used for filtering
    Eigen::Matrix<double, 6, 1> F_ext = Eigen::MatrixXd::Zero(6, 1);                         // external forces
    Eigen::Matrix<double, 6, 1> F_cmd = Eigen::MatrixXd::Zero(6, 1);                         // commanded contact force
    Eigen::Matrix<double, 7, 1> q_d_nullspace_;
    Eigen::Matrix<double, 6, 1> error;                                                       // pose error (6d)
    double nullspace_stiffness_{0.001};
    double nullspace_stiffness_target_{0.001};

    //Logging
    int outcounter = 0;
    const int update_frequency = 2; //frequency for update outputs

    //Integrator
    Eigen::Matrix<double, 6, 1> I_error = Eigen::MatrixXd::Zero(6, 1);                      // pose error (6d)
    Eigen::Matrix<double, 6, 1> I_F_error = Eigen::MatrixXd::Zero(6, 1);                    // force error integral
    Eigen::Matrix<double, 6, 1> integrator_weights = 
      (Eigen::MatrixXd(6,1) << 75.0, 75.0, 75.0, 75.0, 75.0, 4.0).finished();
    Eigen::Matrix<double, 6, 1> max_I = 
      (Eigen::MatrixXd(6,1) << 30.0, 30.0, 30.0, 50.0, 50.0, 2.0).finished();

   
  
    std::mutex position_and_orientation_d_target_mutex_;

    //Flags
    bool config_control = false;           // sets if we want to control the configuration of the robot in nullspace
    bool do_logging = false;               // set if we do log values

    //Filter-parameters
    double filter_params_{0.001};
};
}  // namespace riemannian_motion_policy