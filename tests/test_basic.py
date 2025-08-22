import pytest
import sys
import os

# Add the src directory to the Python path
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def test_imports():
    """Test that all main modules can be imported."""
    from src.data import data_utils
    from src.models import base
    from src.utils import evaluation

    assert data_utils
    assert base
    assert evaluation


def test_data_utils():
    """Test basic functionality of data_utils."""
    from src.data.data_utils import preprocess_data
    import pandas as pd

    # Create a simple test DataFrame
    df = pd.DataFrame({"feature1": [1, 2, 3], "feature2": [4, 5, 6]})

    processed_df = preprocess_data(df)
    assert isinstance(processed_df, pd.DataFrame)
    assert not processed_df.empty
