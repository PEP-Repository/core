import json
import logging
import os
from __future__ import annotations

from datetime import datetime, timedelta
from typing import Any
from .connectors import Connector, ConnectorConfig


class DataMonitorConfig(ConnectorConfig):
    """Configuration for DataMonitor."""
    pass


class DataMonitor(Connector):
    """Class for checking data existence in PEP"""

    LOG_TAG = "DataMonitor"

    def __init__(self, repository, config: DataMonitorConfig):
        """
        Initialize DataMonitor with a DataMonitorConfig object.

        Args:
            repository: PEPRepository instance
            config: DataMonitorConfig instance (required)
        """
        if not isinstance(config, DataMonitorConfig):
            raise ValueError("config must be an instance of DataMonitorConfig")

        # Initialize parent with the config
        super().__init__(repository, config)
        self.repository = repository
        self.log("DataMonitor initialized", level=logging.DEBUG)

    def find_missing_entries(self, monitor_columns: list[str], info_columns: list[str]):
        """
        Find missing timestamp entries and return corresponding column data

        Args:
            monitor_columns: Columns to check for timestamps
            info_columns: Columns to return info for missing entries

        Returns:
            Tuple containing:
            - List of dictionaries with missing entries information
            - Dictionary with data_info
            - Dictionary with participant_info
        """
        self.log(f"Finding missing entries. Monitor columns: {monitor_columns}, Info columns: {info_columns}", level=logging.DEBUG)
        if not isinstance(monitor_columns, list):
            raise ValueError("Monitor columns must be a list of strings")
        if not isinstance(info_columns, list):
            raise ValueError("Info columns must be a list of strings")

        # Get participant info from the info columns
        self.log("Getting participant info from info columns", level=logging.DEBUG)
        participant_info = self.list_columndata_by_local_pseudonym(info_columns)
        self.log(f"Found {len(participant_info)} participants", level=logging.DEBUG)

        # Get data info for the monitor columns
        self.log("Getting data info for monitor (metadata only)", level=logging.DEBUG)
        data_info = self.get_data_info(monitor_columns)

        missing_entries = []

        # Check each participant
        for lp, info_data in participant_info.items():
            missing_columns = []

            # Check if this local pseudonym exists in data_info
            if lp not in data_info:
                # All monitor columns are missing
                missing_columns = monitor_columns.copy()
                self.log(f"Participant {lp} is missing all monitor columns", level=logging.DEBUG)
            else:
                # Check each monitor column
                for column in monitor_columns:
                    if column not in data_info[lp] or "timestamp" not in data_info[lp][column]:
                        missing_columns.append(column)
                        self.log(f"Participant {lp} is missing column {column}", level=logging.DEBUG)

            # If any columns are missing, add to the results
            if missing_columns:
                missing_entries.append({
                    "local_pseudonym": lp,
                    "info_data": info_data,
                    "missing_columns": missing_columns
                })

        self.log(f"Found {len(missing_entries)} participants with missing data", level=logging.DEBUG)
        return missing_entries, data_info, participant_info

    def read_prometheus_timestamp(self, metrics_file_path):
        """
        Read a Prometheus metrics file and extract the timestamp value.

        Args:
            metrics_file_path: Path to the metrics file

        Returns:
            Datetime object if timestamp found, None otherwise
        """
        self.log(f"Reading prometheus timestamp from {metrics_file_path}", level=logging.DEBUG)
        try:
            with open(metrics_file_path, 'r') as f:
                for line in f:
                    line = line.strip()
                    # Skip comments and empty lines
                    if not line or line.startswith('#'):
                        continue
                    # Look for timestamp metrics
                    if 'pep_importTimestamp_seconds' in line:
                        # Parse the line to extract the value
                        parts = line.split()
                        if len(parts) >= 2:
                            try:
                                timestamp_value = float(parts[-1])
                                timestamp = datetime.fromtimestamp(timestamp_value)
                                self.log(f"Found timestamp: {timestamp}", level=logging.DEBUG)
                                return timestamp
                            except (ValueError, TypeError):
                                self.log(f"Error parsing timestamp value from {line}", level=logging.WARNING)
            self.log("No timestamp found in metrics file", level=logging.DEBUG)
            return None
        except FileNotFoundError:
            # File doesn't exist, which is expected in some cases
            self.log(f"Metrics file not found: {metrics_file_path}", level=logging.DEBUG)
            return None
        except Exception as e:
            self.log(f"Error reading prometheus metrics file {metrics_file_path}: {e}", level=logging.ERROR)
            return None

    def generate_prometheus_stats_html(self):
        """
        Generate HTML for prometheus metrics statistics

        Returns:
            HTML string with prometheus metrics information
        """
        self.log("Generating prometheus stats HTML", level=logging.DEBUG)

        # Map component suffixes to descriptive names
        component_descriptions = {
            "Interview": "Interview Upload",
            "Teacher": "Teacher Upload",
            "ThinkAloud": "ThinkAloud Upload",
            "Mailsender": "Email Sender",
            "DataMonitor": "Data Monitor",
            "LogData": "Snappet LogData"
        }

        # Dictionary to store the timestamps
        timestamps = {}

        # Check each metrics file using all keys from component_descriptions
        for suffix in component_descriptions.keys():
            prefix_without_datamonitor = self.env_prefix.replace("DataMonitor", "")
            metrics_filename = f"{prefix_without_datamonitor}{suffix}_pep_run.prom"
            metrics_path = os.path.join(self.prometheus_dir, metrics_filename)
            self.log(f"Checking metrics file: {metrics_path}", level=logging.DEBUG)
            timestamp = self.read_prometheus_timestamp(metrics_path)
            if timestamp:
                timestamps[suffix] = timestamp
                self.log(f"Found timestamp for {suffix}: {timestamp}", level=logging.DEBUG)

        # If no timestamps found, return empty string
        if not timestamps:
            self.log("No timestamps found", level=logging.DEBUG)
            return ""

        # Build the HTML output with JavaScript for dynamic age calculation
        html = """
        <div class="metrics-section">
            <h3>Container Metrics</h3>
            <table class="metrics-table">
                <tr>
                    <th>Container</th>
                    <th>Last Run</th>
                    <th>Time Elapsed since last run</th>
                </tr>
        """

        # Add a row for each component with timestamp as data attribute
        for suffix, timestamp in timestamps.items():
            timestamp_iso = timestamp.isoformat()
            timestamp_formatted = timestamp.strftime("%Y-%m-%d %H:%M:%S")
            # Get the descriptive name for the component, or use suffix if not found
            component_display_name = component_descriptions.get(suffix, suffix)

            html += f"""
                <tr>
                    <td>{component_display_name}</td>
                    <td>{timestamp_formatted}</td>
                    <td class="component-age" data-timestamp="{timestamp_iso}">Calculating...</td>
                </tr>
            """

        html += """
            </table>
            <script>
                // Function to calculate and update ages
                function updateComponentAges() {
                    const now = new Date();
                    const ageElements = document.querySelectorAll('.component-age');

                    ageElements.forEach(element => {
                        const timestamp = new Date(element.getAttribute('data-timestamp'));
                        const ageMs = now - timestamp;

                        // Convert to minutes
                        const ageSec = Math.floor(ageMs / 1000);
                        const days = Math.floor(ageSec / 86400);
                        const hours = Math.floor((ageSec % 86400) / 3600);
                        const minutes = Math.floor((ageSec % 3600) / 60);

                        // Format age nicely with minute precision
                        let ageStr = '';
                        if (days > 0) {
                            ageStr = `${days} days, ${hours} hours`;
                        } else if (hours > 0) {
                            ageStr = `${hours} hours, ${minutes} minutes`;
                        } else {
                            ageStr = `${minutes} minutes`;
                        }

                        element.textContent = ageStr;
                    });
                }

                // Update immediately and then every minute instead of every second
                updateComponentAges();
                setInterval(updateComponentAges, 60000);
            </script>
        </div>
        """

        self.log(f"Generated HTML for {len(timestamps)} prometheus components", level=logging.DEBUG)
        return html

    def _calculate_column_stats(self, monitor_columns: list[str], participant_info: dict, data_info: dict) -> dict:
        """
        Calculate column completion statistics.

        Args:
            monitor_columns: List of columns to calculate stats for
            participant_info: Dictionary of participant information
            data_info: Dictionary of data information

        Returns:
            Dictionary with completion statistics for each column
        """
        total_participants = len(participant_info)
        self.log(f"Calculating statistics for {total_participants} participants", level=logging.DEBUG)
        column_stats = {}

        for col in monitor_columns:
            filled_count = 0
            for lp in participant_info:
                if lp in data_info and col in data_info[lp] and "timestamp" in data_info[lp][col]:
                    filled_count += 1
            column_stats[col] = {
                "filled": filled_count,
                "total": total_participants,
                "percentage": round((filled_count / total_participants) * 100, 1) if total_participants > 0 else 0
            }
            self.log(f"Column {col}: {filled_count}/{total_participants} ({column_stats[col]['percentage']}%)", level=logging.DEBUG)

        return column_stats

    def _calculate_sent_and_filled_counts(self, monitor_columns: list[str | dict], participant_info: dict, 
                                          data_info: dict) -> dict:
        """
        Calculate sent and filled counts for each monitor column.

        Args:
            monitor_columns: List of column dicts with column_name, config_item_name, and survey_id
            participant_info: Dictionary of participant information (must include EmailsSent column)
            data_info: Dictionary of data information

        Returns:
            Dictionary mapping column names to {"sent": count, "filled": count}
        """
        total_participants = len(participant_info)
        self.log(f"Calculating sent and filled counts for {total_participants} participants", level=logging.DEBUG)
        self.log(f"data_info has {len(data_info)} participants with data", level=logging.DEBUG)

        column_counts = {}

        for col_info in monitor_columns:
            # Handle both dict and string column formats
            if isinstance(col_info, dict):
                col = col_info["column_name"]
                config_item_name = col_info.get("config_item_name")
                survey_id = col_info.get("survey_id")
            else:
                col = col_info
                config_item_name = None
                survey_id = None

            filled_count = 0
            sent_count = 0

            self.log(f"Processing column: {col}", level=logging.DEBUG)

            for lp in participant_info:
                # Count filled columns
                if lp in data_info and col in data_info[lp] and "timestamp" in data_info[lp][col]:
                    filled_count += 1
                    self.log(f"Column {col}: Found filled data for participant {lp}", level=logging.DEBUG)

                # Count sent items (only first send, not reminders)
                if config_item_name and survey_id is not None:
                    info_data = participant_info[lp]
                    emails_sent_data = info_data["columns"].get("EmailsSent", "")

                    if emails_sent_data:
                        try:
                            emails_sent = json.loads(emails_sent_data)
                            survey_types = emails_sent.get("survey_types", {})

                            if config_item_name in survey_types:
                                survey_id_str = str(survey_id)
                                if survey_id_str in survey_types[config_item_name]:
                                    timestamps = survey_types[config_item_name][survey_id_str]
                                    # Only count first send (index 0), not reminders
                                    if timestamps and len(timestamps) > 0:
                                        sent_count += 1
                        except (json.JSONDecodeError, ValueError) as e:
                            self.log(f"Error parsing EmailsSent for {lp}: {e}", level=logging.WARNING)

            column_counts[col] = {
                "sent": sent_count,
                "filled": filled_count
            }

            self.log(f"Column {col}: {sent_count} sent, {filled_count} filled (out of {total_participants} participants)", level=logging.DEBUG)

        return column_counts

    def _filter_recent_monitor_columns(self, monitor_columns: list[str | dict], 
                                       participant_info: dict, max_columns: int = 3) -> list[str | dict]:
        """
        Filter monitor columns to only include the most recently sent ones.

        Args:
            monitor_columns: List of column dicts with column_name, config_item_name, and survey_id
            participant_info: Dictionary of participant information (must include EmailsSent column)
            max_columns: Maximum number of recent columns to return (default: 3)

        Returns:
            Filtered list of monitor columns, ordered by most recent send timestamp
        """
        self.log(f"Filtering monitor columns to {max_columns} most recent", level=logging.DEBUG)

        # Separate columns into those that can be filtered (have email tracking) and those that can't
        trackable_columns = []
        non_trackable_columns = []

        for col_info in monitor_columns:
            # Handle both dict and string column formats
            if isinstance(col_info, dict):
                config_item_name = col_info.get("config_item_name")
                survey_id = col_info.get("survey_id")
            else:
                config_item_name = None
                survey_id = None

            # Columns without config_item_name or with null survey_id can't be tracked via EmailsSent
            if not config_item_name or survey_id is None:
                non_trackable_columns.append(col_info)
            else:
                trackable_columns.append(col_info)

        # If there are no trackable columns, return all columns (limited to max_columns)
        if not trackable_columns:
            self.log(f"No trackable columns found, returning all {len(monitor_columns)} columns", level=logging.DEBUG)
            return monitor_columns[:max_columns]

        # Track earliest send timestamp for each trackable column
        column_timestamps = {}

        for col_info in trackable_columns:
            col = col_info["column_name"]
            config_item_name = col_info.get("config_item_name")
            survey_id = col_info.get("survey_id")

            earliest_send = None

            # Find earliest send timestamp across all participants
            for lp in participant_info:
                info_data = participant_info[lp]
                emails_sent_data = info_data["columns"].get("EmailsSent", "")

                if emails_sent_data:
                    try:
                        emails_sent = json.loads(emails_sent_data)
                        survey_types = emails_sent.get("survey_types", {})

                        if config_item_name in survey_types:
                            survey_id_str = str(survey_id)
                            if survey_id_str in survey_types[config_item_name]:
                                timestamps = survey_types[config_item_name][survey_id_str]
                                # Get first send (index 0)
                                if timestamps and len(timestamps) > 0:
                                    first_send = datetime.fromisoformat(timestamps[0])
                                    if earliest_send is None or first_send < earliest_send:
                                        earliest_send = first_send
                    except (json.JSONDecodeError, ValueError) as e:
                        self.log(f"Error parsing EmailsSent for {lp}: {e}", level=logging.WARNING)

            if earliest_send:
                column_timestamps[col] = earliest_send
                self.log(f"Column {col}: earliest send at {earliest_send}", level=logging.DEBUG)

        # Sort trackable columns by timestamp (most recent first) and take the most recent ones
        sorted_columns = sorted(column_timestamps.items(), key=lambda x: x[1], reverse=True)
        recent_column_names = [col_name for col_name, _ in sorted_columns[:max_columns]]

        # Filter original monitor_columns to keep only recent trackable ones
        filtered_trackable = [
            col_info for col_info in trackable_columns
            if (col_info["column_name"] if isinstance(col_info, dict) else col_info) in recent_column_names
        ]

        # Combine non-trackable columns (like Snappet LogData) with filtered trackable columns
        # Non-trackable columns are always included
        filtered_columns = non_trackable_columns + filtered_trackable

        # Limit to max_columns total
        filtered_columns = filtered_columns[:max_columns]

        self.log(f"Filtered to {len(filtered_columns)} columns: {[c['column_name'] if isinstance(c, dict) else c for c in filtered_columns]}", level=logging.DEBUG)
        return filtered_columns

    def _generate_html_header(self, include_javascript: bool) -> str:
        """
        Generate HTML header with styles and optional JavaScript.

        Args:
            include_javascript: Whether to include JavaScript functionality

        Returns:
            HTML header string
        """
        html_head = """
            <title>Data Monitoring Report</title>
            <style>
                body { font-family: Arial, sans-serif; margin: 20px; }
                table { border-collapse: collapse; width: 100%; margin-bottom: 20px; }
                th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
                th { background-color: #f2f2f2; }
                tr:nth-child(even) { background-color: #f9f9f9; }
                .missing { background-color: #ffcccc; }
                .present { background-color: #ccffcc; }
                .timestamp { font-size: 0.8em; color: #666; }
                h1 { color: #333; }
                .progress-bar-container { 
                    width: 100px; 
                    background-color: #e0e0e0; 
                    height: 15px; 
                    display: inline-block; 
                    border: 1px solid #ccc;
                    border-radius: 3px;
                    overflow: hidden;
                }
                .progress-bar { 
                    height: 100%; 
                    background-color: #4CAF50 !important;
                    transition: width 0.3s ease;
                    min-width: 0%;
                }
                .progress-bar-empty {
                    height: 100%; 
                    background-color: #f44336 !important;
                    width: 5%;
                    min-width: 5%;
                }
                .metrics-section {
                    margin-bottom: 30px;
                    padding: 15px;
                    background-color: #f9f9f9;
                    border: 1px solid #ddd;
                    border-radius: 5px;
                }
                .metrics-table {
                    border-collapse: collapse;
                    width: 100%;
                    margin-top: 10px;
                }
                .metrics-table th, .metrics-table td {
                    border: 1px solid #ddd;
                    padding: 8px;
                    text-align: left;
                }
                .metrics-table th {
                    background-color: #f2f2f2;
                }
                /* PDF-specific styles for better text wrapping */
                @media print {
                    table {
                        table-layout: fixed;
                        width: 100%;
                    }
                    th, td {
                        word-wrap: break-word;
                        word-break: break-all;
                        white-space: normal;
                        overflow: hidden;
                    }
                    .progress-bar-container {
                        border: 2px solid #000;
                        background-color: #fff !important;
                    }
                    .progress-bar {
                        background-color: #4CAF50 !important;
                    }
                    .progress-bar-empty {
                        background-color: #f44336 !important;
                    }
                }
                .copy-btn {
                    display: inline-flex;
                    align-items: center;
                    justify-content: center;
                    background-color: transparent;
                    border: none;
                    cursor: pointer;
                    padding: 4px;
                    border-radius: 4px;
                    margin-left: 8px;
                    transition: all 0.2s;
                }
                .copy-btn:hover {
                    background-color: rgba(0, 0, 0, 0.05);
                }
                .copy-btn:active {
                    background-color: rgba(0, 0, 0, 0.1);
                }
                .copy-btn svg {
                    width: 16px;
                    height: 16px;
                    fill: #666;
                    transition: fill 0.2s;
                }
                .copy-btn:hover svg {
                    fill: #333;
                }
                .copy-btn.success svg {
                    fill: #4CAF50;
                }
                .copy-message {
                    position: absolute;
                    top: -24px;
                    left: 50%;
                    transform: translateX(-50%);
                    background-color: rgba(0, 0, 0, 0.7);
                    color: white;
                    padding: 3px 8px;
                    border-radius: 3px;
                    font-size: 12px;
                    opacity: 0;
                    transition: opacity 0.3s;
                    pointer-events: none;
                    white-space: nowrap;
                }
                .copy-message.show {
                    opacity: 1;
                }
                .pp-container {
                    display: flex;
                    align-items: center;
                    position: relative;
                }
                .truncated-text {
                    max-width: 150px; 
                    white-space: nowrap;
                    overflow: hidden;
                    text-overflow: ellipsis;
                    display: inline-block;
                }
                .tooltip {
                    position: relative;
                    display: inline-block;
                }
                .tooltip .tooltip-text {
                    visibility: hidden;
                    width: auto;
                    background-color: #555;
                    color: #fff;
                    text-align: center;
                    border-radius: 6px;
                    padding: 5px;
                    position: absolute;
                    z-index: 1;
                    bottom: 125%;
                    left: 50%;
                    transform: translateX(-50%);
                    opacity: 0;
                    transition: opacity 0.3s;
                }
                .tooltip:hover .tooltip-text {
                    visibility: visible;
                    opacity: 1;
                }
            </style>
        """

        if include_javascript:
            html_head += """
            <script>
                function copyToClipboard(text, btnId, msgId) {
                    const btn = document.getElementById(btnId);
                    const msgBox = document.getElementById(msgId);

                    navigator.clipboard.writeText(text)
                        .then(() => {
                            // Show success message
                            msgBox.textContent = 'Copied!';
                            msgBox.classList.add('show');

                            // Add success style to button
                            btn.classList.add('success');

                            // Reset after animation completes
                            setTimeout(() => {
                                msgBox.classList.remove('show');
                                btn.classList.remove('success');
                            }, 1500);
                        })
                        .catch(err => {
                            console.error('Failed to copy: ', err);
                            msgBox.textContent = 'Error!';
                            msgBox.classList.add('show');
                            setTimeout(() => {
                                msgBox.classList.remove('show');
                            }, 1500);
                        });
                }
            </script>
            """

        return html_head

    def _generate_table_header(self, participant_info: dict, include_javascript: bool, statistics_only: bool = False) -> str:
        """
        Generate table header with participant columns or statistics-only header.

        Args:
            participant_info: Dictionary of participant information
            include_javascript: Whether to include interactive elements
            statistics_only: Whether to generate statistics-only header

        Returns:
            HTML table header string
        """
        if statistics_only:
            # Generate statistics-only table header
            return """
                <tr>
                    <th>Veld</th>
                    <th>Verzonden</th>
                    <th>Ingevuld</th>
                    <th>Progressie</th>
                </tr>
            """

        html = """
                <tr>
                <th>Veld</th>
                <th>Ingevuld</th>
                <th>Progressie %</th>
        """

        # Create a list of local pseudonyms and their PP for consistent ordering
        lp_list = list(participant_info.keys())
        pp_index = 0

        # Add participant pseudonyms as column headers
        for lp in lp_list:
            info_data = participant_info[lp]
            full_pp = info_data["pp"]

            # Add polymorphic pseudonym with copy button and truncation as column header
            if include_javascript:
                html += f"""
                    <th>
                        <div class="pp-container">
                            <span class="tooltip">
                                <span class="truncated-text">{full_pp}</span>
                                <span class="tooltip-text">{full_pp}</span>
                            </span>
                            <button id="copy-btn-{pp_index}" class="copy-btn" onclick="copyToClipboard('{full_pp}', 'copy-btn-{pp_index}', 'copy-msg-{pp_index}')">
                                <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
                                    <path d="M16 1H4c-1.1 0-2 .9-2 2v14h2V3h12V1zm3 4H8c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h11c1.1 0 2-.9 2-2V7c0-1.1-.9-2-2-2zm0 16H8V7h11v14z"/>
                                </svg>
                            </button>
                            <span id="copy-msg-{pp_index}" class="copy-message"></span>
                        </div>
                    </th>
                """
            else:
                # Simpler version without interactive elements when not using javascript
                html += f"""
                    <th>
                        <span title="{full_pp}">{full_pp}</span>
                    </th>
                """
            pp_index += 1

        html += "</tr>\n"
        return html

    def _generate_info_rows(self, info_columns: list[str | dict], participant_info: dict) -> str:
        """
        Generate HTML rows for info columns including special handling for EmailsSent.

        Args:
            info_columns: List of column name strings OR list of dicts with 'column_name' and optional 'display_name' keys
            participant_info: Dictionary of participant information

        Returns:
            HTML rows string
        """
        # Extract column names and build display name mapping
        if info_columns and isinstance(info_columns[0], dict):
            info_column_names = [col["column_name"] for col in info_columns]
            info_column_display_names = {
                col["column_name"]: col.get("display_name", col["column_name"]) 
                for col in info_columns
            }
        else:
            info_column_names = info_columns
            info_column_display_names = None

        html = ""
        lp_list = list(participant_info.keys())

        # Add rows for each info column
        for col in info_column_names:
            # Skip EmailsSent here as we'll handle it separately
            if col == "EmailsSent":
                continue

            # Use display name if provided, otherwise use column name
            display_name = info_column_display_names.get(col, col) if info_column_display_names else col
            html += f"<tr>\n<td>{display_name}</td>\n"

            # Add N/A for filled count and completion percentage for info columns
            html += """<td>N/A</td>\n<td>N/A</td>\n"""

            for lp in lp_list:
                info_data = participant_info[lp]
                value = info_data["columns"].get(col, "")
                html += f"<td>{value}</td>\n"

            html += "</tr>\n"

        # Add special row for hashed email if EmailsSent is in info_columns
        if "EmailsSent" in info_column_names:
            html += "<tr>\n<td>Hashed Email</td>\n"
            html += """<td>N/A</td>\n<td>N/A</td>\n"""

            for lp in lp_list:
                info_data = participant_info[lp]
                emails_sent_data = info_data["columns"].get("EmailsSent", "")
                hashed_email = ""

                if emails_sent_data:
                    try:
                        emails_sent = json.loads(emails_sent_data)
                        if emails_sent and "hashed_email" in emails_sent:
                            hashed_email = emails_sent["hashed_email"]
                    except Exception:
                        pass

                html += f"<td>{hashed_email}</td>\n"

            html += "</tr>\n"

            # Add special row for survey information
            html += "<tr>\n<td>Surveys Sent</td>\n"
            html += """<td>N/A</td>\n<td>N/A</td>\n"""

            for lp in lp_list:
                info_data = participant_info[lp]
                emails_sent_data = info_data["columns"].get("EmailsSent", "")
                survey_info = ""

                if emails_sent_data:
                    try:
                        emails_sent = json.loads(emails_sent_data)
                        if emails_sent and "survey_types" in emails_sent:
                            survey_types = emails_sent["survey_types"]
                            # Format survey information as an HTML list
                            survey_items = []
                            for survey_type, surveys_ids in survey_types.items():
                                for survey_id, timestamps in surveys_ids.items():
                                    for timestamp in timestamps:
                                        survey_items.append(
                                            f"<div><strong>{survey_type}</strong>: {survey_id} ({timestamp})</div>"
                                        )
                            if survey_items:
                                survey_info = "".join(survey_items)
                    except Exception:
                        survey_info = "Error parsing survey data"

                html += f"<td>{survey_info}</td>\n"

            html += "</tr>\n"

        return html

    def _generate_monitor_rows(self, monitor_columns: list[str | dict], column_counts: dict, 
                              participant_info: dict, data_info: dict, missing_by_lp: dict,
                              statistics_only: bool = False, total_consented: int = None) -> str:
        """
        Generate HTML rows for monitor columns with completion stats.

        Args:
            monitor_columns: List of column name strings OR list of dicts with 'column_name' and optional 'display_name' keys
            column_counts: Dictionary with sent and filled counts per column
            participant_info: Dictionary of participant information
            data_info: Dictionary of data information
            missing_by_lp: Dictionary mapping local pseudonyms to missing columns
            statistics_only: Whether to generate statistics-only rows
            total_consented: Total number of consented participants (for percentage calculation)

        Returns:
            HTML rows string
        """
        # Extract column names and build display name mapping
        if monitor_columns and isinstance(monitor_columns[0], dict):
            monitor_column_names = [col["column_name"] for col in monitor_columns]
            monitor_column_display_names = {
                col["column_name"]: col.get("display_name", col["column_name"]) 
                for col in monitor_columns
            }
        else:
            monitor_column_names = monitor_columns
            monitor_column_display_names = None

        if statistics_only:
            # Use total_consented if provided, otherwise use participant_info length
            total = total_consented if total_consented is not None else len(participant_info)

            # Generate statistics-only rows
            html = ""
            for col in monitor_column_names:
                counts = column_counts[col]
                sent_count = counts["sent"]
                filled_count = counts["filled"]

                percentage = round((filled_count / total) * 100, 1) if total > 0 else 0

                # Use different classes and styling for 0% vs non-zero percentages
                if percentage == 0:
                    progress_bar_html = '<div class="progress-bar-empty"></div>'
                else:
                    progress_bar_html = f'<div class="progress-bar" style="width: {percentage}%;"></div>'

                # Use display name if provided, otherwise use column name
                display_name = monitor_column_display_names.get(col, col) if monitor_column_display_names else col

                html += f"""
                <tr>
                    <td>{display_name}</td>
                    <td>{sent_count}</td>
                    <td>{filled_count}</td>
                    <td>
                        <div class="progress-bar-container">
                            {progress_bar_html}
                        </div>
                        {percentage}%
                    </td>
                </tr>
                """
            return html

        html = ""
        total = len(participant_info)

        # Add rows for each monitor column with completion stats
        for col in monitor_column_names:
            counts = column_counts[col]
            filled_count = counts["filled"]
            percentage = round((filled_count / total) * 100, 1) if total > 0 else 0

            # Use display name if provided, otherwise use column name
            display_name = monitor_column_display_names.get(col, col) if monitor_column_display_names else col

            html += f"""
            <tr>
                <td>{display_name}</td>
                <td>{filled_count}/{total}</td>
                <td>
                    <div class="progress-bar-container">
                        <div class="progress-bar" style="width: {percentage}%;"></div>
                    </div>
                    {percentage}%
                </td>
            """

            for lp in participant_info.keys():
                # Get missing columns for this local pseudonym (if any)
                missing_columns = missing_by_lp.get(lp, [])

                if col in missing_columns:
                    html += '<td class="missing">Missing</td>\n'
                else:
                    # Column is present, get timestamp if available
                    timestamp = ""
                    if lp in data_info and col in data_info[lp] and "timestamp" in data_info[lp][col]:
                        dt_timestamp = data_info[lp][col]["timestamp"]
                        # Format to seconds accuracy only
                        timestamp = dt_timestamp.strftime("%Y-%m-%d %H:%M:%S")
                    html += f'<td class="present">Present<br><span class="timestamp">{timestamp}</span></td>\n'

            html += "</tr>\n"

        return html

    def _generate_html_content(self, monitor_columns: list[str | dict], info_columns: list[str | dict], 
                              participant_info: dict, data_info: dict, missing_by_lp: dict, 
                              column_counts: dict, report_title: str, include_javascript: bool,
                              statistics_only: bool = False, total_consented: int = None,
                              total_participants: int = None) -> str:
        """
        Generate the actual HTML content for the report.

        Args:
            monitor_columns: List of column name strings OR list of dicts with 'column_name' and optional 'display_name' keys
            info_columns: List of column name strings OR list of dicts with 'column_name' and optional 'display_name' keys
            participant_info: Dictionary of participant information
            data_info: Dictionary of data information
            missing_by_lp: Dictionary mapping local pseudonyms to missing columns
            column_counts: Dictionary with sent and filled counts per column
            report_title: Title for the HTML report
            include_javascript: Whether to include JavaScript functionality
            statistics_only: Whether to only show statistics without individual participant data
            total_consented: Number of consented participants (for stats calculation)
            total_participants: Total number of participants (before filtering)

        Returns:
            Complete HTML content as a string
        """

        html = """
        <!DOCTYPE html>
        <html>
        <head>
        """

        html += self._generate_html_header(include_javascript)

        html += f"""
        </head>
        <body>
            <h1>{report_title}</h1>
        """

        # Add participant count information if provided
        if total_participants is not None and total_consented is not None:
            html += f"""
            <div style="margin-bottom: 20px; padding: 10px; background-color: #f0f0f0; border-radius: 5px;">
                <p style="margin: 5px 0;"><strong>Aantal deelnemers met consent:</strong> {total_consented}</p>
                <p style="margin: 5px 0;"><strong>Totaal aantal deelnemers:</strong> {total_participants}</p>
            </div>
            """

        # Generate prometheus metrics section if available
        if hasattr(self, 'prometheus_dir') and self.prometheus_dir and hasattr(self, 'env_prefix') and self.env_prefix:
            if include_javascript:
                prometheus_html = self.generate_prometheus_stats_html()
                if prometheus_html:
                    html += prometheus_html
                else:
                    self.log("Error generating Prometheus metrics HTML", level=logging.ERROR)
            else:
                self.log("Skipping Prometheus metrics section as JavaScript is not included", level=logging.INFO)

        html += """
            <table>
        """

        html += self._generate_table_header(participant_info, include_javascript, statistics_only)

        if not statistics_only:
            html += self._generate_info_rows(info_columns, participant_info)

        html += self._generate_monitor_rows(monitor_columns, column_counts, participant_info, data_info, missing_by_lp, 
                                           statistics_only, total_consented)

        # Close table and HTML
        html += """
            </table>
        </body>
        </html>
        """

        return html

    def generate_html_report(self, monitor_columns: list[str | dict], info_columns: list[str | dict], output_path: str = None, 
                            include_javascript: bool = True, report_title: str = "Data Monitoring Report",
                            statistics_only: bool = False) -> str:
        """
        Generate an HTML report showing the status of monitored data columns for all participants.

        Args:
            monitor_columns: List of column name strings OR list of dicts with 'column_name' and optional 'display_name' keys
            info_columns: List of column name strings OR list of dicts with 'column_name' and optional 'display_name' keys
            output_path: Path where to save the HTML report (if None, returns HTML as string)
            include_javascript: Whether to include sections with javascript (default: True)
            report_title: Title for the HTML report
            statistics_only: Whether to only show statistics without individual participant data

        Returns:
            HTML file path that was saved, or the HTML content as a string if output_path is None
        """
        self.log(f"Generating HTML report. Monitor columns: {monitor_columns}, Info columns: {info_columns}", level=logging.INFO)
        if not isinstance(monitor_columns, list):
            raise ValueError("Monitor columns must be a list of strings")
        if not isinstance(info_columns, list):
            raise ValueError("Info columns must be a list of strings")

        # Extract column names for data fetching
        monitor_column_names = [col["column_name"] if isinstance(col, dict) else col for col in monitor_columns]
        info_column_names = [col["column_name"] if isinstance(col, dict) else col for col in info_columns]

        # Get all data using find_missing_entries
        missing_entries, data_info, participant_info = self.find_missing_entries(monitor_column_names, info_column_names)

        # Create a lookup of missing columns by local pseudonym for quick reference
        missing_by_lp = {}
        for entry in missing_entries:
            lp = entry["local_pseudonym"]
            missing_by_lp[lp] = entry["missing_columns"]

        # Calculate column completion statistics
        column_stats = self._calculate_column_stats(monitor_column_names, participant_info, data_info)

        # Generate the HTML content
        html = self._generate_html_content(
            monitor_columns=monitor_columns,
            info_columns=info_columns,
            participant_info=participant_info,
            data_info=data_info,
            missing_by_lp=missing_by_lp,
            column_counts=column_stats,
            report_title=report_title,
            include_javascript=include_javascript,
            statistics_only=statistics_only
        )

        # If output_path is None, return the HTML directly
        if output_path is None:
            self.log("Returning HTML report as string", level=logging.DEBUG)
            return html

        # Otherwise write to the specified file
        self.log(f"Writing HTML report to {output_path}", level=logging.DEBUG)
        with open(output_path, 'w') as f:
            f.write(html)

        self.log(f"HTML report saved to {output_path}", level=logging.INFO)
        return output_path

    def generate_filtered_html_report(self, monitor_columns: list[str | dict], info_columns: list[str | dict], 
                                     participant_info: dict, output_path: str = None, 
                                     include_javascript: bool = True, 
                                     report_title: str = "Data Monitoring Report",
                                     statistics_only: bool = False) -> str:
        """
        Generate an HTML report showing the status of monitored data columns for pre-filtered participants.

        Args:
            monitor_columns: List of column name strings OR list of dicts with 'column_name' and optional 'display_name' keys
            info_columns: List of column name strings OR list of dicts with 'column_name' and optional 'display_name' keys
            participant_info: Pre-filtered participant info dictionary
            output_path: Path where to save the HTML report (if None, returns HTML as string)
            include_javascript: Whether to include sections with javascript (default: True)
            report_title: Title for the HTML report
            statistics_only: Whether to only show statistics without individual participant data

        Returns:
            HTML file path that was saved, or the HTML content as a string if output_path is None
        """
        self.log(f"Generating filtered HTML report for {len(participant_info)} participants", level=logging.INFO)
        if not isinstance(monitor_columns, list):
            raise ValueError("Monitor columns must be a list of strings")
        if not isinstance(info_columns, list):
            raise ValueError("Info columns must be a list of strings")
        if not isinstance(participant_info, dict):
            raise ValueError("Participant info must be a dictionary")

        # Extract column names for data fetching
        monitor_column_names = [col["column_name"] if isinstance(col, dict) else col for col in monitor_columns]

        # Get data info for the monitor columns
        self.log("Getting data info for monitor columns", level=logging.DEBUG)
        data_info = self.get_data_info(monitor_column_names)

        # Filter data_info to only include participants in the participant_info
        filtered_data_info = {lp: data for lp, data in data_info.items() if lp in participant_info}

        # Find missing entries for this filtered set
        missing_entries = []
        for lp, info_data in participant_info.items():
            missing_columns = []

            # Check if this local pseudonym exists in filtered_data_info
            if lp not in filtered_data_info:
                # All monitor columns are missing
                missing_columns = monitor_column_names.copy()
                self.log(f"Participant {lp} is missing all monitor columns", level=logging.DEBUG)
            else:
                # Check each monitor column
                for column in monitor_column_names:
                    if column not in filtered_data_info[lp] or "timestamp" not in filtered_data_info[lp][column]:
                        missing_columns.append(column)
                        self.log(f"Participant {lp} is missing column {column}", level=logging.DEBUG)

            # If any columns are missing, add to the results
            if missing_columns:
                missing_entries.append({
                    "local_pseudonym": lp,
                    "info_data": info_data,
                    "missing_columns": missing_columns
                })

        # Create a lookup of missing columns by local pseudonym for quick reference
        missing_by_lp = {}
        for entry in missing_entries:
            lp = entry["local_pseudonym"]
            missing_by_lp[lp] = entry["missing_columns"]

        # Calculate column completion statistics for the filtered set
        column_stats = self._calculate_column_stats(monitor_column_names, participant_info, filtered_data_info)

        # Generate the HTML content
        html = self._generate_html_content(
            monitor_columns=monitor_columns,
            info_columns=info_columns,
            participant_info=participant_info,
            data_info=filtered_data_info,
            missing_by_lp=missing_by_lp,
            column_counts=column_stats,
            report_title=report_title,
            include_javascript=include_javascript,
            statistics_only=statistics_only
        )

        # If output_path is None, return the HTML directly
        if output_path is None:
            self.log("Returning filtered HTML report as string", level=logging.DEBUG)
            return html

        # Otherwise write to the specified file
        self.log(f"Writing filtered HTML report to {output_path}", level=logging.DEBUG)
        with open(output_path, 'w') as f:
            f.write(html)

        self.log(f"Filtered HTML report saved to {output_path}", level=logging.INFO)
        return output_path

    def _group_participants_by_column(self, participant_info: dict, group_column: str) -> dict:
        """
        Group participants by a specific column value.

        Args:
            participant_info: Dictionary of participant information
            group_column: Column name to group by

        Returns:
            Dictionary with group values as keys and participant info dictionaries as values
        """
        self.log(f"Grouping participants by column: {group_column}", level=logging.DEBUG)
        groups = {}
        skipped_count = 0

        for lp, info_data in participant_info.items():
            # Get the value from the specified column
            group_value = info_data["columns"].get(group_column)

            # Skip participants with None or empty string group values
            if group_value is None or group_value == "":
                skipped_count += 1
                self.log(f"Skipping participant {lp} with None/empty {group_column}", level=logging.DEBUG)
                continue

            # Initialize group if it doesn't exist
            if group_value not in groups:
                groups[group_value] = {}

            # Add participant to the group
            groups[group_value][lp] = info_data

        self.log(f"Created {len(groups)} groups, skipped {skipped_count} participants with None/empty {group_column}", level=logging.DEBUG)
        for group_value, group_participants in groups.items():
            self.log(f"Group '{group_value}': {len(group_participants)} participants", level=logging.DEBUG)

        return groups

    def generate_grouped_html_reports(self, monitor_columns: list[str | dict] | None, info_columns: list[str | dict] | None, 
                                                 group_column: str, 
                                                 consent_column: str | None = "Consent.Bool",
                                                 survey_participant_column: str | None = "TeacherInfo.IsSurveyParticipant",
                                                 emails_sent_column: str | None = "EmailsSent",
                                                 include_javascript: bool = True, statistics_only: bool = True,
                                                 max_recent_columns: int = 3) -> dict[str, str]:
        """
        Generate separate HTML reports for each group based on a shared info column value,
        filtered to only include participants who are survey participants and have given consent.

        Args:
            monitor_columns: List of column name strings OR list of dicts with 'column_name' and optional 'display_name' keys, or None
            info_columns: List of column name strings OR list of dicts with 'column_name' and optional 'display_name' keys, or None
            group_column: Column name to group participants by
            consent_column: Column name containing consent information (default: "Consent.Bool")
            survey_participant_column: Column name containing survey participant flag (default: "TeacherInfo.IsSurveyParticipant")
            emails_sent_column: Column name containing emails sent information
            include_javascript: Whether to include sections with javascript (default: True)
            statistics_only: Whether to only show statistics without individual participant data
            max_recent_columns: Maximum number of recent monitor columns to display (default: 3)

        Returns:
            Dictionary with group names as keys and HTML content as values
        """
        self.log(f"Generating grouped HTML reports by column: {group_column}", level=logging.INFO)

        if monitor_columns is not None and not isinstance(monitor_columns, list):
            raise ValueError("Monitor columns must be a list or None, its now: {}".format(type(monitor_columns)))
        if info_columns is not None and not isinstance(info_columns, list):
            raise ValueError("Info columns must be a list or None, its now: {}".format(type(info_columns)))

        # At least one of monitor_columns or info_columns must be provided
        if monitor_columns is None and info_columns is None:
            raise ValueError("At least one of monitor_columns or info_columns must be provided")

        # Default to empty list if None
        if monitor_columns is None:
            monitor_columns = []
        if info_columns is None:
            info_columns = []

        # Build list of all columns needed for report generation
        all_columns_for_report = []

        # Add info columns
        for col in info_columns:
            col_name = col["column_name"] if isinstance(col, dict) else col
            if col_name not in all_columns_for_report:
                all_columns_for_report.append(col_name)

        # Add special columns
        if group_column and group_column not in all_columns_for_report:
            all_columns_for_report.append(group_column)
        if consent_column and consent_column not in all_columns_for_report:
            all_columns_for_report.append(consent_column)
        if survey_participant_column and survey_participant_column not in all_columns_for_report:
            all_columns_for_report.append(survey_participant_column)
        if emails_sent_column and emails_sent_column not in all_columns_for_report:
            all_columns_for_report.append(emails_sent_column)

        # Fetch data using LOCAL pseudonyms for DataMonitor compatibility
        self.log(f"Fetching participant data using local pseudonyms for report generation", level=logging.DEBUG)
        all_participant_info = self.list_columndata_by_local_pseudonym(all_columns_for_report)
        self.log(f"Retrieved data for {len(all_participant_info)} participants", level=logging.DEBUG)

        # Group ALL participants by the specified column (before consent filtering)
        all_groups = self._group_participants_by_column(all_participant_info, group_column)

        generated_reports = {}

        # Generate a report for each group
        for group_value, group_all_participants in all_groups.items():
            # First filter by survey participant flag to get total_in_group
            if survey_participant_column:
                group_filtered_participants = self._filter_participants_by_flag(
                    group_all_participants, 
                    survey_participant_column
                )
                total_in_group = len(group_filtered_participants)
            else:
                group_filtered_participants = group_all_participants
                total_in_group = len(group_all_participants)

            # Then filter by consent within survey participants
            if consent_column:
                group_filtered_participants = self._filter_participants_by_flag(
                    group_filtered_participants,
                    consent_column
                )

            filtered_in_group = len(group_filtered_participants)

            # Skip groups with no filtered participants
            if filtered_in_group == 0:
                self.log(f"Group '{group_value}': Skipping group with no filtered participants", level=logging.INFO)
                continue

            try:
                # Filter to most recent monitor columns based on send timestamps
                filtered_monitor_columns = self._filter_recent_monitor_columns(
                    monitor_columns, group_filtered_participants, max_recent_columns
                )

                # If no columns have been sent yet, skip this group
                if not filtered_monitor_columns:
                    self.log(f"Group '{group_value}': No emails sent yet, skipping report generation", level=logging.INFO)
                    continue

                # Extract filtered column names
                if filtered_monitor_columns and isinstance(filtered_monitor_columns[0], dict):
                    filtered_monitor_column_names = [col["column_name"] for col in filtered_monitor_columns]
                else:
                    filtered_monitor_column_names = filtered_monitor_columns

                # Get data info for filtered monitor columns
                self.log(f"Getting data info for filtered monitor columns: {filtered_monitor_column_names}", level=logging.DEBUG)
                data_info = self.get_data_info(filtered_monitor_column_names)

                # Filter data_info to only include filtered participants in this group
                filtered_data_info = {lp: data for lp, data in data_info.items() if lp in group_filtered_participants}

                # Calculate sent and filled counts
                column_counts = self._calculate_sent_and_filled_counts(
                    filtered_monitor_columns, group_filtered_participants, filtered_data_info
                )

                # Find missing entries for this group
                missing_by_lp = {}
                for lp in group_filtered_participants.keys():
                    missing_columns = []
                    if lp not in filtered_data_info:
                        missing_columns = filtered_monitor_column_names.copy()
                    else:
                        for column in filtered_monitor_column_names:
                            if column not in filtered_data_info[lp] or "timestamp" not in filtered_data_info[lp][column]:
                                missing_columns.append(column)
                    if missing_columns:
                        missing_by_lp[lp] = missing_columns

                report_title = f"Participatierapport - {group_value}"
                html_content = self._generate_html_content(
                    monitor_columns=filtered_monitor_columns,
                    info_columns=info_columns,
                    participant_info=group_filtered_participants,
                    data_info=filtered_data_info,
                    missing_by_lp=missing_by_lp,
                    column_counts=column_counts,
                    report_title=report_title,
                    include_javascript=include_javascript,
                    statistics_only=statistics_only,
                    total_consented=filtered_in_group,
                    total_participants=total_in_group
                )

                generated_reports[group_value] = html_content
                self.log(f"Generated report for group '{group_value}'", level=logging.INFO)

            except Exception as e:
                self.log(f"Error generating report for group '{group_value}': {e}", level=logging.ERROR)
                raise

        self.log(f"Generated {len(generated_reports)} grouped HTML reports with filters", level=logging.INFO)
        return generated_reports

    def _filter_participants_by_flag(self, participant_info: dict, flag_column: str) -> dict:
        """
        Filter participants to only include those who have a truthy value in the specified flag column.
        Assumes flag data is already included in participant_info columns.

        Args:
            participant_info: Dictionary of participant information (must include flag_column in columns)
            flag_column: Column name containing the flag to filter by

        Returns:
            Filtered dictionary containing only participants with the flag set
        """
        self.log(f"Filtering participants by column: {flag_column}", level=logging.DEBUG)

        filtered_participants = {}
        flag_count = 0
        total_count = len(participant_info)

        for lp, info_data in participant_info.items():
            # Check if participant has the flag set to "1" in the already-fetched columns
            flag_value = info_data["columns"].get(flag_column)
            has_flag = flag_value == "1"

            if has_flag:
                filtered_participants[lp] = info_data
                flag_count += 1
                self.log(f"Participant {lp} has {flag_column}", level=logging.DEBUG)
            else:
                self.log(f"Participant {lp} does not have {flag_column}", level=logging.DEBUG)

        self.log(f"Filtered {flag_count}/{total_count} participants with {flag_column}", level=logging.INFO)
        return filtered_participants