// Comprehensive host-side verification suite for the refactored VirtualMatrixPanel_T.
// Compiles directly against ../src/ESP32-HUB75-VirtualMatrixPanel_T.hpp using the
// mock DMA/GFX headers in ./mock_esp/ across all three GFX build configurations:
//
//   g++ -std=c++17 -O2 -Wall -Wextra -Werror -Imock_esp -I../src test_virtual_matrix_t.cpp -o /tmp/test_gfx && /tmp/test_gfx
//   g++ -std=c++17 -O2 -Wall -Wextra -Werror -DUSE_GFX_LITE -Imock_esp -I../src test_virtual_matrix_t.cpp -o /tmp/test_lite && /tmp/test_lite
//   g++ -std=c++17 -O2 -Wall -Wextra -Werror -DNO_GFX -Imock_esp -I../src test_virtual_matrix_t.cpp -o /tmp/test_nogfx && /tmp/test_nogfx

#include <cassert>
#include <cstdio>
#include <set>
#include <utility>
#include <vector>

#include "mock_esp/ESP32-HUB75-MatrixPanel-I2S-DMA.h"
#include "ESP32-HUB75-VirtualMatrixPanel_T.hpp"

// -----------------------------------------------------------------------------
// Compile-time checks (C++17 constexpr, -Winvalid-constexpr clean, EBO)
// -----------------------------------------------------------------------------
static_assert(sizeof(VirtualCoords) == 4, "VirtualCoords must be compact 4-byte pair");

constexpr PanelGeometry kGeom80x40{80, 40, 80, 40, 1, 1};
constexpr VirtualCoords kZz0 = FourScan80x40_ZigZag8{}.map(VirtualCoords{0, 0}, kGeom80x40);
static_assert(kZz0.x == 7 && kZz0.y == 0, "80x40 ZIGZAG8 (0,0) must map to DMA (7,0)");

constexpr VirtualCoords kId0 = TwoScanMapping{}.map(VirtualCoords{15, 7}, kGeom80x40);
static_assert(kId0.x == 15 && kId0.y == 7, "TwoScanMapping must be identity");

struct FinalCustomPolicy final : ScanPolicyTraits<1, 1> {
	constexpr VirtualCoords map(VirtualCoords c, const PanelGeometry& geom) const noexcept {
		const int panel_idx = c.x / geom.panel_res_x;
		const int local_x   = c.x % geom.panel_res_x;
		c.x = static_cast<int16_t>(panel_idx * geom.panel_res_x + (geom.panel_res_x - 1 - local_x));
		return c;
	}
};

static_assert(
	sizeof(VirtualMatrixPanel_T<CHAIN_NONE, TwoScanMapping>) ==
	sizeof(VirtualMatrixPanel_T<CHAIN_NONE, FourScanMapping<8, FOUR_SCAN_SWAP_BLOCKS>>),
	"Stateless policies must occupy 0 extra bytes via PolicyHolder EBO");

// -----------------------------------------------------------------------------
// Historical reference formulas (directly from prior issues, PRs, and testing/)
// -----------------------------------------------------------------------------
static VirtualCoords refP8_40x20(int x, int y) {
	const int panel = x / 40;
	const int col   = x % 40;
	const int is_lower = (y / 5) % 2;
	const int u     = is_lower ? col : col + 4;
	const int dma_x = 16 * (u / 8) + (u % 8) + (is_lower ? 4 : -4);
	return VirtualCoords{
		static_cast<int16_t>(panel * 80 + dma_x),
		static_cast<int16_t>((y % 5) + 5 * (y / 10))};
}

static VirtualCoords refP5_64x32_5px(int x, int y) {
	const int panel = x / 64;
	const int col   = x % 64;
	const int q     = y / 8;
	const int r     = y % 8;
	const int band  = 1 - (q % 2);
	return VirtualCoords{
		static_cast<int16_t>(panel * 128 + 8 * (col / 4) + 4 * band + (col % 4)),
		static_cast<int16_t>(r + 8 * (q / 2))};
}

static VirtualCoords refSM16208ZigZag8(int x, int y) {
	constexpr int base = 8;
	const int panel = x / 80;
	const int lx    = x % 80;
	const int rg    = y / 10;
	const int cg    = lx / base;
	const int cw    = lx % base;
	int nx = panel * 160 + cg * (base * 2) + ((rg % 2 == 0) ? (base - 1 - cw) : (base + cw));
	int ny = (y / 20) * 10 + (y % 10);
	return VirtualCoords{static_cast<int16_t>(nx), static_cast<int16_t>(ny)};
}

