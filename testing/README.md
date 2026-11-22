# Host Verification Tests

Host-side C++ test programs for verifying [`VirtualMatrixPanel_T`](../src/ESP32-HUB75-VirtualMatrixPanel_T.hpp) mappings, chaining layouts, rotation, scaling, and historical formula equivalences without ESP32 hardware.

## `test_virtual_matrix_t.cpp` (Direct Header Verification)

Uses lightweight DMA/GFX stubs in [`mock_esp/`](./mock_esp/) to compile [`src/ESP32-HUB75-VirtualMatrixPanel_T.hpp`](../src/ESP32-HUB75-VirtualMatrixPanel_T.hpp) directly under all three GFX build modes:

```bash
# 1. Default Adafruit_GFX mode
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Imock_esp -I../src test_virtual_matrix_t.cpp -o /tmp/test_gfx && /tmp/test_gfx

# 2. USE_GFX_LITE mode (includes CRGB overload checks)
g++ -std=c++17 -O2 -Wall -Wextra -Werror -DUSE_GFX_LITE -Imock_esp -I../src test_virtual_matrix_t.cpp -o /tmp/test_lite && /tmp/test_lite

# 3. NO_GFX standalone mode
g++ -std=c++17 -O2 -Wall -Wextra -Werror -DNO_GFX -Imock_esp -I../src test_virtual_matrix_t.cpp -o /tmp/test_nogfx && /tmp/test_nogfx
```

## Legacy / Historical Standalone Reference Checks

```bash
g++ -std=c++11 -O2 -o /tmp/four_scan_64x64_16s four_scan_64x64_16s.cpp && /tmp/four_scan_64x64_16s
g++ -std=c++11 -O2 -o /tmp/four_scan_40_80px_hfarcan four_scan_40_80px_hfarcan.cpp && /tmp/four_scan_40_80px_hfarcan
g++ -o /tmp/virtual virtual.cpp && /tmp/virtual
```