import json
import logging
import smtplib
import hashlib
import getpass
import os
import time
import re
from pydantic import (
    BaseModel,
    Field,
    field_validator,
    model_validator,
    ConfigDict,
    EmailStr,
    HttpUrl,
    FilePath
)
from typing import Literal, Any
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from email.mime.base import MIMEBase
from email.mime.image import MIMEImage
from email.utils import formatdate
from email.utils import make_msgid
from email import encoders
from datetime import datetime, timedelta
from .connectors import Connector, ConnectorConfig
from .peprepository import PEPRepository, PEPConfig
from .datamonitor import DataMonitor
from pypdf import PdfWriter
import weasyprint


class EmailConfig(BaseModel):
    """Configuration for email/SMTP settings."""

    sender: EmailStr
    smtp_server: str
    smtp_port: int = Field(..., ge=1, le=65535)
    reply_to: EmailStr | None = None
    smtp_username: str | None = None
    smtp_password: str | None = None
    smtp_auth_required: bool = True

    model_config = ConfigDict(validate_assignment=True)


class MailSenderSurveyConfig(BaseModel):
    """Configuration for a mail sender survey type."""

    # Core fields
    enabled: bool
    pep_sp_column: str
    repetition_type: Literal["once", "sequence", "schedule"]

    # Email-related fields
    pep_email_column: str | None = None
    pep_name_column: str | None = None
    pep_emails_sent_column: str | None = None
    email_subject: str | None = None
    email_template: str | None = None

    # Survey fields
    pep_survey_ids_column: str | None = None
    template_survey_ids: list[int] | None = None
    survey_base_url: HttpUrl | None = None
    copy_survey: bool = False

    # Timing fields
    start_dates: list[str] | None = None
    start_dates_column: str | None = None
    interval_days: int = 0
    max_reminders: int = 0
    days_between_reminders: int = 7
    max_days_retroactive: int | None = None

    # Attachments, footer, conditions as dicts (not separate dataclasses)
    attachments: list[dict[str, Any]] | None = None
    footer_image: dict[str, Any] | None = None
    conditions: list[dict[str, Any]] | None = None
    data_columns: list[str] | None = None

    # PDF merging
    merge_pdfs: dict[str, Any] | None = None
    is_report_type: bool = False

    @model_validator(mode='after')
    def validate_repetition_type(self):
        if self.enabled:
            valid_types = ['once', 'sequence', 'schedule']
            if self.repetition_type not in valid_types:
                raise ValueError(f"repetition_type must be one of: {', '.join(valid_types)}")
        return self

    @model_validator(mode='after')
    def validate_timing_requirements(self):
        if not self.enabled:
            return self

        if self.repetition_type in ['sequence', 'schedule']:
            if not self.start_dates and not self.start_dates_column:
                raise ValueError(f"repetition_type '{self.repetition_type}' requires either start_dates or start_dates_column")

        return self

    @field_validator('start_dates')
    @classmethod
    def validate_start_dates(cls, v):
        if v:
            for date_str in v:
                try:
                    datetime.fromisoformat(date_str)
                except ValueError:
                    raise ValueError(f"start_dates must be in ISO 8601 date format (YYYY-MM-DD), got: {date_str}")
        return v

    @field_validator('attachments')
    @classmethod
    def validate_attachments(cls, v):
        if v:
            for att in v:
                if not isinstance(att, dict) or 'path' not in att:
                    raise ValueError("Each attachment must be a dict with 'path' field")
                if not os.path.isfile(att['path']):
                    raise ValueError(f"Attachment file does not exist: {att['path']}")
        return v

    @field_validator('footer_image')
    @classmethod
    def validate_footer_image(cls, v):
        if v:
            if not isinstance(v, dict) or 'path' not in v:
                raise ValueError("footer_image must be a dict with 'path' field")
            if not os.path.isfile(v['path']):
                raise ValueError(f"Footer image file does not exist: {v['path']}")
        return v

    model_config = ConfigDict(validate_assignment=True)


class MailSenderConfig(ConnectorConfig):
    """Configuration for MailSender connector with comprehensive email and survey settings."""

    model_config = ConfigDict(
        validate_assignment=True,
        arbitrary_types_allowed=True  # Allow PEPConfig
    )

    # Email configuration as a nested config object
    email_config: EmailConfig | None = None
    smtp_credentials_file: FilePath | None = None

    # LimeSurvey configuration
    limesurvey_api_token_path: FilePath | None = None

    # Survey types configuration
    survey_types: dict[str, MailSenderSurveyConfig] = Field(default_factory=dict)

    # Debug mode
    debug_mode: bool = False

    @classmethod
    def from_dict(cls, data: dict[str, Any], pep_config: PEPConfig) -> "MailSenderConfig":
        """Create MailSenderConfig from configuration dictionary.

        Args:
            data: Dictionary with connector configuration
            pep_config: PEPConfig instance

        Returns:
            Fully constructed and validated MailSenderConfig
        """
        return cls(pep_config=pep_config, **data)


