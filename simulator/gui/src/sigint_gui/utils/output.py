# gui/src/sigint_gui/utils/output.py
from pathlib import Path

DEFAULT_OUTPUT_DIR = Path.cwd() / "output"   # project root /output

def ensure_output_dir() -> Path:
    DEFAULT_OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    return DEFAULT_OUTPUT_DIR