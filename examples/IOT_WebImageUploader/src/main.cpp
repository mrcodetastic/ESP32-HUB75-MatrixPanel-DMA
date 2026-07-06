/*******************************************************************************
 * IOT_WebImageUploader
 * ------------------------------------------------------------------------
 * An example for ESP32-HUB75-MatrixPanel-DMA (mrcodetastic) that lets you
 * upload a JPEG from any phone/laptop browser and have it appear on the
 * LED matrix - no re-flashing required.
 *
 * WHAT IT DOES
 *   1. Connects to WiFi.
 *   2. Mounts LittleFS (auto-formats it if it has never been used before).
 *   3. Serves a tiny mobile-friendly web page with a file picker.
 *   4. Accepts an uploaded JPEG over an async HTTP POST, streams it
 *      straight to flash (never buffers the whole file in RAM).
 *   5. Decodes the JPEG block-by-block (MCU by MCU) and downsamples it
 *      on the fly into a small RGB565 framebuffer sized exactly to the
 *      panel resolution - so a 4000x3000 photo costs the same ~10 bytes
 *      of decode overhead per pixel-block as a 64x32 icon. Nothing close
 *      to the source image's full resolution is ever allocated.
 *   6. Pushes the finished framebuffer to the panel in one shot via
 *      drawRGBBitmap(), then flips the DMA back-buffer.
 *
 * WHY A SEPARATE FreeRTOS TASK FOR DECODING?
 *   JPEG decoding with this decoder library is inherently a "pull" loop -
 *   you call JpegDec.read() until it's done, there's no clean way to
 *   pause it mid-block and hand control back to the browser. So instead
 *   of decoding inside loop() (which could make the AsyncWebServer feel
 *   sluggish while a big photo decodes), decoding happens in its own task
 *   pinned to the second CPU core. The web server only ever has to flip a
 *   semaphore - it's back to handling requests immediately.
 *
 * ABOUT GIFs
 *   The brief asked to prioritise a solid, memory-safe JPEG path first -
 *   this sketch does that. Animated GIF support is a materially different
 *   problem (you need to hold a decoded frame + a palette in RAM and keep
 *   re-decoding on a timer), so it's deliberately left out of scope here.
 *   See the "EXTENDING TO GIF" note near the bottom of this file for how
 *   you'd bolt that on with bitbank2/AnimatedGIF.
 *
 * REQUIRED LIBRARIES (install via Library Manager / PlatformIO, exact
 * names in the accompanying README.md):
 *   - ESP32-HUB75-MatrixPanel-DMA   (mrcodetastic)
 *   - ESP32Async/ESPAsyncWebServer
 *   - ESP32Async/AsyncTCP
 *   - JPEGDecoder                   (Bodmer)
 *
 * Tested against: Arduino-ESP32 core 3.x, ESP32-HUB75-MatrixPanel-DMA
 * 3.0.x. Because all three of these libraries evolve, if something here
 * doesn't compile against the version you have installed, check that
 * library's own examples/ folder first - the field/method names below
 * are current as of writing but APIs do shift over time.
 ******************************************************************************/

#include <WiFi.h>
#include <FS.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <JPEGDecoder.h>

// =============================================================================
//  USER CONFIGURATION
// =============================================================================

// ---- WiFi ----
// Prefer hardcoded credentials for an example like this - simple, and there's
// no ambiguity about which network it'll try to join. Swap in WiFiManager
// (https://github.com/tzapu/WiFiManager) if you want a captive-portal setup
// flow instead; just replace setupWiFi() below with your WiFiManager calls.
const char* WIFI_SSID     = "SSID";      // <-- EDIT ME
const char* WIFI_PASSWORD = "PASSWORD";  // <-- EDIT ME

// ---- Panel geometry ----
// Standard 64x32, 1/16 scan indoor panel, single module. Bump PANEL_CHAIN up
// if you've physically daisy-chained multiple panels into one long display.
#define PANEL_RES_X   64
#define PANEL_RES_Y   32
#define PANEL_CHAIN   1

// Effective full display size (handles chaining automatically).
#define DISPLAY_WIDTH  (PANEL_RES_X * PANEL_CHAIN)
#define DISPLAY_HEIGHT (PANEL_RES_Y)