static VirtualCoords refHFarcan80x40(int x, int y) {
	constexpr int base = 16;
	const int local_x = x % 80;
	int nx = x;
	if (((y / 10) % 2) ^ ((local_x / base) % 2)) {
		nx += (x / base) * base;
	} else {
		nx += ((x / base) + 1) * base;
	}
	int ny = (y % 10) + 10 * ((y / 20) % 2);
	return VirtualCoords{static_cast<int16_t>(nx), static_cast<int16_t>(ny)};
}

static VirtualCoords ref64x64_16S(int x, int y) {
	const int block = y >> 4;
	int nx = x + (((block & 1) == 0) ? ((x / 64) + 1) * 64 : (x / 64) * 64);
	int ny = ((block >> 1) << 4) + (y & 0x0F);
	return VirtualCoords{static_cast<int16_t>(nx), static_cast<int16_t>(ny)};
}

static VirtualCoords ref64x64_ICN2037BP(int x, int y) {
	if ((y & 8) != ((y & 16) >> 1)) {
		y = (((y & 0b11000) ^ 0b11000) + (y & 0b11100111));
	}
	int nx = x + (((y & 8) == 0) ? ((x / 64) + 1) * 64 : (x / 64) * 64);
	int ny = (y >> 4) * 8 + (y & 0b00000111);
	return VirtualCoords{static_cast<int16_t>(nx), static_cast<int16_t>(ny)};
}

static VirtualCoords refDisc622Part3(int x, int y) {
	constexpr int pxbase = 8;
	int nx = x;
	if ((y & 4) == 0) {
		nx = (x / pxbase) * 2 * pxbase + pxbase + 7 - (x & 0x7);
	} else {
		nx += (x / pxbase) * pxbase;
	}
	int ny = (y >> 3) * 4 + (y & 0x3);
	return VirtualCoords{static_cast<int16_t>(nx), static_cast<int16_t>(ny)};
}

static VirtualCoords refDisc622Reply4(int x, int y) {
	constexpr int pxbase = 8;
	int nx = x;
	if ((y & 4) == 0) {
		nx = (x / pxbase) * 2 * pxbase + 7 - (x & 0x7);
	} else {
		nx += ((x / pxbase) + 1) * pxbase;
	}
	int ny = (y >> 3) * 4 + (y & 0x3);
	return VirtualCoords{static_cast<int16_t>(nx), static_cast<int16_t>(ny)};
}

static VirtualCoords refDisc819_8Scan(int x, int y) {
	constexpr int pxbase = 8;
	const int mult_row = (y / 2) % 4;
	int nx = (x / pxbase) * 4 * pxbase + mult_row * pxbase +
	         ((mult_row % 2) ? (x & 7) : (7 - (x & 7)));
	int ny = (y / 8) * 2 + (1 - (y & 1));
	return VirtualCoords{static_cast<int16_t>(nx), static_cast<int16_t>(ny)};
}

static VirtualCoords refDisc909_8Scan(int x, int y) {
	constexpr int pxbase = 8;
	const int mult_row = 3 - ((y / 2) % 4);
	int nx = (x / pxbase) * 4 * pxbase + mult_row * pxbase +
	         ((mult_row % 2) ? (7 - (x & 7)) : (x & 7));
	int ny = (y / 8) * 2 + (1 - (y & 1));
	return VirtualCoords{static_cast<int16_t>(nx), static_cast<int16_t>(ny)};
}

// -----------------------------------------------------------------------------
// Verification helpers
// -----------------------------------------------------------------------------
template <typename PanelT, typename RefFn>
static void checkExactMatch(const PanelT& panel, int cols, int w, int h, int dma_mult, int dma_div, RefFn ref, const char* name) {
	std::set<std::pair<int16_t, int16_t>> seen;
	const int total_w = cols * w;
	const int dma_w   = total_w * dma_mult;
	const int dma_h   = h / dma_div;

	for (int y = 0; y < h; y++) {
		for (int x = 0; x < total_w; x++) {
			const VirtualCoords got = panel.getCoords(static_cast<int16_t>(x), static_cast<int16_t>(y));
			const VirtualCoords exp = ref(x, y);
			if (got.x != exp.x || got.y != exp.y) {
				std::fprintf(stderr, "FAIL [%s] at (%d,%d): got (%d,%d) expected (%d,%d)\n",
				             name, x, y, got.x, got.y, exp.x, exp.y);
				std::abort();
			}
			assert(got.x >= 0 && got.x < dma_w && got.y >= 0 && got.y < dma_h);
			seen.insert({got.x, got.y});
		}
	}
	assert(static_cast<int>(seen.size()) == total_w * h);
}

