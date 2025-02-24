#include <iostream>
#include <iomanip> // For output formatting


// FOUR_SCAN_40_80PX_HFARCAN test
struct Coords {
    int x;
    int y;
};

int main() {
    int panel_pixel_base = 16; // Updated pixel base
    Coords coords;

    // Header for easy Excel copying (Pipe-delimited format)
    std::cout << "Input X|Input Y|Output X|Output Y" << std::endl;

    // Loop for testing multiple inputs
    for (int y = 0; y < 40; y += 3) { // Updated y increment to 3
        for (int x = 0; x < 80; x += 10) {
            coords.x = x;
            coords.y = y;

            // Store original coordinates for display
            int input_x = coords.x;
            int input_y = coords.y;

            // Mapping logic
            int panel_local_x = coords.x % 80; // Compensate for chain of panels

            if ((((coords.y) / 10) % 2) ^ ((panel_local_x / panel_pixel_base) % 2)) {
                coords.x += ((coords.x / panel_pixel_base) * panel_pixel_base);
            } else {
                coords.x += (((coords.x / panel_pixel_base) + 1) * panel_pixel_base);
            }

            coords.y = (coords.y % 10) + 10 * ((coords.y / 20) % 2);

            // Output results in pipe-delimited format for easy Excel import
            std::cout << input_x << "|" << input_y << "|" << coords.x << "|" << coords.y << std::endl;
        }
    }

    return 0;
}