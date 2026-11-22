/**
 * @file ESP32-HUB75-VirtualMatrixPanel_T.hpp
 * @brief Templated Virtual Matrix Panel class for HUB75 displays.
 *
 * Maps logical virtual (x, y) coordinates onto:
 *   1) Multi-panel chains tiled in serpentine or zigzag grids (PANEL_CHAIN_TYPE),
 *   2) Non-standard physical panel scan geometries such as 4-scan (2*W x H/2)
 *      and 8-scan (4*W x H/4) modules, and
 *   3) Optional compile-time integer scaling (ScaleFactor) and runtime rotation.
 *
 * @tparam ChainScanType  Compile-time chaining layout (default: CHAIN_NONE).
 *                        Use CHAIN_RUNTIME to configure chaining dynamically.
 * @tparam ScanPolicy     Mapping policy type inheriting/matching ScanPolicyTraits
 *                        and providing map(VirtualCoords, const PanelGeometry&) const.
 *                        Default: TwoScanMapping.
 * @tparam ScaleFactor    Compile-time pixel zoom factor (>= 1, default: 1).
 */

#ifndef VIRTUAL_MATRIX_PANEL_TEMPLATE_H
#define VIRTUAL_MATRIX_PANEL_TEMPLATE_H

#include <algorithm>
#include <cstdint>
#include <type_traits>

#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"

#ifdef USE_GFX_LITE
  #include "GFX_Lite.h"
#elif !defined(NO_GFX)
  #include "Adafruit_GFX.h"
#endif

// ----------------------------------------------------------------------
// Data structures and enums
// ----------------------------------------------------------------------

/**
 * @brief Lightweight physical/virtual coordinate pair.
 */
struct VirtualCoords {
	int16_t x = 0;
	int16_t y = 0;
};

/**
 * @brief Physical module and virtual grid dimensions passed to ScanPolicy::map().
 */
struct PanelGeometry {
	uint16_t panel_res_x;   ///< Physical width of a single panel module (pixels).
	uint16_t panel_res_y;   ///< Physical height of a single panel module (pixels).
	uint16_t virtual_res_x; ///< Total unrotated virtual width (vmodule_cols * panel_res_x).
	uint16_t virtual_res_y; ///< Total unrotated virtual height (vmodule_rows * panel_res_y).
	uint8_t  vmodule_rows;  ///< Number of vertically stacked panel rows.
	uint8_t  vmodule_cols;  ///< Number of horizontally tiled panel columns.
};

/**
 * @brief Panel chaining layout across a rectangular grid of modules.
 */
enum PANEL_CHAIN_TYPE {
	CHAIN_NONE,              ///< Single panel or simple linear horizontal chain.
	CHAIN_TOP_LEFT_DOWN,     ///< Serpentine starting top-left, going down.
	CHAIN_TOP_RIGHT_DOWN,    ///< Serpentine starting top-right, going down.
	CHAIN_BOTTOM_LEFT_UP,    ///< Serpentine starting bottom-left, going up.
	CHAIN_BOTTOM_RIGHT_UP,   ///< Serpentine starting bottom-right, going up.
	CHAIN_TOP_LEFT_DOWN_ZZ,  ///< Zigzag (non-inverted rows) starting top-left, going down.
	CHAIN_TOP_RIGHT_DOWN_ZZ, ///< Zigzag (non-inverted rows) starting top-right, going down.
	CHAIN_BOTTOM_RIGHT_UP_ZZ,///< Zigzag (non-inverted rows) starting bottom-right, going up.
	CHAIN_BOTTOM_LEFT_UP_ZZ, ///< Zigzag (non-inverted rows) starting bottom-left, going up.
	CHAIN_RUNTIME            ///< Runtime-selectable chain type via setChainType().
};

/**
 * @brief Bitflags controlling FourScanMapping and RuntimeFourScanMapping.
 */
enum FOUR_SCAN_FLAGS : uint8_t {
	FOUR_SCAN_NONE          = 0,
	FOUR_SCAN_SWAP_BLOCKS   = 1 << 0, ///< Swap half order: even bands go to left slot (+0), odd bands to right (+pxbase).
	FOUR_SCAN_XOR_SEGMENTS  = 1 << 1, ///< Flip half slot on every odd pxbase segment across the panel width.
	FOUR_SCAN_REVERSE_LEFT  = 1 << 2, ///< Reverse column order inside each pxbase segment landing in the left slot.
	FOUR_SCAN_REVERSE_RIGHT = 1 << 3, ///< Reverse column order inside each pxbase segment landing in the right slot.

	// Semantic aliases for common zigzag panels
	FOUR_SCAN_ZIGZAG_LEFT    = FOUR_SCAN_REVERSE_LEFT,
	FOUR_SCAN_ZIGZAG_RIGHT   = FOUR_SCAN_REVERSE_RIGHT,
	FOUR_SCAN_ZIGZAG_SM16208 = FOUR_SCAN_SWAP_BLOCKS | FOUR_SCAN_REVERSE_LEFT
};

/**
 * @brief Bitflags controlling EightScanMapping and RuntimeEightScanMapping (4*W x H/4 DMA folding).
 */
enum EIGHT_SCAN_FLAGS : uint8_t {
	EIGHT_SCAN_NONE               = 0,
	EIGHT_SCAN_REVERSE_BANDS      = 1 << 0, ///< Reverse the 4-band slot order within each half (slot = 3 - band).
	EIGHT_SCAN_REVERSE_EVEN_SLOTS = 1 << 1, ///< Reverse X within pxbase slots 0 and 2.
	EIGHT_SCAN_REVERSE_ODD_SLOTS  = 1 << 2, ///< Reverse X within pxbase slots 1 and 3.
	EIGHT_SCAN_INVERT_ROW_IN_BAND = 1 << 3  ///< Reverse row order within each H/8 band (e.g. 1 - (y & 1) for H=16).
};

