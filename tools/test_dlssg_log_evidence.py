"""Regression: distinguish initialization, creation failure and gate acceptance."""
import unittest
from dlssg_log_evidence import classify_vulkan_callback


class CallbackEvidenceTest(unittest.TestCase):
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
