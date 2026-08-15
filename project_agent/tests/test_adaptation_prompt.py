from app.adaptation_prompt import (
    build_adaptation_prompt,
)


def test_builds_publisher_adaptation_prompt() -> None:
    prompt = build_adaptation_prompt(
        'create_publisher<std_msgs::msg::Float32>'
    )

    assert prompt.template_type == "publisher"
    assert len(prompt.messages) == 2
    assert len(prompt.sources) == 6


def test_contains_complete_publisher_template() -> None:
    prompt = build_adaptation_prompt(
        'create_publisher<std_msgs::msg::Float32>'
    )

    human_content = str(
        prompt.messages[1].content
    )

    assert "void setup_qos()" in human_content
    assert "void request_register()" in human_content
    assert "void request_deregister(" in human_content
    assert "request->monitors.push_back" in human_content


def test_contains_build_and_yaml_references() -> None:
    prompt = build_adaptation_prompt(
        'create_publisher<std_msgs::msg::Float32>'
    )

    human_content = str(
        prompt.messages[1].content
    )

    assert "ros__parameters:" in human_content
    assert "add_executable" in human_content
    assert "<package" in human_content


def test_contains_strict_project_rules() -> None:
    prompt = build_adaptation_prompt(
        'create_publisher<std_msgs::msg::Float32>'
    )

    system_content = str(
        prompt.messages[0].content
    )

    assert "/management/register_module" in system_content
    assert "/management/deregister_module" in system_content
    assert "/management/state" in system_content
    assert "/health/status" in system_content
    assert "--ros-args --params-file" in system_content
    assert "Do not invent" in system_content
    assert "adaptation_manifest.yaml" in system_content
    assert "Do not create a top-level validator key" in system_content


def test_requires_complete_files_not_snippets() -> None:
    prompt = build_adaptation_prompt(
        'create_publisher<std_msgs::msg::Float32>'
    )

    human_content = str(
        prompt.messages[1].content
    )

    assert "### Complete CMakeLists.txt" in human_content
    assert "### Complete package.xml" in human_content
    assert "### Adaptation Manifest" in human_content
    assert "not snippets" in human_content


def test_preserves_user_input_as_data() -> None:
    user_input = (
        'Node("battery_publisher") publishes '
        '12.4F to /battery/voltage'
    )

    prompt = build_adaptation_prompt(user_input)

    human_content = str(
        prompt.messages[1].content
    )

    assert user_input in human_content
    assert "--- USER NODE START ---" in human_content
    assert "--- USER NODE END ---" in human_content