// ----------------------------------------------------------------------
// Internal helpers
// ----------------------------------------------------------------------
namespace hub75_detail {

template <typename Policy, bool Empty = (std::is_empty<Policy>::value && !std::is_final<Policy>::value)>
struct PolicyHolder : private Policy {
	constexpr PolicyHolder() = default;
	constexpr explicit PolicyHolder(const Policy& p) : Policy(p) {}
	constexpr Policy& get() noexcept { return *this; }
	constexpr const Policy& get() const noexcept { return *this; }
};

template <typename Policy>
struct PolicyHolder<Policy, false> {
	Policy policy;
	constexpr PolicyHolder() = default;
	constexpr explicit PolicyHolder(const Policy& p) : policy(p) {}
	constexpr Policy& get() noexcept { return policy; }
	constexpr const Policy& get() const noexcept { return policy; }
};

static constexpr VirtualCoords applyFourScanMath(
	VirtualCoords c,
	uint16_t panel_res_x,
	uint16_t panel_res_y,
	uint16_t pxbase,
	uint8_t flags,
	uint16_t height_override) noexcept
{
	const int eff_w = (panel_res_x > 0) ? static_cast<int>(panel_res_x) : 1;
	const int raw_h = (height_override > 0) ? static_cast<int>(height_override)
	                                        : static_cast<int>(panel_res_y);
	const int eff_h = (raw_h >= 4) ? raw_h : 4;
	const int base  = (pxbase > 0) ? static_cast<int>(pxbase) : eff_w;

	const int half_h = (eff_h >> 1) >= 2 ? (eff_h >> 1) : 2;
	const int q_h    = (half_h >> 1) >= 1 ? (half_h >> 1) : 1;

	const int panel_idx = c.x / eff_w;
	const int local_x   = c.x % eff_w;
	const int seg       = local_x / base;
	int seg_off         = local_x % base;

	const int block = (c.y / q_h) & 1;
	bool right_half = (block == 0);

	if ((flags & FOUR_SCAN_XOR_SEGMENTS) && ((seg & 1) != 0)) {
		right_half = !right_half;
	}
	if (flags & FOUR_SCAN_SWAP_BLOCKS) {
		right_half = !right_half;
	}

	if ((right_half && (flags & FOUR_SCAN_REVERSE_RIGHT)) ||
	    (!right_half && (flags & FOUR_SCAN_REVERSE_LEFT))) {
		seg_off = (base - 1) - seg_off;
	}

	c.x = static_cast<int16_t>(
		panel_idx * (eff_w << 1) +
		seg * (base << 1) +
		(right_half ? base : 0) +
		seg_off);
	c.y = static_cast<int16_t>((c.y / half_h) * q_h + (c.y % q_h));
	return c;
}

static constexpr VirtualCoords applyEightScanMath(
	VirtualCoords c,
	uint16_t panel_res_x,
	uint16_t panel_res_y,
	uint16_t pxbase,
	uint8_t flags,
	uint16_t height_override) noexcept
{
	const int eff_w = (panel_res_x > 0) ? static_cast<int>(panel_res_x) : 1;
	const int raw_h = (height_override > 0) ? static_cast<int>(height_override)
	                                        : static_cast<int>(panel_res_y);
	const int eff_h = (raw_h >= 8) ? raw_h : 8;
	const int base  = (pxbase > 0) ? static_cast<int>(pxbase) : eff_w;

	const int half_h = (eff_h >> 1) >= 4 ? (eff_h >> 1) : 4;
	const int band_h = (half_h >> 2) >= 1 ? (half_h >> 2) : 1;

	const int panel_idx = c.x / eff_w;
	const int local_x   = c.x % eff_w;
	const int seg       = local_x / base;
	int seg_off         = local_x % base;

	int slot = (c.y / band_h) & 3;
	if (flags & EIGHT_SCAN_REVERSE_BANDS) {
		slot = 3 - slot;
	}

	const bool rev = ((slot & 1) == 0)
	               ? ((flags & EIGHT_SCAN_REVERSE_EVEN_SLOTS) != 0)
	               : ((flags & EIGHT_SCAN_REVERSE_ODD_SLOTS) != 0);
	if (rev) {
		seg_off = (base - 1) - seg_off;
	}

	int row_in_band = c.y % band_h;
	if (flags & EIGHT_SCAN_INVERT_ROW_IN_BAND) {
		row_in_band = (band_h - 1) - row_in_band;
	}

	c.x = static_cast<int16_t>(
		panel_idx * (eff_w << 2) +
		seg * (base << 2) +
		slot * base +
		seg_off);
	c.y = static_cast<int16_t>((c.y / half_h) * band_h + row_in_band);
	return c;
}

#ifdef USE_GFX_LITE
using GfxBase = GFX;
#elif !defined(NO_GFX)
using GfxBase = Adafruit_GFX;
#else
struct StandaloneBase {
	constexpr StandaloneBase() = default;
	constexpr StandaloneBase(int16_t, int16_t) noexcept {}
};
using GfxBase = StandaloneBase;
#endif

} // namespace hub75_detail

// ----------------------------------------------------------------------
// Built-in Scan Mapping Policies
// ----------------------------------------------------------------------

/**
 * @brief Base traits providing DMA buffer geometry multipliers and makeDMAConfig() helpers.
 *
 * Custom user mapping policies should inherit from ScanPolicyTraits<WMult, HDiv>
 * (typically <1, 1> for linear, <2, 2> for 4-scan, or <4, 4> for 8-scan) and
 * implement `VirtualCoords map(VirtualCoords coords, const PanelGeometry& geom) const`.
 */
template <uint8_t DmaWidthMult, uint8_t DmaHeightDiv>
struct ScanPolicyTraits {
	static_assert(DmaWidthMult >= 1, "DmaWidthMult must be >= 1");
	static_assert(DmaHeightDiv >= 1, "DmaHeightDiv must be >= 1");

	static constexpr uint8_t dmaWidthMult = DmaWidthMult;
	static constexpr uint8_t dmaHeightDiv = DmaHeightDiv;

	static inline HUB75_I2S_CFG makeDMAConfig(
		uint16_t panel_res_x,
		uint16_t panel_res_y,
		uint16_t chain_length = 1)
	{
		return HUB75_I2S_CFG(
			static_cast<uint16_t>(panel_res_x * DmaWidthMult),
			static_cast<uint16_t>(panel_res_y / DmaHeightDiv),
			chain_length);
	}

	static inline HUB75_I2S_CFG makeDMAConfig(
		uint16_t panel_res_x,
		uint16_t panel_res_y,
		uint16_t chain_length,
		const HUB75_I2S_CFG::i2s_pins& pinmap)
	{
		return HUB75_I2S_CFG(
			static_cast<uint16_t>(panel_res_x * DmaWidthMult),
			static_cast<uint16_t>(panel_res_y / DmaHeightDiv),
			chain_length,
			pinmap);
	}
};

