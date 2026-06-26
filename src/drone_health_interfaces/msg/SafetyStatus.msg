uint8 SAFE=0
uint8 UNSAFE=1
uint8 DEGRADED=2
uint8 UNKNOWN=3

uint8 REASON_NONE=0
uint8 REASON_INSUFFICIENT_BRAKING_DISTANCE=1
uint8 REASON_HEALTH_UNSAFE=2
uint8 REASON_INVALID_INPUT=3
uint8 REASON_WAITING_FOR_INPUTS=4

std_msgs/Header header

uint8 state
uint8 reason

float32 nearest_obstacle_m
float32 speed_mps
float32 braking_distance_m
float32 reaction_distance_m
float32 required_clearance_m
float32 effective_deceleration_mps2
float32 safety_margin_m

