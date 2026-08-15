from app.template_loader import (
    load_template_bundle,
)


def test_loads_complete_publisher_bundle() -> None:
    bundle = load_template_bundle(
        'create_publisher<std_msgs::msg::Float32>'
    )

    assert bundle.template_type == "publisher"

    cpp = bundle.get("cpp")
    yaml_file = bundle.get("yaml")

    assert "void request_register()" in cpp.content
    assert "void request_deregister(" in cpp.content
    assert "void setup_qos()" in cpp.content
    assert "request->monitors.push_back" in cpp.content

    assert "ros__parameters:" in yaml_file.content
    assert "request_deregister_service:" in yaml_file.content


def test_loads_subscriber_bundle() -> None:
    bundle = load_template_bundle(
        'create_subscription<std_msgs::msg::String>'
    )

    assert bundle.template_type == "subscriber"
    assert "subscriber_template" in bundle.get(
        "cpp"
    ).source


def test_loads_service_bundle() -> None:
    bundle = load_template_bundle(
        'create_service<std_srvs::srv::Trigger>'
    )

    assert bundle.template_type == "service"
    assert "service_template" in bundle.get(
        "cpp"
    ).source


def test_bundle_contains_build_and_readme_files() -> None:
    bundle = load_template_bundle(
        "publisher node"
    )

    assert "add_executable" in bundle.get(
        "cmake"
    ).content

    assert "<package" in bundle.get(
        "package_xml"
    ).content

    assert "AI-Assisted Integration" in bundle.get(
        "readme"
    ).content

    assert bundle.get(
        "template_readme"
    ).content
