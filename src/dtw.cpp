#include "dtw.h"
#include <Eigen/Dense>
#include <eigen_conversions/eigen_msg.h>
#include <iostream>
#include <vector>
#include <ros/ros.h>

#define NUM_JOINTS 8
#define JOINT_WEIGHT 10

using geometry_msgs::Point;
using geometry_msgs::Pose;
using geometry_msgs::Quaternion;
using sensor_msgs::JointState;
using std::vector;

double DTW::min_diff(const Action& x, const Action& y) {
    int rows = x.size();
    int cols = y.size();
    double diffs[rows][cols];
    double costs[rows][cols];

    // Getting the differences
    Action::pose_cit x_poses = x.pose_begin();
    Action::joint_cit x_joints = x.joint_begin();
    for (int row = 0; row < rows; row++) {
        Action::pose_cit y_poses = y.pose_begin();
        Action::joint_cit y_joints = y.joint_begin();

        for (int col = 0; col < cols; col++) {
            diffs[row][col] = distance(*x_poses, *y_poses++,
                    *x_joints, *y_joints++);
        }

        x_poses++;
        x_joints++;
    }

    // Setting the first row of costs
    for (int c = 0; c < cols; c++) {
        costs[0][c] = diffs[0][c];
    }

    // Setting the first column of costs
    for (int r = 0; r < rows; r++) {
        costs[r][0] = diffs[r][0];
    }

    // Filling in the costs
    for (int r = 1; r < rows; r++) {
        for (int c = 1; c < cols; c++) {
            costs[r][c] = diffs[r][c]
                    + DTW::min(diffs[r - 1][c - 1],
                            diffs[r - 1][c],
                            diffs[r][c - 1]);
        }
    }

    // Returning the minimum difference between the two Actions
    return costs[rows - 1][cols - 1];
}

double DTW::min(double x, double y, double z) {
    if (x < y) {
        return (x < z) ? x : z;
    } else {
        return (y < z) ? y : z;
    }
}

double DTW::distance(const Pose& p1, const Pose& p2,
        const JointState& js1, const JointState& js2) {
    return position_distance(p1.position, p2.position)
            + quaternion_distance(p1.orientation, p2.orientation)
            + joint_distance(js1, js2);
}

double DTW::position_distance(const Point& x, const Point& y) {
	double dx = pow(y.x - x.x, 2);
	double dy = pow(y.y - x.y, 2);
	double dz = pow(y.z - x.z, 2);
	double dist = sqrt(dx + dy + dz);

	// Returning the sum of the distances
	return dist;
}

double DTW::quaternion_distance(const Quaternion& c, const Quaternion& d) {
    Eigen::Vector4f dv;
 	dv[0] = d.w; dv[1] = d.x; dv[2] = d.y; dv[3] = d.z;
	Eigen::Matrix<float, 3,4> inv;
	inv(0,0) = -c.x; inv(0,1) = c.w; inv(0,2) = -c.z; inv(0,3) = c.y;
  	inv(1,0) = -c.y; inv(1,1) = c.z; inv(1,2) = -c.w; inv(1,3) = -c.x;
	inv(2,0) = -c.z; inv(2,1) = -c.y; inv(2,2) = -c.x; inv(2,3) = c.w;

    Eigen::Vector3f m = inv * dv * -2.0;
    return m.norm();
}

double DTW::joint_distance(const JointState& x, const JointState& y) {
    double pos_diff = 0;

    // Getting distance from just the finger positions
    for (int i = 6; i < NUM_JOINTS; i++) {
        pos_diff += pos_dist(x.position[i], y.position[i]);
    }

    return pos_diff * JOINT_WEIGHT;
}

double DTW::pos_dist(double x, double y) {
    // Getting the distance
    double two_pi = 2 * M_PI;
    double diff = abs(y - x);

    // Moving the difference to [-PI, PI]
    while (diff > M_PI) {
        diff -= two_pi;
    }
    while (diff < M_PI) {
        diff += two_pi;
    }

    return diff;
}