static void checkFourScanBijection(int rows, int cols, int w, int h, uint16_t pxbase, uint8_t flags, bool bitswap64 = false) {
	using DispT = VirtualMatrixPanel_T<CHAIN_TOP_RIGHT_DOWN, RuntimeFourScanMapping>;
	HUB75_I2S_CFG cfg = DispT::makeDMAConfig(w, h, rows, cols);
	MatrixPanel_I2S_DMA dma(cfg);
	FourScanConfig sc{pxbase, flags, 0, bitswap64};
	DispT panel(dma, rows, cols, w, h, sc);
	assert(panel.isDMAConfigValid());

	const int total_w = cols * w;
	const int total_h = rows * h;
	const int dma_w   = rows * cols * w * 2;
	const int dma_h   = h / 2;

	std::set<std::pair<int16_t, int16_t>> seen;
	for (int y = 0; y < total_h; y++) {
		for (int x = 0; x < total_w; x++) {
			VirtualCoords p = panel.getCoords(static_cast<int16_t>(x), static_cast<int16_t>(y));
			assert(p.x >= 0 && p.x < dma_w);
			assert(p.y >= 0 && p.y < dma_h);
			seen.insert({p.x, p.y});
		}
	}
	assert(static_cast<int>(seen.size()) == total_w * total_h);
}

static void checkEightScanBijection(int rows, int cols, int w, int h, uint16_t pxbase, uint8_t flags) {
	using DispT = VirtualMatrixPanel_T<CHAIN_BOTTOM_LEFT_UP, RuntimeEightScanMapping>;
	HUB75_I2S_CFG cfg = DispT::makeDMAConfig(w, h, rows, cols);
	MatrixPanel_I2S_DMA dma(cfg);
	EightScanConfig sc{pxbase, flags, 0};
	DispT panel(dma, rows, cols, w, h, sc);
	assert(panel.isDMAConfigValid());

	const int total_w = cols * w;
	const int total_h = rows * h;
	const int dma_w   = rows * cols * w * 4;
	const int dma_h   = h / 4;

	std::set<std::pair<int16_t, int16_t>> seen;
	for (int y = 0; y < total_h; y++) {
		for (int x = 0; x < total_w; x++) {
			VirtualCoords p = panel.getCoords(static_cast<int16_t>(x), static_cast<int16_t>(y));
			assert(p.x >= 0 && p.x < dma_w);
			assert(p.y >= 0 && p.y < dma_h);
			seen.insert({p.x, p.y});
		}
	}
	assert(static_cast<int>(seen.size()) == total_w * total_h);
}

static void testDMAConfigValidation() {
	mock_esp_state::reset_logs();

	// 1) Intentional mistake: non-folded 80x40 DMA config passed to FourScan panel
	HUB75_I2S_CFG bad_cfg(80, 40, 1);
	MatrixPanel_I2S_DMA bad_dma(bad_cfg);
	VirtualMatrixPanel_T<CHAIN_NONE, FourScan80x40_ZigZag8> bad_panel(bad_dma, 1, 1, 80, 40);
	assert(!bad_panel.isDMAConfigValid());
	assert(mock_esp_state::error_count() > 0);

	// 2) Correct config via makeDMAConfig()
	mock_esp_state::reset_logs();
	HUB75_I2S_CFG good_cfg = VirtualMatrixPanel_T<CHAIN_NONE, FourScan80x40_ZigZag8>::makeDMAConfig(80, 40);
	assert(good_cfg.mx_width == 160 && good_cfg.mx_height == 20 && good_cfg.chain_length == 1);
	MatrixPanel_I2S_DMA good_dma(good_cfg);
	VirtualMatrixPanel_T<CHAIN_NONE, FourScan80x40_ZigZag8> good_panel(good_dma, 1, 1, 80, 40);
	assert(good_panel.isDMAConfigValid());
	assert(mock_esp_state::error_count() == 0);
	assert(mock_esp_state::warn_count() == 0);

	// 3) Intentionally padded DMA width (e.g. #719 104x52 using 110 DMA width) -> valid + warning only
	mock_esp_state::reset_logs();
	HUB75_I2S_CFG padded_cfg(110, 52, 1);
	MatrixPanel_I2S_DMA padded_dma(padded_cfg);
	VirtualMatrixPanel_T<CHAIN_NONE, TwoScanMapping> padded_panel(padded_dma, 1, 1, 104, 52);
	assert(padded_panel.isDMAConfigValid());
	assert(mock_esp_state::error_count() == 0);
	assert(mock_esp_state::warn_count() == 1);

	// 4) Large panel width > 255 (uint16_t non-truncation test)
	mock_esp_state::reset_logs();
	HUB75_I2S_CFG wide_cfg = VirtualMatrixPanel_T<CHAIN_NONE, TwoScanMapping>::makeDMAConfig(300, 64);
	MatrixPanel_I2S_DMA wide_dma(wide_cfg);
	VirtualMatrixPanel_T<CHAIN_NONE, TwoScanMapping> wide_panel(wide_dma, 1, 1, 300, 64);
	assert(wide_panel.isDMAConfigValid());
	assert(wide_panel.panel_res_x == 300);
	assert(wide_panel.getCoords(299, 63).x == 299);
	assert(wide_panel.getCoords(299, 63).y == 63);
}

