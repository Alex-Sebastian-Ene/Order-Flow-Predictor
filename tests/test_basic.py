"""Basic test module."""

import sys
import unittest
from pathlib import Path

# Add the project root to Python path
project_root = Path(__file__).parent.parent
sys.path.insert(0, str(project_root))


class TestBasic(unittest.TestCase):
    """Basic test cases for the order flow predictor."""

    def test_constants(self) -> None:
        """Test that constants are defined."""
        # Add src/python to path for imports
        import sys
        from pathlib import Path

        src_python_path = Path(__file__).parent.parent / "src" / "python"
        sys.path.insert(0, str(src_python_path))

        from order_flow_predictor import constants  # type: ignore

        # Check that key constants exist
        self.assertTrue(hasattr(constants, "MAX_ORDER_BOOK_LEVELS"))
        self.assertTrue(hasattr(constants, "DEFAULT_BATCH_SIZE"))
        self.assertTrue(hasattr(constants, "MAX_POSITION_SIZE"))

    def test_config_loading(self) -> None:
        """Test configuration utilities."""
        try:
            # Import from src.utils.config to avoid module conflicts
            import src.utils.config as config_module

            validate_config = config_module.validate_config

            # Test with valid config
            valid_config = {
                "model": {"type": "test"},
                "data": {"source": "test"},
                "training": {"epochs": 10},
            }
            self.assertTrue(validate_config(valid_config))

            # Test with invalid config
            invalid_config = {"model": {"type": "test"}}
            self.assertFalse(validate_config(invalid_config))
        except ImportError as e:
            self.fail(f"Failed to import config utilities: {e}")


if __name__ == "__main__":
    unittest.main()
