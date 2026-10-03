"""Regression: import-only capabilities must never authorize native Vulkan export."""
import unittest
from review_dlssg_crossapi_interop import export_route


class CapabilityGate(unittest.TestCase):
    def test_observed_import_only_resource_and_heap(self):
        self.assertFalse(export_route(5, 2))
        self.assertFalse(export_route(4, 2))

    def test_memory_export_does_not_imply_fence_export(self):
        self.assertFalse(export_route(6, 2))

    def test_bidirectional_flags_required(self):
        self.assertTrue(export_route(6, 3))
        self.assertFalse(export_route(0, 3))


if __name__ == '__main__':
    unittest.main()
