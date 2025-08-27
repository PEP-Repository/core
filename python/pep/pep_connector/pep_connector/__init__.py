"""
PEP Connector is a Python package that provides a simple way to interact with 
both a PEP Repository and various APIs using a single package. It is a wrapper 
around the PEP Command Line Interface that allows you to easily make requests 
to a PEP Repository, an external data source (e.g., LimeSurvey API, Excel files), and 
interchange the results in a Pythonic way.
"""

from .peprepository import PEPRepository
from .accessgroup import AccessGroup
from .dataadmin import DataAdmin
from .accessadmin import AccessAdmin
from .json_connector import JSONConnector
from .datamonitor import DataMonitor
from .mailsender import MailSender

try:
    from .excel_connector import ExcelConnector
except ImportError:
    ExcelConnector = None

try:
    from .google_forms_connector import GoogleFormsConnector
except ImportError:
    GoogleFormsConnector = None

try:
    from .limesurvey_connector import LimeSurveyConnector
except ImportError:
    LimeSurveyConnector = None

__all__ = [
    "PEPRepository",
    "AccessGroup",
    "DataAdmin",
    "AccessAdmin",
    "JSONConnector",
    "DataMonitor",
    "MailSender"
]

if ExcelConnector:
    __all__.append("ExcelConnector")
if GoogleFormsConnector:
    __all__.append("GoogleFormsConnector")
if LimeSurveyConnector:
    __all__.append("LimeSurveyConnector")
