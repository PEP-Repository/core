import subprocess
import json
import logging
from typing import Literal
from logging.handlers import RotatingFileHandler

# Try to get version from package metadata
try:
    from importlib.metadata import version
    __version__ = version("pep_connector")
except (ImportError, ModuleNotFoundError):
    # If package metadata not available, fallback to hardcoded version
        __version__ = "1.0.0-dev"

# Custom colored formatter for console output
class ColoredFormatter(logging.Formatter):
    """Formatter that adds color to logging level names"""

    # ANSI color codes
    COLORS = {
        'DEBUG': '\033[94m',            # Blue
        'INFO': '\033[92m',             # Green
        'WARNING': '\033[93m',          # Yellow
        'ERROR': '\033[91m',            # Red
        'CRITICAL': '\033[91m\033[1m',  # Bold Red
        'RESET': '\033[0m'              # Reset color
    }

    # Maximum length of level names for padding
    MAX_LEVEL_LENGTH = 8  # Length of "CRITICAL"

    def format(self, record):
        levelname = record.levelname
        if levelname in self.COLORS:
            padded_levelname = levelname.ljust(self.MAX_LEVEL_LENGTH)
            colored_levelname = f"{self.COLORS[levelname]}{padded_levelname}{self.COLORS['RESET']}"
            record.levelname = colored_levelname
        return super().format(record)

class PEPRepository:
    VERSION = __version__
    LOG_TAG = "PEPRepository"

    def __init__(self, config_path: str = None, config = None, logging_level = "INFO", logger_name = "pep_connector"):
        if not config_path and not config:
            raise ValueError("Either config_path or config must be provided")
        if config_path and config:
            raise ValueError("Only one of config_path or config must be provided")
        if config_path:
            self.config = self.load_config(config_path)
        else:
            self.config = config
        self.logger_name = logger_name
        self.logger = self.configure_logging(logging_level)
        self.token = None
        self.pepcli_path = self.config["repo"]["pepcli_path"]
        self.log(f"PEPRepository version: {self.VERSION}")
        self.log(f"Configuration version: {self.config.get('config_version', 'unknown')}")

    def load_config(self, config_path):
        with open(config_path, "r") as config_file:
            return json.load(config_file)

    def configure_logging(self, logging_level: Literal["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"] = "INFO"):
        level = getattr(logging, logging_level, logging.INFO)
        logger = logging.getLogger(self.logger_name)
        logger.setLevel(level)

        # Loop over copy of handler list to clear existing handlers
        for handler in logger.handlers[:]:
            logger.removeHandler(handler)

        # Format string with fixed padding for level names
        format_str = "%(asctime)s <%(levelname)-8s> %(message)s"

        # Stream handler for console output with colors
        ch = logging.StreamHandler()
        ch.setLevel(level)
        colored_formatter = ColoredFormatter(format_str)
        ch.setFormatter(colored_formatter)
        logger.addHandler(ch)

        # Always add file logging
        fh = RotatingFileHandler("pep_connector.log", maxBytes=10*1024*1024, backupCount=5)
        fh.setLevel(level)
        file_formatter = logging.Formatter(format_str)
        fh.setFormatter(file_formatter)
        logger.addHandler(fh)

        return logger

    def log(self, message: str, level=logging.INFO):
        log_message = f"[{self.LOG_TAG}] {message}"
        self.logger.log(level, log_message)

    def authenticate(self, target, extra_token=None):
        self.log("Authenticating user")
        if target in self.config["repo"]["tokens"]:
            self.token = self.config["repo"]["tokens"][target]
        else:
            raise ValueError(f"Invalid target {target}, missing token in configuration")

        # Perform a pepcli query enrollment to check the validity of the token
        command = ["query", "enrollment"]
        try:
            self.run_command(command)
            self.log("Authentication successful")
        except RuntimeError as e:
            self.log(f"Authentication failed: {e}", level=logging.ERROR)
            raise ValueError("Invalid token")

    def __get_base_cmd(self):
        cmd = [self.pepcli_path, "--oauth-token", self.token]
        return cmd

    def run_command(self, command, input_data=None):
        if not self.token:
            raise ValueError("Token not set. Authenticate first.")
        cmd = self.__get_base_cmd() + command
        try:
            result = subprocess.run(cmd, input=input_data, capture_output=True, text=False, timeout=60)

            if result.returncode != 0:
                raise PepError(cmd, returncode=result.returncode, stdout=result.stdout.decode('utf-8'), stderr=result.stderr.decode('utf-8'))

            return result.stdout

        except subprocess.TimeoutExpired as e:
            self.log(f"Command timed out after 60 seconds. Max request body size limit might be reached: {' '.join(cmd)}", level=logging.ERROR)
            raise e

class PepError(Exception):
    """Error for calls to PEP"""

    def __init__(self, cmd, returncode=None, stdout=None, stderr=None):
        self.cmd = cmd
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr
        self.message = f"Error for command {cmd}"
        if returncode is not None:
            self.message += f"\nExit code: {returncode}"
        if stdout is not None:
            self.message += f"\nstdout:\n{stdout}"
        if stderr is not None:
            self.message += f"\nstderr:\n{stderr}"
        super().__init__(self.message)