import json
import logging
import os
import re
from datetime import datetime
from .connectors import Connector


class DataMonitor(Connector):
    """Class for checking data existence in PEP"""

    LOG_TAG = "DataMonitor"

    def __init__(self, repository,
                 prometheus_dir=None, 
                 use_prometheus=False, 
                 env_prefix=None, 
                 job_name=None):
        super().__init__(repository, prometheus_dir, use_prometheus, env_prefix, job_name)
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
                    width: 150px; 
                    background-color: #e0e0e0; 
                    height: 15px; 
                    display: inline-block; 
                    margin-left: 10px; 
                }
                .progress-bar { 
                    height: 100%; 
                    background-color: #4CAF50; 
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
                    <th>Data Field</th>
                    <th>Filled</th>
                    <th>Total</th>
                    <th>Completion %</th>
                </tr>
            """

        html = """
                <tr>
                <th>Data Field</th>
                <th>Filled</th>
                <th>Completion %</th>
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

    def _generate_info_rows(self, info_columns: list[str], participant_info: dict) -> str:
        """
        Generate HTML rows for info columns including special handling for EmailsSent.
        
        Args:
            info_columns: List of info column names
            participant_info: Dictionary of participant information
            
        Returns:
            HTML rows string
        """
        html = ""
        lp_list = list(participant_info.keys())

        # Add rows for each info column
        for col in info_columns:
            # Skip EmailsSent here as we'll handle it separately
            if col == "EmailsSent":
                continue

            html += f"<tr>\n<td>{col}</td>\n"

            # Add N/A for filled count and completion percentage for info columns
            html += """<td>N/A</td>\n<td>N/A</td>\n"""

            for lp in lp_list:
                info_data = participant_info[lp]
                value = info_data["columns"].get(col, "")
                html += f"<td>{value}</td>\n"

            html += "</tr>\n"

        # Add special row for hashed email if EmailsSent is in info_columns
        if "EmailsSent" in info_columns:
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

    def _generate_monitor_rows(self, monitor_columns: list[str], column_stats: dict, 
                              participant_info: dict, data_info: dict, missing_by_lp: dict,
                              statistics_only: bool = False) -> str:
        """
        Generate HTML rows for monitor columns with completion stats.
        
        Args:
            monitor_columns: List of monitor column names
            column_stats: Dictionary with completion statistics
            participant_info: Dictionary of participant information
            data_info: Dictionary of data information
            missing_by_lp: Dictionary mapping local pseudonyms to missing columns
            statistics_only: Whether to generate statistics-only rows
            
        Returns:
            HTML rows string
        """
        if statistics_only:
            # Generate statistics-only rows
            html = ""
            for col in monitor_columns:
                stats = column_stats[col]
                percentage = stats["percentage"]

                html += f"""
                <tr>
                    <td>{col}</td>
                    <td>{stats["filled"]}</td>
                    <td>{stats["total"]}</td>
                    <td>
                        {percentage}%
                        <div class="progress-bar-container">
                            <div class="progress-bar" style="width: {percentage}%;"></div>
                        </div>
                    </td>
                </tr>
                """
            return html

        html = ""
        lp_list = list(participant_info.keys())

        # Add rows for each monitor column with completion stats
        for col in monitor_columns:
            stats = column_stats[col]
            percentage = stats["percentage"]

            html += f"""
            <tr>
                <td>{col}</td>
                <td>{stats["filled"]}/{stats["total"]}</td>
                <td>
                    <div class="progress-bar-container">
                        <div class="progress-bar" style="width: {percentage}%;"></div>
                    </div>
                </td>
            """

            for lp in lp_list:
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

    def _generate_html_content(self, monitor_columns: list[str], info_columns: list[str], 
                              participant_info: dict, data_info: dict, missing_by_lp: dict, 
                              column_stats: dict, report_title: str, include_javascript: bool,
                              statistics_only: bool = False) -> str:
        """
        Generate the actual HTML content for the report.
        
        Args:
            monitor_columns: List of monitor column names
            info_columns: List of info column names
            participant_info: Dictionary of participant information
            data_info: Dictionary of data information
            missing_by_lp: Dictionary mapping local pseudonyms to missing columns
            column_stats: Dictionary with completion statistics
            report_title: Title for the HTML report
            include_javascript: Whether to include JavaScript functionality
            statistics_only: Whether to only show statistics without individual participant data
            
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

        html += self._generate_monitor_rows(monitor_columns, column_stats, participant_info, data_info, missing_by_lp, statistics_only)

        # Close table and HTML
        html += """
            </table>
        </body>
        </html>
        """

        return html

    def generate_html_report(self, monitor_columns: list[str], info_columns: list[str], output_path: str = None, 
                            include_javascript: bool = True, report_title: str = "Data Monitoring Report",
                            statistics_only: bool = False) -> str:
        """
        Generate an HTML report showing the status of monitored data columns for all participants.
        
        Args:
            monitor_columns: List of columns to check for data
            info_columns: List of columns to include as identifying information
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

        # Get all data using find_missing_entries
        missing_entries, data_info, participant_info = self.find_missing_entries(monitor_columns, info_columns)
        
        # Create a lookup of missing columns by local pseudonym for quick reference
        missing_by_lp = {}
        for entry in missing_entries:
            lp = entry["local_pseudonym"]
            missing_by_lp[lp] = entry["missing_columns"]
        
        # Calculate column completion statistics
        column_stats = self._calculate_column_stats(monitor_columns, participant_info, data_info)

        # Generate the HTML content
        html = self._generate_html_content(
            monitor_columns=monitor_columns,
            info_columns=info_columns,
            participant_info=participant_info,
            data_info=data_info,
            missing_by_lp=missing_by_lp,
            column_stats=column_stats,
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

    def generate_filtered_html_report(self, monitor_columns: list[str], info_columns: list[str], 
                                     participant_info: dict, output_path: str = None, 
                                     include_javascript: bool = True, 
                                     report_title: str = "Data Monitoring Report",
                                     statistics_only: bool = False) -> str:
        """
        Generate an HTML report showing the status of monitored data columns for pre-filtered participants.
        
        Args:
            monitor_columns: List of columns to check for data
            info_columns: List of columns to include as identifying information
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

        # Get data info for the monitor columns
        self.log("Getting data info for monitor columns", level=logging.DEBUG)
        data_info = self.get_data_info(monitor_columns)
        
        # Filter data_info to only include participants in the participant_info
        filtered_data_info = {lp: data for lp, data in data_info.items() if lp in participant_info}
        
        # Find missing entries for this filtered set
        missing_entries = []
        for lp, info_data in participant_info.items():
            missing_columns = []

            # Check if this local pseudonym exists in filtered_data_info
            if lp not in filtered_data_info:
                # All monitor columns are missing
                missing_columns = monitor_columns.copy()
                self.log(f"Participant {lp} is missing all monitor columns", level=logging.DEBUG)
            else:
                # Check each monitor column
                for column in monitor_columns:
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
        column_stats = self._calculate_column_stats(monitor_columns, participant_info, filtered_data_info)

        # Generate the HTML content
        html = self._generate_html_content(
            monitor_columns=monitor_columns,
            info_columns=info_columns,
            participant_info=participant_info,
            data_info=filtered_data_info,
            missing_by_lp=missing_by_lp,
            column_stats=column_stats,
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
        
        for lp, info_data in participant_info.items():
            # Get the value from the specified column
            group_value = info_data["columns"].get(group_column, "unknown")
            
            # Initialize group if it doesn't exist
            if group_value not in groups:
                groups[group_value] = {}
                
            # Add participant to the group
            groups[group_value][lp] = info_data
            
        self.log(f"Created {len(groups)} groups", level=logging.DEBUG)
        for group_value, group_participants in groups.items():
            self.log(f"Group '{group_value}': {len(group_participants)} participants", level=logging.DEBUG)
            
        return groups

    def generate_grouped_html_reports(self, monitor_columns: list[str], info_columns: list[str], 
                                    group_column: str, include_javascript: bool = True,
                                    statistics_only: bool = True) -> dict[str, str]:
        """
        Generate separate HTML reports for each group based on a shared info column value.
        This is a convenience function that groups participants and calls generate_filtered_html_report for each group.
        
        Args:
            monitor_columns: List of columns to check for data
            info_columns: List of columns to include as identifying information
            group_column: Column name to group participants by
            include_javascript: Whether to include sections with javascript (default: True)
            statistics_only: Whether to only show statistics without individual participant data
            
        Returns:
            Dictionary with group names as keys and HTML content as values
        """
        self.log(f"Generating grouped HTML reports by column: {group_column}", level=logging.INFO)
        
        if not isinstance(monitor_columns, list):
            raise ValueError("Monitor columns must be a list of strings")
        if not isinstance(info_columns, list):
            raise ValueError("Info columns must be a list of strings")
        if group_column not in info_columns:
            raise ValueError(f"Group column '{group_column}' must be included in info_columns")
        
        # Get all participant info first
        self.log("Getting participant info from info columns", level=logging.DEBUG)
        all_participant_info = self.list_columndata_by_local_pseudonym(info_columns)
        
        # Group participants by the specified column
        groups = self._group_participants_by_column(all_participant_info, group_column)
        
        generated_reports = {}
        
        # Generate a report for each group using the filtered report function
        for group_value, group_participant_info in groups.items():
            self.log(f"Generating report for group '{group_value}' with {len(group_participant_info)} participants", level=logging.DEBUG)
            
            try:
                report_title = f"Data Monitoring Report - {group_value}"
                html_content = self.generate_filtered_html_report(
                    monitor_columns=monitor_columns,
                    info_columns=info_columns,
                    participant_info=group_participant_info,
                    output_path=None,  # Return HTML content instead of writing to file
                    include_javascript=False,  # Disable JavaScript, as it's for email
                    report_title=report_title,
                    statistics_only=statistics_only
                )
                    
                generated_reports[group_value] = html_content
                self.log(f"Generated report for group '{group_value}'", level=logging.INFO)
                
            except Exception as e:
                self.log(f"Error generating report for group '{group_value}': {e}", level=logging.ERROR)
                raise
                
        self.log(f"Generated {len(generated_reports)} grouped HTML reports", level=logging.INFO)
        return generated_reports