// ---- HUB75 pin mapping ----
// These match the library's own documented ESP32 DevKit defaults. If your
// wiring differs (different dev board, breakout shield, etc.), change these
// to suit - see the library README's wiring section.
#define R1_PIN   25
#define G1_PIN   26
#define B1_PIN   27
#define R2_PIN   14
#define G2_PIN   12
#define B2_PIN   13
#define A_PIN    23
#define B_PIN    19
#define C_PIN    5
#define D_PIN    17
#define E_PIN    32   // Changed from -1. Assign a valid unused GPIO to prevent crash.
#define LAT_PIN  4
#define OE_PIN   15
#define CLK_PIN  16

// ---- Storage ----
#define IMAGE_PATH "/image.jpg"
#define MAX_UPLOAD_BYTES  (512 * 1024)  // 512KB safety cap so a huge photo can't fill the flash partition

// =============================================================================
//  GLOBALS
// =============================================================================

MatrixPanel_I2S_DMA *dma_display = nullptr;

AsyncWebServer server(80);

// RGB565 framebuffer, sized EXACTLY to the panel - e.g. 64x32 = 2048 pixels =
// 4096 bytes. This is the only image-sized buffer this sketch ever allocates,
// regardless of how large the uploaded JPEG is.
uint16_t frameBuffer[DISPLAY_HEIGHT][DISPLAY_WIDTH];

// Signals the render task that a fresh, fully-written image is ready to decode.
SemaphoreHandle_t xImageReadySemaphore = nullptr;
TaskHandle_t renderTaskHandle = nullptr;

// Set during an upload so the completion handler knows whether to report
// success or failure back to the browser. Simple global flag - fine for a
// single-user example sketch like this; a multi-client production server
// would track this per-request instead (e.g. via request->_tempObject).
volatile bool uploadAccepted = false;
volatile bool uploadTooLarge = false;

// =============================================================================
//  WEB PAGE (served entirely from flash - nothing here touches LittleFS)
// =============================================================================

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Matrix Image Uploader</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  body {
    margin: 0;
    min-height: 100vh;
    display: flex;
    align-items: center;
    justify-content: center;
    background: #0d1117;
    color: #e6edf3;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    padding: 24px;
  }
  .card {
    width: 100%;
    max-width: 420px;
    background: #161b22;
    border: 1px solid #30363d;
    border-radius: 14px;
    padding: 28px 24px;
  }
  h1 {
    font-size: 1.25rem;
    margin: 0 0 4px 0;
  }
  p.sub {
    margin: 0 0 22px 0;
    color: #8b949e;
    font-size: 0.9rem;
  }
  input[type="file"] {
    width: 100%;
    padding: 10px;
    background: #0d1117;
    border: 1px dashed #30363d;
    border-radius: 8px;
    color: #e6edf3;
    margin-bottom: 16px;
    font-size: 0.9rem;
  }
  button {
    width: 100%;
    padding: 12px;
    border: none;
    border-radius: 8px;
    background: #238636;
    color: white;
    font-size: 1rem;
    font-weight: 600;
    cursor: pointer;
  }
  button:active { background: #1a6e2a; }
  button:disabled { background: #21462b; color: #8b949e; cursor: not-allowed; }
  #status {
    margin-top: 16px;
    font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
    font-size: 0.85rem;
    color: #8b949e;
    white-space: pre-wrap;
    min-height: 1.2em;
  }
  #status.ok  { color: #3fb950; }
  #status.err { color: #f85149; }
</style>
</head>
<body>
  <div class="card">
    <h1>LED Matrix Uploader</h1>
    <p class="sub">Pick a JPEG, it'll be scaled down and shown on the panel.</p>
    <input type="file" id="fileInput" accept="image/jpeg">
    <button id="uploadBtn" onclick="uploadFile()">Upload &amp; Display</button>
    <div id="status"></div>
  </div>

<script>
function setStatus(msg, cls) {
  const el = document.getElementById('status');
  el.textContent = msg;
  el.className = cls || '';
}

async function uploadFile() {
  const fileInput = document.getElementById('fileInput');
  const btn = document.getElementById('uploadBtn');

  if (!fileInput.files.length) {
    setStatus('Please choose a JPEG file first.', 'err');
    return;
  }

  const file = fileInput.files[0];

  // Basic client-side guard rail - the ESP32 double-checks this too.
  if (!file.type.includes('jpeg') && !file.type.includes('jpg')) {
    setStatus('Only JPEG files are supported.', 'err');
    return;
  }

  const formData = new FormData();
  // The field name here ("file") must match what the /upload handler reads.
  formData.append('file', file, file.name);

  btn.disabled = true;
  setStatus('Uploading ' + file.name + ' ...');

  try {
    // fetch() is async by nature - the page stays responsive while this runs.
    const res = await fetch('/upload', { method: 'POST', body: formData });
    const text = await res.text();

    if (res.ok) {
      setStatus('Upload OK - decoding and rendering on the panel now...', 'ok');
    } else {
      setStatus('Upload rejected (' + res.status + '): ' + text, 'err');
    }
  } catch (err) {
    setStatus('Network error: ' + err.message, 'err');
  } finally {
    btn.disabled = false;
  }
}
</script>
</body>
</html>
)rawliteral";

