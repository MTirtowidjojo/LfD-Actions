#include <ros/ros.h>
#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <termios.h>
#include "dataset.h"
#include "dataset.cpp"
#include "geometry_msgs/PoseStamped.h"
#include "geometry_msgs/Pose.h"
#include "sensor_msgs/JointState.h"
#include <message_filters/subscriber.h>
#include <message_filters/synchronizer.h>
#include <message_filters/sync_policies/approximate_time.h>
#include <std_msgs/Header.h>
#include "action.h"
#include "dtw.h"

#define NUM_JOINTS 8
#define NUM_BINS 10
#define DEFAULT_K 3

using std::string;
using std::cin;
using std::cout;
using std::endl;
using std::istream;
using std::vector;
using geometry_msgs::Pose;
using geometry_msgs::PoseStamped;
using sensor_msgs::JointState;
using std::list;

// The topic the arm joints publish to
const string ARM_TOPIC = "/joint_states";
const string CART_TOPIC = "/mico_arm_driver/out/tool_position";

// Vectors used to record the action in callback
list<Pose> pose_list;
vector<Pose> poses;
vector<JointState> joints;

/**
 * Reads a character without blocking execution
 * Returns the read character
 */
int getch() {
    static struct termios oldt, newt;

    // Saving old settings
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;

    // Disable buffering
    newt.c_lflag &= ~(ICANON);
    newt.c_cc[VMIN] = 0;
    newt.c_cc[VTIME] = 0;

    // Apply new settings
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    // Read character without blocking
    int c = getchar();

    // Restore old settings
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    return c;
}

/**
 * Prompts the user to record another actions
 * Returns true if going to record another action, false otherwise
 */
bool repeat() {
    string repeat, clear;
    cout << "Again [Y/y]: ";
    cin >> repeat;
    getline(cin, clear);

    return repeat == "Y" || repeat == "y";
}

/**
 * Outputs the guess from the robot
 */
void print_guess(const string& guess) {
    ROS_INFO("Action guess: %s", guess.c_str());
}

/**
 * Prompts the user to correct the robot if the robot guessed wrong
 * Returns the appropriate label for the action that was performed
 */
string confirm_guess(const string& guess) {
    string confirm;
    string label = guess;

    // Getting if guess correct
    cout << "Guess correct? [Y/N]: ";
    cin >> confirm;

    // If incorrect, getting correct label
    if (confirm != "Y" && confirm != "y") {
        cout << "Enter the correct label: ";
        cin >> label;
    }

    return label;
}

/**
 * Displays error when incorrect command line args given
 */
void print_err() {
    cout << "knn: Missing required command line argument" << endl
         << "Usage: rosrun lfd_actions knn -d <file>" << endl
         << "Options:" << endl
         << "  -h           Print this help message." << endl
         << "  -v           Optional verbose flag." << endl
         << "  -s           Optional supervise flag." << endl
         << "  -d <file>    The dataset file." << endl
         << "  -t <file>    The test file." << endl
         << "  -k <int>     The number of nearest neighbors" << endl;
}

void callback(const JointState::ConstPtr& joint, const PoseStamped::ConstPtr& cart) {
    // Pushing the joint states
    vector<string> names = joint->name;
    // Checking that received the appropriate number of joints
    if (names.size() == 8) {
        joints.push_back(*joint);
        // Pushing the cartesian pose
        poses.push_back(cart->pose);
    }

}

void cart_cb(const PoseStamped::ConstPtr& msg) {
    pose_list.push_back(msg->pose);
}

int main(int argc, char** argv) {
    // Initializing the ros node
    ros::init(argc, argv, "knn");
    ros::NodeHandle n;
    ros::Rate loop_rate(20);

/*
    // Creating the subscribers
    message_filters::Subscriber<JointState> arm_sub(n, ARM_TOPIC, 1000);
    message_filters::Subscriber<PoseStamped> cart_sub(n, CART_TOPIC, 1000);

    typedef message_filters::sync_policies::ApproximateTime<JointState, PoseStamped> policy;
    message_filters::Synchronizer<policy> sync(policy(100), arm_sub, cart_sub);
    sync.registerCallback(boost::bind(&callback, _1, _2));
    */

    // Creating the subscriber
    ros::Subscriber cart_sub = n.subscribe(CART_TOPIC, 100, cart_cb);

    // Getting command line arguments
    string dataset_name = "";;
    string testfile_name = "";
    bool supervise = false;
    bool verbose = false;
    int k = DEFAULT_K;
    for (int i = 1; i < argc; i++) {
        string argv_str(argv[i]);
        verbose = verbose || argv_str == "-v";
        supervise = supervise || argv_str == "-s";

        if (argv_str == "-h") {
            print_err();
            return 1;
        } else if (argv_str == "-d") {
            if (i + 1 <= argc) {
                dataset_name = argv[++i];
            } else {
                print_err();
                return 1;
            }
        } else if (argv_str == "-t") {
            if (i + 1 <= argc) {
                testfile_name = argv[++i];
            } else {
                print_err();
                return 1;
            }
        } else if (argv_str == "-k") {
            if (i + 1 <= argc) {
                k = atoi(argv[++i]);
            } else {
                print_err();
                return 1;
            }
        }
    }

    // Checking that a dataset file has been specified
    if (dataset_name == "") {
        print_err();
        return 1;
    }

    // Outputing command line args
    ROS_INFO("dataset path: %s", dataset_name.c_str());
    if (supervise) {
        ROS_INFO("supervised = true");
    } else {
        ROS_INFO("supervised = false");
    }

    // Building the dataset
    Dataset dataset(dataset_name, k);

    bool again = true;
    while (again) {
        // Clearing the vectors
        poses.clear();
        joints.clear();

        // Waiting to record
        cout << "Press [Enter] to start";
        cin.ignore();

        // Notifying of recording
        cout << "Recording data..." << endl
             << "Press \'q\' to stop" << endl;

        // Recording the data
        while (ros::ok() && getch() != 'q') {
            ros::spinOnce();
            loop_rate.sleep();
        }
        cout << endl;

        // Creating the recorded action
        Action ac(pose_list);
        Action ac_offset(pose_list);

        // Guessing the classification
        string guess = dataset.guess_classification(ac_offset, verbose);

        // Print out the guess for the action
        print_guess(guess);

        // If supervised checking guess with user
        if (supervise) {
            ac.set_label(confirm_guess(guess));
            dataset.update(ac);
        }

        // Getting whether or not to record another action
        again = repeat();
    }

    return 0;
}
