// Synthetic dependency faults for the real shared submission helper.
// These prove error propagation, not successful execution on a Vulkan device.
#include "render/VulkanUtil.hpp"
#include <cstdio>
namespace {
VkResult submitResult, waitResult, resetResult;
unsigned waits, resets;
} // namespace
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(VkQueue, uint32_t,
                                                        const VkSubmitInfo*,
                                                        VkFence) {
    return submitResult;
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkWaitForFences(VkDevice, uint32_t,
                                                          const VkFence*,
                                                          VkBool32, uint64_t) {
    ++waits;
    return waitResult;
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkResetFences(VkDevice, uint32_t,
                                                        const VkFence*) {
    ++resets;
    return resetResult;
}
int main() {
    unsigned failures = 0;
    auto run = [&](VkResult submit, VkResult wait, VkResult reset,
                   bool expected, unsigned expectedWaits,
                   unsigned expectedResets) {
        submitResult = submit;
        waitResult = wait;
        resetResult = reset;
        waits = resets = 0;
        const bool actual = temporal_forge::submitAndWait(
            VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
        if (actual != expected || waits != expectedWaits ||
            resets != expectedResets)
            ++failures;
    };
    run(VK_SUCCESS, VK_SUCCESS, VK_SUCCESS, true, 1, 1);
    run(VK_ERROR_OUT_OF_DEVICE_MEMORY, VK_SUCCESS, VK_SUCCESS, false, 0, 0);
    run(VK_SUCCESS, VK_ERROR_DEVICE_LOST, VK_SUCCESS, false, 1, 0);
    run(VK_SUCCESS, VK_SUCCESS, VK_ERROR_DEVICE_LOST, false, 1, 1);
    std::printf(
        "synthetic Vulkan submission/fence failure propagation: %u failures\n",
        failures);
    return failures ? 1 : 0;
}
