#pragma once
#include <cstdarg>
#include <cstdint>
#include <cstdio>

struct MockSerial {
	void begin(unsigned long) {}
	int available() { return 0; }
	int read() { return -1; }
	void print(const char*) {}
	void print(int) {}
	void println(const char* = "") {}
	void println(int) {}
	void printf(const char*, ...) {}
};

inline MockSerial Serial;
inline void delay(uint32_t) {}
inline uint32_t millis() { static uint32_t t = 0; return t += 25; }
