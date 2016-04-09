// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/. 

#if _MSC_VER == 1700
// Has to be included before TextureScaler.h, else we get those std::bind errors in VS2012.. 
#include "../native/base/basictypes.h"
#endif

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <xbrz.h>


#include "Common/Common.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/CommonFuncs.h"
#include "Common/ThreadPool.h"
#include "Common/CPUDetect.h"
#include "Common/Intrinsics.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/TextureScalerCommon.h"


#if _M_SSE >= 0x401
#include <smmintrin.h>
#endif

// Report the time and throughput for each larger scaling operation in the log
//#define SCALING_MEASURE_TIME

#ifdef SCALING_MEASURE_TIME
#include "native/base/timeutil.h"
#endif

/////////////////////////////////////// Helper Functions (mostly math for parallelization)

namespace {
//////////////////////////////////////////////////////////////////// Various image processing

#define R(_col) ((_col>> 0)&0xFF)
#define G(_col) ((_col>> 8)&0xFF)
#define B(_col) ((_col>>16)&0xFF)
#define A(_col) ((_col>>24)&0xFF)

#define DISTANCE(_p1,_p2) ( abs(static_cast<int>(static_cast<int>(R(_p1))-R(_p2))) + abs(static_cast<int>(static_cast<int>(G(_p1))-G(_p2))) \
							  + abs(static_cast<int>(static_cast<int>(B(_p1))-B(_p2))) + abs(static_cast<int>(static_cast<int>(A(_p1))-A(_p2))) )

// this is sadly much faster than an inline function with a loop, at least in VC10
#define MIX_PIXELS(_p0, _p1, _factors) \
		( (R(_p0)*(_factors)[0] + R(_p1)*(_factors)[1])/255 <<  0 ) | \
		( (G(_p0)*(_factors)[0] + G(_p1)*(_factors)[1])/255 <<  8 ) | \
		( (B(_p0)*(_factors)[0] + B(_p1)*(_factors)[1])/255 << 16 ) | \
		( (A(_p0)*(_factors)[0] + A(_p1)*(_factors)[1])/255 << 24 )

#define BLOCK_SIZE 32

// 3x3 convolution with Neumann boundary conditions, parallelizable
// quite slow, could be sped up a lot
// especially handling of separable kernels
void convolve3x3(u32* data, u32* out, const int kernel[3][3], int width, int height, int l, int u) {
	for (int yb = 0; yb < (u - l) / BLOCK_SIZE + 1; ++yb) {
		for (int xb = 0; xb < width / BLOCK_SIZE + 1; ++xb) {
			for (int y = l + yb*BLOCK_SIZE; y < l + (yb + 1)*BLOCK_SIZE && y < u; ++y) {
				for (int x = xb*BLOCK_SIZE; x < (xb + 1)*BLOCK_SIZE && x < width; ++x) {
					int val = 0;
					for (int yoff = -1; yoff <= 1; ++yoff) {
						int yy = std::max(std::min(y + yoff, height - 1), 0);
						for (int xoff = -1; xoff <= 1; ++xoff) {
							int xx = std::max(std::min(x + xoff, width - 1), 0);
							val += data[yy*width + xx] * kernel[yoff + 1][xoff + 1];
						}
					}
					out[y*width + x] = abs(val);
				}
			}
		}
	}
}

// deposterization: smoothes posterized gradients from low-color-depth (e.g. 444, 565, compressed) sources
void deposterizeH(u32* data, u32* out, int w, int l, int u) {
	static const int T = 8;
	for (int y = l; y < u; ++y) {
		for (int x = 0; x < w; ++x) {
			int inpos = y*w + x;
			u32 center = data[inpos];
			if (x == 0 || x == w - 1) {
				out[y*w + x] = center;
				continue;
			}
			u32 left = data[inpos - 1];
			u32 right = data[inpos + 1];
			out[y*w + x] = 0;
			for (int c = 0; c < 4; ++c) {
				u8 lc = ((left >> c * 8) & 0xFF);
				u8 cc = ((center >> c * 8) & 0xFF);
				u8 rc = ((right >> c * 8) & 0xFF);
				if ((lc != rc) && ((lc == cc && abs((int)((int)rc) - cc) <= T) || (rc == cc && abs((int)((int)lc) - cc) <= T))) {
					// blend this component
					out[y*w + x] |= ((rc + lc) / 2) << (c * 8);
				} else {
					// no change for this component
					out[y*w + x] |= cc << (c * 8);
				}
			}
		}
	}
}
void deposterizeV(u32* data, u32* out, int w, int h, int l, int u) {
	static const int T = 8;
	for (int xb = 0; xb < w / BLOCK_SIZE + 1; ++xb) {
		for (int y = l; y < u; ++y) {
			for (int x = xb*BLOCK_SIZE; x < (xb + 1)*BLOCK_SIZE && x < w; ++x) {
				u32 center = data[y    * w + x];
				if (y == 0 || y == h - 1) {
					out[y*w + x] = center;
					continue;
				}
				u32 upper = data[(y - 1) * w + x];
				u32 lower = data[(y + 1) * w + x];
				out[y*w + x] = 0;
				for (int c = 0; c < 4; ++c) {
					u8 uc = ((upper >> c * 8) & 0xFF);
					u8 cc = ((center >> c * 8) & 0xFF);
					u8 lc = ((lower >> c * 8) & 0xFF);
					if ((uc != lc) && ((uc == cc && abs((int)((int)lc) - cc) <= T) || (lc == cc && abs((int)((int)uc) - cc) <= T))) {
						// blend this component
						out[y*w + x] |= ((lc + uc) / 2) << (c * 8);
					} else {
						// no change for this component
						out[y*w + x] |= cc << (c * 8);
					}
				}
			}
		}
	}
}

