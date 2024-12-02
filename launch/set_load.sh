#!/bin/bash

ros2 service call /service_server/set_load franka_msgs/srv/SetLoad "{
mass: 0.3006,
center_of_mass: [-0.0296, -0.0431, 0.0845],
load_inertia: [1.0000001001611059e-10, -0.00779822451697241, 0.05555520223109967, -0.00779822451697241 , 0.014558499279924893, 0.04175042622805034, 0.05555520223109967,0.04175042622805034, 9.999999873305798e-11]
}"


