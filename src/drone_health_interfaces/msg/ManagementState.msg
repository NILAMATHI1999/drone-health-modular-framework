uint8 REASON_NONE=0
uint8 REASON_MAINTENANCE_MODE=1
uint8 REASON_PLANNED_INACTIVE=2

std_msgs/Header header

bool maintenance_mode
bool mission_active

string[] registry_modules
string[] active_modules

string[] planned_inactive_topics
string[] planned_inactive_topic_reasons

string[] planned_inactive_modules
string[] planned_inactive_module_reasons

string[] registry_topic_modules
string[] registry_topics
string[] registry_topic_types
bool[] registry_topic_critical
bool[] registry_topic_is_heartbeat
int32[] registry_topic_deadline_ms
int32[] registry_topic_liveliness_ms

uint8 reason
string message

