import logging
from .peprepository import PEPRepository
from .connectors import Connector

class JSONConnector(Connector):
    LOG_TAG = "JSONConnector"

    def __init__(self, repository: PEPRepository,
                 prometheus_dir=None,
                 use_prometheus=False,
                 env_prefix=None,
                 job_name=None):
        super().__init__(repository, prometheus_dir, use_prometheus, env_prefix, job_name)
        self.log("JSONConnector initialized", logging.DEBUG)
