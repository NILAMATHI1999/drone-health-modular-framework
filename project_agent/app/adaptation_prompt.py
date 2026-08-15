from dataclasses import dataclass

from langchain_core.messages import (
    BaseMessage,
    HumanMessage,
    SystemMessage,
)

from app.template_loader import (
    TemplateBundle,
    TemplateFile,
    load_template_bundle,
)


SYSTEM_RULES = """You are the Drone Health Framework node adaptation
assistant.

Transform the user's existing ROS 2 node using the supplied complete
template.

The supplied project files are authoritative.

Mandatory rules:
- Preserve the original node logic and ROS message types.
- Preserve the original node name unless a change is required.
- Add only the required Drone Health integration.
- Keep every mandatory registration, heartbeat, QoS, and deregistration
block.
- Register only heartbeat and data/output topics published by the node.
- Never register subscribed input topics owned by another node.
- MonitorSpec message_type must match the actual publisher type.
- Apply configured deadlines to the actual publisher QoS.
- Keep every deadline greater than its corresponding publish period.
- Treat cycle-based auto-deregistration as optional example logic.
- Use /management/register_module for registration.
- Use /management/deregister_module for deregistration.
- Use /management/state and /health/status for verification.
- Do not invent ROS messages, services, topics, packages, or commands.
- Do not use ros2 node kill.
- Use --ros-args --params-file to load YAML.
- Return complete files. Never replace code with comments or ellipses.
- Generate a standalone adapted ROS 2 package, not edits inside drone_health_registrable_template.
- Use package name <node_name>_adapted unless the user supplies a package name.
- Use source path src/<node_name>.cpp and YAML path config/<node_name>.yaml.
- CMakeLists.txt must use project(<node_name>_adapted) and add_executable(<node_name> src/<node_name>.cpp).
- package.xml must use <name><node_name>_adapted</name> and generic maintainer metadata.
- Do not include personal names or personal email addresses in generated package.xml.
- Return complete build and dependency files, not snippets.
- Generate adaptation_manifest.yaml using the required validator schema.
- Do not create a top-level validator key in adaptation_manifest.yaml.
- In adaptation_manifest.yaml, use topics.owned_outputs[].name and
topics.heartbeat.name, not topic.
- In adaptation_manifest.yaml, files.source, files.yaml, files.build,
files.dependencies, and files.launch must be lists.
- If required original-node information is missing, state it instead of
inventing it.
"""


OUTPUT_CONTRACT = """Return exactly these sections:

### Adapted C++ Node
A complete compilable C++ source file.

### Matching YAML Configuration
A complete ROS 2 parameter YAML file whose top-level key exactly matches
Node("...").

### Complete CMakeLists.txt
A complete CMakeLists.txt for the standalone adapted package. Do not return only
additions or snippets. Do not use drone_health_registrable_template as the project name.

### Complete package.xml
A complete package.xml for the standalone adapted package. Do not return only
dependency entries or snippets. Use generic maintainer metadata.

### Adaptation Manifest
A complete adaptation_manifest.yaml using the Required Adaptation Manifest
Format from the README. The files section must point to actual standalone package
files returned by this answer, such as src/<node_name>.cpp and config/<node_name>.yaml.

### Build and Run Commands
Use the actual package name if supplied. Otherwise use <package_name>.
Use --ros-args --params-file for YAML.

### Verification Commands
Use /management/state, /health/status, and the node's published topics.

### Planned Deregistration and Unexpected-Stop Tests
Use the node's request_deregister Trigger service.
For unexpected stop, instruct the user to terminate the running process with
Ctrl+C or kill its PID.

### Assumptions and Sources
List only genuine missing information and supplied project source paths.
"""


@dataclass(frozen=True)
class AdaptationPrompt:
    messages: list[BaseMessage]
    sources: list[str]
    template_type: str


def readme_guidance(content: str) -> str:
    marker = "## AI-Assisted Integration"
    start = content.find(marker)

    if start == -1:
        return content

    return content[start:]


def render_reference(
    template_file: TemplateFile,
    content: str | None = None,
) -> str:
    rendered_content = (
        template_file.content
        if content is None
        else content
    )

    return (
        f"--- PROJECT FILE: {template_file.source} ---\n"
        f"{rendered_content}\n"
        f"--- END PROJECT FILE ---"
    )


def build_reference_context(
    bundle: TemplateBundle,
) -> str:
    references = [
        render_reference(bundle.get("cpp")),
        render_reference(bundle.get("yaml")),
        render_reference(bundle.get("cmake")),
        render_reference(bundle.get("package_xml")),
        render_reference(
            bundle.get("readme"),
            readme_guidance(
                bundle.get("readme").content
            ),
        ),
        render_reference(
            bundle.get("template_readme")
        ),
    ]

    return "\n\n".join(references)


def build_adaptation_prompt(
    user_input: str,
) -> AdaptationPrompt:
    bundle = load_template_bundle(user_input)
    context = build_reference_context(bundle)

    human_content = f"""ADAPTATION TYPE:
{bundle.template_type}

AUTHORITATIVE PROJECT REFERENCES:
{context}

USER NODE TO ADAPT:
--- USER NODE START ---
{user_input}
--- USER NODE END ---

OUTPUT CONTRACT:
{OUTPUT_CONTRACT}

Before answering, check that:
- the original application behavior is preserved;
- registration uses request->monitors;
- heartbeat and owned output topics are registered;
- input-only topics are not registered;
- publisher QoS matches MonitorSpec;
- YAML contains every declared parameter;
- complete standalone CMakeLists.txt and package.xml are returned, not snippets;
- generated package/files are standalone and not placed under drone_health_registrable_template;
- adaptation_manifest.yaml follows the Required Adaptation Manifest Format;
- all commands use interfaces that exist in the supplied project files.
"""

    sources = [
        template_file.source
        for template_file in bundle.files
    ]

    return AdaptationPrompt(
        messages=[
            SystemMessage(content=SYSTEM_RULES),
            HumanMessage(content=human_content),
        ],
        sources=sources,
        template_type=bundle.template_type,
    )


if __name__ == "__main__":
    prompt = build_adaptation_prompt(
        'class BatteryPublisher; '
        'create_publisher<std_msgs::msg::Float32>'
    )

    print(
        f"Template type: {prompt.template_type}"
    )
    print(
        f"Messages: {len(prompt.messages)}"
    )
    print(
        f"Sources: {len(prompt.sources)}"
    )
    print(
        "Prompt characters: "
        f"{sum(len(str(message.content)) for message in prompt.messages)}"
    )