// generates a distance mask value for each pixel in data
// higher values -> larger distance to the surrounding pixels
void generateDistanceMask(u32* data, u32* out, int width, int height, int l, int u) {
	for (int yb = 0; yb < (u - l) / BLOCK_SIZE + 1; ++yb) {
		for (int xb = 0; xb < width / BLOCK_SIZE + 1; ++xb) {
			for (int y = l + yb*BLOCK_SIZE; y < l + (yb + 1)*BLOCK_SIZE && y < u; ++y) {
				for (int x = xb*BLOCK_SIZE; x < (xb + 1)*BLOCK_SIZE && x < width; ++x) {
					const u32 center = data[y*width + x];
					u32 dist = 0;
					for (int yoff = -1; yoff <= 1; ++yoff) {
						int yy = y + yoff;
						if (yy == height || yy == -1) {
							dist += 1200; // assume distance at borders, usually makes for better result
							continue;
						}
						for (int xoff = -1; xoff <= 1; ++xoff) {
							if (yoff == 0 && xoff == 0) continue;
							int xx = x + xoff;
							if (xx == width || xx == -1) {
								dist += 400; // assume distance at borders, usually makes for better result
								continue;
							}
							dist += DISTANCE(data[yy*width + xx], center);
						}
					}
					out[y*width + x] = dist;
				}
			}
		}
	}
}

// mix two images based on a mask
void mix(u32* data, u32* source, u32* mask, u32 maskmax, int width, int l, int u) {
	for (int y = l; y < u; ++y) {
		for (int x = 0; x < width; ++x) {
			int pos = y*width + x;
			u8 mixFactors[2] = { 0, static_cast<u8>((std::min(mask[pos], maskmax) * 255) / maskmax) };
			mixFactors[0] = 255 - mixFactors[1];
			data[pos] = MIX_PIXELS(data[pos], source[pos], mixFactors);
			if (A(source[pos]) == 0) data[pos] = data[pos] & 0x00FFFFFF; // xBRZ always does a better job with hard alpha
		}
	}
}


template<class T>
T clamp(T x, T floor, T ceil)
{
	return std::max(std::min(x, ceil), floor);
}

int lerpi(int a, int b, int p)
{
	return (a*256 + (b-a)*p)/256;
}


//////////////////////////////////////////////////////////////////// Bicubic scaling

// generate the value of a Mitchell-Netravali scaling spline at distance d, with parameters A and B
// B=1 C=0   : cubic B spline (very smooth)
// B=C=1/3   : recommended for general upscaling
// B=0 C=1/2 : Catmull-Rom spline (sharp, ringing)
// see Mitchell & Netravali, "Reconstruction Filters in Computer Graphics"
inline float mitchell(float x, float B, float C) {
	float ax = fabs(x);
	if (ax >= 2.0f) return 0.0f;
	if (ax >= 1.0f) return ((-B - 6 * C)*(x*x*x) + (6 * B + 30 * C)*(x*x) + (-12 * B - 48 * C)*x + (8 * B + 24 * C)) / 6.0f;
	return ((12 - 9 * B - 6 * C)*(x*x*x) + (-18 + 12 * B + 6 * C)*(x*x) + (6 - 2 * B)) / 6.0f;
}

inline float jinc(float x, float A, float B, float AB) {
	x = x + 0.000000001f;
	
	return (sin(x*A)*sin(x*B) / (x*x*AB));
}