class MailSender(Connector):
    """Class for sending emails via SMTP with tracking capabilities"""

    LOG_TAG = "MailSender"

    def __init__(self, repository: PEPRepository, config: MailSenderConfig):
        """
        Initialize MailSender with a MailSenderConfig object.

        Args:
            repository: PEPRepository instance
            config: MailSenderConfig instance (required)
        """
        if not isinstance(config, MailSenderConfig):
            raise ValueError("config must be an instance of MailSenderConfig")

        # Initialize parent with the config
        super().__init__(repository, config)

        self.repository = repository
        self.debug_mode = config.debug_mode
        self.smtp_auth_required = config.smtp_auth_required
        self.smtp_credentials = {}

        # Load credentials if provided in config
        if config.smtp_username and config.smtp_password:
            self.set_smtp_credentials(config.smtp_username, config.smtp_password)

        # Load credentials from file if provided
        if config.smtp_credentials_file:
            self.load_smtp_credentials_from_file(config.smtp_credentials_file)

    def set_debug_mode(self, debug_mode=True) -> None:
        """Set debug mode (prevents actually sending emails)"""
        self.debug_mode = debug_mode

    def hash_email(self, email: str) -> str:
        """Create a hash of an email address for privacy in logs"""
        return hashlib.sha256(email.lower().encode()).hexdigest()

    def prompt_smtp_credentials(self) -> None:
        """Prompt user for SMTP credentials"""
        username = input("Enter SMTP username: ")
        password = getpass.getpass("Enter SMTP password: ")
        self.set_smtp_credentials(username, password)

    def set_smtp_credentials(self, username: str, password: str) -> None:
        """Set SMTP credentials programmatically"""
        self.smtp_credentials = (username, password)

    def load_smtp_credentials_from_file(self, file_path: str) -> None:
        """Load SMTP credentials from a JSON file

        Args:
            file_path: Path to JSON file containing username and password keys
        """
        try:
            with open(file_path, 'r') as file:
                credentials = json.load(file)

            if not isinstance(credentials, dict):
                self.log(f"Invalid credentials format in file: {file_path}", level=logging.ERROR, tag=self.LOG_TAG)
                raise ValueError("Credentials file must contain a JSON object")

            if 'username' not in credentials or 'password' not in credentials:
                self.log(f"Missing username or password in credentials file: {file_path}", level=logging.ERROR, tag=self.LOG_TAG)
                raise ValueError("Credentials file must contain 'username' and 'password' keys")

            self.set_smtp_credentials(credentials['username'], credentials['password'])
            self.log(f"SMTP credentials loaded from {file_path}", level=logging.INFO, tag=self.LOG_TAG)
        except json.JSONDecodeError:
            self.log(f"Invalid JSON format in credentials file: {file_path}", level=logging.ERROR, tag=self.LOG_TAG)
            raise ValueError(f"Invalid JSON format in credentials file: {file_path}")
        except (IOError, OSError) as e:
            self.log(f"Error reading credentials file {file_path}: {str(e)}", level=logging.ERROR, tag=self.LOG_TAG)
            raise ValueError(f"Error reading credentials file {file_path}: {str(e)}")

    def should_send_email(self,
                          emails_sent: dict,
                          survey_id: int,
                          survey_type: str,
                          max_reminders: int = 1,
                          days_between_reminders: int = 7,
                          start_dates: list[str] = None,
                          interval_days: int = 0,
                          position: int = 0,
                          repetition_type: str = "once",
                          max_days_retroactive: int = None) -> tuple[bool, str | None]:
        """
        Determine if an email should be sent and return the reason if not

        Args:
            emails_sent: Dictionary containing email sending history
            survey_id: ID of the survey
            survey_type: Type of survey
            max_reminders: Maximum number of *additional* reminder emails allowed (default is 1)
            days_between_reminders: Minimum days between reminders for this survey (default is 7, ignored if max_reminders is 0)
            start_dates: List of start dates for the surveys (ISO format)
            interval_days: Number of days between survey sends (for sequence type)
            position: Position of this survey in the sequence (0-based index)
            repetition_type: Type of repetition (once, sequence, schedule)
            max_days_retroactive: Maximum days in the past to allow sending emails (None = send all past emails, minimum 1 day)
        """
        # Default to current date if no start_dates provided
        if not start_dates:
            start_dates = [datetime.now().isoformat()]

        # Calculate effective start date based on repetition type
        if repetition_type == "schedule":
            # For schedule type, use the date at the specified position
            if position >= len(start_dates):
                return False, f"No start date available for position {position}"

            # Parse the scheduled date for this position
            try:
                effective_start_date = datetime.fromisoformat(start_dates[position])
            except ValueError:
                return False, f"Invalid date format for date: {start_dates[position]}"

        elif repetition_type == "sequence":
            # For sequence type, use the first date plus interval multiplier
            try:
                base_date = datetime.fromisoformat(start_dates[0])
                effective_start_date = base_date
                if interval_days and position > 0:
                    effective_start_date = base_date + timedelta(days=position * interval_days)
            except (ValueError, IndexError):
                return False, "Invalid or missing start date for sequence"

        else:
            # For once or other types, use the first date
            try:
                effective_start_date = datetime.fromisoformat(start_dates[0])
            except (ValueError, IndexError):
                return False, "Invalid or missing start date"

        # If we haven't reached the effective start date yet
        if datetime.now() < effective_start_date:
            return False, f"Start date {effective_start_date.isoformat()} not yet reached"

        # Check if the email is too far in the past based on max_days_retroactive
        if max_days_retroactive is not None and max_days_retroactive >= 1:
            days_since_start = (datetime.now() - effective_start_date).days
            if days_since_start > max_days_retroactive:
                return False, f"Start date {effective_start_date.isoformat()} is {days_since_start} days ago (maximum allowed: {max_days_retroactive} days)"

        # Check if emails_sent is empty or if survey_type key doesn't exist
        if not emails_sent or "survey_types" not in emails_sent:
            return True, None

        # Check if the survey_type key exists but is empty/None
        if not emails_sent.get("survey_types"):
            return True, None

        # Check if a specific survey_type key exists 
        if survey_type not in emails_sent["survey_types"]:
            return True, None

        # Check if the specific survey_type key exists but is empty/None
        if not emails_sent["survey_types"].get(survey_type):
            return True, None

        # Check if the survey_id key exists
        if str(survey_id) not in emails_sent["survey_types"][survey_type]:
            return True, None  # First time sending for this survey_id

        # Check if we've reached the maximum number of sends (initial + reminders)
        sends = emails_sent["survey_types"][survey_type][str(survey_id)]
        total_allowed_sends = max_reminders + 1  # Initial email + max_reminders
        if len(sends) >= total_allowed_sends:
            return False, f"Maximum sends ({total_allowed_sends}: 1 initial + {max_reminders} reminders) reached"

        # Check days between reminders
        if max_reminders > 0:
            last_send = datetime.fromisoformat(sends[-1])
            time_since_last_send = (datetime.now() - last_send)
            if time_since_last_send < timedelta(days=days_between_reminders):
                return False, f"Only {time_since_last_send.days} days since last send (minimum is {days_between_reminders} days)"

            # If we got here, it means we haven't reached the max sends and the time constraint is met (or not applicable), so this email is a reminder.
            return True, "Reminder"
        else:
            # If we got here, something else is wrong
            self.log(f"Unexpected state in should_send_email, max reminders is not > 0 but max sends still not yet reached: {emails_sent}, {survey_id}, {survey_type}",
                    level=logging.ERROR, tag=self.LOG_TAG)
            raise RuntimeError("Unexpected state in should_send_email")

    def _send_email_with_retry(self, message, smtp_server, smtp_port, max_retries, retry_delay):
        """
        Send an email message with retry logic for transient errors

        Args:
            message: The email message to send
            smtp_server: SMTP server address
            smtp_port: SMTP server port
            max_retries: Maximum number of retry attempts
            retry_delay: Base delay in seconds between retries (exponential backoff)

        Raises:
            Exception: If email sending fails after all retries
        """
        last_exception = None
        for attempt in range(max_retries + 1):
            try:
                with smtplib.SMTP(smtp_server, smtp_port, timeout=30) as server:

                    # port 587 is used for TLS, port 25 is used for non-TLS
                    if smtp_port != 25:
                        server.starttls()

                    # Only authenticate if required
                    if self.smtp_auth_required:
                        if not self.smtp_credentials:
                            self.prompt_smtp_credentials()

                        smtp_username, smtp_password = self.smtp_credentials

                        if not smtp_username or not smtp_password:
                            self.log("SMTP authentication is required but no credentials provided", level=logging.ERROR, tag=self.LOG_TAG)
                            raise Exception("SMTP authentication is required. Please provide credentials.")

                        try:
                            server.login(smtp_username, smtp_password)
                        except smtplib.SMTPAuthenticationError:
                            self.log("SMTP authentication failed. Please check your credentials.", level=logging.ERROR, tag=self.LOG_TAG)
                            raise Exception("SMTP authentication failed. Please check your credentials.")
                    else:
                        self.log("Sending email without SMTP authentication", level=logging.DEBUG, tag=self.LOG_TAG)

                    server.send_message(message)

                # If we get here, email was sent successfully
                break

            except smtplib.SMTPSenderRefused as e:
                last_exception = e
                if e.smtp_code == 451 and attempt < max_retries:
                    wait_time = retry_delay * (2 ** attempt)  # Exponential backoff
                    self.log(f"Temporary SMTP error (451): {e.smtp_error}. Retrying in {wait_time} seconds (attempt {attempt + 1}/{max_retries})...", 
                            level=logging.WARNING, tag=self.LOG_TAG)
                    time.sleep(wait_time)
                    continue
                else:
                    # Permanent error or max retries reached
                    self.log(f"Email sending failed after {attempt + 1} attempts: {str(e)}", level=logging.ERROR, tag=self.LOG_TAG)
                    raise
            except Exception as e:
                self.log(f"Email sending failed: {str(e)}", level=logging.ERROR, tag=self.LOG_TAG)
                raise
        else:
            # Loop completed without break (all retries exhausted)
            if last_exception:
                self.log(f"Email sending failed after {max_retries + 1} attempts: {str(last_exception)}", level=logging.ERROR, tag=self.LOG_TAG)
                raise last_exception

    def send_email(self, recipient_email, subject, body=None, template=None, template_vars=None, 
                   attachments=None, footer_image=None, use_html=True, is_reminder=False) -> None:
        """Send an email to a recipient with optional template, variables, attachments, and footer image

        Args:
            recipient_email: Email address of the recipient
            subject: Subject of the email (can contain template variables)
            body: Plain text body of the email (used if no template is provided)
            template: Email template string with placeholders for template_vars
            template_vars: Dictionary of variables to substitute in the template and subject
            attachments: List of attachment file paths or dict with 'path', 'filename' (optional), and 'mimetype' (optional)
            footer_image: Dict with 'path', 'alt' (optional), 'width' (optional), and 'height' (optional) for a footer image
            use_html: Whether to send the email as HTML (default is True)
            is_reminder: Whether this is a reminder email (will prefix "Reminder: " to the subject)
        """

        # Only prompt for credentials if authentication is required and not in debug mode
        if self.smtp_auth_required and not self.smtp_credentials and not self.debug_mode:
            self.prompt_smtp_credentials()

        sender_email = self.email_config.get("sender")
        reply_to_email = self.email_config.get("reply_to")

        # Apply template variables to subject if provided
        formatted_subject = subject
        if template_vars and "{" in subject:
            try:
                formatted_subject = subject.format(**template_vars)
            except KeyError as e:
                self.log(f"Warning: Could not format subject with template vars: {e}", 
                         level=logging.ERROR, tag=self.LOG_TAG)
                raise e

        # Add "Reminder: " prefix for reminder emails
        if is_reminder:
            formatted_subject = f"Reminder: {formatted_subject}"

        # Determine whether to use HTML (required for footer image or if explicitly requested)
        use_html = use_html or footer_image is not None

        # If using HTML with a footer image, we need a related MIME structure
        if use_html:
            message = MIMEMultipart('related')
            message_alternative = MIMEMultipart('alternative')
            message.attach(message_alternative)
        else:
            message = MIMEMultipart()

        message["From"] = sender_email
        message["To"] = recipient_email
        message["Subject"] = formatted_subject

        # Add Date header (prevents MISSING_DATE flag)
        message["Date"] = formatdate(localtime=True)

        # Add Message-ID header (prevents MISSING_MID flag)
        message["Message-ID"] = make_msgid(domain=sender_email.split('@')[1])

        # Set Reply-To header if configured
        if reply_to_email:
            message["Reply-To"] = reply_to_email

        # Determine the body content (direct body or template)
        if template and template_vars:
            email_body = template.format(**template_vars)
        elif template:
            email_body = template
        else:
            email_body = body or ""

        # Add plain text content
        if use_html:
            # Add both plain text and HTML versions
            message_alternative.attach(MIMEText(email_body, "plain"))

            # Convert plain text to HTML and add footer image
            html_body = email_body.replace('\n', '<br>\n')

            # Add the footer image
            if footer_image and isinstance(footer_image, dict) and 'path' in footer_image:
                img_path = footer_image['path']
                img_alt = footer_image.get('alt', 'Footer Image')
                img_width = footer_image.get('width', '')
                img_height = footer_image.get('height', '')

                # Set dimensions if provided
                dimensions = ''
                if img_width:
                    dimensions += f' width="{img_width}"'
                if img_height:
                    dimensions += f' height="{img_height}"'

                # Add image reference to HTML
                html_body += f'<br><br><img src="cid:footer_image" alt="{img_alt}"{dimensions}>'

                # Attach the image with Content-ID
                try:
                    with open(img_path, 'rb') as img_file:
                        img_data = img_file.read()
                        img = MIMEImage(img_data)
                        img.add_header('Content-ID', '<footer_image>')
                        img.add_header('Content-Disposition', 'inline')
                        message.attach(img)
                except Exception as e:
                    self.log(f"Failed to attach footer image {img_path}: {str(e)}", 
                             level=logging.ERROR, tag=self.LOG_TAG)
                    raise

            # Wrap the body in basic HTML
            html_content = f"""
            <!DOCTYPE html>
            <html>
            <head>
                <meta charset="UTF-8">
            </head>
            <body>
            {html_body}
            </body>
            </html>
            """
            message_alternative.attach(MIMEText(html_content, "html"))
        else:
            # Plain text email
            message.attach(MIMEText(email_body, "plain"))

        # Handle attachments if provided
        if attachments:
            if not isinstance(attachments, list):
                attachments = [attachments]  # Convert single attachment to list

            for attachment in attachments:
                # Handle both string paths and dict specifications
                if isinstance(attachment, str):
                    filepath = attachment
                    filename = os.path.basename(filepath)
                    mimetype = None  # Will be guessed based on extension
                else:
                    filepath = attachment['path']
                    filename = attachment.get('filename', os.path.basename(filepath))
                    mimetype = attachment.get('mimetype')

                try:
                    with open(filepath, "rb") as file:
                        part = MIMEBase('application', 'octet-stream')
                        if mimetype:
                            main_type, sub_type = mimetype.split('/', 1)
                            part = MIMEBase(main_type, sub_type)

                        part.set_payload(file.read())
                        encoders.encode_base64(part)
                        part.add_header(
                            "Content-Disposition", 
                            f"attachment; filename={filename}")
                        message.attach(part)
                except Exception as e:
                    self.log(f"Failed to attach {filepath}: {str(e)}", level=logging.ERROR, tag=self.LOG_TAG)
                    raise

        hashed_email = self.hash_email(recipient_email)
        if self.debug_mode:
            self.log("\n=== DEBUG: Email that would be sent ===", level=logging.DEBUG, tag=self.LOG_TAG)
            self.log(f"To: [HASHED]{hashed_email}", level=logging.DEBUG, tag=self.LOG_TAG)
            self.log(f"From: {sender_email}", level=logging.DEBUG, tag=self.LOG_TAG)
            if reply_to_email:
                self.log(f"Reply-To: {reply_to_email}", level=logging.DEBUG, tag=self.LOG_TAG)
            self.log(f"Subject: {formatted_subject}", level=logging.DEBUG, tag=self.LOG_TAG)
            self.log(f"Format: {'HTML with footer image' if use_html else 'Plain text'}", level=logging.DEBUG, tag=self.LOG_TAG)
            self.log("\nBody:", level=logging.DEBUG, tag=self.LOG_TAG)
            self.log(email_body, level=logging.DEBUG, tag=self.LOG_TAG)
            if footer_image:
                self.log("\nFooter Image:", level=logging.DEBUG, tag=self.LOG_TAG)
                self.log(f" - {footer_image.get('path')}", level=logging.DEBUG, tag=self.LOG_TAG)
            if attachments:
                self.log("\nAttachments:", level=logging.DEBUG, tag=self.LOG_TAG)
                for attachment in attachments:
                    if isinstance(attachment, str):
                        self.log(f" - {attachment}", level=logging.DEBUG, tag=self.LOG_TAG)
                    else:
                        self.log(f" - {attachment.get('path')} as {attachment.get('filename', os.path.basename(attachment.get('path')))}", 
                                level=logging.DEBUG, tag=self.LOG_TAG)
            self.log("===================================\n", level=logging.DEBUG, tag=self.LOG_TAG)
        else:
            smtp_server = self.email_config.get("smtp_server")
            smtp_port = self.email_config.get("smtp_port", 587)
            max_retries = self.email_config.get("max_retries", 3)
            retry_delay = self.email_config.get("retry_delay", 60)

            # Send email with retry logic
            self._send_email_with_retry(message, smtp_server, smtp_port, max_retries, retry_delay)

            # Make sure to sleep for rate limit avoidance
            rate_limit_seconds = self.email_config.get("rate_limit_seconds", 100)
            self.log(f"Sleeping for {rate_limit_seconds} seconds to avoid rate limit", level=logging.INFO, tag=self.LOG_TAG)
            time.sleep(rate_limit_seconds)
            self.log("Waking up from sleep", level=logging.DEBUG, tag=self.LOG_TAG)

    def record_email_send(self, short_pseudonym: str, column: str, emails_sent: dict, email: str, is_primary_email: bool, survey_id: int, survey_type: str) -> None:
        """Record that we sent an email to this address for this survey"""
        try:
            if not emails_sent:
                emails_sent = {}

            # Only store hashed email for primary (teacher) emails, and always update it
            # This ensures the hash stays current if the teacher changes their email
            # Student emails (for interview/thinkaloud) are never hashed and stored as we don't
            # need these to be searchable in datamonitor
            if is_primary_email:
                hashed_email = self.hash_email(email)
                if "hashed_email" in emails_sent and emails_sent["hashed_email"] != hashed_email:
                    self.log(f"Updating hashed email for short pseudonym: {short_pseudonym}", level=logging.WARNING, tag=self.LOG_TAG)
                emails_sent["hashed_email"] = hashed_email

            if "survey_types" not in emails_sent:
                emails_sent["survey_types"] = {}

            # Check if this specific survey_type exists in the dictionary
            if survey_type not in emails_sent["survey_types"]:
                emails_sent["survey_types"][survey_type] = {}

            # Check if this survey_id exists for this survey_type
            if str(survey_id) not in emails_sent["survey_types"][survey_type]:
                emails_sent["survey_types"][survey_type][str(survey_id)] = []

            if str(survey_id) not in emails_sent["survey_types"][survey_type]:
                emails_sent["survey_types"][survey_type][str(survey_id)] = []

            # Append the current timestamp
            emails_sent["survey_types"][survey_type][str(survey_id)].append(datetime.now().isoformat())

            if self.debug_mode:
                self.log("Skipping saving sent email to PEP in debug mode", level=logging.DEBUG, tag=self.LOG_TAG)
                self.log("\n=== DEBUG: Entry that would be saved ===", level=logging.DEBUG, tag=self.LOG_TAG)
                self.log(json.dumps(emails_sent, indent=2), level=logging.DEBUG, tag=self.LOG_TAG)
                return
            else:
                self.upload_data_by_short_pseudonym(short_pseudonym=short_pseudonym, column=column, data=json.dumps(emails_sent), file_extension=".json")
        except Exception as e:
            self.log(f"FATAL ERROR, MANUAL ACTION REQUIRED: Email sent, but failed to save email sending history for short pseudonym: {short_pseudonym}, survey_id: {survey_id}, survey_type: {survey_type}: Error: {str(e)}", level=logging.CRITICAL, tag=self.LOG_TAG)
            raise

    def _validate_file_exists(self, file_path, file_type="File") -> bool:
        """
        Validates that a file exists at the given path

        Args:
            file_path: Path to the file to check
            file_type: Type of file for error messages (e.g., "Attachment", "Footer image")

        Returns:
            bool: True if file exists, False otherwise
        """
        if not file_path:
            return False

        if not os.path.exists(file_path):
            self.log(f"{file_type} not found: {file_path}", level=logging.WARNING, tag=self.LOG_TAG)
            return False

        if not os.path.isfile(file_path):
            self.log(f"{file_type} path is not a file: {file_path}", level=logging.WARNING, tag=self.LOG_TAG)
            return False

        return True

    def _format_column_name(self, column, position, format_type=None) -> str:
        """
        Format a column name based on position and format type

        Args:
            column: Base column name
            position: Position index (0-based)
            format_type: Type of formatting to apply (e.g., "postfix")

        Returns:
            Formatted column name
        """
        if not format_type:
            return column

        # Position is 0-based, but for display we use 1-based (week numbers)
        week_num = position + 1

        if format_type == "postfix":
            # Format with two digits (01, 02, etc.)
            formatted_week = f"{week_num:02d}"
            return f"{column}{formatted_week}"

        # Default: return original column
        return column

    def _check_conditions(self, conditions, data_columns, short_pseudonym, position) -> tuple[bool, str | None]:
        """
        Check if all conditions are met for running this survey

        Args:
            conditions: List of condition dictionaries with column, condition, and value
            data_columns: Dictionary of column values from PEP
            short_pseudonym: The short pseudonym to check existence for is_empty/is_not_empty conditions
            position: The position of this survey in a sequence (0-based index)

        Returns:
            tuple: (should_run, reason)
        """
        if not conditions:
            return True, None  # If no conditions, always run

        # Separate conditions into existence checks and value checks
        existence_conditions = []
        value_conditions = []

        for condition in conditions:
            operator = condition.get("condition", "is")
            if operator in ["is_empty", "is_not_empty"]:
                existence_conditions.append(condition)
            else:
                value_conditions.append(condition)

        # Process existence conditions (is_empty and is_not_empty) first
        if existence_conditions and short_pseudonym:
            # Collect all columns to check in a single API call
            columns_to_check = []
            for cond in existence_conditions:
                if cond.get("column"):
                    # Apply formatting if format is specified
                    base_column = cond.get("column")
                    format_type = cond.get("format")
                    formatted_column = self._format_column_name(base_column, position, format_type)
                    columns_to_check.append(formatted_column)

            if columns_to_check:
                # Check data existence for all columns at once
                existence_results = self.check_data_existence_by_sp(short_pseudonym, columns_to_check)

                # Evaluate each existence condition
                for condition in existence_conditions:
                    base_column = condition.get("column")
                    format_type = condition.get("format")
                    column = self._format_column_name(base_column, position, format_type)
                    operator = condition.get("condition")

                    if column and column in existence_results:
                        data_exists = existence_results[column]

                        if operator == "is_empty" and data_exists:
                            return False, f"Not running: {column} is not empty"
                        elif operator == "is_not_empty" and not data_exists:
                            return False, f"Not running: {column} is empty"
                    else:
                        self.log(f"Column {column} not found in existence check results", 
                                  level=logging.ERROR, tag=self.LOG_TAG)

        # Process value conditions (all other condition types)
        for condition in value_conditions:
            base_column = condition.get("column")
            format_type = condition.get("format")
            column = self._format_column_name(base_column, position, format_type)
            operator = condition.get("condition", "is")
            expected_value = condition.get("value")

            if not column:
                self.log("Condition missing required 'column' field", level=logging.WARNING, tag=self.LOG_TAG)
                continue

            # Skip validation for conditions that don't require a value
            if "value" not in condition and operator not in ["is_empty", "is_not_empty"]:
                self.log(f"Condition for column '{column}' missing required 'value' field", level=logging.WARNING, tag=self.LOG_TAG)
                continue

            actual_value = data_columns.get(column)

            # Convert values to strings for comparison
            if actual_value is not None:
                actual_value = str(actual_value)

            # Handle list of values for is_one_of and is_not_one_of
            if operator in ["is_one_of", "is_not_one_of"]:
                if not isinstance(expected_value, list):
                    # Try to parse as JSON list if string
                    try:
                        if isinstance(expected_value, str):
                            expected_value = json.loads(expected_value)
                    except json.JSONDecodeError as e:
                        self.log(f"Expected value for '{column}' is not a valid JSON list", level=logging.ERROR, tag=self.LOG_TAG)
                        raise e

                if not isinstance(expected_value, list):
                    self.log(f"Expected value for '{column}' is not a list", level=logging.ERROR, tag=self.LOG_TAG)
                    raise ValueError(f"Expected value for '{column}' is not a list")
                if len(expected_value) <= 1:
                    self.log(f"Expected value for '{column}' is not a list of more than one element", level=logging.ERROR, tag=self.LOG_TAG)
                    raise ValueError(f"Expected value for '{column}' is not a list of more than one element")

                # Convert all list values to strings
                expected_values = [str(val) for val in expected_value]

                if operator == "is_one_of" and actual_value not in expected_values:
                    return False, f"Not running: {column} is not one of {', '.join(expected_values)}"
                elif operator == "is_not_one_of" and actual_value in expected_values:
                    return False, f"Not running: {column} is one of {', '.join(expected_values)}"
            elif operator == "is" and actual_value != str(expected_value):
                return False, f"Not running: {column} is not {expected_value}"
            elif operator == "is_not" and actual_value == str(expected_value):
                return False, f"Not running: {column} is {expected_value}"
            elif operator == "contains" and (not actual_value or str(expected_value) not in actual_value):
                return False, f"Not running: {column} does not contain {expected_value}"
            elif operator == "not_contains" and actual_value and str(expected_value) in actual_value:
                return False, f"Not running: {column} contains {expected_value}"

        return True, None  # All conditions passed

    def _create_survey(self, limesurvey_connector, template_survey_id, survey_type, short_pseudonym, survey_index, copy_survey):
        """
        Create or assign a survey based on configuration

        Args:
            limesurvey_connector: LimeSurvey connector object
            template_survey_id: ID of the template survey
            survey_type: Type of survey being created
            short_pseudonym: Short pseudonym of the participant
            survey_index: Index of this survey in the sequence
            copy_survey: Whether to copy the survey or use the template

        Returns:
            int: The ID of the created or assigned survey
        """
        if self.debug_mode:
            survey_id = survey_index + 1  # Debug mode: use position + 1 as survey ID
            return survey_id

        if copy_survey:
            # Copy survey from template
            if survey_index == 0:
                # For surveys that are not repeated, don't use the index
                survey_name = f"{survey_type.capitalize()} - {short_pseudonym}"
            else:
                survey_name = f"{survey_type.capitalize()} {survey_index+1} - {short_pseudonym}"

            survey_id = limesurvey_connector.copy_survey(template_survey_id, survey_name)

            # Add token table, allows adding participants as tokens
            limesurvey_connector.activate_tokens(survey_id)

            # Add participant and activate the survey
            limesurvey_connector.add_participant_as_token(survey_id, short_pseudonym)
            limesurvey_connector.activate_survey(survey_id)
        else:
            survey_id = template_survey_id

            survey_properties = limesurvey_connector.get_survey_properties(survey_id)

            # Activate token table if not already (dirty fix for now, needs better replacement)
            # If the token table is already active, this will fail, but we can safely continue
            try:
                limesurvey_connector.activate_tokens(survey_id)
            except Exception:
                pass

            # Check if survey is not anonymized, which disallows access to token table
            if survey_properties.get("anonymized") == "Y":
                self.log(f"Skipping subject: Cannot access token table, survey {survey_id} is anonymized.", level=logging.ERROR, tag=self.LOG_TAG)
                return None

            # Check if survey is active, and activate if not
            if survey_properties.get("active") == "N":
                self.log(f"Skipping subject: Survey {survey_id} is not active. Activating", level=logging.WARNING, tag=self.LOG_TAG)
                limesurvey_connector.activate_survey(survey_id)

            limesurvey_connector.add_participant_as_token(survey_id, short_pseudonym)  # Ensure token is added

        return survey_id

    def _load_html_file(self, file_path: str) -> str:
        """
        Load HTML content from a file

        Args:
            file_path: Path to the HTML file

        Returns:
            HTML content as string, or empty string if file not found
        """
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
                self.log(f"Loaded HTML content from {file_path} ({len(content)} characters)", level=logging.DEBUG, tag=self.LOG_TAG)
                return content
        except FileNotFoundError:
            self.log(f"HTML file not found: {file_path}", level=logging.WARNING, tag=self.LOG_TAG)
            raise
        except Exception as e:
            self.log(f"Error reading HTML file {file_path}: {e}", level=logging.ERROR, tag=self.LOG_TAG)
            raise

    def _merge_pdfs(self, pdf_infos: list, output_path: str) -> str:
        """
        Merge multiple PDFs given by a list of dicts with 'path' keys.
        Returns the output path.
        """
        merger = PdfWriter()
        for pdf_info in pdf_infos:
            merger.append(pdf_info["pdf_path"])
        merger.write(output_path)
        merger.close()
        return output_path

    def _generate_expert_report_pdfs(self, monitor_columns, info_columns, group_column, 
                                    consent_column, survey_participant_column, emails_sent_column="EmailsSent",
                                    report_filename="Participatierapport.pdf", output_dir="/tmp",
                                    max_recent_columns=3):
        """
        Generate grouped HTML reports and PDFs for expert report emailing.
        Returns a dict: {group_name: {"pdf_path": ..., "filename": ..., "mimetype": ...}}

        Args:
            monitor_columns: List of dicts with column_name and optional display_name keys
            info_columns: List of dicts with column_name and optional display_name keys
            group_column: Column to group by
            consent_column: Column containing consent information
            survey_participant_column: Column indicating survey participation
            emails_sent_column: Column containing emails sent information
            report_filename: Filename for the generated PDF
            output_dir: Directory to save PDFs to
            max_recent_columns: Maximum number of recent columns to display
        """
        monitor = DataMonitor(self.repository)
        grouped_reports = monitor.generate_grouped_html_reports(
            monitor_columns=monitor_columns,
            info_columns=info_columns,
            group_column=group_column,
            consent_column=consent_column,
            emails_sent_column=emails_sent_column,
            include_javascript=False,
            statistics_only=True,
            max_recent_columns=max_recent_columns
        )

        pdf_paths_dict = {}
        for grouped_val, report in grouped_reports.items():
            safe_filename = re.sub(r'[^\w-]', '_', grouped_val.strip('. '))[:100]
            pdf_path = f"{output_dir}/{safe_filename}_report.pdf"
            weasyprint.HTML(string=report).write_pdf(pdf_path)
            pdf_paths_dict[grouped_val] = {
                "pdf_path": pdf_path,
                "filename": report_filename,
                "mimetype": "application/pdf"
            }
        return pdf_paths_dict

    def send_survey_emails(self, limesurvey_connector, config, survey_type) -> tuple[int, int, int]:
        self.validate_config(config)

        # Load required config items
        survey_participant_column = "TeacherInfo.IsSurveyParticipant"
        template_survey_ids = config.get("template_survey_ids")
        short_pseudonym_column = config.get("pep_sp_column")
        recipient_email_column = config.get("pep_email_column")
        is_primary_email = config.get("primary_email", True)
        emails_sent_column = config.get("pep_emails_sent_column")
        survey_ids_column = config.get("pep_survey_ids_column")
        email_subject = config.get("email_subject")
        email_template = config.get("email_template")
        # Load survey-specific reminder settings
        max_reminders = config.get("max_reminders", 1)
        days_between_reminders = config.get("days_between_reminders", 7)
        max_days_retroactive = config.get("max_days_retroactive")

        # Get attachments and footer image if specified
        attachments = config.get("attachments")
        footer_image = config.get("footer_image")

        pep_columns = [survey_participant_column,
                       recipient_email_column,
                       emails_sent_column,
                       survey_ids_column]

        # Load optional config items
        recipient_name_column = config.get("pep_name_column")
        start_dates_column = config.get("start_dates_column")
        custom_html_file = config.get("custom_html_file")

        custom_html = None
        if custom_html_file:
            custom_html = self._load_html_file(custom_html_file)

        if recipient_name_column:
            pep_columns.append(recipient_name_column)

        if start_dates_column:
            pep_columns.append(start_dates_column)

        # Surveys are not copied by default if no parameter is provided
        copy_survey = config.get("copy_survey", False)

        repetition_type = config.get("repetition_type", "once")
        start_dates = config.get("start_dates")
        _end_date = config.get("end_date")  # Reserved for future time-bounded surveys
        interval_days = config.get("interval_days")

        # Get conditions if specified
        conditions = config.get("conditions", [])

        is_report = config.get("is_report_type", False)

        # Check if this is an expert report type that needs school-specific PDFs
        report_info = config.get("report_info", {})
        is_expert_report = report_info.get("type") == "expert"

        if is_expert_report:
            expert_teacher_column = "TeacherInfo.IsExpertTeacher"
            pep_report_subject_column = report_info.get("subject_column")
            report_filename = report_info.get("combined_filename", "Participatierapport.pdf")
            monitor_columns = report_info.get("monitor_columns")
            info_columns = report_info.get("info_columns")
            group_column = report_info.get("filter_by_column")
            consent_column = report_info.get("pep_consent_column", "Consent.Bool")
            max_recent_columns = report_info.get("max_recent_columns", 4)
            output_dir = report_info.get("output_dir", "/tmp")

            # Ensure columns are added for PEP fetch
            if group_column and group_column not in pep_columns:
                pep_columns.append(group_column)
            if expert_teacher_column not in pep_columns:
                pep_columns.append(expert_teacher_column)
            if pep_report_subject_column and pep_report_subject_column not in pep_columns:
                pep_columns.append(pep_report_subject_column)

            # Add info columns for the report         
            if info_columns:
                for col in info_columns:
                    col_name = col["column_name"] if isinstance(col, dict) else col
                    if col_name not in pep_columns:
                        pep_columns.append(col_name)

            # Add consent column for report generation if not already present
            if consent_column and consent_column not in pep_columns:
                pep_columns.append(consent_column)

        if is_report:
            # For reports we do not require any survey IDs, but if we want repeating reports,
            # we still need to have set a list of unique "dummy" template survey IDs in config, these should be equal to the number of start_dates
            if not template_survey_ids:
                if repetition_type == "once":
                    # Single dummy ID for one-time reports
                    template_survey_ids = [0]
                elif repetition_type == "schedule":
                    # For schedule, generate dummy IDs based on start_dates
                    template_survey_ids = list(range(len(start_dates)))

                self.log(f"Generated {len(template_survey_ids)} dummy template_survey_ids for report type: {template_survey_ids}", level=logging.DEBUG, tag=self.LOG_TAG)
        else:
            # Only surveys require survey_base_url
            survey_base_url = config.get("survey_base_url")

        # Add any condition columns to the columns we need to fetch
        # is_empty and is_not_empty conditions are not added to the list
        # as they are checked separately
        for condition in conditions:
            if "column" in condition and condition["condition"] not in ["is_empty", "is_not_empty"] and condition["column"] not in pep_columns:
                pep_columns.append(condition["column"])

        # Load config from PEP
        subject_info = limesurvey_connector.list_columndata_by_short_pseudonym(short_pseudonym_column, pep_columns)

        # Generate PDFs for expert report after fetching all data
        if is_expert_report:
            pdf_paths_dict = self._generate_expert_report_pdfs(
                monitor_columns=monitor_columns,
                info_columns=info_columns,
                group_column=group_column,
                consent_column=consent_column,
                survey_participant_column=survey_participant_column,
                emails_sent_column=emails_sent_column,
                report_filename=report_filename,
                output_dir=output_dir,
                max_recent_columns=max_recent_columns
            )
        else:
            pdf_paths_dict = {}

        total_subjects = len(subject_info)
        subjects_emailed_count = 0
        skipped_count = 0

        # Iterate over each subject
        for subject_index, (short_pseudonym, data) in enumerate(subject_info.items(), start=1):

            subject_received_email = False

            self.log(f"{survey_type} ({subject_index}/{total_subjects}): {short_pseudonym}: Processing subject.", level=logging.INFO, tag=self.LOG_TAG)

            # Parse data retrieved from PEP and hardcoded conditions
            def parse_flag(value):
                if value == "1":
                    return True
                if value == "0":
                    return False
                return None

            # Only check expert teacher flag if this is an expert report
            if is_expert_report:
                is_expert_teacher = parse_flag(data["columns"].get(expert_teacher_column))

                if is_expert_teacher is None:
                    skipped_count += 1
                    self.log(
                        f"{survey_type} ({subject_index}/{total_subjects}): {short_pseudonym}: "
                        f"Invalid expert teacher flag in {expert_teacher_column}. Expected '1' or '0'.",
                        level=logging.ERROR, tag=self.LOG_TAG,
                    )
                    continue

                if is_expert_teacher:
                    self.log(
                        f"{survey_type} ({subject_index}/{total_subjects}): {short_pseudonym}: Included.",
                        level=logging.DEBUG, tag=self.LOG_TAG,
                    )
                else:
                    skipped_count += 1
                    self.log(
                        f"{survey_type} ({subject_index}/{total_subjects}): {short_pseudonym}: "
                        f"Skipping: Not an expert teacher.",
                        level=logging.INFO, tag=self.LOG_TAG,
                    )
                    continue
            else:
                # For non-expert reports and surveys, only check survey participant
                is_survey_participant = parse_flag(data["columns"].get(survey_participant_column))

                if is_survey_participant is None:
                    skipped_count += 1
                    self.log(
                        f"{survey_type} ({subject_index}/{total_subjects}): {short_pseudonym}: "
                        f"Invalid survey participant flag in {survey_participant_column}. Expected '1' or '0'.",
                        level=logging.ERROR, tag=self.LOG_TAG,
                    )
                    continue

                if is_survey_participant:
                    self.log(
                        f"{survey_type} ({subject_index}/{total_subjects}): {short_pseudonym}: Included.",
                        level=logging.DEBUG, tag=self.LOG_TAG,
                    )
                else:
                    skipped_count += 1
                    self.log(
                        f"{survey_type} ({subject_index}/{total_subjects}): {short_pseudonym}: "
                        f"Skipping: Not a survey participant.",
                        level=logging.INFO, tag=self.LOG_TAG,
                    )
                    continue

            # Recipient name is optional
            recipient_name = data["columns"].get(recipient_name_column) if recipient_name_column else None

            # Parse start_date from PEP column if specified (single date only)
            subject_start_dates = start_dates  # Default to config dates
            if start_dates_column:
                start_dates_data = data["columns"].get(start_dates_column)
                if start_dates_data:
                    # Only accept single date string
                    try:
                        pep_date = datetime.fromisoformat(start_dates_data)

                        # For schedule type: replace first date with PEP date
                        if repetition_type == "schedule" and start_dates:
                            subject_start_dates = [pep_date.isoformat()] + start_dates[1:]

                            self.log(f"{survey_type} ({subject_index}/{total_subjects}): {short_pseudonym}: Replaced first date with PEP date {pep_date.date()}", 
                                        level=logging.DEBUG, tag=self.LOG_TAG)
                        else:
                            # For non-schedule types (once, sequence), just use the PEP date
                            subject_start_dates = [pep_date.isoformat()]

                    except ValueError as e:
                        self.log(f"{survey_type} ({subject_index}/{total_subjects}): {short_pseudonym}: Invalid date format in column {start_dates_column}: {start_dates_data}. Error: {str(e)}", 
                                level=logging.ERROR, tag=self.LOG_TAG)
                        raise ValueError(f"Invalid date format in PEP column {start_dates_column} for {short_pseudonym}: {start_dates_data}")
                else:
                    self.log(f"{survey_type} ({subject_index}/{total_subjects}): {short_pseudonym}: start_dates_column specified but no data in column {start_dates_column}, using config dates", 
                            level=logging.WARNING, tag=self.LOG_TAG)

            # Email Address is required to send emails
            email = data["columns"].get(recipient_email_column)
            if not email:
                skipped_count += 1
                self.log(f"{survey_type} ({subject_index}/{total_subjects}): {short_pseudonym}: Skipping subject: Missing email.", 
                    level=logging.WARNING, tag=self.LOG_TAG)
                continue

            # For expert reports, get school name and prepare school-specific attachments
            subject_attachments = attachments
            if is_expert_report:
                # For expert reports, skip non-expert teachers
                if not is_expert_teacher:
                    skipped_count += 1
                    self.log(f"{survey_type} ({subject_index}/{total_subjects}): {short_pseudonym}: Skipping: Not an expert teacher.", 
                            level=logging.INFO, tag=self.LOG_TAG)
                    continue

                # Skip if no report subjects configured
                report_subjects_raw = data["columns"].get(pep_report_subject_column)
                if not report_subjects_raw:
                    skipped_count += 1
                    self.log(f"{survey_type} ({subject_index}/{total_subjects}): {short_pseudonym}: Skipping: Expert teacher but no report subjects specified.", 
                            level=logging.WARNING, tag=self.LOG_TAG)
                    continue

                report_subjects = self.parse_pep_python_list(report_subjects_raw)

                pdf_infos = []
                for report_subject in report_subjects:
                    pdf_info = pdf_paths_dict.get(report_subject)
                    if pdf_info:
                        pdf_infos.append(pdf_info)
                    else:
                        self.log(f"{survey_type} ({subject_index}/{total_subjects}): {short_pseudonym}: No PDF info found for subject {report_subject}", level=logging.ERROR, tag=self.LOG_TAG)
                if pdf_infos:
                    merged_pdf_path = f"/tmp/merged_{short_pseudonym}.pdf"
                    self._merge_pdfs(pdf_infos, merged_pdf_path)
                    merged_pdf_info = {
                        "path": merged_pdf_path,
                        "filename": report_filename,
                        "mimetype": "application/pdf"
                    }

                    # Combine with existing attachments if any
                    if subject_attachments:
                        subject_attachments = subject_attachments + [merged_pdf_info]
                    else:
                        subject_attachments = [merged_pdf_info]
                    self.log(f"{survey_type} ({subject_index}/{total_subjects}): {short_pseudonym}: Added merged PDF for expert teacher: {merged_pdf_path}", 
                            level=logging.DEBUG, tag=self.LOG_TAG)
                else:
                    skipped_count += 1
                    self.log(f"{survey_type} ({subject_index}/{total_subjects}): {short_pseudonym}: Skipping: No PDFs to merge for expert teacher.", 
                            level=logging.WARNING, tag=self.LOG_TAG)
                    continue

            # Emails sent data is required to track email history
            emails_sent_data = data["columns"].get(emails_sent_column)
            emails_sent = {}
            if emails_sent_data:
                try:
                    emails_sent = json.loads(emails_sent_data)
                except json.JSONDecodeError as e:
                    skipped_count += 1
                    self.log(f"{survey_type} ({subject_index}/{total_subjects}): {short_pseudonym}: Skipping data subject: Failed to load emails sent data: {str(e)}.", 
                    level=logging.ERROR, tag=self.LOG_TAG)
                    continue

            # Survey IDs data is required to track which surveys have been sent
            survey_ids_data = data["columns"].get(survey_ids_column)
            pep_survey_ids = {}
            if survey_ids_data:
                try:
                    pep_survey_ids = json.loads(survey_ids_data)
                except json.JSONDecodeError as e:
                    skipped_count += 1
                    self.log(f"{survey_type} ({subject_index}/{total_subjects}): {short_pseudonym}: Skipping data subject: Failed to load survey IDs data: {str(e)}.", 
                    level=logging.ERROR, tag=self.LOG_TAG)
                    continue

            # Initialize survey IDs list if it doesn't exist
            if survey_type not in pep_survey_ids or pep_survey_ids.get(survey_type) is None:
                pep_survey_ids[survey_type] = []

            # Loop over each repeated survey, for both survey creation and email sending
            for survey_index, template_survey_id in enumerate(template_survey_ids):

                # Check user defined conditions for this specific survey index
                should_run, condition_reason = self._check_conditions(conditions=conditions, 
                                                                      data_columns=data["columns"], 
                                                                      short_pseudonym=short_pseudonym, 
                                                                      position=survey_index)
                if not should_run:
                    self.log(f"{survey_type} ({subject_index}/{total_subjects}): {short_pseudonym}: Skipping survey {survey_index+1}: {condition_reason}", 
                        level=logging.INFO, tag=self.LOG_TAG)
                    continue

                #### Optionally create a new survey ####

                # Check if we already have a survey ID for this position
                # warning: this will disallow overwriting of previously added survey ids,
                # SurveyIDs must then be manually modified in PEP
                if survey_index < len(pep_survey_ids[survey_type]):
                    survey_id = pep_survey_ids[survey_type][survey_index]

                # Store a new survey for this template survey id
                else:
                    # Report types do not create surveys, they just send emails
                    if not is_report:
                        survey_id = self._create_survey(limesurvey_connector=limesurvey_connector,
                                                        template_survey_id=template_survey_id,
                                                        survey_type=survey_type,
                                                        short_pseudonym=short_pseudonym,
                                                        survey_index=survey_index,
                                                        copy_survey=copy_survey)
                    else:
                        survey_id = template_survey_id  # For reports, use the dummy ID directly

                    # If survey creation failed, skip to the next survey
                    if survey_id is None:
                        self.log(f"{survey_type} ({subject_index}/{total_subjects}): {short_pseudonym}: Skipping survey {survey_index+1}, cannot create survey.", 
                            level=logging.WARNING, tag=self.LOG_TAG)
                        continue

                    # Add the created survey ID to the list
                    pep_survey_ids[survey_type].append(survey_id)

                    # Save the updated list of survey IDs back to PEP after a new survey creation
                    limesurvey_connector.upload_data_by_short_pseudonym(short_pseudonym=short_pseudonym, 
                                                                        column=survey_ids_column, 
                                                                        data=json.dumps(pep_survey_ids), 
                                                                        file_extension=".json")

                #### Now send the email for this survey ####

                should_send, reason = self.should_send_email(emails_sent=emails_sent,
                                                              survey_id=survey_id,
                                                              survey_type=survey_type,
                                                              max_reminders=max_reminders,
                                                              days_between_reminders=days_between_reminders,
                                                              start_dates=subject_start_dates,
                                                              interval_days=interval_days,
                                                              position=survey_index,
                                                              repetition_type=repetition_type,
                                                              max_days_retroactive=max_days_retroactive)

                if not should_send:
                    self.log(f"{survey_type} ({subject_index}/{total_subjects}): {short_pseudonym}: Skipping survey {survey_index+1}: {reason}.", 
                        level=logging.INFO, tag=self.LOG_TAG)
                    continue

                # Check if this is a reminder email
                is_reminder = (reason == "Reminder")

                # Calculate deadline as start_date + 14 days
                deadline = "over 2 weken"
                if subject_start_dates and survey_index < len(subject_start_dates):
                    try:
                        deadline_date = datetime.fromisoformat(subject_start_dates[survey_index])+ timedelta(days=14)
                        deadline = deadline_date.date().isoformat()
                    except ValueError:
                        self.log(f"Invalid date format for start_date at index {survey_index}", 
                                level=logging.WARNING, tag=self.LOG_TAG)

                template_vars = {
                    "recipient_name": recipient_name,
                    "deadline": deadline
                }

                # Load custom HTML content if specified
                if custom_html:
                    template_vars.update({
                        "custom_html": custom_html
                    })

                # Extend template vars with survey-specific variables
                if not is_report:
                    survey_link = f"{survey_base_url}/{survey_id}?token={short_pseudonym}"

                    template_vars.update({
                        "survey_link": survey_link, 
                        "survey_number": survey_index + 1,
                        "survey_count": len(template_survey_ids)
                    })

                self.log(f"{survey_type} ({subject_index}/{total_subjects}): {short_pseudonym}: Sending {'reminder ' if is_reminder else ''}email for survey {survey_index+1}.", 
                         level=logging.INFO, tag=self.LOG_TAG)

                # Send the survey email
                self.send_email(recipient_email=email,
                                subject=email_subject,
                                template=email_template,
                                template_vars=template_vars,
                                attachments=subject_attachments,
                                footer_image=footer_image,
                                use_html=True,
                                is_reminder=is_reminder)

                # Record the email send for this specific survey
                self.record_email_send(short_pseudonym=short_pseudonym,
                                        column=emails_sent_column,
                                        emails_sent=emails_sent,
                                        email=email,
                                        is_primary_email=is_primary_email,
                                        survey_id=survey_id,
                                        survey_type=survey_type)
                subject_received_email = True

            # If any email was sent
            if subject_received_email:
                subjects_emailed_count += 1

        # After all subjects are processed, log statistics
        processed_subjects = total_subjects - skipped_count
        self.log(f"------ {survey_type.upper()} Sending Summary ------", level=logging.INFO, tag=self.LOG_TAG)
        self.log(f"Total subjects found: {total_subjects}", level=logging.INFO, tag=self.LOG_TAG)
        self.log(f"Subjects skipped (missing data/errors): {skipped_count}", level=logging.INFO, tag=self.LOG_TAG)
        self.log(f"Subjects processed: {processed_subjects}", level=logging.INFO, tag=self.LOG_TAG)
        self.log(f"Subjects who received at least one email: {subjects_emailed_count}", level=logging.INFO, tag=self.LOG_TAG)
        self.log("-----------------------------------------", level=logging.INFO, tag=self.LOG_TAG)

        # Return statistics for this survey type to be shown in final statistics
        return total_subjects, skipped_count, subjects_emailed_count

    def send_all_survey_emails(self, limesurvey_connector, config):
        overall_total_subjects = 0
        overall_skipped_count = 0
        overall_processed_subjects = 0
        overall_subjects_emailed_count = 0
        processed_survey_types = []

        for survey_type, survey_config in config.items():
            if survey_config.get("enabled", False):
                self.log(f"Processing survey type: {survey_type}", level=logging.INFO, tag=self.LOG_TAG)

                total, skipped, emailed = self.send_survey_emails(limesurvey_connector, survey_config, survey_type)

                processed = total - skipped

                overall_total_subjects += total
                overall_skipped_count += skipped
                overall_processed_subjects += processed
                overall_subjects_emailed_count += emailed
                processed_survey_types.append(survey_type)

        self.log("======== Overall Sending Summary ========", level=logging.INFO, tag=self.LOG_TAG)
        self.log(f"Processed survey types: {', '.join(processed_survey_types) if processed_survey_types else 'None'}", level=logging.INFO, tag=self.LOG_TAG)
        self.log(f"Overall total subjects considered: {overall_total_subjects}", level=logging.INFO, tag=self.LOG_TAG)
        self.log(f"Overall subjects skipped: {overall_skipped_count}", level=logging.INFO, tag=self.LOG_TAG)
        self.log(f"Overall subjects processed: {overall_processed_subjects}", level=logging.INFO, tag=self.LOG_TAG)
        self.log(f"Overall subjects who received at least one email: {overall_subjects_emailed_count}", level=logging.INFO, tag=self.LOG_TAG)
        self.log("=========================================", level=logging.INFO, tag=self.LOG_TAG)
