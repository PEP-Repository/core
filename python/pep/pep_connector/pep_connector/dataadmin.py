from .accessgroup import AccessGroup
from .peprepository import PEPRepository
from .peprepository import PepError
import logging

class DataAdmin(AccessGroup):
    LOG_TAG = "DataAdmin"

    def __init__(self, repository: PEPRepository):
        super().__init__(repository)
        self.repository = repository
        if not self.__user_is_data_admin():
            raise RuntimeError("User is not a Data Administrator")
        self.log("DataAdmin initialized", level=logging.DEBUG)

    def __user_is_data_admin(self) -> bool:
        _, group = self.query_enrollment()
        if group == "Data Administrator":
            return True
        return False

    def create_column(self, column: str) -> None:
        command = ["ama", "column", "create", column]
        try:
            self.repository.run_command(command)
            self.log(f"Successfully created column {column}")
        except PepError as e:
            if "already exists" in e.stderr:
                self.log(f"Column {column} already exists", level=logging.WARNING)
            else:
                self.log(f"Failed to create column. Error: {e}", level=logging.ERROR)
                raise
        except Exception as e:
            self.log(f"Failed to create column. Error: {e}", level=logging.ERROR)
            raise

    def create_columns(self, columns: list[str]) -> None:
        if not isinstance(columns, list):
            raise ValueError("Columns must be a list of strings")
        for column in columns:
            self.create_column(column)

    def remove_column(self, column: str) -> None:
        command = ["ama", "column", "remove", column]
        try:
            self.repository.run_command(command)
            self.log(f"Successfully removed column {column}")
        except PepError as e:
            if "does not exist" in e.stderr:
                self.log(f"Column {column} does not exist", level=logging.WARNING)
            else:
                self.log(f"Failed to remove column. Error: {e}", level=logging.ERROR)
                raise
        except Exception as e:
            self.log(f"Failed to remove column. Error: {e}", level=logging.ERROR)
            raise

    def remove_columns(self, columns: list[str]) -> None:
        if not isinstance(columns, list):
            raise ValueError("Columns must be a list of strings")
        for column in columns:
            self.remove_column(column)

    def create_column_group(self, columngroup: str) -> None:
        command = ["ama", "columnGroup", "create", columngroup]
        try:
            self.repository.run_command(command)
            self.log(f"Successfully created column group {columngroup}")
        except PepError as e:
            if "already exists" in e.stderr:
                self.log(f"Column group {columngroup} already exists", level=logging.WARNING)
            else:
                self.log(f"Failed to create column group. Error: {e}", level=logging.ERROR)
                raise
        except Exception as e:
            self.log(f"Failed to create column group. Error: {e}", level=logging.ERROR)
            raise

    def remove_column_group(self, columngroup: str, force: bool = False) -> None:
        command = ["ama", "columnGroup", "remove", columngroup]
        if force:
            command.append("--force")
        try:
            self.repository.run_command(command)
            self.log(f"Successfully removed column group {columngroup}")
        except PepError as e:
            if "does not exist" in e.stderr:
                self.log(f"Column group {columngroup} does not exist", level=logging.WARNING)
            else:
                self.log(f"Failed to remove column group. Error: {e}", level=logging.ERROR)
                raise
        except Exception as e:
            self.log(f"Failed to remove column group. Error: {e}", level=logging.ERROR)
            raise

    def remove_column_groups(self, columngroups: list[str], force: bool = False) -> None:
        if not isinstance(columngroups, list):
            raise ValueError("Columngroups must be a list of strings")
        for columngroup in columngroups:
            self.remove_column_group(columngroup, force)

    def force_remove_column_group(self, columngroup: str) -> None:
        """
        Force remove a column group, in contrast to regular removal, this also removes all access rules.
        """
        self.remove_column_group(columngroup, force=True)

    def force_remove_column_groups(self, columngroups: list[str]) -> None:
        """
        Force remove column groups, in contrast to regular removal, this also removes all access rules.
        """
        self.remove_column_groups(columngroups, force=True)

    def add_column_to_column_group(self, column: str, columngroup: str) -> None:
        command = ["ama", "column", "addTo", column, columngroup]
        try:
            self.repository.run_command(command)
            self.log(f"Successfully added column {column} to column group {columngroup}")
        except PepError as e:
            if "No such column:" in e.stderr:
                self.log(f"No such column: {column}", level=logging.WARNING)
            elif "is already part of column-group" in e.stderr:
                self.log(f"Column {column} already in column group {columngroup}", level=logging.WARNING)
            elif "No such column-group:" in e.stderr:
                self.log(f"No such column group: {columngroup}", level=logging.WARNING)
            else:
                self.log(f"Failed to add column to column group. Error: {e}", level=logging.ERROR)
                raise
        except Exception as e:
            self.log(f"Failed to add column to column group. Error: {e}", level=logging.ERROR)
            raise

    def add_columns_to_column_group(self, columns: list[str], columngroup: str) -> None:
        if not isinstance(columns, list):
            raise ValueError("Columns must be a list of strings")
        for column in columns:
            self.add_column_to_column_group(column, columngroup)

    def remove_column_from_column_group(self, column: str, columngroup: str) -> None:
        command = ["ama", "column", "removeFrom", column, columngroup]
        try:
            self.repository.run_command(command)
            self.log(f"Successfully added column to column group {columngroup}")
        except PepError as e:
            if "is not part of column-group" in e.stderr:
                self.log(f"Column {column} not in column group {columngroup}", level=logging.WARNING)
            else:
                self.log(f"Failed to remove column from column group. Error: {e}", level=logging.ERROR)
                raise
        except Exception as e:
            self.log(f"Failed to remove column from column group. Error: {e}", level=logging.ERROR)
            raise

    def remove_columns_from_column_group(self, columns: list[str], columngroup: str) -> None:
        if not isinstance(columns, list):
            raise ValueError("Columns must be a list of strings")
        for column in columns:
            self.remove_column_from_column_group(column, columngroup)

    def create_subject_group(self, subjectgroup: str) -> None:
        command = ["ama", "group", "create", subjectgroup]
        try:
            self.repository.run_command(command)
            self.log(f"Successfully created subject group {subjectgroup}")
        except PepError as e:
            if "already exists" in e.stderr:
                self.log(f"Subject group {subjectgroup} already exists", level=logging.WARNING)
            else:
                self.log(f"Failed to create subject group. Error: {e}", level=logging.ERROR)
                raise
        except Exception as e:
            self.log(f"Failed to create subject group. Error: {e}", level=logging.ERROR)
            raise

    def create_subject_groups(self, subjectgroups: list[str]) -> None:
        if not isinstance(subjectgroups, list):
            raise ValueError("Subjectgroups must be a list of strings")
        for subjectgroup in subjectgroups:
            self.create_subject_group(subjectgroup)

    def remove_subject_group(self, subjectgroup: str) -> None:
        command = ["ama", "group", "remove", subjectgroup]
        try:
            self.repository.run_command(command)
            self.log(f"Successfully removed subject group {subjectgroup}")
        except PepError as e:
            if "does not exist" in e.stderr:
                self.log(f"Subject group {subjectgroup} does not exist", level=logging.WARNING)
            else:
                self.log(f"Failed to remove subject group. Error: {e}", level=logging.ERROR)
                raise
        except Exception as e:
            self.log(f"Failed to remove subject group. Error: {e}", level=logging.ERROR)
            raise

    def remove_subject_groups(self, subjectgroups: list[str]) -> None:
        if not isinstance(subjectgroups, list):
            raise ValueError("Subjectgroups must be a list of strings")
        for subjectgroup in subjectgroups:
            self.remove_subject_group(subjectgroup)

    def add_subject_to_subject_group(self, subjectgroup: str, subject: str) -> None:
        command = ["ama", "group", "addTo", subjectgroup, subject]
        try:
            self.repository.run_command(command)
            self.log(f"Successfully added subject to subject group {subjectgroup}")
        except PepError as e:
            if "No such participant-group:" in e.stderr:
                self.log(f"No such subject group: {subjectgroup}", level=logging.WARNING)
            elif "Participant is already in participant-group:" in e.stderr:
                self.log(f"Subject {subject} already in subject group {subjectgroup}", level=logging.WARNING)
            elif "No such participant known" in e.stderr:
                self.log(f"No such subject: {subject}", level=logging.WARNING)
            else:
                self.log(f"Failed to add subject to subject group. Error: {e}", level=logging.ERROR)
                raise
        except Exception as e:
            self.log(f"Failed to add subject to subject group. Error: {e}", level=logging.ERROR)
            raise

    def add_subjects_to_subject_group(self, subjectgroup: str, subjects: list[str]) -> None:
        """
        Add a list of subjects to a subject group.
        """
        if not isinstance(subjects, list):
            raise ValueError("Subjects must be a list of strings")
        for subject in subjects:
            self.add_subject_to_subject_group(subjectgroup, subject)

    def remove_subject_from_subject_group(self, subjectgroup: str, subject: str) -> None:
        command = ["ama", "group", "removeFrom", subjectgroup, subject]
        try:
            self.repository.run_command(command)
            self.log(f"Successfully removed subject from subject group {subjectgroup}")
        except PepError as e:
            if "This participant is not part of participant-group" in e.stderr:
                self.log(f"Subject {subject} not in subject group {subjectgroup}", level=logging.WARNING)
            else:
                self.log(f"Failed to remove subject from subject group. Error: {e}", level=logging.ERROR)
                raise
        except Exception as e:
            self.log(f"Failed to remove subject from subject group. Error: {e}", level=logging.ERROR)
            raise

    def remove_subjects_from_subject_group(self, subjectgroup: str, subjects: list[str]) -> None:
        """
        Remove a list of subjects from a subject group.
        """
        if not isinstance(subjects, list):
            raise ValueError("Subjects must be a list of strings")
        for subject in subjects:
            self.remove_subject_from_subject_group(subjectgroup, subject)

    def setup_repo_from_config(self, config) -> None:
        """
        Set up a repository from config.

        Args:
            config (dict): Configuration containing columns, column groups, and subject groups to set up.
                Format:
                {
                    "columns": ["Column1", "Column2", "Column3"],
                    "column_groups": {
                        "GroupName1": ["Column1", "Column2"],
                        "GroupName2": ["Column2", "Column3"],
                        "EmptyGroup": []
                    },
                    "subject_groups": ["Group1", "Group2"]
                }
        """
        if not isinstance(config, dict):
            raise ValueError("Config must be a dictionary")

        # Create all columns
        if "columns" in config and isinstance(config["columns"], list):
            self.log("Creating columns", level=logging.DEBUG)
            self.create_columns(config["columns"])

        # Set up column groups
        if "column_groups" in config and isinstance(config["column_groups"], dict):
            for column_group_name in config["column_groups"]:

                # Create column group
                self.log(f"Creating column group {column_group_name}", level=logging.DEBUG)
                self.create_column_group(column_group_name)

                # Add columns to column group if specified
                columns_in_group = config["column_groups"][column_group_name]
                if columns_in_group:
                    self.log(f"Adding columns to column group {column_group_name}", level=logging.DEBUG)
                    self.add_columns_to_column_group(columns_in_group, column_group_name)

        # Create subject groups
        if "subject_groups" in config and isinstance(config["subject_groups"], list):
            for subject_group in config["subject_groups"]:
                self.log(f"Creating subject group {subject_group}", level=logging.DEBUG)
                self.create_subject_group(subject_group)