// arrays for pre-calculating weights and sums (~20KB)
// Dimensions:
//   0: 0 = BSpline, 1 = mitchell
//   2: 2-5x scaling
// 2,3: 5x5 generated pixels 
// 4,5: 4x4 pixels sampled from. No need for 25 texture lookups. 16 is enough! (Hyllian)
float bicubicWeights[2][4][5][5][4][4];
float bicubicInvSums[2][4][5][5];
float jincWeights[4][5][5][4][4];
float jincInvSums[4][5][5];
// initialize pre-computed weights array
void initFilterWeights() {
	float pi = 3.1415926535897932384626433832795f;
	float wa = 0.42f*pi;
	float wb = 0.92f*pi;
	float wab = wa*wb;
	float B[2] = { 1.0f, 0.334f };
	float C[2] = { 0.0f, 0.334f };
	for (int type = 0; type < 2; ++type) {
		for (int factor = 2; factor <= 5; ++factor) {
			for (int x = 0; x < factor; ++x) {
				for (int y = 0; y < factor; ++y) {
					float sum[2] = { 0.0f, 0.0f };
					for (int sx = 0; sx < 4; ++sx) {
						for (int sy = 0; sy < 4; ++sy) {
							float dx = (x + 0.5f + (factor % 2) / 2.0f) / factor - (sx - 1.0f);
							float dy = (y + 0.5f + (factor % 2) / 2.0f) / factor - (sy - 1.0f);
							float dist = sqrt(dx*dx + dy*dy);
							float weight = mitchell(dist, B[type], C[type]);
							bicubicWeights[type][factor - 2][x][y][sx][sy] = weight;
							sum[0] += weight;
							weight = jinc(dist, wa, wb, wab);
							jincWeights[factor - 2][x][y][sx][sy] = weight;
							sum[1] += weight;
						}
					}
					bicubicInvSums[type][factor - 2][x][y] = 1.0f / sum[0];
					jincInvSums[factor - 2][x][y] = 1.0f / sum[1];
				}
			}
		}
	}
}


// perform bicubic scaling by factor f, with precomputed spline type T
template<int f, int T>
void scaleBicubicT(u32* data, u32* out, int w, int h, int l, int u) {
	int outw = w*f, outh = h*f;
	for (int cy = 0; cy <= h; ++cy) {
		for (int cx = 0; cx <= w; ++cx) {
			float rc[4][4], gc[4][4], bc[4][4], ac[4][4];
			int y_offset = cy*f - f / 2; // Cannot be factored, because they're all integers!
			int x_offset = cx*f - f / 2; // They begin offset by f / 2
			for (int sx = 0; sx < 4; ++sx) {
				for (int sy = 0; sy < 4; ++sy) {
					// clamp pixel locations
					int csy = std::max(std::min(sy - 2 + cy, h - 1), 0);
					int csx = std::max(std::min(sx - 2 + cx, w - 1), 0);
					// sample & add weighted components
					u32 sample = data[csy*w + csx];
					rc[sx][sy] = (float)R(sample);
					gc[sx][sy] = (float)G(sample);
					bc[sx][sy] = (float)B(sample);
					ac[sx][sy] = (float)A(sample);
				}
			}
			for (int y = 0; y < f; ++y) {
				for (int x = 0; x < f; ++x) {
					float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
					// sample supporting pixels in original image
					for (int sx = 0; sx < 4; ++sx) {
						for (int sy = 0; sy < 4; ++sy) {
							float weight = bicubicWeights[T][f - 2][x%f][y%f][sx][sy];
							// clamp pixel locations
							r += weight*rc[sx][sy];
							g += weight*gc[sx][sy];
							b += weight*bc[sx][sy];
							a += weight*ac[sx][sy];
						}
					}
					// generate and write result
					float invSum = bicubicInvSums[T][f - 2][x%f][y%f];
					int ri = clamp(static_cast<int>(ceilf(r*invSum)), 0, 255);
					int gi = clamp(static_cast<int>(ceilf(g*invSum)), 0, 255);
					int bi = clamp(static_cast<int>(ceilf(b*invSum)), 0, 255);
					int ai = clamp(static_cast<int>(ceilf(a*invSum)), 0, 255);
					int yline = clamp(y + y_offset, 0, outh - 1);
					int xline = clamp(x + x_offset, 0, outw - 1);
					out[yline*outw + xline] = (ai << 24) | (bi << 16) | (gi << 8) | ri;
				}
			}
		}
	}
}


/* Jinc code uses MIT LICENSE

   Hyllian's jinc windowed-jinc 2-lobe sharper with anti-ringing
   
   Copyright (C) 2011/2016 Hyllian - sergiogdb@gmail.com

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is 
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.

*/