// =============================================================================
//  MATRIX PANEL SETUP
// =============================================================================

void setupMatrix() {
  // به جای تعریف دستی پین‌ها، فقط سایز پنل را می‌دهیم
  // کتابخانه خودش می‌داند روی S3 از چه پین‌هایی استفاده کند
  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN);

  mxconfig.double_buff = true;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(64);   
  dma_display->clearScreen();
  if (mxconfig.double_buff) dma_display->flipDMABuffer();
  
  Serial.println("[Matrix] Initialized with default S3 pins.");
}
// =============================================================================
//  FILESYSTEM SETUP
// =============================================================================

void setupFilesystem() {
  Serial.println("[FS] Mounting LittleFS...");

  // false = don't auto-format on a failed mount; we do that explicitly below
  // so the two steps ("try mount" / "format then mount") are visible and
  // easy to reason about, exactly as requested.
  if (!LittleFS.begin(false)) {
    Serial.println("[FS] Mount failed - formatting LittleFS...");

    if (!LittleFS.format()) {
      Serial.println("[FS] FATAL: format failed. Halting.");
      while (true) { delay(1000); }
    }

    if (!LittleFS.begin(false)) {
      Serial.println("[FS] FATAL: mount failed even after format. Halting.");
      while (true) { delay(1000); }
    }
  }

  Serial.println("[FS] LittleFS mounted OK.");

  // Note: unlike most LittleFS examples, there is nothing to "upload" into
  // the filesystem ahead of time here - the web page itself lives in flash
  // as a PROGMEM string above, and the only file LittleFS ever holds is
  // whatever image the user uploads at runtime. So you do NOT need to run
  // "Upload Filesystem Image" / build a data/ folder for this sketch.
}

// =============================================================================
//  WIFI SETUP
// =============================================================================

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("[WiFi] Connecting");
  uint32_t startAttempt = millis();

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    if (millis() - startAttempt > 20000) {
      Serial.println("\n[WiFi] Timed out - continuing boot anyway, will keep retrying in the background.");
      break;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] Connected. Open http://%s/ in a browser.\n", WiFi.localIP().toString().c_str());
  }
}

// =============================================================================
//  JPEG DECODE + DOWNSCALE  (the heart of this example)
// =============================================================================

