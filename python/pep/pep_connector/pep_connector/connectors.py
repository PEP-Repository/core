import logging
import sys
import json
from .accessgroup import AccessGroup
from .peprepository import PEPRepository
from .peprepository import PepError
from datetime import datetime

class Connector(AccessGroup):
    LOG_TAG = "Connector"

    def __init__(self, repository: PEPRepository, prometheus_dir=None, use_prometheus=False, env_prefix=None, job_name=None):
        super().__init__(repository)
        self.prometheus_dir = prometheus_dir
        self.env_prefix = env_prefix
        self.job_name = job_name
        
        # Initialize Prometheus metrics with exception handler
        self.prometheus_metrics = None
        if use_prometheus and prometheus_dir:
            self.prometheus_metrics = self.setup_prometheus_metrics(prometheus_dir, env_prefix, job_name)
        
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
            Dictionary with local pseudonyms as keys and column timestamps as values
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