/**
 * @brief Default two-scan linear identity mapping (1/8, 1/16 on 32px, 1/32 on 64px).
 */
struct TwoScanMapping : ScanPolicyTraits<1, 1> {
	constexpr VirtualCoords map(VirtualCoords coords, const PanelGeometry&) const noexcept {
		return coords;
	}
};

/**
 * @brief Parameterized compile-time 4-scan mapping (DMA folded 2*W x H/2).
 *
 * Covers ~95% of 1/4, 1/5, 1/6, 1/8, 1/10, and 1/16 ("16S") outdoor/indoor
 * non-two-scan panels at zero runtime overhead.
 *
 * @tparam PxBase      Segment width in pixels (e.g., 1, 4, 8, 16, 32). 0 uses panel_res_x.
 * @tparam Flags       Bitwise OR of FOUR_SCAN_FLAGS (default: FOUR_SCAN_NONE).
 * @tparam PanelHeight Override module height in rows (0 auto-reads geom.panel_res_y).
 */
template <uint16_t PxBase = 0, uint8_t Flags = FOUR_SCAN_NONE, uint16_t PanelHeight = 0>
struct FourScanMapping : ScanPolicyTraits<2, 2> {
	static constexpr uint16_t pxbase       = PxBase;
	static constexpr uint8_t  flags        = Flags;
	static constexpr uint16_t panel_height = PanelHeight;

	constexpr VirtualCoords map(VirtualCoords coords, const PanelGeometry& geom) const noexcept {
		return hub75_detail::applyFourScanMath(
			coords, geom.panel_res_x, geom.panel_res_y, PxBase, Flags, PanelHeight);
	}
};

/**
 * @brief Special 64x64 4-scan bit-swapped mapping (e.g., ICN2037BP outdoor 64x64 1/16S, issue #345).
 *
 * Folds 64 rows in 8-row blocks after swapping address bits 3 and 4, unlike standard
 * 16S panels (such as MBI5124) which fold in 16-row blocks via FourScanMapping<>.
 */
template <uint16_t PxBase = 0, uint8_t Flags = FOUR_SCAN_NONE>
struct FourScan64pxBitSwapMapping : ScanPolicyTraits<2, 2> {
	static constexpr uint16_t pxbase = PxBase;
	static constexpr uint8_t  flags  = Flags;

	constexpr VirtualCoords map(VirtualCoords coords, const PanelGeometry& geom) const noexcept {
		if ((coords.y & 8) != ((coords.y & 16) >> 1)) {
			coords.y = static_cast<int16_t>(
				((coords.y & 0b11000) ^ 0b11000) + (coords.y & 0b11100111));
		}
		return hub75_detail::applyFourScanMath(
			coords, geom.panel_res_x, geom.panel_res_y, PxBase, Flags, 32);
	}
};

/**
 * @brief Parameterized compile-time 8-scan mapping (DMA folded 4*W x H/4).
 *
 * Supports panels lighting 8 rows in parallel per refresh step (e.g., 32x16 1/2 scan
 * and 32x32 1/4 scan modules, see Discussions #819, #909, Issues #850, #967).
 *
 * @tparam PxBase      Segment width in pixels (e.g., 8, 16, 32). 0 uses panel_res_x.
 * @tparam Flags       Bitwise OR of EIGHT_SCAN_FLAGS (default: EIGHT_SCAN_NONE).
 * @tparam PanelHeight Override module height in rows (0 auto-reads geom.panel_res_y).
 */
template <uint16_t PxBase = 0, uint8_t Flags = EIGHT_SCAN_NONE, uint16_t PanelHeight = 0>
struct EightScanMapping : ScanPolicyTraits<4, 4> {
	static constexpr uint16_t pxbase       = PxBase;
	static constexpr uint8_t  flags        = Flags;
	static constexpr uint16_t panel_height = PanelHeight;

	constexpr VirtualCoords map(VirtualCoords coords, const PanelGeometry& geom) const noexcept {
		return hub75_detail::applyEightScanMath(
			coords, geom.panel_res_x, geom.panel_res_y, PxBase, Flags, PanelHeight);
	}
};

/**
 * @brief Configuration struct for RuntimeFourScanMapping.
 */
struct FourScanConfig {
	uint16_t pxbase       = 0;              ///< Segment width in pixels (0 = use panel_res_x).
	uint8_t  flags        = FOUR_SCAN_NONE; ///< Bitwise OR of FOUR_SCAN_FLAGS.
	uint16_t panel_height = 0;              ///< Override logical height (0 = use panel_res_y).
	bool     bitswap_64px = false;          ///< Enable #345 ICN2037BP 8-row bit-swap for 64px panels.
};

/**
 * @brief Runtime-configurable 4-scan policy (useful for diagnostic sketches like Pixel_Mapping_Test).
 */
struct RuntimeFourScanMapping : ScanPolicyTraits<2, 2> {
	FourScanConfig config;

	constexpr RuntimeFourScanMapping() = default;
	constexpr RuntimeFourScanMapping(const FourScanConfig& cfg) noexcept : config(cfg) {}

	constexpr VirtualCoords map(VirtualCoords coords, const PanelGeometry& geom) const noexcept {
		if (config.bitswap_64px) {
			if ((coords.y & 8) != ((coords.y & 16) >> 1)) {
				coords.y = static_cast<int16_t>(
					((coords.y & 0b11000) ^ 0b11000) + (coords.y & 0b11100111));
			}
			return hub75_detail::applyFourScanMath(
				coords, geom.panel_res_x, geom.panel_res_y, config.pxbase, config.flags, 32);
		}
		return hub75_detail::applyFourScanMath(
			coords, geom.panel_res_x, geom.panel_res_y, config.pxbase, config.flags, config.panel_height);
	}
};

/**
 * @brief Configuration struct for RuntimeEightScanMapping.
 */
struct EightScanConfig {
	uint16_t pxbase       = 0;               ///< Segment width in pixels (0 = use panel_res_x).
	uint8_t  flags        = EIGHT_SCAN_NONE; ///< Bitwise OR of EIGHT_SCAN_FLAGS.
	uint16_t panel_height = 0;               ///< Override logical height (0 = use panel_res_y).
};