// Decodes /image.jpg from LittleFS directly into frameBuffer, downsampling
// on the fly so the panel-sized output is the ONLY image-sized allocation
// that ever happens - regardless of whether the source photo is 200x150 or
// 4000x3000.
bool decodeAndBufferJPEG() {
  File jpegFile = LittleFS.open(IMAGE_PATH, "r");
  if (!jpegFile) {
    Serial.println("[JPEG] Could not open image file.");
    return false;
  }

  // Clear to black first so if the new image doesn't fill the full frame
  // (shouldn't normally happen since we scale to fill exactly) we don't see
  // leftover pixels from whatever was displayed before.
  memset(frameBuffer, 0, sizeof(frameBuffer));

  // decodeFsFile() accepts a LittleFS fs::File handle directly - the decoder
  // then pulls bytes from it MCU by MCU as needed. It does NOT read the
  // whole file into RAM up front.
  bool decoded = JpegDec.decodeFsFile(jpegFile);

  if (!decoded) {
    Serial.println("[JPEG] Decode failed - not a valid baseline JPEG?");
    jpegFile.close();
    return false;
  }

  const uint32_t imgWidth  = JpegDec.width;
  const uint32_t imgHeight = JpegDec.height;

  Serial.printf("[JPEG] Source %ux%u -> scaling to panel %dx%d\n", imgWidth, imgHeight, DISPLAY_WIDTH, DISPLAY_HEIGHT);

  uint32_t mcuCount = 0;

  // JpegDec.read() pulls and decodes ONE MCU (Minimum Coded Unit - typically
  // an 8x8 or 16x16 pixel block) at a time. JpegDec.pImage then points at
  // that block's decoded RGB565 pixels. This is what keeps memory use flat
  // no matter how big the source photo is - we only ever hold one small
  // block's worth of pixel data at once.
  while (JpegDec.read()) {
    uint16_t *pImg = JpegDec.pImage;

    int mcu_x = JpegDec.MCUx * JpegDec.MCUWidth;
    int mcu_y = JpegDec.MCUy * JpegDec.MCUHeight;
    int mcu_w = JpegDec.MCUWidth;
    int mcu_h = JpegDec.MCUHeight;

    // The image width/height usually isn't an exact multiple of the MCU
    // size, so the last column/row of MCUs is padded by the encoder past
    // the real edge of the photo. Clip so we don't read/scale garbage
    // pixels from that padding.
    if ((mcu_x + mcu_w) > (int)imgWidth)  mcu_w = imgWidth  - mcu_x;
    if ((mcu_y + mcu_h) > (int)imgHeight) mcu_h = imgHeight - mcu_y;
    if (mcu_w <= 0 || mcu_h <= 0) continue;

    for (int row = 0; row < mcu_h; row++) {
      for (int col = 0; col < mcu_w; col++) {

        // --- NEAREST-NEIGHBOUR DOWNSCALE ---
        // We map every source pixel straight onto the small panel grid
        // using pure integer math (no floats - keeps this fast on a
        // low-clock MCU with no FPU acceleration for this kind of loop).
        //
        //   destX = srcX * panelWidth  / sourceWidth
        //   destY = srcY * panelHeight / sourceHeight
        //
        // When shrinking a large photo down to 64x32, many source pixels
        // land on the same destination pixel - we simply let the last one
        // written win. That's a deliberate simplification: true
        // area-averaging downscaling would look marginally better, but
        // costs extra passes and buffering for a 64x32 preview panel where
        // the difference is barely visible. If you want to improve on
        // this later, averaging all source pixels that map to a given
        // destination pixel (instead of overwriting) is the next step up.
        int srcX = mcu_x + col;
        int srcY = mcu_y + row;

        int destX = (srcX * DISPLAY_WIDTH)  / imgWidth;
        int destY = (srcY * DISPLAY_HEIGHT) / imgHeight;

        if (destX >= 0 && destX < DISPLAY_WIDTH &&
            destY >= 0 && destY < DISPLAY_HEIGHT) {
          // pImg is always stride-width JpegDec.MCUWidth internally, even
          // for a clipped edge block - so index using the *nominal* MCU
          // width, not our clipped mcu_w.
          frameBuffer[destY][destX] = pImg[row * JpegDec.MCUWidth + col];
        }
      }
    }

    // Yield every few MCUs so a large/detailed photo's decode loop can't
    // starve the WiFi stack or trip the watchdog. Cheap insurance.
    if ((++mcuCount % 8) == 0) {
      vTaskDelay(1);
    }
  }

  jpegFile.close();
  return true;
}

// Blits the finished framebuffer to the panel in one shot and flips the
// DMA back-buffer, so the whole image appears atomically with no
// half-drawn frame ever visible.
void updateMatrixDisplay() {
  dma_display->drawRGBBitmap(0, 0, (uint16_t *)frameBuffer, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  dma_display->flipDMABuffer();
}

// =============================================================================
//  RENDER TASK  (runs on its own core, decoupled from the web server)
// =============================================================================

void renderTask(void *parameter) {
  for (;;) {
    // Blocks here with zero CPU usage until the upload handler signals a
    // freshly-saved image is ready.
    if (xSemaphoreTake(xImageReadySemaphore, portMAX_DELAY) == pdTRUE) {
      Serial.println("[Render] New image signalled, decoding...");

      if (decodeAndBufferJPEG()) {
        updateMatrixDisplay();
        Serial.println("[Render] Panel updated.");
      } else {
        Serial.println("[Render] Decode failed, previous image left on screen.");
      }
    }
  }
}

// =============================================================================
//  ASYNC WEB SERVER
// =============================================================================

// Streaming upload handler: called repeatedly with chunks of the incoming
// file as they arrive off the network, so the ESP32 never needs to hold
// the whole upload in RAM - each chunk is written straight to flash.
void handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  static File uploadFile;

  if (index == 0) {
    // First chunk of a new upload - (re)open the destination file.
    Serial.printf("[Upload] Start: %s\n", filename.c_str());
    uploadAccepted = false;
    uploadTooLarge = false;

    String lowerName = filename;
    lowerName.toLowerCase();
    if (!lowerName.endsWith(".jpg") && !lowerName.endsWith(".jpeg")) {
      Serial.println("[Upload] Rejected: not a .jpg/.jpeg file.");
      return;  // leave uploadAccepted = false; later chunks/finish will no-op below
    }

    if (LittleFS.exists(IMAGE_PATH)) {
      LittleFS.remove(IMAGE_PATH);
    }
    uploadFile = LittleFS.open(IMAGE_PATH, "w");
    uploadAccepted = uploadFile ? true : false;
  }

  if (!uploadAccepted) {
    return;  // Wrong file type or failed to open - silently drop remaining chunks.
  }

  if ((index + len) > MAX_UPLOAD_BYTES) {
    Serial.println("[Upload] Aborting: file exceeds size cap.");
    uploadTooLarge = true;
    uploadAccepted = false;
    if (uploadFile) uploadFile.close();
    LittleFS.remove(IMAGE_PATH);
    return;
  }

  if (uploadFile) {
    uploadFile.write(data, len);
  }

  if (final) {
    if (uploadFile) uploadFile.close();

    if (uploadAccepted) {
      Serial.printf("[Upload] Complete: %u bytes\n", (unsigned)(index + len));
      xSemaphoreGive(xImageReadySemaphore);  // wake the render task
    }
  }
}

