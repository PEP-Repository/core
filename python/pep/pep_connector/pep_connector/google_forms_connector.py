from __future__ import annotations

import logging
import importlib.util
from .peprepository import PEPRepository
from .connectors import Connector, ConnectorConfig


class GoogleFormsConfig(ConnectorConfig):
    """Configuration for Google Forms connector."""
    pass


if not importlib.util.find_spec("google.oauth2.service_account"):
    raise ImportError("google.oauth2 is required for GoogleFormsConnector. Install it with 'pip install pep-connector[google_forms_connector]'")
else:
    from google.oauth2 import service_account  # noqa: F401

if not importlib.util.find_spec("googleapiclient.discovery"):
    raise ImportError("googleapiclient.discovery is required for GoogleFormsConnector. Install it with 'pip install pep-connector[google_forms_connector]'")
else:
    from googleapiclient.discovery import build  # noqa: F401
class GoogleFormsConnector(Connector):
    LOG_TAG = "GoogleFormsConnector"

    def __init__(self, repository: PEPRepository, config: GoogleFormsConfig):
        """
        Initialize GoogleFormsConnector with a GoogleFormsConfig object.

        Args:
            repository: PEPRepository instance
            config: GoogleFormsConfig instance (required)
        """
        if not isinstance(config, GoogleFormsConfig):
            raise ValueError("config must be an instance of GoogleFormsConfig")

        # Initialize parent with the config
        super().__init__(repository, config)
        self.log("GoogleFormsConnector initialized", logging.DEBUG)

    def read_google_form_responses(self):
        pass

    def parse_google_forms_responses(self, responses):
        pass

    def upload_to_pep(self, parsed_responses):
        pass
