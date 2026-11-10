import copy
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("round_gameplay", Path(__file__).parents[1] / "configure_round_gameplay.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def data():
    return {"game_mode": None, "markers": [{"name": f"Chicken{i}"} for i in range(4)],
            "starts": [{"name": f"Start{i}", "tags": ["ArenaSpawn"]} for i in range(4)]}


def previews():
    return [{"name": f"Chicken{i}", "attached": False, "controlled": False, "owned": False,
             "net_load_on_client": True, "tags": []} for i in range(4)]


class RoundGameplayTests(unittest.TestCase):
    def test_no_mode_and_known_test_mode_are_supported(self):
        source = data()
        before = copy.deepcopy(source)
        result = module.make_plan(source, previews())
        self.assertEqual(result["game_mode"], module.ROUND_MODE)
        self.assertEqual(result["preview_names"], [f"Chicken{i}" for i in range(4)])
        self.assertEqual(source, before)
        source["game_mode"] = module.TEST_MODE
        module.make_plan(source, previews())

    def test_unexpected_game_mode_is_not_overwritten(self):
        source = data()
        source["game_mode"] = "/Game/CustomMode.CustomMode_C"
        with self.assertRaises(ValueError):
            module.make_plan(source, previews())

    def test_exact_four_matching_previews_required(self):
        with self.assertRaises(ValueError):
            module.make_plan(data(), previews()[:3])
        selected = previews()
        selected[0]["name"] = "Other"
        with self.assertRaises(ValueError):
            module.make_plan(data(), selected)

    def test_attached_or_controlled_preview_is_not_removed(self):
        for field in ("attached", "controlled", "owned"):
            selected = previews()
            selected[0][field] = True
            with self.assertRaises(ValueError):
                module.make_plan(data(), selected)

    def test_partial_preview_tags_are_rejected(self):
        selected = previews()
        selected[0]["tags"] = [module.PREVIEW_TAG]
        with self.assertRaises(ValueError):
            module.make_plan(data(), selected)

    def test_case_insensitive_tags_and_repeatability(self):
        source = data()
        source["game_mode"] = module.ROUND_MODE
        for start in source["starts"]:
            start["tags"] = ["arenaspawn"]
        selected = previews()
        for preview in selected:
            preview["tags"] = [module.PREVIEW_TAG.lower(), "KeepThis"]
        self.assertEqual(module.make_plan(source, selected), module.make_plan(source, selected))
        self.assertTrue(all("KeepThis" in p["tags"] for p in selected))

    def test_missing_or_extra_tagged_spawns_are_rejected(self):
        source = data()
        source["starts"][0]["tags"] = []
        with self.assertRaises(ValueError):
            module.make_plan(source, previews())

        source = data()
        source["starts"].append({"name": "Extra", "tags": ["ArenaSpawn"]})
        with self.assertRaises(ValueError):
            module.make_plan(source, previews())

    def test_preview_is_not_loaded_as_a_network_client_pawn(self):
        self.assertFalse(module.make_plan(data(), previews())["net_load_on_client"])


if __name__ == "__main__":
    unittest.main()
