import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("player_tint", Path(__file__).parents[1] / "configure_player_tint.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class PlayerTintPlanTests(unittest.TestCase):
    def snapshot(self):
        return dict(path=module.MATERIAL, dirty=False, pie=False, has_base=True,
                    parameters=[], marker="", base_marker="")

    def test_existing_base_is_preserved(self):
        self.assertEqual(module.make_plan(self.snapshot()), "multiply_base")

    def test_owned_setup_is_repeatable(self):
        data = self.snapshot()
        data.update(parameters=[module.PARAMETER], marker=module.MARKER, base_marker=module.MARKER)
        self.assertEqual(module.make_plan(data), "unchanged")

    def test_wrong_material_rejected(self):
        data = self.snapshot(); data["path"] = "/Game/Other.Other"
        with self.assertRaises(ValueError): module.make_plan(data)

    def test_dirty_or_playing_rejected(self):
        for key in ("dirty", "pie"):
            data = self.snapshot(); data[key] = True
            with self.assertRaises(ValueError): module.make_plan(data)

    def test_unknown_parameter_owner_rejected(self):
        data = self.snapshot(); data["parameters"] = [module.PARAMETER]
        with self.assertRaises(ValueError): module.make_plan(data)

    def test_missing_base_rejected(self):
        data = self.snapshot(); data["has_base"] = False
        with self.assertRaises(ValueError): module.make_plan(data)


if __name__ == "__main__": unittest.main()
