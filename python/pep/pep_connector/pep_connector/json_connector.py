from __future__ import annotations

import logging
from typing import Any
from .peprepository import PEPRepository
from .connectors import Connector, ConnectorConfig


class JSONConnectorConfig(ConnectorConfig):
    """Configuration for JSON connector."""
    pass


class JSONConnector(Connector):
    LOG_TAG = "JSONConnector"

    def __init__(self, repository: PEPRepository, config: JSONConnectorConfig):
        """
        Initialize JSONConnector with a JSONConnectorConfig object.

        Args:
            repository: PEPRepository instance
            config: JSONConnectorConfig instance (required)
        """
        if not isinstance(config, JSONConnectorConfig):
            raise ValueError("config must be an instance of JSONConnectorConfig")

        # Initialize parent with the config
        super().__init__(repository, config)
        self.log("JSONConnector initialized", logging.DEBUG)
