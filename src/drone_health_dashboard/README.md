# drone_health_dashboard

Web dashboard and ROS bridge for the drone health monitoring framework.

## Purpose

This package provides the browser dashboard and the Python ROS bridge that streams ROS status data
to the frontend.

The dashboard is only a visualization and command interface. It does not contain real safety,
health, or management logic.

## Contains

- `scripts/dashboard_bridge.py`
- `web/index.html`
- `web/styles.css`
- `web/app.js`

## Inputs

The bridge subscribes to ROS topics such as:

- `/health/status`
- `/safety/status`
- `/supervisor/status`
- `/management/state`
- `/lidar/nearest_obstacle`
- `/vehicle/velocity`

## Outputs

- Browser dashboard on `http://localhost:8080`
- HTTP/SSE stream for dashboard updates

## Build

```bash
colcon build --packages-select drone_health_dashboard
```

## Run

```bash
ros2 run drone_health_dashboard dashboard_bridge.py
```

Open:

```text
http://localhost:8080
```

## Expected Behavior

The dashboard displays:

- supervisor/system state
- safety state
- management/mission state
- node health
- obstacle and velocity values
- planned inactive or deregistered modules

## Architecture Rule

The dashboard must not contain safety or management decision logic.

Correct responsibility split:

```text
ROS nodes decide.
Dashboard visualizes and sends commands.
```

## Failure Behavior

If the ROS bridge stops, the browser should show dashboard disconnected or stale data.

If a ROS node stops, the dashboard should display the health/supervisor state published by the ROS
backend.