void setupWebServer() {
  // Root page.
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  // Quiet the browser's automatic favicon request.
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(204);
  });

  // Upload endpoint. The final response is chosen based on flags the
  // handleFileUpload() callback set while streaming the file in.
  server.on("/upload", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      if (uploadTooLarge) {
        request->send(413, "text/plain", "File too large.");
      } else if (uploadAccepted) {
        request->send(200, "text/plain", "OK");
      } else {
        request->send(400, "text/plain", "Only .jpg/.jpeg files are accepted.");
      }
    },
    handleFileUpload
  );

  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
  });

  server.begin();
  Serial.println("[HTTP] Async web server started on port 80.");
}

// =============================================================================
//  SETUP / LOOP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== IOT_WebImageUploader booting ===");

  setupMatrix();
  Serial.println("[Matrix] Ready.");
  setupFilesystem();
  Serial.println("[FS] Ready.");
  setupWiFi();
  Serial.println("[WiFi] Ready.");

  xImageReadySemaphore = xSemaphoreCreateBinary();

  // Pin the decode/render task to core 1 (APP CPU). Arduino's WiFi/lwIP
  // stack - and by extension most of AsyncWebServer's background work -
  // leans on core 0, so keeping decoding on the other core keeps a big
  // photo's decode time from ever competing with network handling.
  xTaskCreatePinnedToCore(renderTask, "RenderTask", 8192, nullptr, 2, &renderTaskHandle, 1);

  setupWebServer();

  // If a previous session already left an image on flash, show it again on
  // boot instead of starting on a blank panel.
  if (LittleFS.exists(IMAGE_PATH)) {
    xSemaphoreGive(xImageReadySemaphore);
  }

  Serial.println("=== Setup complete ===");
}

void loop() {
  // Intentionally near-empty. The matrix is refreshed continuously by DMA
  // in the background (no CPU loop required to "keep" the image on
  // screen), the web server runs on its own async task, and rendering
  // happens on renderTask above. We just idle here and let FreeRTOS get on
  // with things.
  vTaskDelay(pdMS_TO_TICKS(1000));
}

// =============================================================================
//  EXTENDING TO GIF (not implemented here - notes only)
// =============================================================================
// The brief prioritised a solid JPEG path first, so animated GIF support is
// left as a documented extension point rather than bolted on half-finished:
//
//   1. Add bitbank2/AnimatedGIF as a dependency.
//   2. Save uploads as /image.gif instead of /image.jpg when the uploaded
//      filename ends in .gif (handleFileUpload already has the filename).
//   3. AnimatedGIF's callback-per-scanline API is a much better fit than
//      JPEGDecoder's MCU model for this - you get one fully decoded 8-bit
//      (or RGB565, depending on config) scanline at a time per frame, which
//      you'd downscale the same nearest-neighbour way as above, write into
//      a second frameBuffer, then call updateMatrixDisplay() once per GIF
//      frame on a timer (GIF frame delay is in the file's metadata).
//   4. The tricky part is memory: unlike a single static JPEG, you need to
//      keep the decoder's per-frame state alive between calls, so plan for
//      a dedicated FreeRTOS task (much like renderTask here) that owns the
//      whole animation loop rather than trying to time it from loop().
