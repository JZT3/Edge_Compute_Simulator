"""Structured logging configuration for sigint_gui."""

from __future__ import annotations

import json
import logging
import os
from pathlib import Path
from typing import Optional
from datetime import datetime, timezone


class JSONFormatter(logging.Formatter):
    """Format log records as JSON lines."""

    def format(self, record: logging.LogRecord) -> str:
        log_entry = {
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "level": record.levelname,
            "logger": record.name,
            "message": record.getMessage(),
        }
        if record.exc_info and record.exc_info[1]:
            log_entry["exception"] = str(record.exc_info[1])
        return json.dumps(log_entry)


def setup_logging(
    log_file: Optional[str] = None,
    level: int = logging.INFO,
    console: bool = True,
) -> None:
    """Configure structured logging for the application.

    Args:
        log_file: Path to a log file; if None, only console logging is used.
        level: Logging level (e.g., logging.DEBUG, logging.INFO).
        console: If True, also log to stdout.
    """
    assert level in (
        logging.DEBUG,
        logging.INFO,
        logging.WARNING,
        logging.ERROR,
        logging.CRITICAL,
    ), f"Invalid log level: {level}"

    root_logger = logging.getLogger()
    root_logger.setLevel(level)

    # Remove any existing handlers to avoid duplication
    for handler in root_logger.handlers[:]:
        root_logger.removeHandler(handler)

    formatter = JSONFormatter()

    if console:
        console_handler = logging.StreamHandler()
        console_handler.setFormatter(formatter)
        root_logger.addHandler(console_handler)

    if log_file:
        log_path = Path(log_file)
        log_path.parent.mkdir(parents=True, exist_ok=True)
        file_handler = logging.FileHandler(log_path, mode="a")
        file_handler.setFormatter(formatter)
        root_logger.addHandler(file_handler)

    # Silence noisy third‑party loggers (e.g., matplotlib)
    logging.getLogger("matplotlib").setLevel(logging.WARNING)