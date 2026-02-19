from __future__ import annotations

import logging
import sys
import json
from pydantic import BaseModel, model_validator, ConfigDict, DirectoryPath
from typing import Any, Self
from deepmerge import always_merger
from .accessgroup import AccessGroup
from .peprepository import PEPRepository
from .peprepository import PepError
from datetime import datetime


class ConnectorConfig(BaseModel):
    """Base configuration for all connectors."""

    model_config = ConfigDict(
        validate_assignment=True,
        arbitrary_types_allowed=True
    )

    prometheus_dir: DirectoryPath | None = None
    use_prometheus: bool = False
    env_prefix: str | None = None
    job_name: str | None = None

    @model_validator(mode='after')
    def validate_prometheus_dir(self):
        if self.use_prometheus and not self.prometheus_dir:
            raise ValueError("prometheus_dir must be provided when use_prometheus is True")
        return self

    @classmethod
    def from_dict(cls, data: dict[str, Any], **kwargs) -> Self:
        """Create ConnectorConfig from a dictionary.

        Args:
            data: Dictionary with config values
            **kwargs: Additional keyword arguments to override/supplement dict values
        """
        # Apply defaults to survey types using deep merge (if applicable)
        if "defaults" in data and "survey_types" in data:
            defaults = data["defaults"]
            for survey_name, survey_config in data["survey_types"].items():
                merged = defaults.copy()  # Start with copy of defaults, else the original defaults are modified in-place by deepmerge
                merged = always_merger.merge(merged, survey_config)  # Apply survey config on top
                data["survey_types"][survey_name] = merged
            # Remove defaults section after merging
            del data["defaults"]
        
        # Merge data with kwargs (kwargs take precedence)
        merged = {**data, **kwargs}
        return cls(**merged)        