/**
 * @brief Runtime-configurable 8-scan policy (4*W x H/4 DMA folding).
 */
struct RuntimeEightScanMapping : ScanPolicyTraits<4, 4> {
	EightScanConfig config;

	constexpr RuntimeEightScanMapping() = default;
	constexpr RuntimeEightScanMapping(const EightScanConfig& cfg) noexcept : config(cfg) {}

	constexpr VirtualCoords map(VirtualCoords coords, const PanelGeometry& geom) const noexcept {
		return hub75_detail::applyEightScanMath(
			coords, geom.panel_res_x, geom.panel_res_y, config.pxbase, config.flags, config.panel_height);
	}
};

// ----------------------------------------------------------------------
// Convenience aliases for common panel heights & confirmed issue presets
// ----------------------------------------------------------------------
template <uint16_t PxBase = 0, uint8_t Flags = FOUR_SCAN_NONE>
using FourScan16px = FourScanMapping<PxBase, Flags, 16>;

template <uint16_t PxBase = 0, uint8_t Flags = FOUR_SCAN_NONE>
using FourScan32px = FourScanMapping<PxBase, Flags, 32>;

template <uint16_t PxBase = 0, uint8_t Flags = FOUR_SCAN_NONE>
using FourScan40px = FourScanMapping<PxBase, Flags, 40>;

using FourScan64_16S          = FourScanMapping<0, FOUR_SCAN_NONE, 64>;
using FourScanP8_40x20        = FourScanMapping<4, FOUR_SCAN_SWAP_BLOCKS | FOUR_SCAN_XOR_SEGMENTS, 20>;
using FourScan80x40_ZigZag8   = FourScanMapping<8, FOUR_SCAN_ZIGZAG_SM16208, 40>;
using FourScan80x40_HFarcan   = FourScanMapping<16, FOUR_SCAN_XOR_SEGMENTS, 40>;
using EightScan32x16_819      = EightScanMapping<8, EIGHT_SCAN_REVERSE_EVEN_SLOTS | EIGHT_SCAN_INVERT_ROW_IN_BAND, 16>;
using EightScan32x16_SM16208  = EightScanMapping<8, EIGHT_SCAN_REVERSE_BANDS | EIGHT_SCAN_REVERSE_ODD_SLOTS | EIGHT_SCAN_INVERT_ROW_IN_BAND, 16>;

// ----------------------------------------------------------------------
// Extended DMA Config Wrapper (avoids repeating panel/grid dimensions)
// ----------------------------------------------------------------------
/**
 * @brief Lightweight HUB75_I2S_CFG extension storing physical panel and grid sizes.
 *
 * Returned by VirtualMatrixPanel_T::makeDMAConfig() so sketches can write:
 * @code
 * auto mxconfig = VirtualDisplay::makeDMAConfig(64, 32, 2, 2);
 * auto *dma = new MatrixPanel_I2S_DMA(mxconfig);
 * dma->begin();
 * auto *vdisp = new VirtualDisplay(*dma, mxconfig);
 * @endcode
 * Implicitly slices cleanly to HUB75_I2S_CFG when stored explicitly as HUB75_I2S_CFG.
 */
struct VirtualDMAConfig : public HUB75_I2S_CFG {
	uint16_t panel_res_x;
	uint16_t panel_res_y;
	uint16_t vmodule_rows;
	uint16_t vmodule_cols;

	VirtualDMAConfig(
		const HUB75_I2S_CFG& cfg,
		uint16_t _panel_res_x,
		uint16_t _panel_res_y,
		uint16_t _vmodule_rows = 1,
		uint16_t _vmodule_cols = 1)
	  : HUB75_I2S_CFG(cfg),
		panel_res_x(_panel_res_x),
		panel_res_y(_panel_res_y),
		vmodule_rows(_vmodule_rows),
		vmodule_cols(_vmodule_cols) {}
};

// ----------------------------------------------------------------------
// VirtualMatrixPanel_T Declaration
// ----------------------------------------------------------------------
template <PANEL_CHAIN_TYPE ChainScanType = CHAIN_NONE,
          class ScanPolicy = TwoScanMapping,
          int ScaleFactor = 1>
class VirtualMatrixPanel_T : public hub75_detail::GfxBase {
	static_assert(ScaleFactor >= 1, "ScaleFactor must be >= 1");

public:
	static constexpr PANEL_CHAIN_TYPE chain_type   = ChainScanType;
	static constexpr int              scale_factor = ScaleFactor;
	using scan_policy_type                         = ScanPolicy;

	/**
	 * @brief Derive a matching VirtualDMAConfig (compatible with HUB75_I2S_CFG)
	 *        from physical module geometry and grid size.
	 */
	static inline VirtualDMAConfig makeDMAConfig(
		uint16_t panel_res_x,
		uint16_t panel_res_y,
		uint16_t vmodule_rows = 1,
		uint16_t vmodule_cols = 1)
	{
		return VirtualDMAConfig(
			ScanPolicy::makeDMAConfig(
				panel_res_x, panel_res_y, static_cast<uint16_t>(vmodule_rows * vmodule_cols)),
			panel_res_x, panel_res_y, vmodule_rows, vmodule_cols);
	}

	static inline VirtualDMAConfig makeDMAConfig(
		uint16_t panel_res_x,
		uint16_t panel_res_y,
		uint16_t vmodule_rows,
		uint16_t vmodule_cols,
		const HUB75_I2S_CFG::i2s_pins& pinmap)
	{
		return VirtualDMAConfig(
			ScanPolicy::makeDMAConfig(
				panel_res_x, panel_res_y, static_cast<uint16_t>(vmodule_rows * vmodule_cols), pinmap),
			panel_res_x, panel_res_y, vmodule_rows, vmodule_cols);
	}