// perform jinc scaling by factor f.
template<int f>
void scaleJincT(u32* data, u32* out, int w, int h) {
	int outw = w*f, outh = h*f;
	for (int cy = 0; cy <= h; ++cy) {
		for (int cx = 0; cx <= w; ++cx) {
			int rmin = 255, gmin = 255, bmin = 255, amin = 255, rmax = 0, gmax = 0, bmax = 0, amax = 0;
			float rc[4][4], gc[4][4], bc[4][4], ac[4][4];
			int raux, baux, gaux, aaux;
			int y_offset = cy*f - f / 2; // Cannot be factored, because they're all integers!
			int x_offset = cx*f - f / 2; // They begin offset by f / 2
			for (int sx = 0; sx < 4; ++sx) {
				for (int sy = 0; sy < 4; ++sy) {
					// clamp pixel locations
					int csy = std::max(std::min(sy - 2 + cy, h - 1), 0);
					int csx = std::max(std::min(sx - 2 + cx, w - 1), 0);
					// sample & add weighted components
					u32 sample = data[csy*w + csx];
					rc[sx][sy] = (float)(raux = R(sample));
					gc[sx][sy] = (float)(gaux = G(sample));
					bc[sx][sy] = (float)(baux = B(sample));
					ac[sx][sy] = (float)(aaux = A(sample));
					// Filling the anti-ringing code.
					if (sx >= 1 && sx < 3 && sy >= 1 && sy < 3) {
						rmin = std::min(raux, rmin); rmax = std::max(raux, rmax);
						gmin = std::min(gaux, gmin); gmax = std::max(gaux, gmax);
						bmin = std::min(baux, bmin); bmax = std::max(baux, bmax);
						amin = std::min(aaux, amin); amax = std::max(aaux, amax);
					}
				}
			}
			for (int y = 0; y < f; ++y) {
				for (int x = 0; x < f; ++x) {
					float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
					// sample supporting pixels in original image
					for (int sx = 0; sx < 4; ++sx) {
						for (int sy = 0; sy < 4; ++sy) {
							float weight = jincWeights[f - 2][x%f][y%f][sx][sy];
							// clamp pixel locations
							r += weight*rc[sx][sy];
							g += weight*gc[sx][sy];
							b += weight*bc[sx][sy];
							a += weight*ac[sx][sy];
						}
					}
					// generate and write result
					float invSum = jincInvSums[f - 2][x%f][y%f];
					int ri = clamp(raux = clamp(static_cast<int>(ceilf(r*invSum)), 0, 255), rmin, rmax); // Anti-ringing.
					int gi = clamp(gaux = clamp(static_cast<int>(ceilf(g*invSum)), 0, 255), gmin, gmax);
					int bi = clamp(baux = clamp(static_cast<int>(ceilf(b*invSum)), 0, 255), bmin, bmax);
					int ai = clamp(aaux = clamp(static_cast<int>(ceilf(a*invSum)), 0, 255), amin, amax);
					ri = lerpi(ri, raux, 128);
					gi = lerpi(gi, gaux, 128);
					bi = lerpi(bi, baux, 128);
					ai = lerpi(ai, aaux, 128);
					int yline = clamp(y + y_offset, 0, outh - 1);
					int xline = clamp(x + x_offset, 0, outw - 1);
					out[yline*outw + xline] = (ai << 24) | (bi << 16) | (gi << 8) | ri;
				}
			}
		}
	}
}


#if _M_SSE >= 0x401

template<int f, int T>
void scaleBicubicTSSE41(u32* data, u32* out, int w, int h, int l, int u) {
	int outw = w*f, outh = h*f;
	for (int cy = 0; cy <= h; ++cy) {
		for (int cx = 0; cx <= w; ++cx) {
			__m128 color[4][4], col;
			int y_offset = cy*f - f / 2; // Cannot be factored, because they're all integers!
			int x_offset = cx*f - f / 2; // They begin offset by f / 2
			for (int sx = 0; sx < 4; ++sx) {
				for (int sy = 0; sy < 4; ++sy) {
					// clamp pixel locations
					int csy = std::max(std::min(sy - 2 + cy, h - 1), 0);
					int csx = std::max(std::min(sx - 2 + cx, w - 1), 0);
					// Sample supporting pixels in original image
					__m128i sample = _mm_cvtsi32_si128(data[csy*w + csx]); // reads i32 and put at r0. 128bit = r0r1r2r3.
					sample = _mm_cvtepu8_epi32(sample); // separates 32 bits in four 8 bits, one at each r#.
					color[sx][sy] = _mm_cvtepi32_ps(sample); // convert int to float.
				}
			}
			for (int y = 0; y < f; ++y) {
				for (int x = 0; x < f; ++x) {
					__m128 result = _mm_set1_ps(0.0f);
					// Add weighted components
					for (int sx = 0; sx < 4; ++sx) {
						for (int sy = 0; sy < 4; ++sy) {
							float weight = bicubicWeights[T][f - 2][x%f][y%f][sx][sy];
							col = _mm_mul_ps(color[sx][sy], _mm_set1_ps(weight)); // multiply four 32 bits by weight.
							result = _mm_add_ps(result, col); // Accumulate result with col.
						}
					}
					// generate and write result
					__m128i pixel = _mm_cvtps_epi32(_mm_mul_ps(result, _mm_set1_ps(bicubicInvSums[T][f - 2][x%f][y%f]))); // float to int
					pixel = _mm_packs_epi32(pixel, pixel); // 4x32bit to 8x16bit
					pixel = _mm_packus_epi16(pixel, pixel); // 8x16bit to 16x8bit
					int yline = clamp(y + y_offset, 0, outh - 1);
					int xline = clamp(x + x_offset, 0, outw - 1);
					out[yline*outw + xline] = _mm_cvtsi128_si32(pixel); // r = r0.
				}
			}
		}
	}
}

