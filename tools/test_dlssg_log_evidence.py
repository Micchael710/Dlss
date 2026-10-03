"""Regression: distinguish initialization, creation failure and gate acceptance."""
import unittest
from dlssg_log_evidence import classify_vulkan_callback, empty_kernel_diagnostic


class CallbackEvidenceTest(unittest.TestCase):
    def test_empty_probe_does_not_override_real_kernel_creation(self):
        probe = {'bytes': 0, 'entry': '', 'routed': False, 'shader': '0x0', 'status': -14, 'feature': 1}
        self.assertTrue(empty_kernel_diagnostic(probe))
        named_failure = {**probe, 'entry': 'Kernel_BlendCandidatesFused', 'bytes': 17056}
        self.assertFalse(empty_kernel_diagnostic(named_failure))
        success = {**named_failure, 'routed': True, 'shader': '0x1234', 'status': 0}
        self.assertFalse(empty_kernel_diagnostic(success))

    def test_startup_is_not_kernel_success(self):
        self.assertEqual(classify_vulkan_callback(['NGXCubinKernelMap::InitCubins Loading NGXCubin kernels'], 'callback')['kernel_status'], 'NOT_OBSERVED')

    def test_observed_module_creation_failure(self):
        data = classify_vulkan_callback([
            'GPU architecture : 0x170, Snippet expects at least : 0x170',
            'NGXCubinVulkan::CreateKernel:498 Error: vkCreateCuModuleNVX() for Kernel_BlendCandidatesFused failed -3'], 'callback')
        self.assertEqual(data['architecture_gate'], 'PASS')
        self.assertEqual(data['kernel_status'], 'FAIL_OBSERVED')
        self.assertEqual(data['kernel_failures'][0]['symbolic'], 'VK_ERROR_INITIALIZATION_FAILED')
        self.assertEqual(data['kernel_failures'][0]['line'], 2)

    def test_stock_gate_is_distinct(self):
        data = classify_vulkan_callback(['GPU architecture : 0x170, Snippet expects at least : 0x190'], 'callback')
        self.assertEqual(data['architecture_gate'], 'FAIL')
        self.assertEqual(data['kernel_status'], 'NOT_OBSERVED')


if __name__ == '__main__':
    unittest.main()