	/**
	 * @brief Construct VirtualMatrixPanel_T with explicit grid and module dimensions.
	 */
	VirtualMatrixPanel_T(
		MatrixPanel_I2S_DMA& disp,
		uint16_t _vmodule_rows,
		uint16_t _vmodule_cols,
		uint16_t _panel_res_x,
		uint16_t _panel_res_y,
		const ScanPolicy& scan_policy = ScanPolicy())
	  : hub75_detail::GfxBase(
			static_cast<int16_t>((_vmodule_cols * _panel_res_x) / ScaleFactor),
			static_cast<int16_t>((_vmodule_rows * _panel_res_y) / ScaleFactor)),
		panel_res_x(_panel_res_x),
		panel_res_y(_panel_res_y),
		display(&disp),
		policy_holder_(scan_policy),
		vmodule_rows(static_cast<uint8_t>(_vmodule_rows)),
		vmodule_cols(static_cast<uint8_t>(_vmodule_cols)),
		virtual_res_x(static_cast<uint16_t>(_vmodule_cols * _panel_res_x)),
		virtual_res_y(static_cast<uint16_t>(_vmodule_rows * _panel_res_y)),
		dma_max_x(static_cast<uint16_t>((_panel_res_x * _vmodule_rows * _vmodule_cols) > 0
		                                ? (_panel_res_x * _vmodule_rows * _vmodule_cols - 1)
		                                : 0)),
		_raw_res_x(virtual_res_x),
		_raw_res_y(virtual_res_y),
		_scaled_res_x(static_cast<uint16_t>(virtual_res_x / ScaleFactor)),
		_scaled_res_y(static_cast<uint16_t>(virtual_res_y / ScaleFactor)),
		_rotate(0),
		runtime_chain_type_(CHAIN_NONE)
	{
		dma_config_valid_ = validateDMAConfig();
	}

	/**
	 * @brief Construct directly from VirtualDMAConfig returned by makeDMAConfig()
	 *        without repeating any panel or grid dimensions.
	 */
	VirtualMatrixPanel_T(
		MatrixPanel_I2S_DMA& disp,
		const VirtualDMAConfig& vcfg,
		const ScanPolicy& scan_policy = ScanPolicy())
	  : VirtualMatrixPanel_T(
			disp, vcfg.vmodule_rows, vcfg.vmodule_cols, vcfg.panel_res_x, vcfg.panel_res_y, scan_policy)
	{
		if (!validatePassedCfg(vcfg, false)) {
			dma_config_valid_ = false;
		}
	}

	/**
	 * @brief Construct from a plain HUB75_I2S_CFG (1 row x cfg.chain_length cols;
	 *        panel dimensions derived automatically from DMA width/height and ScanPolicy).
	 */
	VirtualMatrixPanel_T(
		MatrixPanel_I2S_DMA& disp,
		const HUB75_I2S_CFG& cfg,
		const ScanPolicy& scan_policy = ScanPolicy())
	  : VirtualMatrixPanel_T(
			disp, 1, cfg.chain_length, derivePanelResX(cfg), derivePanelResY(cfg), scan_policy)
	{
		if (!validatePassedCfg(cfg, true)) {
			dma_config_valid_ = false;
		}
	}

	/**
	 * @brief Construct from a plain HUB75_I2S_CFG plus explicit 2D grid (rows, cols),
	 *        deriving panel width/height automatically from cfg and ScanPolicy.
	 */
	VirtualMatrixPanel_T(
		MatrixPanel_I2S_DMA& disp,
		const HUB75_I2S_CFG& cfg,
		uint16_t _vmodule_rows,
		uint16_t _vmodule_cols,
		const ScanPolicy& scan_policy = ScanPolicy())
	  : VirtualMatrixPanel_T(
			disp, _vmodule_rows, _vmodule_cols, derivePanelResX(cfg), derivePanelResY(cfg), scan_policy)
	{
		if (!validatePassedCfg(cfg, true)) {
			dma_config_valid_ = false;
		}
	}

	/**
	 * @brief Shorthand constructor deriving panel dimensions and 1 x chain_length
	 *        layout directly from MatrixPanel_I2S_DMA::getCfg().
	 */
	explicit VirtualMatrixPanel_T(
		MatrixPanel_I2S_DMA& disp,
		const ScanPolicy& scan_policy = ScanPolicy())
	  : VirtualMatrixPanel_T(
			disp, 1, disp.getCfg().chain_length, derivePanelResX(disp.getCfg()), derivePanelResY(disp.getCfg()), scan_policy)
	{
		if (!validatePassedCfg(disp.getCfg(), true)) {
			dma_config_valid_ = false;
		}
	}

	/**
	 * @brief Grid-only constructor deriving panel width/height directly from MatrixPanel_I2S_DMA::getCfg().
	 */
	VirtualMatrixPanel_T(
		MatrixPanel_I2S_DMA& disp,
		uint16_t _vmodule_rows,
		uint16_t _vmodule_cols,
		const ScanPolicy& scan_policy = ScanPolicy())
	  : VirtualMatrixPanel_T(
			disp, _vmodule_rows, _vmodule_cols, derivePanelResX(disp.getCfg()), derivePanelResY(disp.getCfg()), scan_policy)
	{
		if (!validatePassedCfg(disp.getCfg(), true)) {
			dma_config_valid_ = false;
		}
	}

	// ------------------------------------------------------------------
	// Validation & Policy Accessors
	// ------------------------------------------------------------------
	[[nodiscard]] inline bool isDMAConfigValid() const noexcept {
		return dma_config_valid_;
	}

	inline ScanPolicy& scanPolicy() noexcept {
		return policy_holder_.get();
	}

	inline const ScanPolicy& scanPolicy() const noexcept {
		return policy_holder_.get();
	}

	inline void setChainType(PANEL_CHAIN_TYPE chain) noexcept {
		runtime_chain_type_ = chain;
	}

	[[nodiscard]] inline PANEL_CHAIN_TYPE getChainType() const noexcept {
		if constexpr (ChainScanType == CHAIN_RUNTIME) {
			return runtime_chain_type_;
		} else {
			return ChainScanType;
		}
	}

	[[nodiscard]] inline MatrixPanel_I2S_DMA* getDisplay() const noexcept {
		return display;
	}

	// ------------------------------------------------------------------
	// Drawing methods
	// ------------------------------------------------------------------
	inline void drawPixel(int16_t x, int16_t y, uint16_t color) {
		drawScaledPixel(x, y, [this, color](int16_t px, int16_t py) {
			display->drawPixel(px, py, color);
		});
	}

	inline void fillScreen(uint16_t color) {
		display->fillScreen(color);
	}

	inline void fillScreenRGB888(uint8_t r, uint8_t g, uint8_t b) {
		display->fillScreenRGB888(r, g, b);
	}