// perform jinc scaling by factor f.
template<int f>
void scaleJincTSSE41(u32* data, u32* out, int w, int h) {
	int outw = w*f, outh = h*f;
	for (int cy = 0; cy <= h; ++cy) {
		for (int cx = 0; cx <= w; ++cx) {
			__m128i min_sample = _mm_set1_epi32(255);
			__m128i max_sample = _mm_set1_epi32(0);
			__m128i aux;
			__m128 color[4][4], col;
			int y_offset = cy*f - f / 2; // Cannot be factored, because they're all integers!
			int x_offset = cx*f - f / 2; // They begin offset by f / 2
			for (int sx = 0; sx < 4; ++sx) {
				for (int sy = 0; sy < 4; ++sy) {
					// clamp pixel locations
					int csy = std::max(std::min(sy - 2 + cy, h - 1), 0);
					int csx = std::max(std::min(sx - 2 + cx, w - 1), 0);
					// Sample supporting pixels in original image
					__m128i sample = _mm_cvtsi32_si128(data[csy*w + csx]); // reads i32 and put at r0. 128bit = r0r1r2r3.
					sample = _mm_cvtepu8_epi32(sample); // separates 32 bits in four 8 bits, one at each r#.
					// Fill the anti-ringing code
					if (sx >= 1 && sx < 3 && sy >= 1 && sy < 3) {
						min_sample = _mm_min_epi32(sample, min_sample); // Setting floor and ceil anti-ringing samples.
						max_sample = _mm_max_epi32(sample, max_sample);
					}
					color[sx][sy] = _mm_cvtepi32_ps(sample); // convert int to float.
				}
			}
			for (int y = 0; y < f; ++y) {
				for (int x = 0; x < f; ++x) {
					__m128 result = _mm_set1_ps(0.0f);
					// Add weighted components
					for (int sx = 0; sx < 4; ++sx) {
						for (int sy = 0; sy < 4; ++sy) {
							float weight = jincWeights[f - 2][x%f][y%f][sx][sy];
							col = _mm_mul_ps(color[sx][sy], _mm_set1_ps(weight)); // multiply four 32 bits by weight.
							result = _mm_add_ps(result, col); // Accumulate result with col.
						}
					}
					// generate and write result
					__m128i pixel = aux = _mm_cvtps_epi32(_mm_mul_ps(result, _mm_set1_ps(jincInvSums[f - 2][x%f][y%f]))); // float to int
					pixel = _mm_max_epi32(pixel, min_sample); // Anti-ringing.
					pixel = _mm_min_epi32(pixel, max_sample);
					pixel = _mm_srai_epi32(_mm_add_epi32(pixel, aux), 1); // Perform a mix between pixel and aux at 50%.
					pixel = _mm_packs_epi32(pixel, pixel); // 4x32bit to 8x16bit
					pixel = _mm_packus_epi16(pixel, pixel); // 8x16bit to 16x8bit
					int yline = clamp(y + y_offset, 0, outh - 1);
					int xline = clamp(x + x_offset, 0, outw - 1);
					out[yline*outw + xline] = _mm_cvtsi128_si32(pixel); // r = r0.
				}
			}
		}
	}
}

#endif

void scaleBicubicBSpline(int factor, u32* data, u32* out, int w, int h, int l, int u) {
#if _M_SSE >= 0x401
	if (cpu_info.bSSE4_1) {
		switch (factor) {
		case 2: scaleBicubicTSSE41<2, 0>(data, out, w, h, l, u); break; // when I first tested this, 
		case 3: scaleBicubicTSSE41<3, 0>(data, out, w, h, l, u); break; // it was even slower than I had expected
		case 4: scaleBicubicTSSE41<4, 0>(data, out, w, h, l, u); break; // turns out I had not included
		case 5: scaleBicubicTSSE41<5, 0>(data, out, w, h, l, u); break; // any of these break statements
		default: ERROR_LOG(VIDEO, "Bicubic upsampling only implemented for factors 2 to 5");
		}
	} else {
#endif
		switch (factor) {
		case 2: scaleBicubicT<2, 0>(data, out, w, h, l, u); break; // when I first tested this, 
		case 3: scaleBicubicT<3, 0>(data, out, w, h, l, u); break; // it was even slower than I had expected
		case 4: scaleBicubicT<4, 0>(data, out, w, h, l, u); break; // turns out I had not included
		case 5: scaleBicubicT<5, 0>(data, out, w, h, l, u); break; // any of these break statements
		default: ERROR_LOG(VIDEO, "Bicubic upsampling only implemented for factors 2 to 5");
		}
#if _M_SSE >= 0x401
	}
#endif
}

void scaleBicubicMitchell(int factor, u32* data, u32* out, int w, int h, int l, int u) {
#if _M_SSE >= 0x401
	if (cpu_info.bSSE4_1) {
		switch (factor) {
		case 2: scaleBicubicTSSE41<2, 1>(data, out, w, h, l, u); break;
		case 3: scaleBicubicTSSE41<3, 1>(data, out, w, h, l, u); break;
		case 4: scaleBicubicTSSE41<4, 1>(data, out, w, h, l, u); break;
		case 5: scaleBicubicTSSE41<5, 1>(data, out, w, h, l, u); break;
		default: ERROR_LOG(VIDEO, "Bicubic upsampling only implemented for factors 2 to 5");
		}
	} else {
#endif
		switch (factor) {
		case 2: scaleBicubicT<2, 1>(data, out, w, h, l, u); break;
		case 3: scaleBicubicT<3, 1>(data, out, w, h, l, u); break;
		case 4: scaleBicubicT<4, 1>(data, out, w, h, l, u); break;
		case 5: scaleBicubicT<5, 1>(data, out, w, h, l, u); break;
		default: ERROR_LOG(VIDEO, "Bicubic upsampling only implemented for factors 2 to 5");
		}
#if _M_SSE >= 0x401
	}
#endif
}


