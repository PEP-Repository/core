import logging
import importlib.util

from .peprepository import PEPRepository
from .connectors import Connector

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

    def __init__(self, repository: PEPRepository,
                 prometheus_dir=None,
                 use_prometheus=False,
                 env_prefix=None,
                 job_name=None):
        super().__init__(repository, prometheus_dir, use_prometheus, env_prefix, job_name)
        self.log("GoogleFormsConnector initialized", logging.DEBUG)

    def read_google_form_responses(self):
        pass

    def parse_google_forms_responses(self, responses):
        pass

    def upload_to_pep(self, parsed_responses):
        pass
