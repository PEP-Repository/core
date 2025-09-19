import logging
from .accessgroup import AccessGroup
from .peprepository import PEPRepository
from .peprepository import PepError
from typing import Literal, Any

class AccessAdmin(AccessGroup):
    def __init__(self, repository: PEPRepository):
        super().__init__(repository)
        self.repository = repository
        if not self.__user_is_access_admin():
            raise RuntimeError("User is not an Access Administrator")
        self.log("AccessAdmin initialized", level=logging.DEBUG)

    def __user_is_access_admin(self) -> bool:
        _, group = self.query_enrollment()
        if group == "Access Administrator":
            return True
        return False

    def create_column_group_access_rule(self, columngroup: str, access_group: str, permission: Literal["read", "write", "read-meta", "write-meta"]) -> None:
        """
        Create a column group access rule (CGAR) for the specified column group and access group with the given permission (read, write, read-meta, or write-meta).
        """
        if permission not in ["read", "write", "read-meta", "write-meta"]:
            raise ValueError("Permission must be either 'read', 'write', 'read-meta', or 'write-meta'")
        command = ["ama", "cgar", "create", columngroup, access_group, permission]
        try:
            self.repository.run_command(command)
            self.log(f"Successfully created column group access rule for {columngroup}")
        except PepError as e:
            if "This column-group-access-rule already exists" in e.stderr:
                self.log(f"Column group access rule for {columngroup} already exists", level=logging.WARNING)
            elif "No such column-group" in e.stderr:
                self.log(f"No such column group: {columngroup}", level=logging.WARNING)
            elif "Cannot grant explicit" in e.stderr:
                self.log(f"Cannot grant explicit read-meta access rules for {access_group} because all column-groups are implicitly accessible", level=logging.WARNING)
            else:
                self.log(f"Error: {e}", level=logging.ERROR)
                raise
        except Exception as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise

    def remove_column_group_access_rule(self, columngroup: str, access_group: str, permission: Literal["read", "write", "read-meta", "write-meta"]) -> None:
        if permission not in ["read", "write", "read-meta", "write-meta"]:
            raise ValueError("Permission must be either 'read', 'write', 'read-meta', or 'write-meta'")
        command = ["ama", "cgar", "remove", columngroup, access_group, permission]
        try:
            self.repository.run_command(command)
            self.log(f"Successfully removed column group access rule for {columngroup}")
        except PepError as e:
            if "There is no such column-group-access-rule" in e.stderr:
                self.log(f"Column group access rule for {columngroup} does not exist", level=logging.WARNING)
            else:
                self.log(f"Error: {e}", level=logging.ERROR)
                raise
        except Exception as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise

    def remove_column_group_access_rules(self, columngroup: str, access_group: str):
        """
        Remove both 'read' and 'write' access rules for a given column group and access group.
        """
        self.remove_column_group_access_rule(columngroup, access_group, "read")
        self.remove_column_group_access_rule(columngroup, access_group, "write")

    def create_subject_group_access_rule(self, subject_group: str, access_group: str, permission: Literal["access", "enumerate"]) -> None:
        """
        Create a subject group access rule (PGAR) for the specified subject group and access group with the given permission (access or enumerate).
        """
        if permission not in ["access", "enumerate"]:
            raise ValueError("Permission must be either 'access' or 'enumerate'")
        command = ["ama", "pgar", "create", subject_group, access_group, permission]
        try:
            self.repository.run_command(command)
            self.log(f"Successfully created subject group access rule for {subject_group} with {permission} permission")
        except PepError as e:
            if "No such participant-group" in e.stderr:
                self.log(f"No such subject group: {subject_group}", level=logging.WARNING)
            elif "This participant-group-access-rule already exists" in e.stderr:
                self.log(f"Subject group access rule for {subject_group} with {permission} permission already exists", level=logging.WARNING)
            elif "Cannot create explicit participant-group-access-rules" in e.stderr:
                self.log(f"Cannot create explicit subject group access rule for {subject_group} because all subject groups are implicitly accessible", level=logging.WARNING)
            elif "No such mode" in e.stderr:
                self.log(f"No such mode: {permission}", level=logging.WARNING)
            else:
                self.log(f"Error creating subject group access rule for {subject_group} with {permission} permission: {e}", level=logging.ERROR)
                raise
        except Exception as e:
            self.log(f"Error creating subject group access rule for {subject_group} with {permission} permission: {e}", level=logging.ERROR)
            raise

    def remove_subject_group_access_rule(self, subject_group: str, access_group: str, permission: Literal["access", "enumerate"]) -> None:
        if permission not in ["access", "enumerate"]:
            raise ValueError("Permission must be either 'access' or 'enumerate'")
        command = ["ama", "pgar", "remove", subject_group, access_group, permission]
        try:
            self.repository.run_command(command)
            self.log(f"Successfully removed subject group access rule for {subject_group}")
        except PepError as e:
            if "There is no such participant-group-access-rule" in e.stderr:
                self.log(f"Subject group access rule for {subject_group} does not exist", level=logging.WARNING)
            else:
                self.log(f"Error removing subject group access rule for {subject_group}: {e}", level=logging.ERROR)
                raise e
        except RuntimeError as e:
            self.log(f"Error: {e}", level=logging.ERROR)

    def remove_subject_group_access_rules(self, subject_group: str, access_group: str) -> None:
        """
        Remove both 'access' and 'enumerate' access rules for a given subject group and access group.
        """
        self.remove_subject_group_access_rule(subject_group=subject_group,
                                              access_group=access_group,
                                              permission="access")
        self.remove_subject_group_access_rule(subject_group=subject_group,
                                              access_group=access_group,
                                              permission="enumerate")

    def create_user_group(self, name: str, max_auth_validity: str = None) -> None:
        """
        Create a user group with the specified name and optional max_auth_validity with the format YYYYMMDD.
        """
        command = ["user", "group", "create", name]
        if max_auth_validity:
            command.extend(["--max-auth-validity", max_auth_validity])
        try:
            self.repository.run_command(command)
            self.log(f"Successfully created user group {name}")
        except PepError as e:
            if "already exists" in e.stderr:
                self.log(f"User group {name} already exists", level=logging.WARNING)
            else:
                self.log(f"Error: {e}", level=logging.ERROR)
                raise
        except Exception as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise

    def create_user_groups(self, user_groups: list[str], max_auth_validity: str = None):
        """
        Create multiple user groups from a list, with the specified names and optional max_auth_validity with the format YYYYMMDD.
        """
        if not isinstance(user_groups, list):
            raise ValueError("User groups must be a list of strings")
        for user_group in user_groups:
            self.create_user_group(user_group=user_group,
                                   max_auth_validity=max_auth_validity)

    def modify_user_group(self, name: str, max_auth_validity: str = None) -> None:
        """
        Modify a user group with the specified name and optional max_auth_validity with the format YYYYMMDD.
        """
        command = ["user", "group", "modify", name]
        if max_auth_validity:
            command.extend(["--max-auth-validity", max_auth_validity])
        try:
            self.repository.run_command(command)
            self.log(f"Successfully modified user group {name}")
        except PepError as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise
        except Exception as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise

    def create_user(self, uid: str) -> None:
        """
        Create a user with the specified UID.
        """
        command = ["user", "create", uid]
        try:
            self.repository.run_command(command)
            self.log(f"Successfully created user {uid}")
        except PepError as e:
            if "already exists" in e.stderr:
                self.log(f"User {uid} already exists", level=logging.WARNING)
            else:
                self.log(f"Failed to create user. Error: {e}", level=logging.ERROR)
                raise
        except Exception as e:
            self.log(f"Failed to create user. Error: {e}", level=logging.ERROR)
            raise

    def remove_user(self, uid: str) -> None:
        """
        Remove a user with the specified UID.
        """
        command = ["user", "remove", uid]
        try:
            self.repository.run_command(command)
            self.log(f"Successfully removed user {uid}")
        except PepError as e:
            if "does not exist" in e.stderr:
                self.log(f"User {uid} does not exist", level=logging.WARNING)
            elif "still in user groups" in e.stderr:
                self.log(f"User {uid} is still in user groups", level=logging.WARNING)
            else:
                self.log(f"Failed to remove user. Error: {e}", level=logging.ERROR)
                raise
        except Exception as e:
            self.log(f"Failed to remove user. Error: {e}", level=logging.ERROR)
            raise

    def add_identifier_for_user(self, existing_uid: str, new_uid: str) -> None:
        """
        Add an identifier for an existing user.
        """
        command = ["user", "addIdentifier", existing_uid, new_uid]
        try:
            self.repository.run_command(command)
            self.log(f"Successfully added identifier {new_uid} for user {existing_uid}")
        except PepError as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise
        except Exception as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise

    def remove_identifier_for_user(self, uid: str) -> None:
        """
        Remove an identifier for a user.
        """
        command = ["user", "removeIdentifier", uid]
        try:
            self.repository.run_command(command)
            self.log(f"Successfully removed identifier {uid}")
        except PepError as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise
        except Exception as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise

    def remove_user_from_group(self, uid: str, group: str) -> None:
        """
        Remove a user from a group.
        """
        command = ["user", "removeFrom", uid, group]
        try:
            self.repository.run_command(command)
            self.log(f"Successfully removed user {uid} from group {group}")
        except PepError as e:
            if "is not part of group" in e.stderr:
                self.log(f"User {uid} is not in group {group}", level=logging.WARNING)
            else:
                self.log(f"Failed to remove user from group. Error: {e}", level=logging.ERROR)
                raise
        except Exception as e:
            self.log(f"Failed to remove user from group. Error: {e}", level=logging.ERROR)
            raise

    def add_user_to_group(self, uid: str, group: str) -> None:
        """
        Add a user to a group.
        """
        command = ["user", "addTo", uid, group]
        try:
            self.repository.run_command(command)
            self.log(f"Successfully added user {uid} to group {group}")
        except PepError as e:
            if "already in group" in e.stderr:
                self.log(f"User {uid} is already in group {group}", level=logging.WARNING)
            elif "No such group" in e.stderr:
                self.log(f"No such group: {group}", level=logging.WARNING)
            else:
                self.log(f"Failed to add user to group. Error: {e}", level=logging.ERROR)
                raise
        except Exception as e:
            self.log(f"Failed to add user to group. Error: {e}", level=logging.ERROR)
            raise

    def add_user_to_groups(self, uid: str, groups: list[str]) -> None:
        for group in groups:
            self.add_user_to_group(uid=uid, group=group)

    def remove_group(self, name: str) -> None:
        """
        Remove a usergroup with the specified name.
        """
        command = ["user", "group", "remove", name]
        try:
            self.repository.run_command(command)
            self.log(f"Successfully removed group {name}")
        except PepError as e:
            if "does not exist" in e.stderr:
                self.log(f"Group {name} does not exist", level=logging.WARNING)
            elif "still has users" in e.stderr:
                self.log(f"Group {name} still has users", level=logging.WARNING)
            else:
                self.log(f"Failed to remove group. Error: {e}", level=logging.ERROR)
                raise
        except Exception as e:
            self.log(f"Failed to remove group. Error: {e}", level=logging.ERROR)
            raise

    def query(self) -> str:
        """
        Query for the usergroup configuration.
        """
        command = ["user", "query"]
        try:
            output = self.repository.run_command(command)
            self.log("Successfully executed asa query")
            return output
        except PepError as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise
        except Exception as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise

    def request_token(self, subject: str, user_group: str, expiration_unixtime=None, expiration_yyyymmdd=None, json_output=False) -> str:
        """
        Request a token for a subject and user group.
        """
        command = ["token", "request", subject, user_group]
        if expiration_unixtime:
            command.append(expiration_unixtime)
        if expiration_yyyymmdd:
            command.extend(["--expiration-yyyymmdd", expiration_yyyymmdd])
        if json_output:
            command.insert(3, "--json")
        try:
            output = self.repository.run_command(command)
            self.log("Successfully requested token")
            return output
        except PepError as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise
        except Exception as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise

    def block_token(self, subject: str, user_group: str, message: str, issued_before_unixtime=None, issued_before_yyyymmdd=None):
        """
        Block a token for a subject and user group.
        """
        command = ["token", "block", "create", "--message", message, subject, user_group]
        if issued_before_unixtime:
            command.extend(["--issuedBefore-unixtime", issued_before_unixtime])
        if issued_before_yyyymmdd:
            command.extend(["--issuedBefore-yyyymmdd", issued_before_yyyymmdd])
        try:
            output = self.repository.run_command(command)
            self.log("Successfully blocked token")
            return output
        except PepError as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise
        except Exception as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise

    def list_blocked_tokens(self) -> str:
        """
        List all blocked tokens.
        """
        command = ["token", "block", "list"]
        try:
            output = self.repository.run_command(command)
            self.log("Successfully listed blocked tokens")
            return output
        except PepError as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise
        except Exception as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise

    def remove_blocked_token(self, token_id: str):
        """
        Remove a blocked token by its ID.
        """
        command = ["token", "block", "remove", token_id]
        try:
            output = self.repository.run_command(command)
            self.log("Successfully removed blocked token")
            return output
        except PepError as e:
            if "does not exist" in e.stderr:
                self.log(f"Blocked token {token_id} does not exist", level=logging.WARNING)
            else:
                self.log(f"Error: {e}", level=logging.ERROR)
                raise e
        except Exception as e:
            self.log(f"Error: {e}", level=logging.ERROR)
            raise

    def remove_blocked_tokens(self, token_ids: list[str]) -> None:
        """
        Remove multiple blocked tokens in a list by their IDs.
        """
        for token_id in token_ids:
            self.remove_blocked_token(token_id)

    def setup_access_from_config(self, config: dict[str, Any]) -> None:
        """
        Set up user groups and access rules based on config.

        Args:
            config: Configuration containing user groups and access rules.
                Format:
            {
                "user_groups": {
                    "group1": {"max_auth_validity": "180d"},
                    "group2": {"max_auth_validity": "90d"},
                    "group3": {}
                },
                "access_rules": {
                    "group1": {
                        "column_groups": {
                            "TeacherInfo": ["read", "write"],
                            "Consent": ["read", "write"]
                        },
                        "subject_groups": {
                            "subject_group1": ["enumerate"]
                        }
                    },
                    "group2": {
                        "column_groups": {
                            "TeacherInfo": ["read"],
                            "Consent": ["read"]
                        },
                        "subject_groups": {
                            "subject_group1": ["access"],
                            "subject_group2": ["access", "enumerate"]
                        }
                    }
                }
            }
        """
        if not isinstance(config, dict):
            raise ValueError("Config must be a dictionary")

        # Create user groups
        if "user_groups" in config and isinstance(config["user_groups"], dict):
            for group_name, group_config in config["user_groups"].items():
                max_validity = group_config.get("max_auth_validity")
                self.create_user_group(group_name, max_auth_validity=max_validity)

        # Set up access rules
        if "access_rules" in config and isinstance(config["access_rules"], dict):
            for access_group, rules in config["access_rules"].items():

                # Set up column group access rules
                if "column_groups" in rules and isinstance(rules["column_groups"], dict):
                    for column_group, permissions in rules["column_groups"].items():
                        for permission in permissions:
                            self.create_column_group_access_rule(column_group, access_group, permission)

                # Set up subject group access rules
                if "subject_groups" in rules and isinstance(rules["subject_groups"], dict):
                    for subject_group, permissions in rules["subject_groups"].items():
                        for permission in permissions:
                            self.create_subject_group_access_rule(subject_group, access_group, permission)