void scaleJinc(int factor, u32* data, u32* out, int w, int h) {
#if _M_SSE >= 0x401
	if (cpu_info.bSSE4_1) {
		switch (factor) {
		case 2: scaleJincTSSE41<2>(data, out, w, h); break;
		case 3: scaleJincTSSE41<3>(data, out, w, h); break;
		case 4: scaleJincTSSE41<4>(data, out, w, h); break;
		case 5: scaleJincTSSE41<5>(data, out, w, h); break;
		default: ERROR_LOG(VIDEO, "Jinc upsampling only implemented for factors 2 to 5");
		}
	}
	else {
#endif
		switch (factor) {
		case 2: scaleJincT<2>(data, out, w, h); break;
		case 3: scaleJincT<3>(data, out, w, h); break;
		case 4: scaleJincT<4>(data, out, w, h); break;
		case 5: scaleJincT<5>(data, out, w, h); break;
		default: ERROR_LOG(VIDEO, "Jinc upsampling only implemented for factors 2 to 5");
		}
#if _M_SSE >= 0x401
	}
#endif
}


//////////////////////////////////////////////////////////////////// Bilinear scaling

const static u8 BILINEAR_FACTORS[4][3][2] = {
		{ { 44, 211 }, { 0, 0 }, { 0, 0 } }, // x2
		{ { 64, 191 }, { 0, 255 }, { 0, 0 } }, // x3
		{ { 77, 178 }, { 26, 229 }, { 0, 0 } }, // x4
		{ { 102, 153 }, { 51, 204 }, { 0, 255 } }, // x5
};
// integral bilinear upscaling by factor f, horizontal part
template<int f>
void bilinearHt(u32* data, u32* out, int w, int l, int u) {
	static_assert(f > 1 && f <= 5, "Bilinear scaling only implemented for factors 2 to 5");
	int outw = w*f;
	for (int y = l; y < u; ++y) {
		for (int x = 0; x < w; ++x) {
			int inpos = y*w + x;
			u32 left = data[inpos - (x == 0 ? 0 : 1)];
			u32 center = data[inpos];
			u32 right = data[inpos + (x == w - 1 ? 0 : 1)];
			int i = 0;
			for (; i < f / 2 + f % 2; ++i) { // first half of the new pixels + center, hope the compiler unrolls this
				out[y*outw + x*f + i] = MIX_PIXELS(left, center, BILINEAR_FACTORS[f - 2][i]);
			}
			for (; i < f; ++i) { // second half of the new pixels, hope the compiler unrolls this
				out[y*outw + x*f + i] = MIX_PIXELS(right, center, BILINEAR_FACTORS[f - 2][f - 1 - i]);
			}
		}
	}
}
void bilinearH(int factor, u32* data, u32* out, int w, int l, int u) {
	switch (factor) {
	case 2: bilinearHt<2>(data, out, w, l, u); break;
	case 3: bilinearHt<3>(data, out, w, l, u); break;
	case 4: bilinearHt<4>(data, out, w, l, u); break;
	case 5: bilinearHt<5>(data, out, w, l, u); break;
	default: ERROR_LOG(VIDEO, "Bilinear upsampling only implemented for factors 2 to 5");
	}
}
// integral bilinear upscaling by factor f, vertical part
// gl/gu == global lower and upper bound
template<int f>
void bilinearVt(u32* data, u32* out, int w, int gl, int gu, int l, int u) {
	static_assert(f>1 && f <= 5, "Bilinear scaling only implemented for 2x, 3x, 4x, and 5x");
	int outw = w*f;
	for (int xb = 0; xb < outw / BLOCK_SIZE + 1; ++xb) {
		for (int y = l; y < u; ++y) {
			u32 uy = y - (y == gl ? 0 : 1);
			u32 ly = y + (y == gu - 1 ? 0 : 1);
			for (int x = xb*BLOCK_SIZE; x < (xb + 1)*BLOCK_SIZE && x < outw; ++x) {
				u32 upper = data[uy * outw + x];
				u32 center = data[y * outw + x];
				u32 lower = data[ly * outw + x];
				int i = 0;
				for (; i < f / 2 + f % 2; ++i) { // first half of the new pixels + center, hope the compiler unrolls this
					out[(y*f + i)*outw + x] = MIX_PIXELS(upper, center, BILINEAR_FACTORS[f - 2][i]);
				}
				for (; i < f; ++i) { // second half of the new pixels, hope the compiler unrolls this
					out[(y*f + i)*outw + x] = MIX_PIXELS(lower, center, BILINEAR_FACTORS[f - 2][f - 1 - i]);
				}
			}
		}
	}
}
void bilinearV(int factor, u32* data, u32* out, int w, int gl, int gu, int l, int u) {
	switch (factor) {
	case 2: bilinearVt<2>(data, out, w, gl, gu, l, u); break;
	case 3: bilinearVt<3>(data, out, w, gl, gu, l, u); break;
	case 4: bilinearVt<4>(data, out, w, gl, gu, l, u); break;
	case 5: bilinearVt<5>(data, out, w, gl, gu, l, u); break;
	default: ERROR_LOG(VIDEO, "Bilinear upsampling only implemented for factors 2 to 5");
	}
}