static void testHistoricalEquivalences() {
	// P8 40x20 1/5 scan (1 and 2 chained panels)
	for (uint16_t cols : {1, 2}) {
		using P = FourScanP8_40x20;
		auto cfg = P::makeDMAConfig(40, 20, cols);
		MatrixPanel_I2S_DMA dma(cfg);
		VirtualMatrixPanel_T<CHAIN_NONE, P> panel(dma, 1, cols, 40, 20);
		checkExactMatch(panel, cols, 40, 20, 2, 2, refP8_40x20, "FourScanP8_40x20");

		// Compare runtime variant against compile-time
		RuntimeFourScanMapping rmap({4, FOUR_SCAN_SWAP_BLOCKS | FOUR_SCAN_XOR_SEGMENTS, 0, false});
		VirtualMatrixPanel_T<CHAIN_NONE, RuntimeFourScanMapping> rpanel(dma, 1, cols, 40, 20, rmap);
		checkExactMatch(rpanel, cols, 40, 20, 2, 2, refP8_40x20, "RuntimeFourScan P8_40x20");
	}

	// P5 64x32 5px (HRLOP-P5-1921, 1 and 2 chained panels)
	for (uint16_t cols : {1, 2}) {
		using P = FourScanMapping<4>;
		auto cfg = P::makeDMAConfig(64, 32, cols);
		MatrixPanel_I2S_DMA dma(cfg);
		VirtualMatrixPanel_T<CHAIN_NONE, P> panel(dma, 1, cols, 64, 32);
		checkExactMatch(panel, cols, 64, 32, 2, 2, refP5_64x32_5px, "FourScanMapping<4> 64x32");
	}

	// 80x40 SM16208 ZigZag8 (#964 / #453, 1 and 2 chained panels)
	for (uint16_t cols : {1, 2}) {
		using P = FourScan80x40_ZigZag8;
		auto cfg = P::makeDMAConfig(80, 40, cols);
		MatrixPanel_I2S_DMA dma(cfg);
		VirtualMatrixPanel_T<CHAIN_NONE, P> panel(dma, 1, cols, 80, 40);
		checkExactMatch(panel, cols, 80, 40, 2, 2, refSM16208ZigZag8, "FourScan80x40_ZigZag8");
	}

	// 80x40 HFarcan (#759, 1 and 2 chained panels — note 80/16=5 is odd!)
	for (uint16_t cols : {1, 2}) {
		using P = FourScan80x40_HFarcan;
		auto cfg = P::makeDMAConfig(80, 40, cols);
		MatrixPanel_I2S_DMA dma(cfg);
		VirtualMatrixPanel_T<CHAIN_NONE, P> panel(dma, 1, cols, 80, 40);
		checkExactMatch(panel, cols, 80, 40, 2, 2, refHFarcan80x40, "FourScan80x40_HFarcan");
	}

	// 64x64 16S (#976)
	for (uint16_t cols : {1, 2}) {
		using P = FourScan64_16S;
		auto cfg = P::makeDMAConfig(64, 64, cols);
		MatrixPanel_I2S_DMA dma(cfg);
		VirtualMatrixPanel_T<CHAIN_NONE, P> panel(dma, 1, cols, 64, 64);
		checkExactMatch(panel, cols, 64, 64, 2, 2, ref64x64_16S, "FourScan64_16S");
	}

	// 64x64 ICN2037BP bit-swapped (#345) + runtime variant
	for (uint16_t cols : {1, 2}) {
		using P = FourScan64pxBitSwapMapping<>;
		auto cfg = P::makeDMAConfig(64, 64, cols);
		MatrixPanel_I2S_DMA dma(cfg);
		VirtualMatrixPanel_T<CHAIN_NONE, P> panel(dma, 1, cols, 64, 64);
		checkExactMatch(panel, cols, 64, 64, 2, 2, ref64x64_ICN2037BP, "FourScan64pxBitSwapMapping");

		VirtualMatrixPanel_T<CHAIN_NONE, RuntimeFourScanMapping> rpanel(
			dma, 1, cols, 64, 64, FourScanConfig{0, FOUR_SCAN_NONE, 0, true});
		checkExactMatch(rpanel, cols, 64, 64, 2, 2, ref64x64_ICN2037BP, "Runtime 64px BitSwap");
	}

	// Discussion #622 Part 3 and Reply 4 (32x16 1/4 zigzag variants)
	{
		using P3 = FourScanMapping<8, FOUR_SCAN_REVERSE_RIGHT>;
		auto cfg = P3::makeDMAConfig(32, 16, 1);
		MatrixPanel_I2S_DMA dma(cfg);
		VirtualMatrixPanel_T<CHAIN_NONE, P3> p3(dma, 1, 1, 32, 16);
		checkExactMatch(p3, 1, 32, 16, 2, 2, refDisc622Part3, "#622 Part 3");

		using R4 = FourScanMapping<8, FOUR_SCAN_ZIGZAG_SM16208>;
		VirtualMatrixPanel_T<CHAIN_NONE, R4> r4(dma, 1, 1, 32, 16);
		checkExactMatch(r4, 1, 32, 16, 2, 2, refDisc622Reply4, "#622 Reply 4");
	}

	// EightScan #819 and #909 (32x16 1/2 scan, 1 and 2 chained panels)
	for (uint16_t cols : {1, 2}) {
		using E819 = EightScan32x16_819;
		auto cfg = E819::makeDMAConfig(32, 16, cols);
		assert(cfg.mx_width == 128 && cfg.mx_height == 4);
		MatrixPanel_I2S_DMA dma(cfg);
		VirtualMatrixPanel_T<CHAIN_NONE, E819> p819(dma, 1, cols, 32, 16);
		checkExactMatch(p819, cols, 32, 16, 4, 4, refDisc819_8Scan, "EightScan32x16_819");

		using E909 = EightScan32x16_SM16208;
		VirtualMatrixPanel_T<CHAIN_NONE, E909> p909(dma, 1, cols, 32, 16);
		checkExactMatch(p909, cols, 32, 16, 4, 4, refDisc909_8Scan, "EightScan32x16_SM16208");

		VirtualMatrixPanel_T<CHAIN_NONE, RuntimeEightScanMapping> re909(
			dma, 1, cols, 32, 16,
			EightScanConfig{8, EIGHT_SCAN_REVERSE_BANDS | EIGHT_SCAN_REVERSE_ODD_SLOTS | EIGHT_SCAN_INVERT_ROW_IN_BAND, 0});
		checkExactMatch(re909, cols, 32, 16, 4, 4, refDisc909_8Scan, "RuntimeEightScan #909");
	}
}

