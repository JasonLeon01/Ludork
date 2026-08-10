from __future__ import annotations

import unittest

from ScriptTools.ios_pack import (
    PackError,
    choose_team_id,
    requires_developer_trust,
    xcode_account_team_ids,
)


class IosPackTests(unittest.TestCase):
    team_id = "WU558Y946T"
    stale_account_id = "3660842D-3EBA-4D29-A11C-21505D38FDA5"
    xcode_26_account_id = "EDC68E57-F25E-46A1-88DE-E909D2875EF3"
    current_team_id = "W9854FL96T"

    def account_preferences(self, accounts: list[object]) -> dict[str, object]:
        return {
            "DVTDeveloperAccountManagerAppleIDLists": {
                "IDE.Identifiers.Prod": accounts,
            }
        }

    def provisioning_preferences(
        self,
        account_id: str,
        team_id: str | None = None,
    ) -> dict[str, object]:
        return {
            "IDEProvisioningTeamByIdentifier": {
                account_id: [
                    {
                        "teamID": team_id or self.team_id,
                        "teamName": "Developer (Personal Team)",
                    }
                ],
            },
        }

    def test_stale_team_cache_without_account_is_rejected(self) -> None:
        self.assertEqual(
            xcode_account_team_ids(
                self.account_preferences([]),
                self.provisioning_preferences(self.stale_account_id),
            ),
            set(),
        )

    def test_legacy_string_account_with_team_is_accepted(self) -> None:
        self.assertEqual(
            xcode_account_team_ids(
                self.account_preferences([self.stale_account_id]),
                self.provisioning_preferences(self.stale_account_id),
            ),
            {self.team_id},
        )

    def test_signed_in_account_without_team_is_rejected(self) -> None:
        self.assertNotIn(
            self.team_id,
            xcode_account_team_ids(
                self.account_preferences([self.xcode_26_account_id]),
                self.provisioning_preferences(
                    self.xcode_26_account_id,
                    "AAAAAAAAAA",
                ),
            ),
        )

    def test_xcode_26_account_dictionary_is_accepted(self) -> None:
        self.assertEqual(
            xcode_account_team_ids(
                self.account_preferences(
                    [{"identifier": self.xcode_26_account_id}]
                ),
                self.provisioning_preferences(self.xcode_26_account_id),
            ),
            {self.team_id},
        )

    def test_stale_team_for_removed_account_is_rejected(self) -> None:
        self.assertEqual(
            xcode_account_team_ids(
                self.account_preferences(
                    [{"identifier": self.xcode_26_account_id}]
                ),
                self.provisioning_preferences(self.stale_account_id),
            ),
            set(),
        )

    def test_missing_team_cache_is_rejected(self) -> None:
        self.assertEqual(
            xcode_account_team_ids(
                self.account_preferences([self.xcode_26_account_id]),
                {},
            ),
            set(),
        )

    def test_malformed_account_data_is_rejected(self) -> None:
        self.assertEqual(
            xcode_account_team_ids(
                self.account_preferences([None, 123, {}]),
                self.provisioning_preferences(self.stale_account_id),
            ),
            set(),
        )

    def test_unique_current_account_team_is_selected(self) -> None:
        self.assertEqual(
            choose_team_id({self.current_team_id}, ""),
            self.current_team_id,
        )

    def test_configured_current_account_team_is_selected(self) -> None:
        self.assertEqual(
            choose_team_id(
                {self.team_id, self.current_team_id},
                self.current_team_id.lower(),
            ),
            self.current_team_id,
        )

    def test_stale_certificate_team_cannot_be_selected(self) -> None:
        with self.assertRaisesRegex(PackError, "No signed-in Xcode account"):
            choose_team_id({self.current_team_id}, self.team_id)

    def test_multiple_current_account_teams_require_configuration(self) -> None:
        with self.assertRaisesRegex(PackError, "Multiple signed-in"):
            choose_team_id({self.team_id, self.current_team_id}, "")

    def test_explicitly_untrusted_profile_is_recognized(self) -> None:
        self.assertTrue(
            requires_developer_trust(
                "its profile has not been explicitly trusted by the user"
            )
        )

    def test_other_launch_security_failure_is_not_misclassified(self) -> None:
        self.assertFalse(
            requires_developer_trust("the application has an invalid code signature")
        )


if __name__ == "__main__":
    unittest.main()