#undef BLOCK_SIZE
#undef MIX_PIXELS
#undef DISTANCE
#undef R
#undef G
#undef B
#undef A

// used for debugging texture scaling (writing textures to files)
static int g_imgCount = 0;
void dbgPPM(int w, int h, u8* pixels, const char* prefix = "dbg") { // 3 component RGB
	char fn[32];
	snprintf(fn, 32, "%s%04d.ppm", prefix, g_imgCount++);
	FILE *fp = fopen(fn, "wb");
	fprintf(fp, "P6\n%d %d\n255\n", w, h);
	for (int j = 0; j < h; ++j) {
		for (int i = 0; i < w; ++i) {
			static unsigned char color[3];
			color[0] = pixels[(j*w + i) * 4 + 0];  /* red */
			color[1] = pixels[(j*w + i) * 4 + 1];  /* green */
			color[2] = pixels[(j*w + i) * 4 + 2];  /* blue */
			fwrite(color, 1, 3, fp);
		}
	}
	fclose(fp);
}
void dbgPGM(int w, int h, u32* pixels, const char* prefix = "dbg") { // 1 component
	char fn[32];
	snprintf(fn, 32, "%s%04d.pgm", prefix, g_imgCount++);
	FILE *fp = fopen(fn, "wb");
	fprintf(fp, "P5\n%d %d\n65536\n", w, h);
	for (int j = 0; j < h; ++j) {
		for (int i = 0; i < w; ++i) {
			fwrite((pixels + (j*w + i)), 1, 2, fp);
		}
	}
	fclose(fp);
}
}

/////////////////////////////////////// Texture Scaler

TextureScaler::TextureScaler() {
	initFilterWeights();
}

TextureScaler::~TextureScaler() {
	xbrz::shutdown();
}

bool TextureScaler::IsEmptyOrFlat(u32* data, int pixels) {
	u32 ref = data[0];
	for (int i = 0; i < pixels; ++i) {
		if (data[i] != ref) return false;
	}
	return true;
}

u32* TextureScaler::Scale(u32* data, int width, int height) {
	// prevent processing empty or flat textures (this happens a lot in some games)
	// doesn't hurt the standard case, will be very quick for textures with actual texture
	/*if (IsEmptyOrFlat(data, width*height)) {
		INFO_LOG(VIDEO, "TextureScaler: early exit -- empty/flat texture");
		return nullptr;
	}*/

#ifdef SCALING_MEASURE_TIME
	double t_start = real_time_now();
#endif
	int factor = g_ActiveConfig.iTexScalingFactor;
	//bufInput.resize(width*height); // used to store the input image image if it needs to be reformatted
	bufOutput.resize(width*height*factor*factor); // used to store the upscaled image
	u32 *inputBuf = data;
	u32 *outputBuf = bufOutput.data();

	// deposterize
	if (g_ActiveConfig.bTexDeposterize) {
		bufDeposter.resize(width*height);
		DePosterize(inputBuf, bufDeposter.data(), width, height);
		inputBuf = bufDeposter.data();
	}

	// scale 
	switch (g_ActiveConfig.iTexScalingType) {
	case XBRZ:
		ScaleXBRZ(factor, inputBuf, outputBuf, width, height);
		break;
	case HYBRID:
		ScaleHybrid(factor, inputBuf, outputBuf, width, height);
		break;
	case BICUBIC:
		ScaleBicubicMitchell(factor, inputBuf, outputBuf, width, height);
		break;
	case HYBRID_BICUBIC:
		ScaleHybrid(factor, inputBuf, outputBuf, width, height, true);
		break;
	case JINC:
		ScaleJinc(factor, inputBuf, outputBuf, width, height);
		break;
	default:
		ERROR_LOG(VIDEO, "Unknown scaling type: %d", g_ActiveConfig.iTexScalingType);
	}
#ifdef SCALING_MEASURE_TIME
	if (width*height > 64 * 64 * factor*factor) {
		double t = real_time_now() - t_start;
		NOTICE_LOG(MASTER_LOG, "TextureScaler: processed %9d pixels in %6.5lf seconds. (%9.2lf Mpixels/second)",
			width*height, t, (width*height) / (t * 1000 * 1000));
	}
#endif
	return outputBuf;
}

void TextureScaler::ScaleXBRZ(int factor, u32* source, u32* dest, int width, int height) {
	xbrz::ScalerCfg cfg;
	xbrz::init();
	//GlobalThreadPool::Loop(std::bind(&xbrz::scale, factor, source, dest, width, height, xbrz::ColorFormat::ARGB, cfg, placeholder::_1, placeholder::_2), 0, height);
	xbrz::scale(factor, source, dest, width, height, xbrz::ColorFormat::ARGB, cfg,  0, height);
}

