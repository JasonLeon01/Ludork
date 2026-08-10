from __future__ import annotations

import pathlib
import unittest

from ScriptTools.ui_adapter_check import verify_ui_adapters
from ScriptTools.ui_control_registry import SYSTEM_CONTROLS, adapter_fingerprint


EXPECTED_SYSTEM_CONTROL_IDS = {
    "Engine.Button",
    "Engine.Canvas",
    "Engine.CheckBox",
    "Engine.DropBox",
    "Engine.FunctionalImage",
    "Engine.FunctionalPlainText",
    "Engine.FunctionalRichText",
    "Engine.Image",
    "Engine.ListView",
    "Engine.PlainText",
    "Engine.ProgressBar",
    "Engine.Rect",
    "Engine.RichText",
    "Engine.Slider",
    "Engine.SolidRect",
    "Engine.Window",
}


class UiAdapterCheckTests(unittest.TestCase):
    def test_system_controls_use_root_ids_and_adapters(self) -> None:
        self.assertEqual(
            {
                str(control["controlId"])
                for control in SYSTEM_CONTROLS
            },
            EXPECTED_SYSTEM_CONTROL_IDS,
        )
        self.assertTrue(
            all(
                control["adapter"] == control["controlId"]
                for control in SYSTEM_CONTROLS
            )
        )

    def test_repository_descriptors_match(self) -> None:
        repository_root = pathlib.Path(__file__).resolve().parents[2]

        self.assertTrue(
            (
                repository_root
                / "UiPreviewHost"
                / "src"
                / "UiPreviewHost.cpp"
            ).is_file()
        )
        self.assertEqual(
            verify_ui_adapters(repository_root),
            adapter_fingerprint(),
        )

    def test_project_descriptors_match_without_host_source(self) -> None:
        project_root = pathlib.Path(__file__).resolve().parents[2] / "Sample"

        self.assertFalse((project_root / "src" / "UiPreviewHost.cpp").exists())
        self.assertEqual(
            verify_ui_adapters(project_root),
            adapter_fingerprint(),
        )


if __name__ == "__main__":
    unittest.main()
