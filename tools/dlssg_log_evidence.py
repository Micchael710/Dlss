"""Public callback evidence only. No backend ABI or binary inspection."""
import re


def empty_kernel_diagnostic(item):
    """No image, entry or shader: retain as diagnostic, not named FG kernel proof.

    Backend feature IDs are local IDs, not NVSDK_NGX_Feature enum values.
    No interpretation of private status codes is made here.
    """
    return (item.get('bytes') == 0 and item.get('entry') == '' and
            item.get('routed') is False and item.get('shader') == '0x0')


def classify_vulkan_callback(lines, file):
    architecture = []
    failures = []
    successes = []
    for number, line in enumerate(lines, 1):
        reference = {'file': str(file), 'line': number, 'raw': line}
        match = re.search(r'GPU architecture\s*:\s*(0x[\da-f]+).*Snippet expects at least\s*:\s*(0x[\da-f]+)', line, re.I)
        if match:
            architecture.append({**reference, 'physical': int(match[1], 16), 'minimum': int(match[2], 16)})
        match = re.search(r'NGXCubinVulkan::CreateKernel.*?vkCreateCuModuleNVX\(\) for (\S+) failed\s+(-?\d+)', line)
        if match:
            code = int(match[2])
            failures.append({**reference, 'kernel': match[1], 'api': 'vkCreateCuModuleNVX',
                             'numeric': code, 'symbolic': {-3: 'VK_ERROR_INITIALIZATION_FAILED',
                                 -4: 'VK_ERROR_DEVICE_LOST', -1: 'VK_ERROR_OUT_OF_HOST_MEMORY',
                                 -2: 'VK_ERROR_OUT_OF_DEVICE_MEMORY'}.get(code, 'UNRESOLVED_VKRESULT')})
        # A startup line saying "Loading kernels" is not a successful creation event.
        if 'NGXCubinVulkan::CreateKernel' in line and re.search(r'\bsucceeded\b|\bsuccess\b', line, re.I) and 'failed' not in line.lower():
            successes.append(reference)
    return {'architecture_gate': 'PASS' if architecture and all(x['physical'] >= x['minimum'] for x in architecture)
            else 'FAIL' if architecture else 'NOT_OBSERVED',
            'architecture_evidence': architecture, 'kernel_failures': failures,
            'kernel_successes': successes,
            'kernel_status': 'FAIL_OBSERVED' if failures else 'PASS_OBSERVED' if successes else 'NOT_OBSERVED'}
