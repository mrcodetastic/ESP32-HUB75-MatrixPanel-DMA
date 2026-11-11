// Self-checking test for FOUR_SCAN_64PX_HIGH_16S.
//
// Compares the scan-type mapping against the reference mapping used by the
// Pixel_Mapping_Test example (generic "any other height" branch) for a
// 64px-high panel, and checks that the result is a bijection onto the
// 2*W x H/2 DMA surface.
//
//   g++ -std=c++11 -O2 -o four_scan_64x64_16s four_scan_64x64_16s.cpp
//   ./four_scan_64x64_16s

#include <cstdio>
#include <set>
#include <utility>

struct Coords {
  int x;
  int y;
};

// Mapping as implemented by ScanTypeMapping<FOUR_SCAN_64PX_HIGH_16S>.
static Coords map16s(int x, int y, int W) {
  Coords c = {x, y};
  const int block = c.y >> 4; // 0..3
  if ((block & 1) == 0) {
    c.x += ((c.x / W) + 1) * W;
  } else {
    c.x += (c.x / W) * W;
  }
  c.y = ((block >> 1) << 4) + (c.y & 0x0F);
  return c;
}

// Reference: VirtualMatrixPanel + generic branch of Pixel_Mapping_Test
// for panelResY == 64 (single panel; for a chain x is still virtual).
static Coords reference(int x, int y, int W) {
  Coords c = {x, y};
  const int half = 32;
  if ((c.y % half) < half / 2) {
    c.x += ((c.x / W) + 1) * W;
  } else {
    c.x += (c.x / W) * W;
  }
  c.y = (c.y / half) * (half / 2) + (c.y % (half / 2));
  return c;
}

static bool run(int panels) {
  const int W = 64;
  const int H = 64;
  const int vw = panels * W;

  std::set<std::pair<int, int> > seen;
  int mismatches = 0;

  for (int y = 0; y < H; y++) {
    for (int x = 0; x < vw; x++) {
      const Coords a = map16s(x, y, W);
      const Coords b = reference(x, y, W);
      if (a.x != b.x || a.y != b.y) {
        mismatches++;
        if (mismatches <= 5) {
          std::printf("  mismatch at (%d,%d): got (%d,%d) expected (%d,%d)\n",
                      x, y, a.x, a.y, b.x, b.y);
        }
      }
      if (a.x < 0 || a.x >= 2 * vw || a.y < 0 || a.y >= H / 2) {
        std::printf("  out of DMA bounds at (%d,%d): (%d,%d)\n", x, y, a.x, a.y);
        mismatches++;
      }
      seen.insert(std::make_pair(a.x, a.y));
    }
  }

  const int expected = vw * H;
  std::printf("%d panel(s), %dx%d: %d mismatches, %d unique DMA pixels (expected %d) -> %s\n",
              panels, vw, H, mismatches, (int)seen.size(), expected,
              (mismatches == 0 && (int)seen.size() == expected) ? "PASS" : "FAIL");
  return mismatches == 0 && (int)seen.size() == expected;
}

int main() {
  const bool ok1 = run(1);
  const bool ok2 = run(2);
  return (ok1 && ok2) ? 0 : 1;
}