void TextureScaler::ScaleBilinear(int factor, u32* source, u32* dest, int width, int height) {
	bufTmp1.resize(width*height*factor);
	u32 *tmpBuf = bufTmp1.data();
	//GlobalThreadPool::Loop(std::bind(&bilinearH, factor, source, tmpBuf, width, placeholder::_1, placeholder::_2), 0, height);
	//GlobalThreadPool::Loop(std::bind(&bilinearV, factor, tmpBuf, dest, width, 0, height, placeholder::_1, placeholder::_2), 0, height);
	bilinearH(factor, source, tmpBuf, width,  0, height);
	bilinearV(factor, tmpBuf, dest, width, 0, height, 0, height);
}

void TextureScaler::ScaleBicubicBSpline(int factor, u32* source, u32* dest, int width, int height) {
	//GlobalThreadPool::Loop(std::bind(&scaleBicubicBSpline, factor, source, dest, width, height, placeholder::_1, placeholder::_2), 0, height);
	scaleBicubicBSpline( factor, source, dest, width, height, 0, height);
}

void TextureScaler::ScaleBicubicMitchell(int factor, u32* source, u32* dest, int width, int height) {
	//GlobalThreadPool::Loop(std::bind(&scaleBicubicMitchell, factor, source, dest, width, height, placeholder::_1, placeholder::_2), 0, height);
	scaleBicubicMitchell( factor, source, dest, width, height, 0, height);
}

void TextureScaler::ScaleHybrid(int factor, u32* source, u32* dest, int width, int height, bool bicubic) {
	// Basic algorithm:
	// 1) determine a feature mask C based on a sobel-ish filter + splatting, and upscale that mask bilinearly
	// 2) generate 2 scaled images: A - using Bilinear filtering, B - using xBRZ
	// 3) output = A*C + B*(1-C)

	const static int KERNEL_SPLAT[3][3] = {
			{ 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 }
	};

	bufTmp1.resize(width*height);
	bufTmp2.resize(width*height*factor*factor);
	bufTmp3.resize(width*height*factor*factor);
	//GlobalThreadPool::Loop(std::bind(&generateDistanceMask, source, bufTmp1.data(), width, height, placeholder::_1, placeholder::_2), 0, height);
	//GlobalThreadPool::Loop(std::bind(&convolve3x3, bufTmp1.data(), bufTmp2.data(), KERNEL_SPLAT, width, height, placeholder::_1, placeholder::_2), 0, height);
	generateDistanceMask(source, bufTmp1.data(), width, height, 0, height);
	convolve3x3( bufTmp1.data(), bufTmp2.data(), KERNEL_SPLAT, width, height, 0, height);

	ScaleBilinear(factor, bufTmp2.data(), bufTmp3.data(), width, height);
	// mask C is now in bufTmp3

	ScaleXBRZ(factor, source, bufTmp2.data(), width, height);
	// xBRZ upscaled source is in bufTmp2

	if (bicubic) ScaleBicubicBSpline(factor, source, dest, width, height);
	else ScaleBilinear(factor, source, dest, width, height);
	// Upscaled source is in dest

	// Now we can mix it all together
	// The factor 8192 was found through practical testing on a variety of textures
	//GlobalThreadPool::Loop(std::bind(&mix, dest, bufTmp2.data(), bufTmp3.data(), 8192, width*factor, placeholder::_1, placeholder::_2), 0, height*factor);
	mix(dest, bufTmp2.data(), bufTmp3.data(), 8192, width*factor, 0, height*factor);
}

void TextureScaler::ScaleJinc(int factor, u32* source, u32* dest, int width, int height) {
	//GlobalThreadPool::Loop(std::bind(&scaleSuperXBR, factor, source, dest, width, height, placeholder::_1, placeholder::_2), 0, height);
	scaleJinc(factor, source, dest, width, height);
}

void TextureScaler::DePosterize(u32* source, u32* dest, int width, int height) {
	bufTmp3.resize(width*height);
	//GlobalThreadPool::Loop(std::bind(&deposterizeH, source, bufTmp3.data(), width, placeholder::_1, placeholder::_2), 0, height);
	//GlobalThreadPool::Loop(std::bind(&deposterizeV, bufTmp3.data(), dest, width, height, placeholder::_1, placeholder::_2), 0, height);
	//GlobalThreadPool::Loop(std::bind(&deposterizeH, dest, bufTmp3.data(), width, placeholder::_1, placeholder::_2), 0, height);
	//GlobalThreadPool::Loop(std::bind(&deposterizeV, bufTmp3.data(), dest, width, height, placeholder::_1, placeholder::_2), 0, height);
	deposterizeH(source, bufTmp3.data(), width, 0, height);
	deposterizeV(bufTmp3.data(), dest, width, height, 0, height);
	deposterizeH(dest, bufTmp3.data(), width, 0, height);
	deposterizeV(bufTmp3.data(), dest, width, height, 0, height);
}
