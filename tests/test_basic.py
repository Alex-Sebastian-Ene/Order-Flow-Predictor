"""Basic test module."""

import sys
import unittest
from pathlib import Path

# Add the project root to Python path
project_root = Path(__file__).parent.parent
sys.path.insert(0, str(project_root))


class TestBasic(unittest.TestCase):
    """Basic test cases for the order flow predictor."""

    def test_imports(self) -> None:
        """Test that core modules can be imported."""
        try:
            # These imports work at runtime due to conftest.py setup
            # Import from full paths to ensure CI compatibility
            import sys
            from pathlib import Path

            # Add src/python to path for Python modules
            src_python_path = Path(__file__).parent.parent / "src" / "python"
            sys.path.insert(0, str(src_python_path))

            from models.base import BaseModel  # type: ignore
            from order_flow_predictor.constants import (  # type: ignore
                MAX_ORDER_BOOK_LEVELS,
            )
            from order_flow_predictor.predictor import (  # type: ignore
                OrderFlowPredictor,
            )

            from utils.evaluation import calculate_metrics  # type: ignore

            # Verify imports worked
            self.assertTrue(hasattr(BaseModel, "__init__"))
            self.assertIsInstance(MAX_ORDER_BOOK_LEVELS, int)
            self.assertTrue(hasattr(OrderFlowPredictor, "__init__"))
            self.assertTrue(hasattr(calculate_metrics, "__call__"))
        except ImportError as e:
            self.fail(f"Failed to import core modules: {e}")

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