class Connector(AccessGroup):
    LOG_TAG = "Connector"

    def __init__(self, repository: PEPRepository, config: ConnectorConfig):
        """
        Initialize Connector with a ConnectorConfig object.

        Args:
            repository: PEPRepository instance
            config: ConnectorConfig instance (required)
        """
        super().__init__(repository)

        if not isinstance(config, ConnectorConfig):
            raise ValueError("config must be an instance of ConnectorConfig")

        self.config = config

        self.prometheus_metrics = None
        if config.use_prometheus and config.prometheus_dir:
            self.prometheus_metrics = self.setup_prometheus_metrics(
                config.prometheus_dir, 
                config.env_prefix, 
                config.job_name
            )

        self.log("Connector initialized", logging.DEBUG)

    def log(self, message: str, level=logging.INFO, tag=None):
        super().log(message, level, tag)

        # Add Prometheus metrics tracking for errors and fatal errors
        if self.prometheus_metrics:
            # Count ERROR and CRITICAL level logs for metrics
            if level >= logging.ERROR:
                self.prometheus_metrics.count_error()

            # Treat CRITICAL logs as fatal errors
            if level == logging.CRITICAL:
                self.prometheus_metrics.track_fatal_error(message)

    def get_data_info(self, columns: list[str]) -> dict[str, dict[str, dict]]:
        """
        Get local pseudonyms and timestamps for a given participant and column.

        Args:
            columns: The column names to check

        Returns:
            Dictionary with local pseudonyms as keys and as values a dict of columns with timestamps
        """
        self.log(f"Getting data info for columns: {columns}", level=logging.DEBUG)
        if not isinstance(columns, list):
            raise ValueError("Columns must be a list of strings")
        if not columns:
            raise ValueError("At least one column must be provided")
        command = ["list", "--metadata", "--no-inline-data", "--local-pseudonyms", "-P", "*"]
        for column in columns:
            command.extend(["-c", column])
        try:
            output = self.repository.run_command(command)
            data = json.loads(output)
            self.log(f"Parsed {len(data)} entries from PEP response", level=logging.DEBUG)

            result = {}
            for entry in data:
                lp = entry.get("lp")
                if lp is None:
                    self.log("Entry without local pseudonym found", level=logging.WARNING)
                    continue # should error instead
                result[lp] = {}
                for column in columns:
                    result[lp][column] = {}
                    if "metadata" in entry and column in entry["metadata"]:
                        metadata = entry["metadata"][column]
                        if "timestamp" in metadata and "epochMillis" in metadata["timestamp"]:
                            epoch_millis = int(metadata["timestamp"]["epochMillis"])
                            timestamp = datetime.fromtimestamp(epoch_millis / 1000)
                            result[lp][column]["timestamp"] = timestamp
            self.log(f"Data info processed for {len(result)} local pseudonyms", level=logging.DEBUG)
            return result
        except PepError as e:
            self.log(f"Error checking data info: {e}", level=logging.ERROR)
            raise e
        except json.JSONDecodeError as e:
            self.log(f"Error parsing JSON response: {e}", level=logging.ERROR)
            raise e
        except Exception as e:
            self.log(f"Unexpected error: {e}", level=logging.ERROR)
            raise

    def check_data_existence_by_sp(self, short_pseudonym: str, columns: list[str]) -> dict[str, bool]:
        """
        Check data existence for a given short pseudonym and given columns.

        Args:
            short_pseudonym: The short pseudonym to check
            columns: The column names to check

        Returns:
            Dictionary mapping column names to boolean existence values
            Example: {'column1': True, 'column2': False}
        """
        self.log(f"Checking existence for columns: {columns}", level=logging.DEBUG)
        if not isinstance(columns, list):
            raise ValueError("Columns must be a list of strings")
        if not columns:
            raise ValueError("At least one column must be provided")
        command = ["list", "--metadata", "--no-inline-data", "--local-pseudonyms", "--sp", short_pseudonym]
        for column in columns:
            command.extend(["-c", column])
        try:
            output = self.repository.run_command(command)
            data = json.loads(output)
            # No entries found for this short pseudonym, this is ok, apparently all columns don't have data
            if len(data) == 0:
                self.log(f"No entries found for short pseudonym {short_pseudonym}", level=logging.DEBUG)
                return {column: False for column in columns}
            elif len(data) > 1:
                self.log(f"Expected 1 entry for short pseudonym {short_pseudonym}, got {len(data)}", level=logging.ERROR)
                raise ValueError(f"Expected 1 entry for short pseudonym {short_pseudonym}, got {len(data)}")
            entry = data[0]
            result = {}
            for column in columns:
                if "metadata" in entry and column in entry["metadata"]:
                    result[column] = True
                else:
                    result[column] = False
            self.log(f"Data existence checked for {short_pseudonym}: {result}", level=logging.DEBUG)
            return result
        except PepError as e:
            self.log(f"Error checking data existence: {e}", level=logging.ERROR)
            raise e
        except json.JSONDecodeError as e:
            self.log(f"Error parsing JSON response: {e}", level=logging.ERROR)
            raise e
        except Exception as e:
            self.log(f"Unexpected error: {e}", level=logging.ERROR)
            raise

    def setup_prometheus_metrics(self, prometheus_dir, env_prefix=None, job_name=None):
        try:
            from .prometheus_metrics import PrometheusMetrics
            return PrometheusMetrics(
                prometheus_dir, 
                logger=self.logger,
                exception_handler_callback=self.handle_exception,
                env_prefix=env_prefix, 
                job_name=job_name
            )
        except ImportError as e:
            self.log("Prometheus client not installed. Skipping Prometheus metrics setup. Error: " + str(e), logging.WARNING)
            return None

    def handle_exception(self, exc_type, exc_value, exc_traceback):
        """
        Global exception handler that logs the exception and updates the exception metrics
        """
        # Log the exception using the standard handler
        sys.__excepthook__(exc_type, exc_value, exc_traceback)

        if self.prometheus_metrics:
            self.prometheus_metrics.track_exception(is_exception_occurring=True)
        # Exit with error code
        sys.exit(1)

    def parse_pep_python_list(self, value: str) -> list:
        """
        Parse a Python list stored as a string in a PEP column.
        Example: "['School1', 'School2']" -> ['School1', 'School2']
        """
        import ast
        if not value or not isinstance(value, str):
            raise ValueError("Value must be a non-empty string representing a Python list")
        try:
            parsed = ast.literal_eval(value)
            if isinstance(parsed, list):
                return parsed
            else:
                raise ValueError("Parsed value is not a list")
        except Exception:
            self.log(f"Error parsing Python list from value: {value}", level=logging.ERROR)
            raise