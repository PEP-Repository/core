import os
import sys
import tempfile
import logging
import time
from prometheus_client import CollectorRegistry, Counter, Gauge, Info, generate_latest

class PrometheusMetrics:
    LOG_TAG = "PrometheusMetrics"

    def __init__(self, prometheus_dir, logger=None, exception_handler_callback=None, env_prefix=None, job_name=None):
        self.prometheus_dir = prometheus_dir
        self.env_prefix = env_prefix
        self.logger = logger if logger else logging.getLogger(__name__)
        self.job_name = job_name if job_name else "pep-job"

        self.log("Initializing PrometheusMetrics", logging.DEBUG)

        # Define file paths early
        self.fatal_error_file = self.get_metrics_file_path("pep_fatal_error.prom")
        self.exception_metrics_file = self.get_metrics_file_path("pep_exceptions.prom")
        self.run_metrics_file = self.get_metrics_file_path("pep_run.prom")
        self.error_metrics_file = self.get_metrics_file_path("pep_errors.prom")

        # Initialize exception tracking
        self.exception_counter = 0
        self.error_counter = 0  # Counter for ERROR level logs

        if prometheus_dir:
            # Ensure fatal error metric file exists and is initialized
            self.initialize_fatal_error_metric()

            # Check fatal error status by reading the metric value
            try:
                fatal_error_status = self._read_metric_value('pep_fatal_error', self.fatal_error_file)
                if fatal_error_status == 1:
                    self.log(f"FATAL ERROR detected (value=1 in {self.fatal_error_file}). Connector will not run until the value is manually reset to 0.", logging.ERROR)
                    sys.exit(1)
                elif fatal_error_status == 0:
                    self.log("Fatal error status is OK (value=0).", logging.DEBUG)
                else:
                    self.log(f"Unexpected fatal error status value '{fatal_error_status}' found in {self.fatal_error_file}. Assuming OK.", logging.WARNING)

            except FileNotFoundError:
                self.log(f"Fatal error file {self.fatal_error_file} not found during check, though initialization was attempted.", logging.ERROR)
                sys.exit(1)
            except Exception as e:
                self.log(f"Error reading fatal error status from {self.fatal_error_file}: {e}. Exiting.", logging.ERROR)
                sys.exit(1)

            # Other initializations
            self.track_exception(exception_handler_callback)
            self.track_run()
            self.track_error_count()

    def log(self, message: str, level=logging.INFO, tag=None) -> None:
        if tag is None:
            tag = self.LOG_TAG
        log_message = f"[{tag}] {message}"
        self.logger.log(level, log_message)

    def get_metrics_file_path(self, base_filename: str) -> str:
        """Generate a metrics file path with environment prefix

        Args:
            base_filename: Base filename without prefix

        Returns:
            str: Full path to the metrics file
        """
        prefixed_filename = f"{self.env_prefix}_{base_filename}" if self.env_prefix else base_filename
        return os.path.join(self.prometheus_dir, prefixed_filename)

    def create_textfile(self, metrics, metrics_file="metrics.prom"):
        """Create a Prometheus-format metrics file from a dictionary of metrics

        Args:
            metrics: Dictionary mapping metric names to dictionaries with 'value', 'help', 'type', and optional 'labels'
            metrics_file: Path to the metrics file to create

        Returns:
            bool: True if the file was created successfully
        """

        # If metrics_file is just a filename, add the directory path
        if os.path.dirname(metrics_file) == "":
            metrics_file = os.path.join(self.prometheus_dir, metrics_file)

        try:
            # Create metrics directory if it doesn't exist
            metrics_dir = os.path.dirname(metrics_file)
            os.makedirs(metrics_dir, exist_ok=True)

            # Create a registry for the metrics
            registry = CollectorRegistry()

            # Add each metric to the registry
            for metric_name, metric_data in metrics.items():
                value = metric_data['value']
                metric_help = metric_data.get('help', f"Description of {metric_name}")
                metric_type = metric_data.get('type', 'gauge' if isinstance(value, float) else 'counter')
                labels = metric_data.get('labels', {'job': self.job_name})

                # Create the appropriate metric type
                if metric_type == 'counter':
                    # For counters, we register with the label keys
                    label_keys = list(labels.keys())
                    metric = Counter(metric_name, metric_help, label_keys, registry=registry)

                    # Then we create it with the label values
                    label_values = [labels[key] for key in label_keys]
                    counter = metric.labels(*label_values)
                    counter.inc(value)

                elif metric_type == 'gauge':
                    # For gauges, same approach
                    label_keys = list(labels.keys())
                    metric = Gauge(metric_name, metric_help, label_keys, registry=registry)

                    label_values = [labels[key] for key in label_keys]
                    gauge = metric.labels(*label_values)
                    gauge.set(value)

                elif metric_type == 'info':
                    # Info metrics work differently, they take a dict
                    info = Info(metric_name, metric_help, registry=registry)
                    info.info(labels)

            # Write to temporary file then rename for atomic operation
            with tempfile.NamedTemporaryFile(mode='wb', delete=False, dir=metrics_dir) as tmp_file:
                tmp_file.write(generate_latest(registry))

            # Set file permissions to 746 (rwxr--rw-)
            os.chmod(tmp_file.name, 0o746)

            # Atomically rename the file
            os.replace(tmp_file.name, metrics_file)
            self.log(f"Prometheus metrics written to {metrics_file}")
        except Exception as e:
            self.log(f"Failed to write Prometheus metrics: {e}", logging.ERROR)
            raise

    def _read_metric_value(self, metric_name: str, file_path: str) -> float:
        """
        Reads the value of a simple gauge metric from a Prometheus text file.
        Assumes metric format like: metric_name{label="value",...} value
        Returns the metric value as a float.
        """
        if not os.path.exists(file_path):
            raise FileNotFoundError(f"Metrics file not found: {file_path}")

        try:
            with open(file_path, 'r') as f:
                for line in f:
                    line = line.strip()
                    if line.startswith('#') or not line:
                        continue
                    # Find metric name, ignore labels for simplicity here, the last part should be the value
                    if line.startswith(metric_name):
                        parts = line.split()
                        if len(parts) >= 2:
                            return float(parts[-1])
            # Metric not found in the file
            raise ValueError(f"Metric '{metric_name}' not found in file {file_path}")
        except Exception as e:
            self.log(f"Error parsing metric '{metric_name}' from {file_path}: {e}", logging.ERROR)
            raise

    def initialize_fatal_error_metric(self) -> None:
        """
        Ensures the fatal error metric file exists and is initialized with value 0.
        """
        if not os.path.exists(self.fatal_error_file):
            self.log(f"Fatal error metric file {self.fatal_error_file} not found. Initializing with value 0.", logging.INFO)
            try:
                unix_timestamp = time.time()
                metrics = {
                    "pep_fatal_error": {
                        'value': 0, # Initialize to 0
                        'help': "Indicates if a fatal error has occurred (1=yes, 0=no). Must be manually reset from 1.",
                        'type': "gauge",
                        'labels': {'job': self.job_name}
                    },
                    "pep_fatal_error_timestamp": {
                        'value': unix_timestamp,
                        'help': "Timestamp when the fatal error status was last set",
                        'type': "gauge",
                        'labels': {'job': self.job_name}
                    }
                }
                self.create_textfile(metrics, self.fatal_error_file)
                self.log(f"Initialized fatal error metric file {self.fatal_error_file} with value 0.", logging.DEBUG)
            except Exception as e:
                self.log(f"Failed to initialize fatal error metric file {self.fatal_error_file}: {e}", logging.ERROR)
                raise
        else:
             self.log(f"Fatal error metric file {self.fatal_error_file} already exists.", logging.DEBUG)

    def count_error(self) -> None:
        """
        Increment the error counter for ERROR level logs
        """
        self.error_counter += 1
        self.track_error_count()

    def track_error_count(self) -> None:
        """
        Record the number of ERROR level logs in a metrics file
        """
        metrics = {
            "pep_error_logs_count": {
                'value': self.error_counter,
                'help': "Number of ERROR level logs during the last run",
                'type': "counter",
                'labels': {'job': self.job_name}
            }
        }

        self.create_textfile(metrics, self.error_metrics_file)
        self.log(f"Recorded error count: {self.error_counter}", logging.DEBUG)

    def track_fatal_error(self, error_message: str="Unspecified fatal error") -> None:
        """
        Set the fatal error metric to 1 in the Prometheus format file.
        This indicates a fatal error occurred and requires manual intervention
        (setting the value back to 0) before the connector can run again.

        Args:
            error_message: Description of the fatal error
        """
        try:
            unix_timestamp = time.time()

            metrics = {
                "pep_fatal_error": {
                    'value': 1,
                    'help': "Indicates if a fatal error has occurred (1=yes, 0=no). Must be manually reset from 1.",
                    'type': "gauge",
                    'labels': {'job': self.job_name}
                },
                "pep_fatal_error_timestamp": {
                    'value': unix_timestamp,
                    'help': "Timestamp when the fatal error status was last set",
                    'type': "gauge",
                    'labels': {'job': self.job_name}
                }
            }

            self.create_textfile(metrics, self.fatal_error_file)
            self.log(f"Fatal error recorded: {error_message}. Metric 'pep_fatal_error' set to 1 in {self.fatal_error_file}.", logging.INFO)
        except Exception as e:
            self.log(f"Failed to update fatal error metric file {self.fatal_error_file}: {e}", logging.ERROR)

    def track_exception(self, exception_handler_callback=None, is_exception_occurring=False) -> None:
        """Initialize or update exception tracking

        Args:
            exception_handler_callback: Function to call when an exception is caught
            is_exception_occurring: True if an exception is currently being handled, False for initialization
        """
        # Set counter to 0 during initialization, 1 if an exception is occurring
        self.exception_counter = 1 if is_exception_occurring else 0

        # Register the provided exception handler callback during initialization
        if not is_exception_occurring and exception_handler_callback:
            sys.excepthook = exception_handler_callback
        elif not is_exception_occurring and not exception_handler_callback:
            self.log("No exception handler callback provided. Exceptions will not be tracked.", logging.WARNING)

        metrics = {
            "pep_uncaughtExceptions_count": {
                'value': self.exception_counter,
                'help': "Number of unhandled errors during execution", 
                'type': "counter",
                'labels': {'job': self.job_name}
            }
        }
        
        self.create_textfile(metrics, self.exception_metrics_file)
        if not is_exception_occurring:
            self.log(f"Exception tracking initialized with file {self.exception_metrics_file}. Current count: {self.exception_counter}", logging.DEBUG)
        else:
            self.log(f"Exception recorded in {self.exception_metrics_file}. Count set to {self.exception_counter}")

    def track_run(self) -> None:
        """Record the current run time in a metrics file"""
        timestamp = time.time()
        metrics = {
            "pep_importTimestamp_seconds": {
                'value': timestamp,
                'help': "Unix Timestamp of the last execution", 
                'type': "gauge",
                'labels': {'job': self.job_name}
            }
        }

        self.create_textfile(metrics, self.run_metrics_file)
        self.log(f"Recorded current run time: {timestamp}", logging.DEBUG)
        self.log(f"Run tracking initialized with file {self.run_metrics_file}", logging.DEBUG)