template <PANEL_CHAIN_TYPE Mode>
static void checkChainModeBijectionAndRuntime(uint16_t rows, uint16_t cols) {
	using StaticDisp  = VirtualMatrixPanel_T<Mode, FourScanMapping<8, FOUR_SCAN_ZIGZAG_SM16208>>;
	using RuntimeDisp = VirtualMatrixPanel_T<CHAIN_RUNTIME, FourScanMapping<8, FOUR_SCAN_ZIGZAG_SM16208>>;

	const uint16_t w = 40, h = 20;
	auto cfg = StaticDisp::makeDMAConfig(w, h, rows, cols);
	MatrixPanel_I2S_DMA dma(cfg);

	StaticDisp sdisp(dma, rows, cols, w, h);
	RuntimeDisp rdisp(dma, rows, cols, w, h);
	rdisp.setChainType(Mode);
	assert(rdisp.getChainType() == Mode);

	const int total_w = cols * w;
	const int total_h = rows * h;
	const int dma_w   = rows * cols * w * 2;
	const int dma_h   = h / 2;

	std::set<std::pair<int16_t, int16_t>> seen;
	for (int y = 0; y < total_h; y++) {
		for (int x = 0; x < total_w; x++) {
			VirtualCoords ps = sdisp.getCoords(static_cast<int16_t>(x), static_cast<int16_t>(y));
			VirtualCoords pr = rdisp.getCoords(static_cast<int16_t>(x), static_cast<int16_t>(y));
			assert(ps.x == pr.x && ps.y == pr.y);
			assert(ps.x >= 0 && ps.x < dma_w && ps.y >= 0 && ps.y < dma_h);
			seen.insert({ps.x, ps.y});
		}
	}
	assert(static_cast<int>(seen.size()) == total_w * total_h);
}

