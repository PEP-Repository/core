"""
PEP Connector is a Python package that provides a simple way to interact with 
both a PEP Repository and various APIs using a single package. It is a wrapper 
around the PEP Command Line Interface that allows you to easily make requests 
to a PEP Repository, an external data source (e.g., LimeSurvey API, Excel files), and 
interchange the results in a Pythonic way.
"""

from .peprepository import PEPRepository, PEPConfig
from .accessgroup import AccessGroup
from .dataadmin import DataAdmin
from .accessadmin import AccessAdmin
from .connectors import ConnectorConfig
from .json_connector import JSONConnector, JSONConnectorConfig
from .datamonitor import DataMonitor, DataMonitorConfig

try:
    from .mailsender import (
        MailSender, 
        MailSenderConfig, 
        EmailConfig,
        MailSenderSurveyConfig
    )
except ImportError:
    MailSender = None
    MailSenderConfig = None
    EmailConfig = None
    MailSenderSurveyConfig = None

try:
    from .excel_connector import ExcelConnector, ExcelConnectorConfig
except ImportError:
    ExcelConnector = None
    ExcelConnectorConfig = None

try:
    from .google_forms_connector import GoogleFormsConnector, GoogleFormsConfig
except ImportError:
    GoogleFormsConnector = None
    GoogleFormsConfig = None

try:
    from .limesurvey_connector import LimeSurveyConnector, LimeSurveyConfig, LimeSurveySurveyConfig
except ImportError:
    LimeSurveyConnector = None
    LimeSurveyConfig = None
    LimeSurveySurveyConfig = None

__all__ = [
    "PEPRepository",
    "AccessGroup",
    "DataAdmin",
    "AccessAdmin",
    "ConnectorConfig",
    "PEPConfig",
    "JSONConnector",
    "JSONConnectorConfig",
    "DataMonitor",
    "DataMonitorConfig"
]

if MailSender:
    __all__.extend([
        "MailSender", 
        "MailSenderConfig", 
        "EmailConfig",
        "MailSenderSurveyConfig"
    ])
if ExcelConnector:
    __all__.extend(["ExcelConnector", "ExcelConnectorConfig"])
if GoogleFormsConnector:
    __all__.extend(["GoogleFormsConnector", "GoogleFormsConfig"])
if LimeSurveyConnector:
    __all__.extend(["LimeSurveyConnector", "LimeSurveyConfig", "LimeSurveySurveyConfig"])