	inline void drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b) {
		drawScaledPixel(x, y, [this, r, g, b](int16_t px, int16_t py) {
			display->drawPixelRGB888(px, py, r, g, b);
		});
	}

#ifdef USE_GFX_LITE
	inline void drawPixel(int16_t x, int16_t y, CRGB color) {
		drawScaledPixel(x, y, [this, color](int16_t px, int16_t py) {
			display->drawPixel(px, py, color);
		});
	}

	inline void fillScreen(CRGB color) {
		display->fillScreen(color);
	}
#endif

#ifndef NO_GFX
	inline void drawDisplayTest() {
		this->setTextColor(display->color565(255, 255, 0));
		const int cell_w = static_cast<int>(panel_res_x) / ScaleFactor;
		const int cell_h = static_cast<int>(panel_res_y) / ScaleFactor;
		if (cell_w <= 0 || cell_h <= 0) {
			return;
		}
		for (int col = 0; col < vmodule_cols; col++) {
			for (int row = 0; row < vmodule_rows; row++) {
				const int start_x  = col * cell_w;
				const int start_y  = row * cell_h;
				const int panel_id = col + (row * vmodule_cols) + 1;
				this->drawRect(start_x, start_y, cell_w, cell_h, this->color565(0, 255, 0));
				this->setCursor(start_x + cell_w / 2 - 2, start_y + cell_h / 2 - 4);
				this->print(panel_id);
			}
		}
	}

	inline void drawDisplayTestDMA() {
		// Writes directly to the underlying DMA buffer (two-scan linear layouts only).
		this->display->setTextColor(this->display->color565(255, 255, 0));
		this->display->setTextSize(1);
		const int total_panels = static_cast<int>(vmodule_cols) * vmodule_rows;
		for (int panel = 0; panel < total_panels; panel++) {
			const int top_left_x = panel * panel_res_x;
			this->display->drawRect(top_left_x, 0, panel_res_x, panel_res_y, this->display->color565(0, 255, 0));
			this->display->setCursor((panel * panel_res_x) + 6, panel_res_y - 12);
			this->display->print(total_panels - panel);
		}
	}
#endif

	inline void clearScreen() { display->clearScreen(); }
	inline uint16_t color444(uint8_t r, uint8_t g, uint8_t b) { return display->color444(r, g, b); }
	inline uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return display->color565(r, g, b); }
	inline void flipDMABuffer() { display->flipDMABuffer(); }

	// ------------------------------------------------------------------
	// Rotation (runtime)
	// ------------------------------------------------------------------
	inline void setRotation(uint8_t rotate) {
		_rotate = static_cast<uint8_t>(rotate & 3);
		switch (_rotate) {
			case 0:
			case 2:
				_raw_res_x = virtual_res_x;
				_raw_res_y = virtual_res_y;
				break;
			case 1:
			case 3:
				_raw_res_x = virtual_res_y;
				_raw_res_y = virtual_res_x;
				break;
		}
		_scaled_res_x = static_cast<uint16_t>(_raw_res_x / ScaleFactor);
		_scaled_res_y = static_cast<uint16_t>(_raw_res_y / ScaleFactor);

#ifndef NO_GFX
		this->rotation = _rotate;
		this->_width   = static_cast<int16_t>(_scaled_res_x);
		this->_height  = static_cast<int16_t>(_scaled_res_y);
#endif
	}

	[[nodiscard]] inline uint8_t getRotation() const noexcept {
		return _rotate;
	}

#ifdef NO_GFX
	[[nodiscard]] inline uint16_t width()  const noexcept { return _scaled_res_x; }
	[[nodiscard]] inline uint16_t height() const noexcept { return _scaled_res_y; }
#endif

	// ------------------------------------------------------------------
	// Coordinate Transformation Pipeline (pure & stateless)
	// ------------------------------------------------------------------
	/**
	 * @brief Map an unscaled rotated virtual coordinate to a physical/DMA coordinate.
	 * @return {-1, -1} if out of bounds.
	 */
	[[nodiscard]] inline VirtualCoords getCoords(int16_t virt_x, int16_t virt_y) const noexcept {
		if (virt_x < 0 || virt_x >= static_cast<int32_t>(_raw_res_x) ||
		    virt_y < 0 || virt_y >= static_cast<int32_t>(_raw_res_y)) {
			return VirtualCoords{-1, -1};
		}

		// 1) Runtime rotation -> unrotated virtual grid [0..virtual_res_x) x [0..virtual_res_y)
		VirtualCoords rotated = applyRotation(virt_x, virt_y);

		// 2) Physical chaining grid -> flattened linear chain [0..panels*panel_res_x) x [0..panel_res_y)
		VirtualCoords chained = applyChain(rotated.x, rotated.y);

		// 3) Per-panel electrical scan policy -> DMA buffer coordinates
		const PanelGeometry geom{
			panel_res_x, panel_res_y, virtual_res_x, virtual_res_y, vmodule_rows, vmodule_cols};
		return policy_holder_.get().map(chained, geom);
	}

	const uint16_t panel_res_x;
	const uint16_t panel_res_y;