static void testChainingAndRuntimeChain() {
	checkChainModeBijectionAndRuntime<CHAIN_NONE>(1, 3);
	checkChainModeBijectionAndRuntime<CHAIN_TOP_LEFT_DOWN>(2, 3);
	checkChainModeBijectionAndRuntime<CHAIN_TOP_RIGHT_DOWN>(2, 3);
	checkChainModeBijectionAndRuntime<CHAIN_BOTTOM_LEFT_UP>(2, 3);
	checkChainModeBijectionAndRuntime<CHAIN_BOTTOM_RIGHT_UP>(2, 3);
	checkChainModeBijectionAndRuntime<CHAIN_TOP_LEFT_DOWN_ZZ>(2, 3);
	checkChainModeBijectionAndRuntime<CHAIN_TOP_RIGHT_DOWN_ZZ>(2, 3);
	checkChainModeBijectionAndRuntime<CHAIN_BOTTOM_RIGHT_UP_ZZ>(2, 3);
	checkChainModeBijectionAndRuntime<CHAIN_BOTTOM_LEFT_UP_ZZ>(2, 3);
}

static void testRotationAndScaleFactorAndFinalPolicy() {
	// Test FinalCustomPolicy (proves EBO safety with final user structs) + Rotation 0..3 + ScaleFactor=2
	auto cfg2 = VirtualMatrixPanel_T<CHAIN_NONE, FinalCustomPolicy, 2>::makeDMAConfig(64, 32);
	MatrixPanel_I2S_DMA dma(cfg2);
	VirtualMatrixPanel_T<CHAIN_NONE, FinalCustomPolicy, 2> scaled(dma, 1, 1, 64, 32);

	assert(scaled.isDMAConfigValid());
	assert(scaled.width() == 32 && scaled.height() == 16);
	assert(scaled.getRotation() == 0);

	// Rotation checks (especially critical under NO_GFX!)
	scaled.setRotation(1);
	assert(scaled.getRotation() == 1);
	assert(scaled.width() == 16 && scaled.height() == 32);

	// Draw one scaled pixel at (0, 0) using uint16_t and RGB888 (and CRGB under USE_GFX_LITE)
	dma.clearScreen();
	scaled.drawPixel(0, 0, scaled.color565(255, 0, 0));
	assert(dma.drawn.size() == 4); // 2x2 block!
	// Out-of-bounds pixels must emit 0 DMA writes (never negative coordinates)
	scaled.drawPixel(-1, 0, 0xFFFF);
	scaled.drawPixel(16, 0, 0xFFFF);
	scaled.drawPixel(0, 32, 0xFFFF);
	assert(dma.drawn.size() == 4);

	dma.clearScreen();
	scaled.drawPixelRGB888(1, 2, 10, 20, 30);
	assert(dma.drawn.size() == 4);
	assert(dma.drawn[0].kind == 1 && dma.drawn[0].r == 10 && dma.drawn[0].g == 20 && dma.drawn[0].b == 30);

#ifdef USE_GFX_LITE
	dma.clearScreen();
	scaled.drawPixel(2, 3, CRGB(40, 50, 60));
	assert(dma.drawn.size() == 4);
	assert(dma.drawn[0].kind == 2 && dma.drawn[0].r == 40 && dma.drawn[0].g == 50 && dma.drawn[0].b == 60);
#endif

	// Test ScaleFactor = 3 integer division dimensions (64/3 = 21, 32/3 = 10)
	VirtualMatrixPanel_T<CHAIN_NONE, TwoScanMapping, 3> scaled3(dma, 1, 1, 64, 32);
	assert(scaled3.width() == 21 && scaled3.height() == 10);
	dma.clearScreen();
	scaled3.drawPixel(20, 9, 0x1234);
	assert(dma.drawn.size() == 9); // 3x3 block completely inside 64x32!

	// Verify rotation bijection for all 4 orientations on non-square grid (2x1 of 64x32 = 128x32)
	auto cfg_r = VirtualMatrixPanel_T<CHAIN_TOP_RIGHT_DOWN, FourScanMapping<16>>::makeDMAConfig(64, 32, 1, 2);
	MatrixPanel_I2S_DMA dma_r(cfg_r);
	VirtualMatrixPanel_T<CHAIN_TOP_RIGHT_DOWN, FourScanMapping<16>> disp_r(dma_r, 1, 2, 64, 32);
	for (uint8_t rot = 0; rot < 4; rot++) {
		disp_r.setRotation(rot);
		const int w = disp_r.width();
		const int h = disp_r.height();
		if (rot == 0 || rot == 2) {
			assert(w == 128 && h == 32);
		} else {
			assert(w == 32 && h == 128);
		}
		std::set<std::pair<int16_t, int16_t>> seen;
		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				VirtualCoords p = disp_r.getCoords(static_cast<int16_t>(x), static_cast<int16_t>(y));
				assert(p.x >= 0 && p.x < 256 && p.y >= 0 && p.y < 16);
				seen.insert({p.x, p.y});
			}
		}
		assert(seen.size() == 128u * 32u);
	}
}

