"""Configuration utilities."""

import os
from typing import Any, Dict

import yaml  # type: ignore


def load_config(config_path: str) -> Dict[str, Any]:
    """Load configuration from YAML file.

    Args:
        config_path: Path to the configuration file

    Returns:
        Configuration dictionary

    Raises:
        FileNotFoundError: If config file doesn't exist
        yaml.YAMLError: If config file is invalid
    """
    if not os.path.exists(config_path):
        raise FileNotFoundError(f"Config file not found: {config_path}")

    with open(config_path, "r", encoding="utf-8") as f:
        config = yaml.safe_load(f)
        if config is None:
            return {}
        if not isinstance(config, dict):
            raise yaml.YAMLError(
                f"Config file must contain a YAML object, got {type(config)}"
            )
        return config


def validate_config(config: Dict[str, Any]) -> bool:
    """Validate configuration dictionary.

    Args:
        config: Configuration dictionary to validate

    Returns:
        True if valid, False otherwise
    """
    required_keys = ["model", "data", "training"]
    return all(key in config for key in required_keys)
