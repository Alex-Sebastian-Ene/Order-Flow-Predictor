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
        import sys
        from pathlib import Path

        # Get absolute path to src/python directory
        test_dir = Path(__file__).parent.absolute()
        project_root = test_dir.parent
        src_python_path = project_root / "src" / "python"

        # Ensure the path exists
        if not src_python_path.exists():
            self.fail(f"Source path does not exist: {src_python_path}")

        # Add to Python path if not already there (redundant with conftest.py
        # but ensures robustness)
        src_python_str = str(src_python_path)
        if src_python_str not in sys.path:
            sys.path.insert(0, src_python_str)

        # Test individual imports with specific error handling
        try:
            from models.base import BaseModel  # type: ignore

            self.assertTrue(hasattr(BaseModel, "__init__"))
        except ImportError as e:
            # Alternative import method for CI environments
            sys.path.insert(0, str(src_python_path))
            try:
                from models.base import BaseModel  # type: ignore

                self.assertTrue(hasattr(BaseModel, "__init__"))
            except ImportError:
                # Debug information for CI
                import os

                models_dir = src_python_path / "models"
                models_base_file = models_dir / "base.py"
                available_paths = [p for p in sys.path if "src" in p or "python" in p]
                directory_contents = (
                    list(os.listdir(src_python_path))
                    if src_python_path.exists()
                    else []
                )
                models_contents = (
                    list(os.listdir(models_dir)) if models_dir.exists() else []
                )

                self.fail(
                    f"Failed to import BaseModel: {e}. "
                    f"Src path: {src_python_path} "
                    f"(exists: {src_python_path.exists()}). "
                    f"Models dir: {models_dir} (exists: {models_dir.exists()}). "
                    f"Models base file: {models_base_file} "
                    f"(exists: {models_base_file.exists()}). "
                    f"Python paths with 'src' or 'python': {available_paths}. "
                    f"src/python contents: {directory_contents}. "
                    f"models contents: {models_contents}"
                )

        try:
            from order_flow_predictor.constants import (  # type: ignore
                MAX_ORDER_BOOK_LEVELS,
            )

            self.assertIsInstance(MAX_ORDER_BOOK_LEVELS, int)
        except ImportError as e:
            self.fail(f"Failed to import constants: {e}")

        try:
            from order_flow_predictor.predictor import (  # type: ignore
                OrderFlowPredictor,
            )

            self.assertTrue(hasattr(OrderFlowPredictor, "__init__"))
        except ImportError as e:
            self.fail(f"Failed to import OrderFlowPredictor: {e}")

        try:
            from utils.evaluation import calculate_metrics  # type: ignore

            self.assertTrue(hasattr(calculate_metrics, "__call__"))
        except ImportError as e:
            self.fail(f"Failed to import calculate_metrics: {e}")

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
