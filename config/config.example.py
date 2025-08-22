"""Example configuration file. Copy this to config_local.py and fill in your values."""

# Jupyter configuration
JUPYTER_CONFIG = {
    "token": "<generate-random-token>",  # Generate a secure token for production
    "password": None,  # Set a password hash for production
    "allow_origin": "*",  # Restrict in production
}

# Database configuration
DATABASE_CONFIG = {
    "host": "localhost",
    "port": 5432,
    "database": "orderflow",
    "username": "<your-username>",
    "password": "<your-password>",
}

# API Keys (fill in for production)
API_KEYS = {
    "market_data_provider": "<your-api-key>",
    "exchange_api": "<your-api-key>",
}

# Security settings
SECURITY_CONFIG = {
    "ssl_cert_path": "/path/to/cert.pem",
    "ssl_key_path": "/path/to/key.pem",
    "allowed_ips": ["127.0.0.1"],
}

# Trading parameters
TRADING_CONFIG = {
    "max_position_size": 1000000,
    "risk_limit_percent": 2.0,
    "emergency_stop_loss": 5.0,
}
