import logging
import re
import json
from pathlib import Path
from .peprepository import PepError

class AccessGroup:
    LOG_TAG = "AccessGroup"

    def __init__(self, repository):
        self.repository = repository
        self.logger = repository.logger

    def log(self, message: str, level=logging.INFO, tag=None):
        if tag is None:
            tag = self.LOG_TAG
        log_message = f"[{tag}] {message}"
        self.logger.log(level, log_message)

    def query_enrollment(self):
        """
        Queries the enrollment of the current user and returns the user and group.
        """
        command = ["query", "enrollment"]
        try:
            output = self.repository.run_command(command)
            self.log("Successfully queried enrollment")
            output = output.decode('utf-8')  # Decode bytes to string
            match = re.search(r'Enrolled as user "([^"]+)" in group "([^"]+)"', output)
            if match:
                user = match.group(1)
                group = match.group(2)
                return user, group
            else:
                self.log("User and group not found in the output.", level=logging.WARNING)
                return None, None
        except Exception as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise

    def list_pseudonym_mapping(self, subject_group: str = "*") -> dict[str, dict]:
        """
        Returns a comprehensive mapping of pseudonyms for the specified subject group.

        Args:
            subject_group: The subject group to query (default: "*" for all)

        Returns:
            A dictionary with:
            - Key: Local pseudonym
            - Value: Dictionary containing:
                - "blp": Brief local pseudonym
                - "pp": Polymorphic pseudonym
        """
        command = ["list", "-P", subject_group, "--local-pseudonyms", "--show-dataless", "--group-output"]
        try:
            output = self.repository.run_command(command)
            self.log("Successfully obtained pseudonyms from PEP")
            try:
                data = json.loads(output)
                result = {
                    item["lp"]: {
                        "blp": item["blp"],
                        "pp": item["pp"]
                    }
                    for item in data
                }
                return result
            except json.JSONDecodeError as e:
                self.log(f"Error decoding JSON: {e}", logging.ERROR)
                raise
        except PepError as e:
            self.log(f"Failed to list pseudonyms. Error: {e}", logging.ERROR)
            raise
        except Exception as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise

    def list_short_pseudonyms(self, column_name: str) -> list[str]:
        """
        Reads all short pseudonyms from a given (short pseudonym) column name and returns 
        a list of those short pseudonyms. Provide the column name without the prefix.
        """
        command = ["list", "--inline-data-size-limit", "0", "-c", f"ShortPseudonym.{column_name}", "-P", "*"]
        try:
            output = self.repository.run_command(command)
            self.log("Successfully obtained short pseudonyms from PEP")
            short_pseudonyms = []
            try:
                data = json.loads(output)
                for item in data:
                    short_pseudonym = item["data"].get(f"ShortPseudonym.{column_name}")
                    if short_pseudonym:
                        short_pseudonyms.append(short_pseudonym)
            except json.JSONDecodeError as e:
                self.log(f"Error decoding JSON: {e}", logging.ERROR)
                raise
            return short_pseudonyms
        except PepError as e:
            self.log(f"Failed to list short pseudonyms Error: {e}", logging.ERROR)
            raise
        except Exception as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise

    def list_columndata_by_short_pseudonym(self, short_pseudonym_column_name: str, columns: list[str]) -> dict:
        if not isinstance(columns, list):
            raise ValueError("Columns must be a list of strings, even if it is just one column")
        """
        Reads a set of short pseudonyms and data from a set of columns and returns a dictionary with
        the short pseudonym as key and a dict with the column name as key and the data as value.
        """
        command = ["list", "--group-output", "--inline-data-size-limit", "0", "-c", f"ShortPseudonym.{short_pseudonym_column_name}", "-P", "*"]
        for column in columns:
            command.extend(["-c", column])
        try:
            output = self.repository.run_command(command)
            self.log("Successfully obtained short pseudonyms and associated data from PEP", logging.DEBUG)
            short_pseudonym_data = {}
            try:
                data = json.loads(output)
                for item in data:
                    short_pseudonym = item["data"].get(f"ShortPseudonym.{short_pseudonym_column_name}")
                    if short_pseudonym:
                        # Store column data at different levels in the dictionary
                        short_pseudonym_data[short_pseudonym] = {
                            "columns": {column: item["data"].get(column) for column in columns}
                        }
            except json.JSONDecodeError as e:
                self.log(f"Error decoding JSON: {e}", logging.ERROR)
                raise
            return short_pseudonym_data
        except PepError as e:
            self.log(f"Failed to list short pseudonyms and associated data. Error: {e}", logging.ERROR)
            raise
        except Exception as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise

    def list_columndata_by_local_pseudonym(self, columns: list[str]):
        if not isinstance(columns, list):
            raise ValueError("Columns must be a list of strings, even if it is just one column.")
        """
        Lists the data for a set of columns and returns a dictionary with the local pseudonym as key
        and a dict with the column name as key and the data as value.
        """
        command = ["list", "--group-output", "--inline-data-size-limit", "0", "--local-pseudonyms", "-P", "*"]
        for column in columns:
            command.extend(["-c", column])
        try:
            output = self.repository.run_command(command)
            result = {}
            try:
                data = json.loads(output)
                for item in data:
                    local_pseudonym = item.get("lp")
                    polymorphic_pseudonym = item.get("pp")
                    if local_pseudonym:
                        # Store column data and pp at different levels in the dictionary
                        result[local_pseudonym] = {
                            "columns": {column: item["data"].get(column) for column in columns},
                            "pp": polymorphic_pseudonym
                        }

            except json.JSONDecodeError as e:
                self.log(f"Error decoding JSON: {e}", logging.ERROR)
                raise
            return result
        except PepError as e:
            self.log(f"Failed to list columns and associated data. Error: {e}", logging.ERROR)
            raise 
        except Exception as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise

    def map_local_pseudonyms_to_pulled_data(self, columns: list[str], subject_groups: list[str], output_directory: str = "/tmp/pep_connector") -> dict[str, dict[str, str]]:
        """
        Pulls data from PEP and creates a mapping from local pseudonyms to their pulled data file paths.
        This function pulls data to /tmp/pep_connector, then scans the output directory (organized by brief local pseudonyms)
        and maps it back to local pseudonyms.

        Args:
            columns: List of columns to pull
            subject_groups: List of subject groups to pull
            output_directory: Directory where pulled data is stored

        Returns:
            A dictionary with:
            - Key: Local pseudonym (only included if pulled data exists)
            - Value: Dictionary with column names as keys and file paths as values
                    {"column_name": "/tmp/pep_connector/blp_dir/column_name", ...}
        """
        # Pull the data first
        self.pull_data_for_column_groups_and_subject_groups(
            columns=columns,
            subject_groups=subject_groups,
            force=True,
            output_directory=output_directory,
            suppress_file_extensions=True
        )

        # Get the lp to blp mapping for all subjects
        pseudonym_map = self.list_pseudonym_mapping(subject_group="*")

        result = {}
        output_path = Path(output_directory)

        if not output_path.exists():
            self.log(f"Output directory does not exist: {output_directory}", logging.WARNING)
            return result

        # Iterate through each local pseudonym
        for lp, pseudo_data in pseudonym_map.items():
            blp = pseudo_data["blp"]
            blp_dir = output_path / blp
            
            if blp_dir.exists() and blp_dir.is_dir():
                # Get all files in this blp directory
                column_files = {}
                for file_path in blp_dir.iterdir():
                    if file_path.is_file():
                        # Use the filename as the column name
                        column_name = file_path.name
                        column_files[column_name] = str(file_path)

                if column_files:
                    result[lp] = column_files

        self.log(f"Mapped {len(result)} local pseudonyms to their pulled data files")
        return result

    def pull_data_for_column_groups_and_subject_groups(self, column_groups: list[str] = None, subject_groups: list[str] = None, 
                                                        columns: list[str] = None, subjects: list[str] = None,
                                                        force: bool = False, output_directory: str = None, suppress_file_extensions: bool = False):
        # Validate inputs
        if column_groups is not None and not isinstance(column_groups, list):
            raise ValueError("Column groups must be a list of strings")
        if subject_groups is not None and not isinstance(subject_groups, list):
            raise ValueError("Subject groups must be a list of strings")
        if columns is not None and not isinstance(columns, list):
            raise ValueError("Columns must be a list of strings")
        if subjects is not None and not isinstance(subjects, list):
            raise ValueError("Subjects must be a list of strings")
        
        # Ensure at least one column or column group is provided
        if not column_groups and not columns:
            raise ValueError("At least one column or column group must be provided")
        
        # Ensure at least one subject or subject group is provided
        if not subject_groups and not subjects:
            raise ValueError("At least one subject or subject group must be provided")
        
        command = ["pull"]
        
        # Add column groups
        if column_groups:
            for column_group in column_groups:
                command.extend(["-C", column_group])
        
        # Add individual columns
        if columns:
            for column in columns:
                command.extend(["-c", column])
        
        # Add subject groups
        if subject_groups:
            for subject_group in subject_groups:
                command.extend(["-P", subject_group])
        
        # Add individual subjects
        if subjects:
            for subject in subjects:
                command.extend(["-p", subject])
        
        if suppress_file_extensions:
            command.append("--suppress-file-extensions")
        if force:
            command.extend(["--force"])
        if output_directory:
            command.extend(["--output-directory", output_directory])
        try:
            self.repository.run_command(command)
            self.log("Successfully downloaded data")
        except Exception as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise

    def list_column_access(self):
        command = ["query", "column-access"]
        try:
            output = self.repository.run_command(command)
            self.log("Successfully listed column access")
            output = output.decode('utf-8')  # Decode bytes to string
            column_groups = []
            columns = []
            lines = output.split("\n")
            parsing_columns = False
            for line in lines:
                line = line.strip()
                if line.startswith("ColumnGroups"):
                    parsing_columns = False
                elif line.startswith("Columns"):
                    parsing_columns = True
                elif line.startswith("r"):
                    if parsing_columns:
                        columns.append(line[3:])
                    else:
                        column_groups.append(line[3:])
            return column_groups, columns
        except Exception as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise

    def list_subject_group_access(self):
        command = ["query", "participant-group-access"]
        try:
            output = self.repository.run_command(command)
            self.log("Successfully listed subject group access")
            subject_groups = []
            lines = output.split("\n")
            for line in lines:
                line = line.strip()
                if line and "(" in line and not line.startswith("Participant groups"):
                    group_name, permissions = line.split("(")
                    permissions = permissions.strip(")").split(",")
                    if "access" in [perm.strip() for perm in permissions]:
                        subject_groups.append(group_name.strip())
            return subject_groups
        except Exception as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise

    def register_by_id(self):
        command = ["register", "id"]
        try:
            output = self.repository.run_command(command)
            self.log("Successfully added subject.")
            output = output.decode('utf-8')  # Decode bytes to string
            match = re.search(r"Generated participant with identifier: (\S+)", output)
            if match:
                identifier = match.group(1)
                self.log(f"Registered participant with identifier: {identifier}")
                return identifier
            else:
                self.log("Identifier not found in the output.", level=logging.ERROR)
                return None
        except Exception as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise

    def register_ensure_complete(self):
        command = ["register", "ensure-complete"]
        try:
            self.repository.run_command(command)
            self.log("Successfully ensured registration is complete.")
        except PepError as e:
            self.log(f"Failed to complete registration. Error: {e}", level=logging.ERROR)
            raise
        except Exception as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise

    def store_data(self, column: str, data: str = None, participant: str = None, short_pseudonym: str = None, 
                   input_path: str = None, file_extension: str = None, resolve_symlinks: bool = False, pipe_data = None):
        command = ["store", "--column", column]

        if participant:
            command.extend(["--participant", participant])
        if short_pseudonym:
            command.extend(["--short-pseudonym", short_pseudonym])
        if input_path:
            command.extend(["--input-path", input_path])
        if data:
            command.extend(["--data", data])
        if file_extension:
            command.extend(["--file-extension", file_extension])
        if resolve_symlinks:
            command.append("--resolve-symlinks")

        try:
            if pipe_data:
                self.log(f"Storing data in column {column} with pipe", level=logging.DEBUG)
                # Convert string to bytes if needed
                if isinstance(pipe_data, str):
                    self.log("Converting string to bytes", level=logging.DEBUG)
                    pipe_data = pipe_data.encode('utf-8')
                self.repository.run_command(command, input_data=pipe_data)
            else:
                self.log(f"Storing data in column {column}", level=logging.DEBUG)
                self.repository.run_command(command)
            if participant:
                self.log(f"Successfully uploaded data to participant {participant}, column {column}", level=logging.DEBUG)
            elif short_pseudonym:
                self.log(f"Successfully uploaded data to short pseudonym {short_pseudonym}, column {column}", level=logging.DEBUG)
        except PepError as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise
        except Exception as e:
            self.log(f"Failed to upload data. Error: {e}", level=logging.ERROR)
            raise

    def upload_data_by_short_pseudonym(self, short_pseudonym, column, data, file_extension=None):
        self.store_data(column=column, data=data, short_pseudonym=short_pseudonym, file_extension=file_extension)

    def upload_file_with_pipe_by_short_pseudonym(self, short_pseudonym: str, column: str, data, file_extension=None):
        self.store_data(column=column, short_pseudonym=short_pseudonym, input_path="-", pipe_data=data, file_extension=file_extension)

    def fill_column_by_short_pseudonym(self, short_pseudonym_column_name: str, target_column: str, data: str, file_extension=None):
        self.log(f"Uploading data to all accessible short pseudonyms in column {target_column}.", level=logging.DEBUG)
        short_pseudonyms = self.list_short_pseudonyms(short_pseudonym_column_name)
        self.log(f"Found {len(short_pseudonyms)} short pseudonyms to upload data to.", level=logging.DEBUG)
        for sp in short_pseudonyms:
            self.upload_data_by_short_pseudonym(short_pseudonym=sp, column=target_column, data=data, file_extension=file_extension)

    def fill_column_by_source_column_existence(self, source_column: str, target_column: str, data: str, file_extension=None):
        self.log(f"Obtaining all accessible local pseudonyms in source column {source_column}.", level=logging.DEBUG)
        local_pseudonym_data = self.list_columndata_by_local_pseudonym([source_column])
        self.log(f"Found {len(local_pseudonym_data)} local pseudonyms to upload data to.", level=logging.DEBUG)
        for lp in local_pseudonym_data.keys():
            self.store_data(column=target_column, data=data, participant=lp, file_extension=file_extension)