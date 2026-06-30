uint8 REASON_NONE=0
uint8 REASON_MAINTENANCE_MODE=1
uint8 REASON_PLANNED_INACTIVE=2

std_msgs/Header header

bool maintenance_mode
bool mission_active

drone_health_interfaces/ManagedModule[] managed_modules

string[] planned_inactive_topics
string[] planned_inactive_topic_reasons

string[] planned_inactive_modules
string[] planned_inactive_module_reasons

string[] rejected_modules
string[] rejected_module_reasons

uint8 reason
string message
