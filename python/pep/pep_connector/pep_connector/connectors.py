from __future__ import annotations

import logging
import sys
import json
import copy
from pydantic import BaseModel, model_validator, ConfigDict, DirectoryPath
from typing import Any, Self
from deepmerge import always_merger
from .accessgroup import AccessGroup
from .peprepository import PEPRepository
from .peprepository import PepError
from datetime import datetime


class RunStatistics:
    """Container for run statistics that supports accumulation via += operator."""
    
    def __init__(self, statistics: dict[str, dict[str, Any]] = None):
        """
        Initialize statistics.

        Args:
            statistics: Dict mapping slugs to config dicts with 'description' and optional 'value'.
                       If 'value' is omitted, defaults to 0 for numeric stats.
                       Example: {'total_subjects': {'description': 'Total subjects', 'value': 10}}
                       Or: {'total_subjects': {'description': 'Total subjects'}}  # defaults to 0
        """
        self.stats = {}
        if statistics:
            for slug, config in statistics.items():
                self.stats[slug] = {
                    'description': config['description'],
                    'value': config.get('value', 0)
                }

    def __iadd__(self, other: 'RunStatistics') -> 'RunStatistics':
        """
        Add another RunStatistics instance to this one using +=.

        Numeric values are summed.
        Descriptions are preserved from self.

        Args:
            other: Another RunStatistics instance to add

        Returns:
            self (for chaining)
        """
        if not isinstance(other, RunStatistics):
            raise TypeError(f"unsupported operand type(s) for +=: 'RunStatistics' and '{type(other).__name__}'")

        for slug, data in other.stats.items():
            if slug in self.stats:
                # Sum numeric values
                if isinstance(data['value'], (int, float)):
                    self.stats[slug]['value'] += data['value']
            else:
                # Add new statistic if it doesn't exist
                self.stats[slug] = data.copy()

        return self
    
    def get(self, slug: str, default: Any = None) -> Any:
        """Get the value of a statistic by slug."""
        return self.stats.get(slug, {}).get('value', default)

    def set(self, slug: str, description: str, value: Any) -> None:
        """Set a single statistic."""
        self.stats[slug] = {'description': description, 'value': value}

    def increment(self, slug: str, amount: int | float = 1) -> None:
        """Increment a numeric statistic by the given amount.

        Args:
            slug: The statistic identifier
            amount: Amount to add (default: 1)
        """
        if slug in self.stats:
            if isinstance(self.stats[slug]['value'], (int, float)):
                self.stats[slug]['value'] += amount
            else:
                raise TypeError(f"Cannot increment non-numeric statistic '{slug}'")
        else:
            raise KeyError(f"Statistic '{slug}' not found")
    
    def copy(self) -> 'RunStatistics':
        """Create a deep copy of this RunStatistics instance."""
        new_stats = RunStatistics()
        for slug, data in self.stats.items():
            new_stats.stats[slug] = {'description': data['description'], 'value': data['value']}
        return new_stats
    
    def print(self, title: str, logger_func) -> None:
        """
        Print statistics in a formatted way.
        
        Args:
            title: Title for the statistics section
            logger_func: Function to call for logging (e.g., self.log)
        """
        separator = "=" * (len(title) + 4)
        logger_func(separator, level=logging.INFO)
        logger_func(title, level=logging.INFO)
        for slug, data in self.stats.items():
            logger_func(f"{data['description']}: {data['value']}", level=logging.INFO)
        logger_func(separator, level=logging.INFO)


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
                merged = copy.deepcopy(defaults)  # Deep copy defaults to avoid sharing nested dict references across surveys
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
            config: ConnectorConfig instance
        """
        super().__init__(repository)

        if not isinstance(config, ConnectorConfig):
            raise ValueError("config must be an instance of ConnectorConfig")

        self.config = config
        self.run_statistics = None

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

    def get_column_timestamps(self, columns: list[str], subject_group: str = "*") -> dict[str, dict[str, dict]]:
        """
        Get local pseudonyms and timestamps for given column names.

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
        command = ["list", "--group-output", "--metadata", "--no-inline-data", "--local-pseudonyms", "-P", subject_group]
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
        command = ["list", "--group-output", "--metadata", "--no-inline-data", "--local-pseudonyms", "--sp", short_pseudonym]
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

    def write_prometheus_statistics(self) -> None:
        """
        Write run statistics to Prometheus metrics files if enabled.
        Should be called at the end of connector execution.
        """
        if self.prometheus_metrics and self.run_statistics:
            self.prometheus_metrics.write_extra_run_statistics(self.run_statistics.stats)

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