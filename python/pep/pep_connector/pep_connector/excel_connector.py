import logging
import os
import ast
from .peprepository import PEPRepository
from .connectors import Connector

try:
    import pandas as pd
except ImportError:
    raise ImportError("pandas is required for ExcelConnector. Please make sure it is installed.")

class ExcelConnector(Connector):
    LOG_TAG = "ExcelConnector"

    def __init__(self, repository: PEPRepository,
                 prometheus_dir=None,
                 use_prometheus=False,
                 env_prefix=None,
                 job_name=None):
        super().__init__(repository, prometheus_dir, use_prometheus, env_prefix, job_name)
        self.log("ExcelConnector initialized", logging.DEBUG)

    def __validate_excel_config(self, excel_config, file_type):
        """Validates the Excel configuration for a specific file type."""

        required_keys = ["file_path", "sp_column", "sp_pep_column"]

        for key in required_keys:
            if key not in excel_config:
                raise ValueError(f"Missing required config item for {file_type}: {key}")

        # Ensure at least one output method is defined
        if not any(key in excel_config for key in ["unique_pep_column_mapping", "row_selector", "excel_pep_column_mapping"]):
            raise ValueError(f"At least one of unique_pep_column_mapping, row_selector, or excel_pep_column_mapping must be specified for {file_type}")

        # Validate unique_pep_column_mapping if present
        if "unique_pep_column_mapping" in excel_config and not isinstance(excel_config.get("unique_pep_column_mapping"), dict):
            raise ValueError(f"unique_pep_column_mapping for {file_type} must be a dictionary")

        # Validate excel_pep_column_mapping if present
        if "excel_pep_column_mapping" in excel_config and not isinstance(excel_config.get("excel_pep_column_mapping"), str):
            raise ValueError(f"excel_pep_column_mapping for {file_type} must be a string")

        # Validate row_selector if present
        if "row_selector" in excel_config:
            row_selector = excel_config.get("row_selector")
            if not isinstance(row_selector, dict):
                raise ValueError(f"row_selector for {file_type} must be a dictionary")
            if "column" not in row_selector:
                raise ValueError(f"row_selector.column is required for {file_type}")
            if "mapping" not in row_selector:
                raise ValueError(f"row_selector.mapping is required for {file_type}")
            if not isinstance(row_selector.get("column"), str):
                raise ValueError(f"row_selector.column for {file_type} must be a string")
            if not isinstance(row_selector.get("mapping"), dict):
                raise ValueError(f"row_selector.mapping for {file_type} must be a dictionary")

        # Validate overwrite flag (optional, defaults to True)
        overwrite = excel_config.get("overwrite", True)
        if not isinstance(overwrite, bool):
            raise ValueError(f"overwrite for {file_type} must be a boolean (true or false).")

    def read_excel_file(self, file_path, sheet_name=0):
        """Reads an Excel file and returns a pandas DataFrame."""
        try:
            if not os.path.exists(file_path):
                self.log(f"File not found: {file_path}", logging.ERROR)
                raise FileNotFoundError(f"File not found: {file_path}")

            self.log(f"Reading Excel file: {file_path}", logging.INFO)
            df = pd.read_excel(file_path, sheet_name=sheet_name)
            self.log(f"Successfully read Excel file with {len(df)} rows", logging.INFO)
            return df
        except Exception as e:
            self.log(f"Failed to read Excel file {file_path}: {str(e)}", logging.ERROR)
            raise e

    def parse_pseudonyms(self, pseudonym_value):
        """
        Parse pseudonyms from a string representation of a Python array.
        Handles formats like "['SNPSL4156344']" or "['SNPSL2506956', 'SNPSL5342945']"

        Returns a list of pseudonyms.
        """
        try:
            if pd.isna(pseudonym_value) or pseudonym_value == '':
                return []
                
            # Safely parses the pseudonym value as a Python literal
            if isinstance(pseudonym_value, str):
                return ast.literal_eval(pseudonym_value)
            return pseudonym_value
        except (SyntaxError, ValueError) as e:
            self.log(f"Failed to parse pseudonym value '{pseudonym_value}': {str(e)}", logging.ERROR)
            return []

    def filter_dataframe_by_pseudonym(self, df, sp_column, short_pseudonym):
        """Filters a DataFrame to only include rows with the given short pseudonym."""
        try:
            def is_pseudonym_in_list(pseudonym_value):
                pseudonyms = self.parse_pseudonyms(pseudonym_value)
                return short_pseudonym in pseudonyms

            return df[df[sp_column].apply(is_pseudonym_in_list)]
            
        except Exception as e:
            self.log(f"Failed to filter DataFrame by pseudonym {short_pseudonym}: {str(e)}", logging.ERROR)
            raise e

    def remove_columns(self, df, columns_to_remove):
        """Removes specified columns from DataFrame."""
        if not columns_to_remove:
            return df

        try:
            # Create a list of columns to keep
            cols_to_keep = [col for col in df.columns if col not in columns_to_remove]
            return df[cols_to_keep]
        except Exception as e:
            self.log(f"Failed to remove columns: {str(e)}", logging.ERROR)
            raise e

    def upload_unique_column_data(self, df, short_pseudonym, unique_column_mapping, existence_results=None, overwrite=True):
        """
        Uploads unique column values from a DataFrame to specified PEP columns.

        Args:
            df: The DataFrame containing the data
            short_pseudonym: The subject's short pseudonym
            unique_column_mapping: Dict mapping Excel column names to PEP column names
            existence_results: Dict of pre-checked data existence results
            overwrite: Whether to overwrite existing data

        Returns:
            int: Number of successfully uploaded values
        """
        uploaded_count = 0

        self.log(f"Processing unique column mapping for {len(unique_column_mapping)} columns", logging.DEBUG)
        for excel_col, pep_col in unique_column_mapping.items():
            if excel_col in df.columns:
                # Check if data already exists and overwrite is False
                should_upload = True
                if not overwrite and existence_results and existence_results.get(pep_col, False):
                    self.log(f"Skipping upload for unique column {excel_col} to {pep_col} for {short_pseudonym}: Data exists and overwrite is false.", 
                        logging.INFO)
                    should_upload = False

                if should_upload:
                    # Check if all values in the column are the same
                    if df[excel_col].nunique() == 1:
                        value = str(df[excel_col].iloc[0])
                        self.upload_data_by_short_pseudonym(short_pseudonym=short_pseudonym,
                                                          column=pep_col,
                                                          data=value)
                        self.log(f"Uploaded unique column {excel_col} to {pep_col} for {short_pseudonym}", logging.INFO)
                        uploaded_count += 1
                    else:
                        unique_values = df[excel_col].unique()
                        self.log(f"Column {excel_col} has multiple values for {short_pseudonym}, not uploading. Values: {unique_values}", 
                                logging.WARNING)
            else:
                self.log(f"Column {excel_col} not found for unique column mapping", logging.WARNING)

        return uploaded_count

    def upload_excel_data(self, df, short_pseudonym, target_column, columns_to_remove=None):
        """
        Uploads an entire DataFrame to a PEP column for a subject.
        
        Args:
            df: The DataFrame to upload
            short_pseudonym: The subject's short pseudonym
            target_column: The PEP column to store the data
            columns_to_remove: Optional list of columns to remove before upload
        """
        try:
            # Remove specified columns if any
            if columns_to_remove:
                df = self.remove_columns(df.copy(), columns_to_remove)

            # Convert to CSV and upload
            data_to_upload = df.to_csv(index=False)
            self.upload_file_with_pipe_by_short_pseudonym(
                short_pseudonym=short_pseudonym,
                column=target_column,
                data=data_to_upload,
                file_extension=".csv")

            self.log(f"Uploaded Excel data to {target_column} for {short_pseudonym}", logging.INFO)
            return True
        except Exception as e:
            self.log(f"Failed to upload Excel data to {target_column} for {short_pseudonym}: {str(e)}", logging.ERROR)
            return False

    def upload_row_selected_data(self, df, short_pseudonym, selector_config, columns_to_remove=None, existence_results=None, overwrite=True):
        """
        Uploads rows from a DataFrame based on values in a selector column.

        Args:
            df: The DataFrame containing the data
            short_pseudonym: The subject's short pseudonym
            selector_config: Dict with 'column' (the column to select on) and 'mapping' (value to PEP column mapping)
            columns_to_remove: Optional list of columns to remove before upload
            existence_results: Dict of pre-checked data existence results
            overwrite: Whether to overwrite existing data

        Returns:
            int: Number of successfully uploaded rows
        """
        if not selector_config or "column" not in selector_config or "mapping" not in selector_config:
            self.log("Invalid row selector configuration", logging.ERROR)
            return 0

        selector_column = selector_config["column"]
        mapping = selector_config["mapping"]

        if selector_column not in df.columns:
            self.log(f"Selector column '{selector_column}' not found in DataFrame", logging.ERROR)
            return 0

        uploaded_count = 0

        # Group data by the selector column
        for selector_value, group in df.groupby(selector_column):
            selector_value_str = str(selector_value)  # Convert to string for dict lookup

            # Check if this selector value has a mapping
            if selector_value_str in mapping:
                target_column = mapping[selector_value_str]

                # Check if data already exists and overwrite is False
                should_upload = True
                if not overwrite and existence_results and existence_results.get(target_column, False):
                    self.log(f"Skipping upload for selector value {selector_value_str} to {target_column} for {short_pseudonym}: Data exists and overwrite is false.", 
                        logging.INFO)
                    should_upload = False

                if should_upload:
                    # Remove specified columns before saving
                    filtered_group = self.remove_columns(group, columns_to_remove) if columns_to_remove else group

                    # Convert to CSV and upload
                    data_to_upload = filtered_group.to_csv(index=False)
                    self.upload_file_with_pipe_by_short_pseudonym(
                        short_pseudonym=short_pseudonym,
                        column=target_column,
                        data=data_to_upload,
                        file_extension=".csv")

                    self.log(f"Uploaded data for selector value {selector_value_str} to {target_column} for {short_pseudonym}", logging.INFO)
                    uploaded_count += 1
            else:
                self.log(f"No mapping found for selector value {selector_value_str}, skipping", logging.WARNING)

        return uploaded_count

    def process_excel_file(self, excel_config, file_type):
        """Processes an Excel file according to configuration and uploads data to PEP."""
        self.log(f"Processing {file_type} Excel file", logging.INFO)

        self.__validate_excel_config(excel_config, file_type)

        # Get mandatory configuration values
        file_path = excel_config["file_path"]
        sp_column = excel_config["sp_column"]
        sp_pep_column = excel_config["sp_pep_column"]

        # Optional configuration values
        sheet_name = excel_config.get("sheet_name", 0)
        columns_to_remove = excel_config.get("columns_to_remove", [])
        unique_column_mapping = excel_config.get("unique_pep_column_mapping", {})

        # New configuration options
        excel_pep_column = excel_config.get("excel_pep_column_mapping")
        row_selector = excel_config.get("row_selector")

        # Overwrite flag (defaults to True if not specified)
        overwrite = excel_config.get("overwrite", True)

        df = self.read_excel_file(file_path, sheet_name)
        if df is None or df.empty:
            self.log(f"No data found in Excel file {file_path}", logging.WARNING)
            return

        # Get all unique pseudonyms from the file
        all_pseudonyms = set()
        for _, row in df.iterrows():
            pseudonyms = self.parse_pseudonyms(row[sp_column])
            all_pseudonyms.update(pseudonyms)

        self.log(f"Found {len(all_pseudonyms)} unique pseudonyms in the data", logging.INFO)

        # Get subject info from PEP
        pep_pseudonyms = self.read_short_pseudonyms(sp_pep_column)

        total_subjects = len(pep_pseudonyms)
        processed_count = 0
        skipped_count = 0
        pep_subjects_not_in_file = 0

        # Check for PEP subjects not in file
        pep_sp_not_in_file = set(pep_pseudonyms) - all_pseudonyms
        if pep_sp_not_in_file:
            self.log(f"Warning: {len(pep_sp_not_in_file)} subjects in PEP not found in Excel file: {pep_sp_not_in_file}", logging.WARNING)
            pep_subjects_not_in_file = len(pep_sp_not_in_file)

        # Check for file pseudonyms not in PEP
        file_sp_not_in_pep = all_pseudonyms - set(pep_pseudonyms)
        if file_sp_not_in_pep:
            self.log(f"Warning: {len(file_sp_not_in_pep)} pseudonyms in Excel file not found in PEP: {file_sp_not_in_pep}", logging.WARNING)

        for index, short_pseudonym in enumerate(pep_pseudonyms, start=1):

            if short_pseudonym not in all_pseudonyms:
                continue

            # Filter DataFrame by short pseudonym
            filtered_df = self.filter_dataframe_by_pseudonym(df, sp_column, short_pseudonym)
            if filtered_df.empty:
                self.log(f"{file_type} ({index}/{total_subjects}): {short_pseudonym}: Skipping subject: No data found", logging.WARNING)
                skipped_count += 1
                continue

            columns_to_check = []
            existence_results = {}
            if not overwrite:
                # Collect all target PEP columns that might be written to
                if unique_column_mapping:
                    columns_to_check.extend(unique_column_mapping.values())
                if row_selector and "mapping" in row_selector:
                    columns_to_check.extend(row_selector["mapping"].values())
                if excel_pep_column:
                    columns_to_check.append(excel_pep_column)

                # Check existence of all target columns at once
                if columns_to_check:
                    existence_results = self.check_data_existence_by_sp(short_pseudonym, columns_to_check)
                else:
                    existence_results = {}

            # Process unique column mapping first (before columns are removed)
            if unique_column_mapping:
                self.upload_unique_column_data(
                    filtered_df,
                    short_pseudonym,
                    unique_column_mapping,
                    existence_results,
                    overwrite)

            # Process excel_pep_column_mapping (store entire filtered Excel data to one pep column)
            if excel_pep_column:
                should_upload = True
                if not overwrite and existence_results.get(excel_pep_column, False):
                    self.log(f"Skipping upload for entire Excel data to {excel_pep_column} for {short_pseudonym}: Data exists and overwrite is false.", 
                        logging.INFO)
                    should_upload = False

                if should_upload:
                    self.upload_excel_data(filtered_df, short_pseudonym, excel_pep_column, columns_to_remove)

            # Process row_selector (store specific rows to pep based on value in selector column 
            # e.g. store rows which have value "25-05-2025" in column "date" to pep column "Data_Week1")
            if row_selector:
                self.upload_row_selected_data(
                    filtered_df, 
                    short_pseudonym, 
                    row_selector, 
                    columns_to_remove, 
                    existence_results, 
                    overwrite)

            processed_count += 1

        self.log(f"Completed processing {file_type}. Successful: {processed_count}, Failed: {skipped_count}, PEP subjects not in file: {pep_subjects_not_in_file}", logging.INFO)

    def store_all_excel_data_in_pep(self, config):
        """Processes all Excel files defined in the configuration."""
        for file_type, excel_config in config.items():
            if excel_config.get("enabled", False):
                self.process_excel_file(excel_config, file_type)