static void testFlagSweeps() {
	// Sweep all 16 flag combos across representative heights and pxbases (both 1x1 and 2x3 chained)
	const int four_heights[] = {16, 20, 24, 32, 40, 48, 64};
	for (int h : four_heights) {
		const int w = h * 2;
		for (uint16_t pxb : {uint16_t(1), uint16_t(4), uint16_t(8), uint16_t(16), uint16_t(32), uint16_t(w)}) {
			if (pxb > w || (w % pxb) != 0) continue;
			for (uint8_t flags = 0; flags < 16; flags++) {
				checkFourScanBijection(1, 1, w, h, pxb, flags, false);
				if (h == 16 || h == 20 || h == 32) {
					checkFourScanBijection(2, 3, w, h, pxb, flags, false);
				}
			}
		}
	}
	checkFourScanBijection(2, 2, 64, 64, 16, FOUR_SCAN_SWAP_BLOCKS, true);

	const int eight_heights[] = {16, 32};
	for (int h : eight_heights) {
		const int w = 32;
		for (uint16_t pxb : {uint16_t(1), uint16_t(4), uint16_t(8), uint16_t(32)}) {
			if (pxb > w || (w % pxb) != 0) continue;
			for (uint8_t flags = 0; flags < 16; flags++) {
				checkEightScanBijection(1, 1, w, h, pxb, flags);
				checkEightScanBijection(2, 2, w, h, pxb, flags);
			}
		}
	}
}