private:
	MatrixPanel_I2S_DMA*              display;
	hub75_detail::PolicyHolder<ScanPolicy> policy_holder_;

	uint8_t  vmodule_rows;
	uint8_t  vmodule_cols;
	uint16_t virtual_res_x;
	uint16_t virtual_res_y;
	uint16_t dma_max_x;

	uint16_t _raw_res_x;
	uint16_t _raw_res_y;
	uint16_t _scaled_res_x;
	uint16_t _scaled_res_y;
	uint8_t  _rotate;
	PANEL_CHAIN_TYPE runtime_chain_type_;
	bool     dma_config_valid_ = true;

	template <typename DrawFn>
	inline void drawScaledPixel(int16_t x, int16_t y, DrawFn&& drawFn) const {
		if (x < 0 || x >= static_cast<int32_t>(_scaled_res_x) ||
		    y < 0 || y >= static_cast<int32_t>(_scaled_res_y)) {
			return;
		}
		if constexpr (ScaleFactor > 1) {
			const int16_t base_x = static_cast<int16_t>(x * ScaleFactor);
			const int16_t base_y = static_cast<int16_t>(y * ScaleFactor);
			for (int dx = 0; dx < ScaleFactor; dx++) {
				for (int dy = 0; dy < ScaleFactor; dy++) {
					const VirtualCoords p = getCoords(
						static_cast<int16_t>(base_x + dx),
						static_cast<int16_t>(base_y + dy));
					if (p.x >= 0 && p.y >= 0) {
						drawFn(p.x, p.y);
					}
				}
			}
		} else {
			const VirtualCoords p = getCoords(x, y);
			if (p.x >= 0 && p.y >= 0) {
				drawFn(p.x, p.y);
			}
		}
	}

	[[nodiscard]] inline VirtualCoords applyRotation(int16_t x, int16_t y) const noexcept {
		switch (_rotate) {
			case 1:
				return VirtualCoords{y, static_cast<int16_t>(virtual_res_y - 1 - x)};
			case 2:
				return VirtualCoords{
					static_cast<int16_t>(virtual_res_x - 1 - x),
					static_cast<int16_t>(virtual_res_y - 1 - y)};
			case 3:
				return VirtualCoords{static_cast<int16_t>(virtual_res_x - 1 - y), x};
			default:
				return VirtualCoords{x, y};
		}
	}

	[[nodiscard]] inline VirtualCoords applyChainWithMode(int16_t virt_x, int16_t virt_y, PANEL_CHAIN_TYPE mode) const noexcept {
		int row = virt_y / panel_res_y;
		switch (mode) {
			case CHAIN_TOP_RIGHT_DOWN:
				if ((row & 1) == 1) {
					return VirtualCoords{
						static_cast<int16_t>(dma_max_x - virt_x - (row * virtual_res_x)),
						static_cast<int16_t>(panel_res_y - 1 - (virt_y % panel_res_y))};
				}
				return VirtualCoords{
					static_cast<int16_t>(((vmodule_rows - (row + 1)) * virtual_res_x) + virt_x),
					static_cast<int16_t>(virt_y % panel_res_y)};

			case CHAIN_TOP_RIGHT_DOWN_ZZ:
			case CHAIN_TOP_LEFT_DOWN_ZZ:
				return VirtualCoords{
					static_cast<int16_t>(((vmodule_rows - (row + 1)) * virtual_res_x) + virt_x),
					static_cast<int16_t>(virt_y % panel_res_y)};

			case CHAIN_TOP_LEFT_DOWN:
				if ((row & 1) == 0) {
					return VirtualCoords{
						static_cast<int16_t>(dma_max_x - virt_x - (row * virtual_res_x)),
						static_cast<int16_t>(panel_res_y - 1 - (virt_y % panel_res_y))};
				}
				return VirtualCoords{
					static_cast<int16_t>(((vmodule_rows - (row + 1)) * virtual_res_x) + virt_x),
					static_cast<int16_t>(virt_y % panel_res_y)};

			case CHAIN_BOTTOM_LEFT_UP:
				row = vmodule_rows - row - 1;
				if ((row & 1) == 1) {
					return VirtualCoords{
						static_cast<int16_t>(((vmodule_rows - (row + 1)) * virtual_res_x) + virt_x),
						static_cast<int16_t>(virt_y % panel_res_y)};
				}
				return VirtualCoords{
					static_cast<int16_t>(dma_max_x - (row * virtual_res_x) - virt_x),
					static_cast<int16_t>(panel_res_y - 1 - (virt_y % panel_res_y))};

			case CHAIN_BOTTOM_LEFT_UP_ZZ:
			case CHAIN_BOTTOM_RIGHT_UP_ZZ:
				row = vmodule_rows - row - 1;
				return VirtualCoords{
					static_cast<int16_t>(((vmodule_rows - (row + 1)) * virtual_res_x) + virt_x),
					static_cast<int16_t>(virt_y % panel_res_y)};

			case CHAIN_BOTTOM_RIGHT_UP:
				row = vmodule_rows - row - 1;
				if ((row & 1) == 0) {
					return VirtualCoords{
						static_cast<int16_t>(((vmodule_rows - (row + 1)) * virtual_res_x) + virt_x),
						static_cast<int16_t>(virt_y % panel_res_y)};
				}
				return VirtualCoords{
					static_cast<int16_t>(dma_max_x - (row * virtual_res_x) - virt_x),
					static_cast<int16_t>(panel_res_y - 1 - (virt_y % panel_res_y))};

			case CHAIN_NONE:
			default:
				return VirtualCoords{virt_x, virt_y};
		}
	}

	[[nodiscard]] inline VirtualCoords applyChain(int16_t virt_x, int16_t virt_y) const noexcept {
		if constexpr (ChainScanType == CHAIN_RUNTIME) {
			return applyChainWithMode(virt_x, virt_y, runtime_chain_type_);
		} else if constexpr (ChainScanType == CHAIN_TOP_RIGHT_DOWN) {
			const int row = virt_y / panel_res_y;
			if ((row & 1) == 1) {
				return VirtualCoords{
					static_cast<int16_t>(dma_max_x - virt_x - (row * virtual_res_x)),
					static_cast<int16_t>(panel_res_y - 1 - (virt_y % panel_res_y))};
			}
			return VirtualCoords{
				static_cast<int16_t>(((vmodule_rows - (row + 1)) * virtual_res_x) + virt_x),
				static_cast<int16_t>(virt_y % panel_res_y)};
		} else if constexpr (ChainScanType == CHAIN_TOP_RIGHT_DOWN_ZZ || ChainScanType == CHAIN_TOP_LEFT_DOWN_ZZ) {
			const int row = virt_y / panel_res_y;
			return VirtualCoords{
				static_cast<int16_t>(((vmodule_rows - (row + 1)) * virtual_res_x) + virt_x),
				static_cast<int16_t>(virt_y % panel_res_y)};
		} else if constexpr (ChainScanType == CHAIN_TOP_LEFT_DOWN) {
			const int row = virt_y / panel_res_y;
			if ((row & 1) == 0) {
				return VirtualCoords{
					static_cast<int16_t>(dma_max_x - virt_x - (row * virtual_res_x)),
					static_cast<int16_t>(panel_res_y - 1 - (virt_y % panel_res_y))};
			}
			return VirtualCoords{
				static_cast<int16_t>(((vmodule_rows - (row + 1)) * virtual_res_x) + virt_x),
				static_cast<int16_t>(virt_y % panel_res_y)};
		} else if constexpr (ChainScanType == CHAIN_BOTTOM_LEFT_UP) {
			const int row = vmodule_rows - (virt_y / panel_res_y) - 1;
			if ((row & 1) == 1) {
				return VirtualCoords{
					static_cast<int16_t>(((vmodule_rows - (row + 1)) * virtual_res_x) + virt_x),
					static_cast<int16_t>(virt_y % panel_res_y)};
			}
			return VirtualCoords{
				static_cast<int16_t>(dma_max_x - (row * virtual_res_x) - virt_x),
				static_cast<int16_t>(panel_res_y - 1 - (virt_y % panel_res_y))};
		} else if constexpr (ChainScanType == CHAIN_BOTTOM_LEFT_UP_ZZ || ChainScanType == CHAIN_BOTTOM_RIGHT_UP_ZZ) {
			const int row = vmodule_rows - (virt_y / panel_res_y) - 1;
			return VirtualCoords{
				static_cast<int16_t>(((vmodule_rows - (row + 1)) * virtual_res_x) + virt_x),
				static_cast<int16_t>(virt_y % panel_res_y)};
		} else if constexpr (ChainScanType == CHAIN_BOTTOM_RIGHT_UP) {
			const int row = vmodule_rows - (virt_y / panel_res_y) - 1;
			if ((row & 1) == 0) {
				return VirtualCoords{
					static_cast<int16_t>(((vmodule_rows - (row + 1)) * virtual_res_x) + virt_x),
					static_cast<int16_t>(virt_y % panel_res_y)};
			}
			return VirtualCoords{
				static_cast<int16_t>(dma_max_x - (row * virtual_res_x) - virt_x),
				static_cast<int16_t>(panel_res_y - 1 - (virt_y % panel_res_y))};
		} else { // CHAIN_NONE
			return VirtualCoords{virt_x, virt_y};
		}
	}

	inline bool validateDMAConfig() const {
		if (vmodule_rows == 0 || vmodule_cols == 0 || panel_res_x == 0 || panel_res_y == 0) {
			ESP_LOGE("VirtualMatrixPanel_T",
			         "Invalid grid/panel dimensions: rows=%u cols=%u panel=%ux%u",
			         vmodule_rows, vmodule_cols, panel_res_x, panel_res_y);
			return false;
		}
		if ((panel_res_y % ScanPolicy::dmaHeightDiv) != 0) {
			ESP_LOGE("VirtualMatrixPanel_T",
			         "panel_res_y (%u) is not divisible by ScanPolicy::dmaHeightDiv (%u)",
			         panel_res_y, ScanPolicy::dmaHeightDiv);
			return false;
		}

		const HUB75_I2S_CFG& cfg = display->getCfg();
		const uint32_t exp_dma_total_w =
			static_cast<uint32_t>(panel_res_x) * ScanPolicy::dmaWidthMult * vmodule_rows * vmodule_cols;
		const uint16_t exp_dma_h = static_cast<uint16_t>(panel_res_y / ScanPolicy::dmaHeightDiv);
		const uint32_t actual_dma_total_w =
			static_cast<uint32_t>(cfg.mx_width) * cfg.chain_length;

		if (actual_dma_total_w < exp_dma_total_w ||
		    (ScanPolicy::dmaHeightDiv > 1 && cfg.mx_height != exp_dma_h) ||
		    (ScanPolicy::dmaHeightDiv == 1 && cfg.mx_height < exp_dma_h)) {
			ESP_LOGE("VirtualMatrixPanel_T",
			         "DMA surface mismatch: MatrixPanel_I2S_DMA is %ux%u (chain=%u, total_w=%lu) "
			         "but %ux%u grid of %ux%u panels with DMA fold %ux/%ux needs total_w>=%lu and mx_height=%u. "
			         "Use ScanPolicy::makeDMAConfig(...) when creating HUB75_I2S_CFG.",
			         cfg.mx_width, cfg.mx_height, cfg.chain_length,
			         static_cast<unsigned long>(actual_dma_total_w),
			         vmodule_cols, vmodule_rows, panel_res_x, panel_res_y,
			         ScanPolicy::dmaWidthMult, ScanPolicy::dmaHeightDiv,
			         static_cast<unsigned long>(exp_dma_total_w), exp_dma_h);
			return false;
		}

		if (actual_dma_total_w != exp_dma_total_w || cfg.mx_height != exp_dma_h) {
			ESP_LOGW("VirtualMatrixPanel_T",
			         "DMA surface (%ux%u, chain=%u) differs from nominal folded size (%lu total_w x %u). "
			         "Ignore if intentionally padded (e.g. dummy/skipped driver columns).",
			         cfg.mx_width, cfg.mx_height, cfg.chain_length,
			         static_cast<unsigned long>(exp_dma_total_w), exp_dma_h);
		}
		return true;
	}

	static constexpr uint16_t derivePanelResX(const HUB75_I2S_CFG& cfg) noexcept {
		return static_cast<uint16_t>(cfg.mx_width / ScanPolicy::dmaWidthMult);
	}

	static constexpr uint16_t derivePanelResY(const HUB75_I2S_CFG& cfg) noexcept {
		return static_cast<uint16_t>(cfg.mx_height * ScanPolicy::dmaHeightDiv);
	}

	inline bool validatePassedCfg(const HUB75_I2S_CFG& cfg, bool require_exact_mult) const {
		const HUB75_I2S_CFG& dcfg = display->getCfg();
		if (dcfg.mx_width != cfg.mx_width ||
		    dcfg.mx_height != cfg.mx_height ||
		    dcfg.chain_length != cfg.chain_length) {
			ESP_LOGE("VirtualMatrixPanel_T",
			         "Passed mxconfig (%ux%u, chain=%u) does not match MatrixPanel_I2S_DMA (%ux%u, chain=%u).",
			         cfg.mx_width, cfg.mx_height, cfg.chain_length,
			         dcfg.mx_width, dcfg.mx_height, dcfg.chain_length);
			return false;
		}
		if (require_exact_mult && (cfg.mx_width % ScanPolicy::dmaWidthMult) != 0) {
			ESP_LOGE("VirtualMatrixPanel_T",
			         "DMA mx_width (%u) is not divisible by ScanPolicy::dmaWidthMult (%u).",
			         cfg.mx_width, ScanPolicy::dmaWidthMult);
			return false;
		}
		return true;
	}
};

#endif // VIRTUAL_MATRIX_PANEL_TEMPLATE_H
