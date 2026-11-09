"""Unreal이 필요 없는 대상 선정/보존 가드 검사. 실제 맵 적용 검증은 별도다."""
import copy
import importlib.util
from pathlib import Path
from types import SimpleNamespace
import unittest

spec = importlib.util.spec_from_file_location("round_spawns", Path(__file__).parents[1] / "link_round_spawns.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def markers():
    return [{"name": f"Chicken{i}", "location": [i * 300., 0., 88.],
             "rotation": [0., i * 90., 0.], "radius": 34., "half_height": 88.}
            for i in range(4)]


def starts(tags=()):
    return [{"name": f"Start{i}", "class": "/Script/Engine.PlayerStart", "tags": list(tags)}
            for i in range(4)]


class RoundSpawnPlanTests(unittest.TestCase):
    def test_create_preserves_exact_marker_pose_and_input(self):
        source = markers()
        before = copy.deepcopy(source)
        plan = module.make_plan(list(reversed(source)), [])
        self.assertEqual([r["marker"] for r in plan], before)
        self.assertEqual(source, before)
        self.assertTrue(all(r["start"] is None for r in plan))

    def test_reuse_four_existing_starts(self):
        plan = module.make_plan(markers(), starts())
        self.assertEqual([r["start"] for r in plan], [f"Start{i}" for i in range(4)])

    def test_tagged_starts_selected_without_touching_unrelated_start(self):
        candidates = starts([module.ARENA_TAG])
        candidates.append({"name": "Unrelated", "class": "/Script/Engine.PlayerStart", "tags": ["Other"]})
        self.assertNotIn("Unrelated", [r["start"] for r in module.make_plan(markers(), candidates)])

    def test_rerun_reuses_managed_starts(self):
        candidates = starts([module.ARENA_TAG, module.MANAGED_TAG])
        self.assertTrue(all(r["start"] for r in module.make_plan(markers(), candidates)))

    def test_fname_tags_are_case_insensitive(self):
        candidates = starts(["arenaspawn"])
        candidates.append({"name": "Unrelated", "class": "/Script/Engine.PlayerStart", "tags": []})
        self.assertTrue(all(r["start"] for r in module.make_plan(markers(), candidates)))

    def test_partial_or_conflicting_tags_are_rejected(self):
        candidates = starts()
        candidates[0]["tags"] = [module.ARENA_TAG]
        with self.assertRaises(ValueError):
            module.make_plan(markers(), candidates)
        candidates = starts([module.MANAGED_TAG])
        candidates += [{"name": f"Other{i}", "class": "/Script/Engine.PlayerStart", "tags": [module.ARENA_TAG]} for i in range(4)]
        with self.assertRaises(ValueError):
            module.make_plan(markers(), candidates)

    def test_wrong_character_count_and_overlap_are_rejected(self):
        with self.assertRaises(ValueError):
            module.make_plan(markers()[:3], [])
        source = markers()
        source[1]["location"] = list(source[0]["location"])
        with self.assertRaises(ValueError):
            module.make_plan(source, [])

    def test_capsule_gap_and_vertical_separation(self):
        source = markers()
        source[1]["location"] = [67., 0., 88.]
        with self.assertRaises(ValueError):
            module.make_plan(source, [])
        source[1]["location"] = [69., 0., 88.]
        module.make_plan(source, [])
        source[1]["location"] = [0., 0., 300.]
        module.make_plan(source, [])

    def test_nonfinite_pose_and_custom_start_are_rejected(self):
        source = markers()
        source[0]["location"][0] = float("nan")
        with self.assertRaises(ValueError):
            module.make_plan(source, [])
        candidates = starts()
        candidates[0]["class"] = "/Game/CustomStart.CustomStart_C"
        with self.assertRaises(ValueError):
            module.make_plan(markers(), candidates)

    def test_attached_start_cannot_move_unrelated_actors(self):
        candidates = starts()
        candidates[0]["attached"] = True
        with self.assertRaises(ValueError):
            module.make_plan(markers(), candidates)

    def test_unsaved_map_or_asset_is_not_discarded(self):
        for dirty_maps, dirty_assets in [(["map"], []), ([], ["asset"])]:
            unreal = SimpleNamespace(EditorLoadingAndSavingUtils=SimpleNamespace(
                get_dirty_map_packages=lambda: dirty_maps, get_dirty_content_packages=lambda: dirty_assets))
            with self.assertRaises(RuntimeError):
                module._clean(unreal)


if __name__ == "__main__":
    unittest.main()
