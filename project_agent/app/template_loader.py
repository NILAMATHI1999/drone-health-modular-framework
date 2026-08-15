from dataclasses import dataclass
from pathlib import Path

from app.task_retriever import detect_template_type


PROJECT_ROOT = Path(__file__).resolve().parents[2]

TEMPLATE_ROOT = (
    PROJECT_ROOT
    / "src"
    / "drone_health_registrable_template"
)

TEMPLATE_FILES = {
    "publisher": {
        "cpp": (
            TEMPLATE_ROOT
            / "template_node"
            / "registrable_publisher_template_node.cpp"
        ),
        "yaml": (
            TEMPLATE_ROOT
            / "config"
            / "registrable_publisher_template.yaml"
        ),
    },
    "subscriber": {
        "cpp": (
            TEMPLATE_ROOT
            / "template_node"
            / "registrable_subscriber_template_node.cpp"
        ),
        "yaml": (
            TEMPLATE_ROOT
            / "config"
            / "registrable_subscriber_template.yaml"
        ),
    },
    "service": {
        "cpp": (
            TEMPLATE_ROOT
            / "template_node"
            / "registrable_service_template_node.cpp"
        ),
        "yaml": (
            TEMPLATE_ROOT
            / "config"
            / "registrable_service_template.yaml"
        ),
    },
}

SHARED_FILES = {
    "cmake": TEMPLATE_ROOT / "CMakeLists.txt",
    "package_xml": TEMPLATE_ROOT / "package.xml",
    "readme": TEMPLATE_ROOT / "README.md",
    "template_readme": (
        TEMPLATE_ROOT
        / "template_node"
        / "README.md"
    ),
}


@dataclass(frozen=True)
class TemplateFile:
    role: str
    path: Path
    source: str
    content: str


@dataclass(frozen=True)
class TemplateBundle:
    template_type: str
    files: tuple[TemplateFile, ...]

    def get(self, role: str) -> TemplateFile:
        for template_file in self.files:
            if template_file.role == role:
                return template_file

        raise KeyError(
            f"template bundle has no {role!r} file"
        )


def load_template_file(
    role: str,
    path: Path,
) -> TemplateFile:
    if not path.is_file():
        raise FileNotFoundError(
            f"required template file is missing: {path}"
        )

    content = path.read_text(encoding="utf-8")

    if not content.strip():
        raise ValueError(
            f"required template file is empty: {path}"
        )

    return TemplateFile(
        role=role,
        path=path,
        source=str(path.relative_to(PROJECT_ROOT)),
        content=content,
    )


def load_template_bundle(
    user_input: str,
) -> TemplateBundle:
    template_type = detect_template_type(
        user_input
    )

    selected_files = {
        **TEMPLATE_FILES[template_type],
        **SHARED_FILES,
    }

    files = tuple(
        load_template_file(role, path)
        for role, path in selected_files.items()
    )

    return TemplateBundle(
        template_type=template_type,
        files=files,
    )


if __name__ == "__main__":
    bundle = load_template_bundle(
        'create_publisher<std_msgs::msg::Float32>'
    )

    print(
        f"Template type: {bundle.template_type}"
    )

    for template_file in bundle.files:
        print(
            f"{template_file.role}: "
            f"{template_file.source} "
            f"({len(template_file.content)} characters)"
        )
