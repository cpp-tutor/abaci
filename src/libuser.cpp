#include <cmath>
#include <cstdio>
#include <algorithm>

#ifdef _WIN64
#define EXPORT_FUNCTION __declspec(dllexport)
#else
#define EXPORT_FUNCTION
#endif

extern "C" {
	EXPORT_FUNCTION double sine(double a) { return sin(a); }
	EXPORT_FUNCTION double cosine(double a) { return cos(a); }
	EXPORT_FUNCTION double tangent(double a) { return tan(a); }
	EXPORT_FUNCTION void putstr(char *s) { puts(s); }
	EXPORT_FUNCTION void sort(long long *d, std::size_t l) { std::sort(d, d + l); }
}