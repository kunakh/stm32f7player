#ifndef __SWSCALE_H__
#define __SWSCALE_H__

typedef enum {
  YV12 = 0,           // YV12 is half width and half height chroma channels.
  YV16 = 1,           // YV16 is half width and full height chroma channels.
  YV24 = 2            // YV24 is full width and full height chroma channels.
} YUVType;

typedef enum {
  FILTER_NONE = 0,        // No filter (point sampled).
  FILTER_BILINEAR_H = 1,  // Bilinear horizontal filter.
  FILTER_BILINEAR_V = 2,  // Bilinear vertical filter.
  FILTER_BILINEAR = 3     // Bilinear filter.
} ScaleFilter;

void ScaleYCbCrToRGB565(const uint8_t *y_buf,
                        const uint8_t *u_buf,
                        const uint8_t *v_buf,
                        uint8_t *rgb_buf,
                        int source_width,
                        int source_height,
                        int width,
                        int height,
                        int y_pitch,
                        int uv_pitch,
                        int rgb_pitch,
                        YUVType yuv_type);

#endif // __SWSCALE_H__