static void testMxConfigAndShorthandConstructors() {
	mock_esp_state::reset_logs();

	// A. auto mxconfig (VirtualDMAConfig) on 2x3 grid of 40x20 ZigZag8 (no repeated dims)
	{
		using Disp = VirtualMatrixPanel_T<CHAIN_TOP_RIGHT_DOWN, FourScanMapping<8, FOUR_SCAN_ZIGZAG_SM16208>>;
		auto mxconfig = Disp::makeDMAConfig(40, 20, 2, 3);
		mxconfig.clkphase = false;
		MatrixPanel_I2S_DMA dma(mxconfig);

		Disp p_vcfg(dma, mxconfig);
		Disp p_explicit(dma, 2, 3, 40, 20);
		Disp p_grid(dma, 2, 3);
		HUB75_I2S_CFG plain_cfg = mxconfig; // intentional base slice (as in #645 style)
		Disp p_plain_grid(dma, plain_cfg, 2, 3);

		assert(p_vcfg.isDMAConfigValid());
		assert(p_explicit.isDMAConfigValid());
		assert(p_grid.isDMAConfigValid());
		assert(p_plain_grid.isDMAConfigValid());
		assert(p_vcfg.width() == 120 && p_vcfg.height() == 40);
		assert(p_grid.panel_res_x == 40 && p_grid.panel_res_y == 20);
		assert(p_plain_grid.panel_res_x == 40 && p_plain_grid.panel_res_y == 20);
		assert(mock_esp_state::error_count() == 0);

		for (int y = 0; y < 40; y++) {
			for (int x = 0; x < 120; x++) {
				VirtualCoords a = p_vcfg.getCoords(static_cast<int16_t>(x), static_cast<int16_t>(y));
				VirtualCoords b = p_explicit.getCoords(static_cast<int16_t>(x), static_cast<int16_t>(y));
				VirtualCoords c = p_grid.getCoords(static_cast<int16_t>(x), static_cast<int16_t>(y));
				VirtualCoords d = p_plain_grid.getCoords(static_cast<int16_t>(x), static_cast<int16_t>(y));
				assert(a.x == b.x && a.y == b.y);
				assert(a.x == c.x && a.y == c.y);
				assert(a.x == d.x && a.y == d.y);
			}
		}
	}

	// B. 1x1 & 1x3 shorthand constructors (auto mxconfig, plain HUB75_I2S_CFG, *dma only, + RuntimeFourScanMapping)
	{
		using RuntimeDisp = VirtualMatrixPanel_T<CHAIN_NONE, RuntimeFourScanMapping>;
		auto mxconfig = RuntimeDisp::makeDMAConfig(64, 32, 1, 3);
		MatrixPanel_I2S_DMA dma(mxconfig);
		FourScanConfig fcfg{16, FOUR_SCAN_SWAP_BLOCKS, 0, false};

		RuntimeDisp p_vcfg(dma, mxconfig, fcfg);
		HUB75_I2S_CFG plain_cfg = mxconfig;
		RuntimeDisp p_plain(dma, plain_cfg, fcfg);
		RuntimeDisp p_dma_only(dma, fcfg);
		RuntimeDisp p_grid_only(dma, 1, 3, fcfg);

		assert(p_vcfg.isDMAConfigValid());
		assert(p_plain.isDMAConfigValid());
		assert(p_dma_only.isDMAConfigValid());
		assert(p_grid_only.isDMAConfigValid());
		assert(p_vcfg.width() == 192 && p_vcfg.height() == 32);
		assert(p_dma_only.width() == 192 && p_dma_only.height() == 32);

		VirtualCoords c1 = p_vcfg.getCoords(75, 19);
		VirtualCoords c2 = p_plain.getCoords(75, 19);
		VirtualCoords c3 = p_dma_only.getCoords(75, 19);
		VirtualCoords c4 = p_grid_only.getCoords(75, 19);
		assert(c1.x == c2.x && c1.y == c2.y);
		assert(c1.x == c3.x && c1.y == c3.y);
		assert(c1.x == c4.x && c1.y == c4.y);

		// Also default policy without trailing scan_policy arg
		using Static1x1 = VirtualMatrixPanel_T<CHAIN_NONE, FourScan64_16S>;
		auto cfg1 = Static1x1::makeDMAConfig(64, 64);
		MatrixPanel_I2S_DMA dma1(cfg1);
		Static1x1 p1_auto(dma1);
		Static1x1 p1_vcfg(dma1, cfg1);
		assert(p1_auto.isDMAConfigValid() && p1_vcfg.isDMAConfigValid());
		assert(p1_auto.width() == 64 && p1_auto.height() == 64);
		assert(p1_auto.getCoords(17, 45).x == p1_vcfg.getCoords(17, 45).x);
		assert(p1_auto.getCoords(17, 45).y == p1_vcfg.getCoords(17, 45).y);
	}

	// C. Mismatched mxconfig vs dma_display & odd DMA width on 4-scan auto-derive fail cleanly
	{
		mock_esp_state::reset_logs();
		auto cfg_real   = VirtualMatrixPanel_T<CHAIN_NONE, FourScanMapping<8>>::makeDMAConfig(64, 32);
		auto cfg_other  = VirtualMatrixPanel_T<CHAIN_NONE, FourScanMapping<8>>::makeDMAConfig(32, 16);
		MatrixPanel_I2S_DMA dma_real(cfg_real);
		VirtualMatrixPanel_T<CHAIN_NONE, FourScanMapping<8>> mismatched(dma_real, cfg_other);
		assert(!mismatched.isDMAConfigValid());
		assert(mock_esp_state::error_count() > 0);

		mock_esp_state::reset_logs();
		HUB75_I2S_CFG odd_dma_cfg(63, 16, 1);
		MatrixPanel_I2S_DMA odd_dma(odd_dma_cfg);
		VirtualMatrixPanel_T<CHAIN_NONE, FourScanMapping<>> odd_derived(odd_dma);
		assert(!odd_derived.isDMAConfigValid());
		assert(mock_esp_state::error_count() > 0);
	}
}

int main() {
	testDMAConfigValidation();
	testMxConfigAndShorthandConstructors();
	testHistoricalEquivalences();
	testChainingAndRuntimeChain();
	testRotationAndScaleFactorAndFinalPolicy();
	testFlagSweeps();

	std::printf("PASS: all VirtualMatrixPanel_T host tests passed");
#ifdef USE_GFX_LITE
	std::printf(" [mode=USE_GFX_LITE]\n");
#elif defined(NO_GFX)
	std::printf(" [mode=NO_GFX]\n");
#else
	std::printf(" [mode=Adafruit_GFX]\n");
#endif
	return 0;
}
