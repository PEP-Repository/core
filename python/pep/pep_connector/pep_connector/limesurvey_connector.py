import json
import logging
import xmlrpc.client
import base64
import getpass
import csv
import atexit
from io import StringIO
from typing import Literal, Callable, Any
from .peprepository import PEPRepository
from .connectors import Connector

class LimeSurveyConnector(Connector):
    LOG_TAG = "LimeSurveyConnector"

    def __init__(self, repository: PEPRepository, url="https://questions.socsci.ru.nl/index.php/admin/remotecontrol", 
                 token_file=None, 
                 prometheus_dir=None, 
                 use_prometheus=False, 
                 env_prefix=None, 
                 job_name=None):
        super().__init__(repository, prometheus_dir, use_prometheus, env_prefix, job_name)
        self.url = url
        self.token_file = token_file

        # Save the ServerProxy function for use in shutdown, as interpreter mightve already dropped the xmlrpc module
        self._server_proxy_func = xmlrpc.client.ServerProxy

        # Authenticate and get session key
        self.session_key = self._get_session_key()
        self.session_released = False

        # Register an exit handler to ensure release_session_key is called
        atexit.register(self._ensure_session_key_released)

    # Makes sure that the session key is released when the object is destroyed
    def __del__(self):
        self._ensure_session_key_released()

    def _ensure_session_key_released(self) -> None:
        """
        Ensure the session key is released exactly once
        """
        if hasattr(self, 'session_key') and self.session_key and not self.session_released:
            try:
                self._release_session_key()
                self.session_released = True
            except Exception as e:
                # If we still get an error, log it but don't prevent cleanup
                if hasattr(self, 'log'):
                    self.log(f"Error during final session key release: {str(e)}", logging.ERROR)

    def _get_session_key(self) -> str:
        """
        Get the session key from limesurvey.
        If a token file is provided, read credentials from it.
        Otherwise, prompt for username and password.
        """
        if self.token_file:
            try:
                with open(self.token_file, "r") as json_file:
                    credentials = json.load(json_file)
                    username = credentials["username"]
                    password = credentials["password"]
                    auth_plugin = None
            except Exception:
                self.log("Error reading token file.", logging.ERROR)
                raise
        # if no token file is set, use interactive prompt for limesurvey credentials
        else:
            username = input("Enter your LimeSurvey username: ")
            password = getpass.getpass("Enter your LimeSurvey password: ")
            auth_plugin = "AuthLDAP"

        server = xmlrpc.client.ServerProxy(self.url, allow_none=True)
        try:
            if auth_plugin is None:
                session_key = server.get_session_key(username, password)
            else:
                session_key = server.get_session_key(username, password, auth_plugin)
            if isinstance(session_key, dict) and "status" in session_key:
                self.log(f"Error: {session_key['status']}", logging.ERROR)
                raise ValueError(session_key["status"])
            return session_key
        except xmlrpc.client.Fault as err:
            self.log(f"Error: {err.faultString}", logging.ERROR)
            raise RuntimeError(f"Failed to get session key: {err.faultString}")

    def _release_session_key(self) -> None:
        """
        Release the limesurvey session key
        """
        if not hasattr(self, 'session_key') or not self.session_key:
            return

        try:
            # Use the cached ServerProxy function instead of directly accessing the module
            server = self._server_proxy_func(self.url, allow_none=True)
            result = server.release_session_key(self.session_key)

            if hasattr(self, 'log'):
                self.log("Session key released successfully", logging.DEBUG)
            return result
        except Exception as e:
            if hasattr(self, 'log'):
                self.log(f"Error releasing session key: {str(e)}", logging.ERROR)
            raise
    
    def _call_ls_api(self, api_call: Callable, max_retries: int = 1, context: str = "API call") -> Any:
        """
        Execute an API call and retry if the session key is invalid.

        Args:
            api_call: The function to call
            max_retries: Maximum number of retries (default: 1)
            context: Context information to include in error messages
        """
        retries = 0

        while True:
            try:
                result = api_call()

                # First check if the session has expired
                if isinstance(result, dict) and 'status' in result and 'Invalid session key' in result['status']:
                    if retries < max_retries:
                        self.log("Session key invalid, re-authenticating...", logging.WARNING)
                        self.session_key = self._get_session_key()
                        retries += 1
                        continue

                return result

            except xmlrpc.client.Fault as e:
                # activate_tokens API call might fail with a Fault, this is expected and caught upstream
                if not context.startswith("activate_tokens"):
                    self.log(f"XML-RPC call failed in {context}: {e.faultString}", logging.ERROR)
                raise

    ##### Actual API calls #####

    def get_survey_responses(self, survey_id: int, document_type="json", language_code="en", completion_status="complete"):
        """
        Retrieves all survey responses.
        Args:
            survey_id: ID of the Survey
            document_type: Type of document to return (json or csv)
            language_code: Language code for the survey, will fail if incorrect
            completion_status: Completion status of the survey
        Returns:
            The survey responses as list of dicts or CSV string
        """

        server = xmlrpc.client.ServerProxy(self.url, allow_none=True)

        context = f"get_survey_responses (survey_id={survey_id})"

        responses_base64 = self._call_ls_api(
            lambda: server.export_responses(self.session_key, 
                                            survey_id, 
                                            document_type, 
                                            language_code, 
                                            completion_status), context=context)
        try:
            # Try to decode as base64
            responses = base64.b64decode(responses_base64).decode("utf-8")
            
            # Handle empty CSV responses (just newline characters)
            if document_type != "json" and responses.strip() in ["", "\r\n", "\n"]:
                self.log(f"No data in CSV response for survey ID {survey_id}", logging.INFO)
                return None

            if document_type == "json":
                responses_json = json.loads(responses)
                # Handle empty JSON responses
                if "responses" in responses_json and len(responses_json["responses"]) == 0:
                    self.log(f"Empty responses array for survey ID {survey_id}", logging.INFO)
                    return None
                return responses_json
            else:
                return responses
        except (base64.binascii.Error, ValueError, TypeError):
            # If base64 decoding fails, try to load as JSON
            try:
                if isinstance(responses_base64, dict):
                    responses_json = responses_base64
                else:
                    responses_json = json.loads(responses_base64)
                if responses_json.get("status") == "No Data, survey table does not exist.":
                    self.log(f"No data for survey ID {survey_id}", logging.WARNING)
                    return None
                else:
                    raise RuntimeError(f"Unexpected status in JSON response: {responses_json.get('status')}")
            except (json.JSONDecodeError, TypeError) as e:
                self.log(f"JSON decode error: {e}", logging.ERROR)
                raise

    def get_survey_responses_by_token(self, survey_id: int, document_type="json", tokens: list[str] = None, language_code="nl", completion_status="all"):
        """
        Get survey responses by token.
        Args:
            survey_id: ID of the Survey
            document_type: Type of document to return (json or csv)
            tokens: List of tokens to filter responses
            language_code: Language code for the survey, will fail if incorrect
            completion_status: Completion status of the survey
        Returns:
            The survey responses as list of dicts or CSV string
        """

        if not isinstance(survey_id, int):
            raise ValueError("Survey ID must be an integer")
        if tokens is None:
            raise ValueError("Tokens must be provided as a list")
        if not isinstance(tokens, list):
            raise ValueError("Tokens must be a list")

        server = xmlrpc.client.ServerProxy(self.url, allow_none=True)
        
        context = f"get_survey_responses_by_token (survey_id={survey_id}, tokens={tokens})"
        
        responses_base64 = self._call_ls_api(
            lambda: server.export_responses_by_token(
                self.session_key, survey_id, document_type, tokens, language_code, completion_status), context=context)
        try:
            # Try to decode as base64
            responses = base64.b64decode(responses_base64).decode("utf-8")

            # Handle empty CSV responses (just newline characters)
            if document_type != "json" and responses.strip() in ["", "\r\n", "\n"]:
                self.log(f"No data in CSV response for survey ID {survey_id}", logging.INFO)
                return None

            if document_type == "json":
                responses_json = json.loads(responses)
                # Handle empty JSON responses
                if "responses" in responses_json and len(responses_json["responses"]) == 0:
                    self.log(f"Empty responses array for survey ID {survey_id}", logging.INFO)
                    return None
                if "responses" in responses_json and len(responses_json["responses"]) > 1 and len(tokens) == 1:
                    raise RuntimeError("Something went wrong with the LimeSurvey API. More than one response found for a single token. Response: " + str(responses_json))
                return responses_json
            else:
                return responses
        except (base64.binascii.Error, ValueError, TypeError):
            # If base64 decoding fails, try to load as JSON
            try:
                if isinstance(responses_base64, dict):
                    responses_json = responses_base64
                else:
                    responses_json = json.loads(responses_base64)
                response_status = responses_json.get("status")
                if "No Data, survey table does not exist" in response_status:
                    self.log(f"No data for survey ID {survey_id}, {response_status}", logging.WARNING)
                    return None
                if "No Data, could not get max id" in response_status:
                    self.log(f"No data for survey ID {survey_id}, {response_status}", logging.WARNING)
                    return None
                if "Language code not found for this survey" in response_status:
                    self.log(f"Language code not found for survey ID {survey_id}, {response_status}", logging.WARNING)
                    return None
                if "No Response found for Token" in response_status:
                    self.log(f"No Response yet for survey ID {survey_id}, {response_status}", logging.WARNING)
                    return None
                else:
                    self.log(f"Unexpected status in JSON response: {response_status}", logging.ERROR)
                    return None
            except (json.JSONDecodeError, TypeError) as e:
                self.log(f"JSON decode error: {e}", logging.ERROR)
                raise RuntimeError("Failed to retrieve responses")

    def get_survey_response_by_token(self, survey_id: int, document_type: str = "json", token: str = None, language_code: str = "nl", completion_status: str = "complete"):
        """
        Return the survey responses for a single token.

        Returns:
            The survey response as dict or CSV string
        """
        response = self.get_survey_responses_by_token(survey_id=survey_id, document_type=document_type, tokens=[token], language_code=language_code, completion_status=completion_status)
        if document_type == "json" and response is not None:              
            response = response["responses"][0]
        return response

    def list_surveys(self):
        """
        List all surveys available in limesurvey.

        Returns:
            List of dicts of surveys with their 'sid', 'surveyls_title', 'startdate', 'expires' and 'active' keys and their respective values
        """

        server = xmlrpc.client.ServerProxy(self.url, allow_none=True)
        context = "list_surveys"
        return self._call_ls_api(lambda: server.list_surveys(self.session_key), context=context)

    def export_statistics(self, survey_id: int):
        """
        Export statistics for a specific survey.

        Args:
            survey_id: ID of the Survey

        Returns:
            The statistics data
        """
        if not isinstance(survey_id, int):
            raise ValueError("Survey ID must be an integer")

        server = xmlrpc.client.ServerProxy(self.url, allow_none=True)
        context = f"export_statistics (survey_id={survey_id})"
        return self._call_ls_api(lambda: server.export_statistics(self.session_key, survey_id), context=context)

    def list_participants(self, survey_id: int, start: int = 0, limit: int = 100, unused: bool = True, attributes: bool = True):
        """
        List all participants for a specific survey.

        Args:
            survey_id: ID of the Survey
            start: Start index for pagination
            limit: Number of participants to retrieve
            unused: Whether to include unused participants
            attributes: Whether to include participant attributes

        Returns:
            List of participants with their details
        """

        if not isinstance(survey_id, int):
            raise ValueError("Survey ID must be an integer")
        server = xmlrpc.client.ServerProxy(self.url, allow_none=True)
        context = f"list_participants (survey_id={survey_id})"
        return self._call_ls_api(
            lambda: server.list_participants(self.session_key, survey_id, start, limit, unused, attributes), context=context)

    def add_participants(self, survey_id: int, participant_data: list[dict], create_token: bool = False):
        """
        Add participants to the survey.

        Args:
            survey_id: ID of the Survey
            participant_data: List of dicts with data of the participants to be added
                Format: [ {"email":"me@example.com","lastname":"Bond","firstname":"James"},{"email":"me2@example.com","attribute_1":"example"} ]
            create_token: Optional - Defaults to False and determines if the access token is automatically created

        Returns:
            The inserted data including additional new information like the Token entry ID and the token string.
        """
        server = xmlrpc.client.ServerProxy(self.url, allow_none=True)
        context = f"add_participants (survey_id={survey_id})"
        
        result = self._call_ls_api(
            lambda: server.add_participants(self.session_key, survey_id, participant_data, create_token), context=context)

        return result

    def add_participants_as_tokens(self, survey_id: int, tokens: list[str]):
        if not isinstance(tokens, list):
            raise ValueError("Tokens must be a list of strings")
        """
        Add participants to the survey with only tokens as attributes.

        Args:
            survey_id: ID of the Survey
            tokens: List of tokens to be added
                Format: ["token1", "token2"]

        Returns:
            Data on the values added
        """
        participant_data = [{"token": token} for token in tokens]
        return self.add_participants(survey_id, participant_data, create_token=False)
    
    def add_participant_as_token(self, survey_id: int, token: str):
        """
        Add a single participant to the survey with only a token as attribute.

        Args:
            survey_id: ID of the Survey
            token: Token to be added
        Returns:
            Data on the value added
        """
        return self.add_participants_as_tokens(survey_id, [token])

    def get_uploaded_files(self, survey_id: int, token: str = None, response_id: int = None) -> dict:
        """Get all uploaded files for a specific response.

        Args:
            survey_id: ID of the Survey
            token (optional): Response token. Defaults to None.
            response_id (optional): Response ID. Defaults to None.

        Returns:
            dict: Dictionary containing the uploaded files with their metadata and content
                  Format:   {
                  "<filename>": {
                    "meta": {
                      "title": "<text>",
                      "comment": "",
                      "size": 0.0048828125,
                      "name": "<text.txt>",
                      "filename": "<filename>",
                      "ext": "txt",
                      "question": {
                        "title": "G01Q04",
                        "qid": 214504
                      },
                      "index": 0
                    },
                    "content": "<file_in_base64>"
                  },
                  "<filename>": {}
                }

            None if no response is found
        """
        server = xmlrpc.client.ServerProxy(self.url, allow_none=True)
        context = f"get_uploaded_files (survey_id={survey_id}, token={token})"
        
        try:
            if response_id:
                result = self._call_ls_api(
                    lambda: server.get_uploaded_files(self.session_key, survey_id, token, response_id),
                    context=context)
            else:
                result = self._call_ls_api(
                    lambda: server.get_uploaded_files(self.session_key, survey_id, token),
                    context=context)

            if isinstance(result, dict) and 'status' in result:
                if result['status'] == 'No Response found':
                    self.log(f"No response found for survey {survey_id} with token {token}", logging.WARNING)
                    return None
                elif result['status'] != 'OK':
                    raise RuntimeError(f"Error getting files: {result['status']}")
            return result
        except RuntimeError as err:
            if "No Response found" in str(err):
                self.log(f"No response found for survey {survey_id} with token {token}", logging.WARNING)
                return None
            raise

    def copy_survey(self, survey_id: int, new_name: str, destination_survey_id=None) -> int:
        """
        Create a copy of an existing survey.

        Args:
            survey_id: ID of the source survey to copy
            new_name: Name for the new survey
            destination_survey_id (optional): Specific ID for the new survey.
                If this ID is already in use, a random one will be assigned.

        Returns:
            int: The ID of the newly created survey
        """
        server = xmlrpc.client.ServerProxy(self.url, allow_none=True)
        context = f"copy_survey (survey_id={survey_id}, new_name={new_name})"

        # Pass parameters as positional arguments, not as keywords
        if destination_survey_id is not None:
            result = self._call_ls_api(
                lambda: server.copy_survey(self.session_key, survey_id, new_name, int(destination_survey_id)),
                context=context)
        else:
            result = self._call_ls_api(
                lambda: server.copy_survey(self.session_key, survey_id, new_name),
                context=context)

        # Check the result
        if isinstance(result, dict) and 'newsid' in result:
            new_survey_id = int(result['newsid'])
            self.log(f"Successfully copied survey {survey_id} to new survey {new_survey_id}", logging.INFO)
            return new_survey_id
        else:
            error_msg = f"Failed to copy survey {survey_id}: Unexpected response format"
            self.log(error_msg, logging.ERROR)
            raise RuntimeError(error_msg)

    def activate_survey(self, survey_id: int, activation_settings=None):
        """
        Activate the survey.

        Args:
            survey_id: ID of the survey to activate
            activation_settings (optional): Survey activation settings to change prior to activation.
                Format is a dictionary of settingName: settingValue pairs.

        Returns:
            dict: Result of the activation

        Just logs warning if survey is already active.
        """
        server = xmlrpc.client.ServerProxy(self.url, allow_none=True)
        context = f"activate_survey (survey_id={survey_id})"
        
        if activation_settings:
            result = self._call_ls_api(
                lambda: server.activate_survey(self.session_key, survey_id, activation_settings),
                context=context)
        else:
            result = self._call_ls_api(
                lambda: server.activate_survey(self.session_key, survey_id),
                context=context)

        # If the survey is already active, this is no problem
        if isinstance(result, dict) and result.get('status') == 'Error: Survey already active':
            self.log(f"Survey {survey_id} is already active", logging.WARNING)
        else:
            self.log(f"Successfully activated survey {survey_id}", logging.INFO)
            
        return result

    def activate_tokens(self, survey_id: int, attribute_fields=None) -> dict:
        """
        Initializes the survey participant table of a survey, which is where new participant tokens may be later added.

        Args:
            survey_id: ID of the Survey where a survey participants table will be created
            attribute_fields (optional): An array of integers describing any additional attribute fields

        Returns:
            dict: with status='OK' when successful, otherwise the error description
        """
        server = xmlrpc.client.ServerProxy(self.url, allow_none=True)
        context = f"activate_tokens (survey_id={survey_id})"
        
        # Ensure survey_id is an integer
        survey_id = int(survey_id)

        if attribute_fields:
            result = self._call_ls_api(
                lambda: server.activate_tokens(self.session_key, survey_id, attribute_fields), context=context)
        else:
            result = self._call_ls_api(
                lambda: server.activate_tokens(self.session_key, survey_id), context=context)
        # Check if activation was successful
        if isinstance(result, dict) and result.get('status') == 'OK':
            self.log(f"Successfully activated survey participants table for survey {survey_id}", logging.INFO)
            return result
        else:
            error_msg = f"Failed to activate survey participants table for survey {survey_id}"
            if isinstance(result, dict) and 'status' in result:
                error_msg += f": {result['status']}"
            self.log(error_msg, logging.ERROR)
            raise RuntimeError(error_msg)

    def get_survey_properties(self, survey_id: int, survey_settings=None) -> dict:
        """
        Get properties of a survey.
        
        Args:
            survey_id: The id of the Survey to be checked
            survey_settings (optional): The specific properties to get. If None, all properties are returned.

        Returns:
            dict: Survey properties
        """
        server = xmlrpc.client.ServerProxy(self.url, allow_none=True)
        context = f"get_survey_properties (survey_id={survey_id})"

        if survey_settings is not None:
            result = self._call_ls_api(
                lambda: server.get_survey_properties(self.session_key, survey_id, survey_settings), context=context)
        else:
            result = self._call_ls_api(
                lambda: server.get_survey_properties(self.session_key, survey_id), context=context)

        if isinstance(result, dict) and result.get('status') in ['Error', 'No permission', 'Invalid survey ID']:
            error_msg = f"Failed to get survey properties for survey {survey_id}"
            if 'status' in result:
                error_msg += f": {result['status']}"
            self.log(error_msg, logging.ERROR)
            raise RuntimeError(error_msg)

        return result

    def set_survey_properties(self, survey_id: int, **survey_settings) -> dict:
        """
        Set properties of a survey.
        
        Args:
            survey_id: The id of the Survey to be checked
            survey_settings: The specific properties to set. 
                Format: {settingName: settingValue, ...}

        Returns:
            dict: Survey properties
        """
        server = xmlrpc.client.ServerProxy(self.url, allow_none=True)
        context = f"set_survey_properties (survey_id={survey_id})"
        
        # Ensure survey_id is an integer
        survey_id = int(survey_id)

        result = self._call_ls_api(
            lambda: server.set_survey_properties(self.session_key, survey_id, survey_settings), context=context)

        # Check if request was successful
        if isinstance(result, dict) and result.get('status') in ['Error', 'No permission', 'Invalid survey ID']:
            error_msg = f"Failed to set survey properties for survey {survey_id}"
            if 'status' in result:
                error_msg += f": {result['status']}"
            self.log(error_msg, logging.ERROR)
            raise RuntimeError(error_msg)

        return result

    ##### Response handling and upload #####

    def process_response(self, response, document_type: Literal["csv", "json"], answers_to_remove: list[str] = None):
        if response is None:
            self.log("No response to process", logging.WARNING)
            return {}
        if document_type == "json":
            return self._process_response_json(response, answers_to_remove)
        elif document_type == "csv":
            return self._process_response_csv(response, answers_to_remove)

    def _process_response_json(self, response: dict, answers_to_remove: list[str]):
        if answers_to_remove:
            for key in answers_to_remove:
                if key in response:
                    del response[key]
                else:
                    self.log(f"Warning: Key '{key}' specified in answers_to_remove not found in response", 
                            level=logging.WARNING, tag=self.LOG_TAG)
        return json.dumps(response)

    def _process_response_csv(self, response: str, answers_to_remove: list[str] = None):
        self.log("Processing response as csv", logging.DEBUG)

        # Define a custom CSV dialect
        class CustomDialect(csv.Dialect):
            delimiter = ';'
            quotechar = '"'
            doublequote = True
            skipinitialspace = False
            lineterminator = '\n'
            quoting = csv.QUOTE_ALL

        # Remove BOM if present
        if response.startswith("\ufeff"):
            response = response[1:]

        reader = csv.reader(StringIO(response), dialect=CustomDialect)

        # Read the header
        header = next(reader)

        # Handle None case for answers_to_remove
        if answers_to_remove is None:
            answers_to_remove = []

        # Find indices of columns to remove
        indices_to_remove = []
        for answer in answers_to_remove:
            try:
                index = header.index(answer)
                indices_to_remove.append(index)
            except ValueError:
                self.log(f"Warning: Column '{answer}' specified in answers_to_remove not found in CSV header", 
                        level=logging.WARNING, tag=self.LOG_TAG)

        # Process each row
        for row in reader:
            if not row:  # Skip empty rows
                continue

            # Write the row to a StringIO object
            row_output = StringIO()
            writer = csv.writer(row_output, dialect=CustomDialect)

            # Create filtered header and row
            filtered_header = [h for i, h in enumerate(header) if i not in indices_to_remove]
            filtered_row = [val for i, val in enumerate(row) if i not in indices_to_remove]

            writer.writerow(filtered_header)
            writer.writerow(filtered_row)

        return row_output.getvalue()

    def upload_response(self, response, short_pseudonym: str, column_name: str, file_extension):
        try:
            self.upload_file_with_pipe_by_short_pseudonym(short_pseudonym, column_name, response, file_extension=file_extension)
        except Exception as e:
            self.log(f"Failed to upload data for short pseudonym {short_pseudonym}. Error: {e}", logging.ERROR)
            raise

    def _validate_survey_config(self, survey_config, survey_type):

        required_keys = ["survey_ids_pep_column", "document_type", "sp_pep_column", "language_code"]

        for key in required_keys:
            if key not in survey_config:
                raise ValueError(f"Missing required config item: {key}")

        # Special requirement for consent surveys
        if survey_type == "consent":
            if not survey_config.get("researcher_check_question_id"):
                raise ValueError("researcher_check_question_id is required for consent surveys")
            if survey_config.get("document_type") != "json":
                raise ValueError("Consent surveys must have document_type set to 'json'.")
        
        # Checks for optional keys
        completion_status = survey_config.get("completion_status", "complete")
        if completion_status not in ["complete", "incomplete", "all"]:
            raise ValueError("completion_status must be 'complete', 'incomplete', or 'all'.")

        # Validate overwrite flag (optional, defaults to True)
        overwrite = survey_config.get("overwrite", True)
        if not isinstance(overwrite, bool):
            raise ValueError("overwrite must be a boolean (true or false).")

    def store_survey_responses_in_pep(self, survey_config, survey_type) -> tuple[int, int, int, int, int, int, int]:

        self._validate_survey_config(survey_config, survey_type)

        # Required fields
        survey_ids_column = survey_config.get("survey_ids_pep_column")
        doc_type = survey_config.get("document_type")
        short_pseudonym_column = survey_config.get("sp_pep_column")
        language_code = survey_config.get("language_code")

        pep_columns = [survey_ids_column]

        consent_bool_column = survey_config.get("consent_bool_pep_column")
        if consent_bool_column:
            pep_columns.append(consent_bool_column)

        # Optional fields

        # If only files or specific questions are uploaded, this can be empty
        survey_upload_column_name = survey_config.get("survey_pep_column_mapping")

        # Completion status can be "complete", "incomplete", or "all", defaults to "complete"
        completion_status = survey_config.get("completion_status", "complete")

        # File upload
        file_upload_column_names = survey_config.get("file_pep_column_mapping", {})

        # Question-specific PEP column mapping
        question_columns = survey_config.get("question_pep_column_mapping", {})

        if not survey_upload_column_name and not file_upload_column_names and not question_columns:
            raise ValueError("At least one of survey_pep_column_mapping, file_pep_column_mapping, or question_pep_column_mapping must be specified")

        repetition_type = survey_config.get("repetition_type", "once")

        # Adding id and token fileds as hardcoded defaults so these will always be removed even if config is not set
        answers_to_remove = survey_config.get("answers_to_remove", ["id", "token"])

        # Get the researcher check question ID (no default fallback, so it will raise an error if not set)
        researcher_check_question_id = survey_config.get("researcher_check_question_id")

        # Overwrite flag (defaults to True if not specified)
        overwrite = survey_config.get("overwrite", True)

        # Load config from PEP
        subject_info = self.read_short_pseudonym_and_associated_columns(short_pseudonym_column, pep_columns)

        total_subjects = len(subject_info)
        processed_count = 0
        skipped_count = 0

        uploaded_main_count = 0
        uploaded_question_count = 0
        uploaded_file_count = 0
        uploaded_consent_count = 0

        for index, (short_pseudonym, data) in enumerate(subject_info.items(), start=1):

            self.log(f"{survey_type} ({index}/{total_subjects}): {short_pseudonym}: Processing subject", logging.DEBUG)

            survey_ids_data = data["columns"].get(survey_ids_column)
            pep_survey_ids = {}
            if survey_ids_data:
                try:
                    pep_survey_ids = json.loads(survey_ids_data)
                except json.JSONDecodeError as e:
                    skipped_count += 1
                    
                    self.log(f"{survey_type} ({index}/{total_subjects}): {short_pseudonym}: Skipping subject: Failed to load survey IDs data: {str(e)}.", 
                    level=logging.WARNING)
                    continue

            if survey_type == "consent":
                # For consent surveys, skip if consent already given
                consent_bool = data["columns"].get(consent_bool_column)
                if consent_bool:
                    skipped_count += 1
                    self.log(f"{survey_type} ({index}/{total_subjects}): {short_pseudonym}: Skipping subject: Consent already given.", 
                        level=logging.INFO)
                    continue

            if survey_type not in pep_survey_ids or pep_survey_ids.get(survey_type) is None:
                skipped_count += 1
                self.log(f"{survey_type} ({index}/{total_subjects}): {short_pseudonym}: Skipping subject: No survey IDs found for {survey_type}.", 
                    level=logging.INFO)
                continue

            for i, survey_id in enumerate(pep_survey_ids[survey_type]):


                survey_properties = self.get_survey_properties(survey_id)

                if survey_properties.get("active") == "N":
                    self.log(f"{survey_type} ({index}/{total_subjects}): {short_pseudonym}: Skipping subject: Survey {survey_id} is not active.", 
                        level=logging.WARNING)
                    continue
                if survey_properties.get("anonymized") == "Y":
                    self.log(f"{survey_type} ({index}/{total_subjects}): {short_pseudonym}: Skipping subject: Cannot access token table, survey {survey_id} is anonymized.", 
                        level=logging.ERROR)
                    continue

                # For sequential columns (like weekly surveys)
                current_survey_column = survey_upload_column_name
                current_file_upload_columns = file_upload_column_names.copy() if file_upload_column_names else {}
                current_question_columns = question_columns.copy() if question_columns else {}

                if repetition_type == "sequence":
                    week = i + 1

                    # Update survey column name with sequence number
                    if survey_upload_column_name:
                        current_survey_column = f"{survey_upload_column_name}{week:02d}"
                        self.log(f"Using survey column name: {current_survey_column}", logging.DEBUG)

                    # Update question specific column mapping with sequence number
                    if question_columns:
                        current_question_columns = {
                            k: f"{v}{week:02d}" 
                            for k, v in question_columns.items()
                        }

                    # Update file column names with sequence number
                    if current_file_upload_columns:
                        current_file_upload_columns = {
                            k: f"{v}{week:02d}" 
                            for k, v in current_file_upload_columns.items()
                        }

                # Check existence for all potential columns once if we dont overwrite
                existence_results = None
                if not overwrite:
                    all_columns_to_check = []
                    if current_survey_column:
                        all_columns_to_check.append(current_survey_column)
                    if current_question_columns:
                        all_columns_to_check.extend(current_question_columns.values())
                    if current_file_upload_columns:
                        all_columns_to_check.extend(current_file_upload_columns.values())
                    if survey_type == "consent" and consent_bool_column:
                        all_columns_to_check.append(consent_bool_column)

                    existence_results = self.check_data_existence_by_sp(short_pseudonym, all_columns_to_check)

                response = self.get_survey_response_by_token(survey_id, document_type=doc_type, token=short_pseudonym, language_code=language_code, completion_status=completion_status)

                if response is None:
                    self.log(f"{survey_type} ({index}/{total_subjects}): {short_pseudonym}: No response found or other limesurvey API problems for survey ID {survey_id}.", 
                        level=logging.INFO)
                    continue

                if survey_type == "consent":
                    researcher_check = response.get(researcher_check_question_id)

                    if researcher_check == "":
                        skipped_count += 1
                        self.log(f"{survey_type} ({index}/{total_subjects}): {short_pseudonym}: Skipping subject: Researcher check not yet completed.", 
                        level=logging.INFO)
                        continue

                    if not researcher_check:
                        skipped_count += 1
                        self.log(f"{survey_type} ({index}/{total_subjects}): {short_pseudonym}: Skipping subject: Researcher check field '{researcher_check_question_id}' changed or nonexistent.", 
                        level=logging.ERROR)
                        continue

                    if researcher_check == "N":
                        skipped_count += 1
                        self.log(f"{survey_type} ({index}/{total_subjects}): {short_pseudonym}: Skipping subject: Researcher marked no consent.", 
                        level=logging.INFO)
                        continue

                    if researcher_check == "Y":
                        # Consent given, proceed
                        pass
                    else:
                        raise Exception(f"Unexpected researcher check value: {researcher_check}")

                # Handle the main survey response
                if current_survey_column:
                    should_upload_main = True
                    # Use pre-fetched existence_results
                    if existence_results and existence_results.get(current_survey_column, False):
                        self.log(f"{survey_type} ({index}/{total_subjects}): {short_pseudonym}: Skipping main response upload for survey ID {survey_id}: Data already exists in {current_survey_column} and overwrite is false.", 
                            level=logging.INFO)
                        should_upload_main = False
                    
                    if should_upload_main:
                        self.log(f"{survey_type}: Processing main response for survey ID: {survey_id}", logging.DEBUG)
                        processed_response = self.process_response(response, document_type=doc_type, answers_to_remove=answers_to_remove)

                        self.log(f"{survey_type}: Uploading main response for survey ID: {survey_id}", logging.DEBUG)
                        self.upload_response(processed_response, short_pseudonym, current_survey_column, file_extension=f".{doc_type}")
                        uploaded_main_count += 1
                        self.log(f"{survey_type}: Uploaded main response for survey ID: {survey_id}", logging.INFO)

                # Handle question-specific column mapping
                if current_question_columns:
                    self.log(f"{survey_type}: Processing specific question columns", logging.DEBUG)
                    question_uploads = self._process_question_columns(response=response,
                                                    short_pseudonym=short_pseudonym,
                                                    question_columns=current_question_columns,
                                                    document_type=doc_type,
                                                    existence_results=existence_results)
                    uploaded_question_count += question_uploads
                    self.log(f"{survey_type}: Finished processing specific question columns for {short_pseudonym} (Uploaded: {question_uploads})", logging.INFO)

                # Handle file uploads
                if current_file_upload_columns:
                    self.log(f"{survey_type}: Retrieving file uploads for survey ID: {survey_id}", logging.DEBUG)
                    file_dict = self.get_uploaded_files(survey_id=survey_id, token=short_pseudonym)

                    self.log(f"{survey_type}: Processing file uploads for survey ID: {survey_id}", logging.DEBUG)
                    file_uploads = self._process_file_uploads(file_dict=file_dict,
                                                short_pseudonym=short_pseudonym,
                                                file_upload_columns=current_file_upload_columns,
                                                existence_results=existence_results)
                    uploaded_file_count += file_uploads
                    self.log(f"{survey_type}: Finished processing file uploads for {short_pseudonym} (Uploaded: {file_uploads})", logging.INFO)

                # Consent survey specific handling
                if survey_type == "consent" and consent_bool_column:
                    should_upload_consent = True

                    if existence_results and existence_results.get(consent_bool_column, False):
                        self.log(f"{survey_type} ({index}/{total_subjects}): {short_pseudonym}: Skipping consent status upload: Data already exists in {consent_bool_column} and overwrite is false.", 
                            level=logging.INFO)
                        should_upload_consent = False
                    
                    if should_upload_consent:
                        # store consent bool, using string instead of bytes to make it printable
                        self.upload_data_by_short_pseudonym(short_pseudonym, consent_bool_column, "1")
                        uploaded_consent_count += 1
                        self.log(f"{survey_type}: Set consent status for {short_pseudonym}", logging.INFO)

            processed_count += 1

        self.log(f"------ {survey_type.upper()} Upload Summary ------", logging.INFO, tag=self.LOG_TAG)
        self.log(f"Total subjects found: {total_subjects}", logging.INFO, tag=self.LOG_TAG)
        self.log(f"Subjects skipped (no survey ID, consent given/denied, etc.): {skipped_count}", logging.INFO, tag=self.LOG_TAG)
        self.log(f"Subjects processed (attempted upload): {processed_count}", logging.INFO, tag=self.LOG_TAG)
        self.log(f"Successfully uploaded main responses: {uploaded_main_count}", logging.INFO, tag=self.LOG_TAG)
        self.log(f"Successfully uploaded question-specific values: {uploaded_question_count}", logging.INFO, tag=self.LOG_TAG)
        self.log(f"Successfully uploaded files: {uploaded_file_count}", logging.INFO, tag=self.LOG_TAG)
        if survey_type == "consent":
            self.log(f"Successfully uploaded consent statuses: {uploaded_consent_count}", logging.INFO, tag=self.LOG_TAG)
        self.log("-----------------------------------------", logging.INFO, tag=self.LOG_TAG)

        # Return statistics for this survey type including successful uploads
        return (total_subjects, skipped_count, processed_count, 
                uploaded_main_count, uploaded_question_count, uploaded_file_count, uploaded_consent_count)

    def _process_question_columns(self, response, short_pseudonym: str, question_columns: dict[str, str], document_type: Literal["csv", "json"], existence_results: dict | None) -> int:
        """
        Process and upload specific question responses to designated PEP columns.

        Args:
            response: The survey response data
            short_pseudonym: The participant's short pseudonym
            question_columns: Mapping of question IDs to PEP column names
            document_type: The format of the data (json/csv)
            existence_results: Pre-fetched data existence results, or None if overwrite is True.

        Returns:
            int: Number of questions successfully uploaded
        """
        successful_uploads = 0
        self.log(f"Processing {len(question_columns)} specific question columns", logging.DEBUG)

        if document_type == "json":
            for question_id, column_name in question_columns.items():
                self.log(f"Processing question {question_id} for column {column_name}", logging.DEBUG)
                if question_id in response:
                    # Check existence using pre-fetched results
                    should_upload = True
                    if existence_results and existence_results.get(column_name, False):
                        self.log(f"Skipping upload for question {question_id} to {column_name}: Data exists and overwrite is false.", logging.INFO)
                        should_upload = False

                    if should_upload:
                        question_value = response[question_id]

                        if not isinstance(question_value, str):
                            question_value = json.dumps(question_value)

                        try:
                            self.upload_data_by_short_pseudonym(short_pseudonym=short_pseudonym,
                                                                column=column_name,
                                                                data=question_value)
                            successful_uploads += 1
                            self.log(f"Uploaded question {question_id} to column {column_name}", logging.INFO)
                        except Exception as e:
                            self.log(f"Failed to upload question {question_id} for {short_pseudonym}: {str(e)}", logging.ERROR)
                            raise
                else:
                    self.log(f"Question ID {question_id} not found in response", logging.WARNING)

        elif document_type == "csv":
            # For CSV, we need to parse it to extract specific questions
            try:
                # Define a custom CSV dialect
                class CustomDialect(csv.Dialect):
                    delimiter = ';'
                    quotechar = '"'
                    doublequote = True
                    skipinitialspace = False
                    lineterminator = '\n'
                    quoting = csv.QUOTE_ALL

                # Remove BOM (byte order mark) if present
                if isinstance(response, str) and response.startswith("\ufeff"):
                    response = response[1:]

                reader = csv.reader(StringIO(response), dialect=CustomDialect)

                # Read the header
                header = next(reader)

                # Look for the question IDs in the header
                question_indices = {}
                for question_id in question_columns.keys():
                    try:
                        question_indices[question_id] = header.index(question_id)
                    except ValueError:
                        self.log(f"Question ID {question_id} not found in CSV header", logging.WARNING)

                try:
                    # Process the first data row
                    row = next(reader)
                    
                    if not row:
                        self.log("Empty data row in CSV response", logging.WARNING)
                    else:
                        # Extract and upload each question's value
                        for question_id, index in question_indices.items():
                            if index < len(row):
                                column_name = question_columns[question_id]
                                
                                # Check existence using pre-fetched results
                                should_upload = True
                                if existence_results and existence_results.get(column_name, False):
                                    self.log(f"Skipping upload for CSV question {question_id} to {column_name}: Data exists and overwrite is false.", logging.INFO)
                                    should_upload = False

                                if should_upload:
                                    question_value = row[index]
                                    try:
                                        self.upload_data_by_short_pseudonym(short_pseudonym=short_pseudonym,
                                                                            column=column_name,
                                                                            data=question_value)
                                        successful_uploads += 1
                                        self.log(f"Uploaded CSV question {question_id} to column {column_name}", logging.INFO)
                                    except Exception as e:
                                        self.log(f"Failed to upload CSV question {question_id} for {short_pseudonym}: {str(e)}", logging.ERROR)
                                        raise
                    
                    # Check if there are additional rows
                    additional_row = next(reader, None)
                    if additional_row:
                        self.log("Multiple response rows found in CSV data.", logging.ERROR)
                        raise ValueError("Expected only one data row in CSV response, but found multiple rows.")
                        
                except StopIteration:
                    self.log("No data rows found in CSV response", logging.WARNING)

            except Exception as e:
                self.log(f"Failed to process CSV for question columns: {str(e)}", logging.ERROR)
                raise

        return successful_uploads

    def _process_file_uploads(self, file_dict: dict, short_pseudonym: str, file_upload_columns: dict, existence_results: dict | None) -> int:
        """
        Process and upload files from a survey response.

        Args:
            file_dict: Dictionary containing file information
            short_pseudonym: The participant's short pseudonym
            file_upload_columns: Mapping of question IDs to PEP column names
            existence_results: Pre-fetched data existence results, or None if overwrite is True.

        Returns:
            int: Number of files successfully uploaded
        """
        successful_uploads = 0

        if file_dict:
            for file_name, file_info in file_dict.items():
                self.log(f"Processing file {file_name} for {short_pseudonym}", logging.DEBUG)
                question_id = file_info["meta"]["question"]["title"]

                if question_id in file_upload_columns:
                    self.log(f"Processing file upload for question {question_id} for {short_pseudonym}", logging.DEBUG)
                    target_column = file_upload_columns[question_id]

                    if not target_column:
                        self.log(f"Skipping file upload for question {question_id} for {short_pseudonym}: No target column specified", logging.WARNING)
                        continue

                    # Check existence using pre-fetched results
                    should_upload = True
                    if existence_results and existence_results.get(target_column, False):
                        self.log(f"Skipping file upload for question {question_id} to {target_column}: Data exists and overwrite is false.", logging.INFO)
                        should_upload = False

                    if should_upload:
                        file_content = file_info["content"]
                        file_ext = file_info["meta"]["ext"]

                        # Decode and upload the file either with or without file ext
                        try:
                            file_bytes = base64.b64decode(file_content)
                            if file_ext:
                                self.upload_response(file_bytes,
                                                     short_pseudonym,
                                                     target_column,
                                                     file_extension=f".{file_ext}")
                                successful_uploads += 1
                                self.log(f"Uploaded file from question {question_id}, with file extension {file_ext} to {target_column} for {short_pseudonym}", logging.INFO)
                            else:
                                self.upload_response(file_bytes,
                                                     short_pseudonym,
                                                     target_column)
                                successful_uploads += 1
                                self.log(f"Uploaded file from question {question_id} to {target_column} for {short_pseudonym}", logging.INFO)
                        except Exception as e:
                            self.log(f"Failed to decode and/or upload file for {short_pseudonym}: {str(e)}", logging.ERROR)
                            raise
        else:
            self.log(f"Parameter file_upload is specified, but no files found for token {short_pseudonym}", logging.WARNING)

        return successful_uploads

    def store_all_survey_responses_in_pep(self, config):
        overall_total_subjects = 0
        overall_skipped_count = 0
        overall_processed_subjects = 0
        overall_uploaded_main = 0
        overall_uploaded_questions = 0
        overall_uploaded_files = 0
        overall_uploaded_consent = 0
        processed_survey_types = []

        for survey_type, survey_config in config.items():
            if survey_config.get("enabled", False):
                self.log(f"Processing survey type for upload: {survey_type}", level=logging.INFO, tag=self.LOG_TAG)

                (total, skipped, processed, 
                  uploaded_main, uploaded_questions, uploaded_files, uploaded_consent) = self.store_survey_responses_in_pep(survey_config, survey_type)

                # Aggregate overall counts
                overall_total_subjects += total
                overall_skipped_count += skipped
                overall_processed_subjects += processed
                overall_uploaded_main += uploaded_main
                overall_uploaded_questions += uploaded_questions
                overall_uploaded_files += uploaded_files
                overall_uploaded_consent += uploaded_consent
                processed_survey_types.append(survey_type)

        self.log("======== Overall Upload Summary ========", level=logging.INFO, tag=self.LOG_TAG)
        self.log(f"Processed survey types for upload: {', '.join(processed_survey_types) if processed_survey_types else 'None'}", level=logging.INFO, tag=self.LOG_TAG)
        self.log(f"Overall total subjects considered: {overall_total_subjects}", level=logging.INFO, tag=self.LOG_TAG)
        self.log(f"Overall subjects skipped: {overall_skipped_count}", level=logging.INFO, tag=self.LOG_TAG)
        self.log(f"Overall subjects processed (upload attempted): {overall_processed_subjects}", level=logging.INFO, tag=self.LOG_TAG)
        self.log(f"Overall successful uploads - Main responses: {overall_uploaded_main}", level=logging.INFO, tag=self.LOG_TAG)
        self.log(f"Overall successful uploads - Specific questions: {overall_uploaded_questions}", level=logging.INFO, tag=self.LOG_TAG)
        self.log(f"Overall successful uploads - Files: {overall_uploaded_files}", level=logging.INFO, tag=self.LOG_TAG)
        self.log(f"Overall successful uploads - Consent status: {overall_uploaded_consent}", level=logging.INFO, tag=self.LOG_TAG)
        self.log("=========================================", level=logging.INFO, tag=self.LOG_TAG)
