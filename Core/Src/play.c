#include "main.h"
#include "config.h"
#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>

#include "wrapper.h"
#include "libavutil/avstring.h"
#include "libavutil/colorspace.h"
#include "libavutil/eval.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "libavutil/time.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavcodec/avfft.h"
#include "libswresample/swresample.h"
#include "libswresample/audioconvert.h"
//#include "libavresample/audio_data.h"

#if CONFIG_AVFILTER
# include "libavfilter/avcodec.h"
# include "libavfilter/avfilter.h"
# include "libavfilter/buffersink.h"
# include "libavfilter/buffersrc.h"
#endif

#include "cmdutils.h"
#include <assert.h>

#include "scale.h"

#define LCD_X_SIZE          RK043FN48H_WIDTH
#define LCD_Y_SIZE          RK043FN48H_HEIGHT
#define FRAME_BUFFER_SIZE   (LCD_X_SIZE * LCD_Y_SIZE * 2) // rgb565

#define AV_TASK_STACK      (10*1024)
#define AV_TASK_PRIORITY   7
#define AV_QUEUE_LENGTH    16

#define READER_TASK_PRIORITY  5
#define READER_TASK_STACK     (10*1024)

#define AUDIO_QUEUE_LENGTH    128

#pragma location = "__iram"
#pragma data_alignment=32
uint32_t av_task_stack[AV_TASK_STACK];

#pragma location = "__iram"
#pragma data_alignment=32
uint32_t reader_task_stack[READER_TASK_STACK];

typedef struct {
    AVFrame *pFrame;
    AVCodecContext *pCodecCtx;
    AVPacket *packet;
} av_queue_t;

typedef struct {
    uint8_t *data;
    size_t size;
} audio_queue_t;

static QueueHandle_t      av_queue;
static QueueHandle_t      audio_queue;

//static void *audio_frame;
//static TaskHandle_t       av_task;
//static TaskHandle_t       reader_task;
//static SemaphoreHandle_t  lcd_sema;

DMA2D_HandleTypeDef Dma2dHandle;
volatile char *lcd_fb_start = NULL;
volatile char *lcd_fb_active = NULL;
static int fps = 0;

AVDictionary *format_opts = NULL;
void *_impure_ptr = NULL;

extern void* __iar_dlmemalign(uint32_t, uint32_t);
void *memalign(size_t align, size_t size)
{
//  printf("[%s] size: %d\n", __FUNCTION__, size);
  return __iar_dlmemalign(align, size);
}

void Error_Handler(void)
{
  BSP_LED_Init(LED1);
  BSP_LED_On(LED1);
  while(1){}
}

int __errno(int e)
{
  printf("[%s]: %i\n", __FUNCTION__, e);
  return 0;
}

int __fpclassifyf (float x)
{
    return fpclassify(x);
}

int __fpclassifyd (double x)
{
    return fpclassify(x);
}

int decode_interrupt_cb(void *ctx)
{
//  printf("[%s]\n", __FUNCTION__);
    return 0;
}

#pragma data_alignment=32
__ALIGN_BEGIN static FATFS SDFatFs __ALIGN_END;

static char SDPath[4];
static FIL tmp_fil;
static char tmp_file_name[128];

size_t read(int fd, void *buf, size_t nbyte)
{
//  printf("[%s] buf: %x, size: %d\n", __FUNCTION__, buf, nbyte);
    size_t size = 0;
    f_read(&tmp_fil, buf, nbyte, &size);
    return size;
}

int close(int fd)
{
  printf("[%s]\n", __FUNCTION__);
    return f_close(&tmp_fil);
}

int open(const char *path, int oflag, ... )
{
  printf("[%s]\n", __FUNCTION__);

/*
#define	FA_OPEN_EXISTING	0x00
#define	FA_READ				0x01
#define	FA_WRITE			0x02
#define	FA_CREATE_NEW		0x04
#define	FA_CREATE_ALWAYS	0x08
#define	FA_OPEN_ALWAYS		0x10
#define FA__WRITTEN			0x20
#define FA__DIRTY			0x40
*/
  // TODO
    if(f_open(&tmp_fil, path, FA_OPEN_EXISTING | FA_READ) != FR_OK)
        return -1;

    snprintf(tmp_file_name, sizeof(tmp_file_name), path);

    return 0;
}

struct stat {
    uint16_t  st_dev;
    uint16_t  st_ino;
    uint32_t  st_mode;
    uint16_t  st_nlink;
    uint16_t  st_uid;
    uint16_t  st_gid;
    uint16_t  st_rdev;
    size_t    st_size;    /* total size, in bytes */
    time_t    st_atime;
    long      st_spare1;
    time_t    st_mtime;
    long      st_spare2;
    time_t    st_ctime;
    long      st_spare3;
    long      st_blksize;
    long      st_blocks;
    long      st_spare4[2];
};

int stat(const char *path, struct stat *buf)
{
//    printf("[%s]\n", __FUNCTION__);
//    FILINFO info;
//    int ret = f_stat(path, &info);
    if(buf) {
        memset(buf, 0, sizeof(*buf));
        buf->st_size = tmp_fil.fsize;//info.fsize;
//        buf->st_mtime = info.ftime;
    }
    return 0;
}

int fstat(int fd, struct stat *buf)
{
//  printf("[%s]\n", __FUNCTION__);
//  FILINFO info;
//  int ret = f_stat(tmp_file_name, &info);
  if(buf) {
    memset(buf, 0, sizeof(*buf));
    buf->st_size = tmp_fil.fsize;//info.fsize;
//    buf->st_mtime = info.ftime;
//      buf-> = info.fdate;
//      buf->st_mode = info.fattrib;
//      buf-> = info.fname;
  }
  return 0;
}

#define	SEEK_SET	0
#define	SEEK_CUR	1
#define	SEEK_END	2
long lseek(int fd, long offset, int whence)
{
//  printf("[%s] whence: %d, offset: %d\n", __FUNCTION__, whence, offset);
  static int cur = 0;
  if(whence == SEEK_CUR)
    offset += cur;
  else if (whence == SEEK_END)
    offset += tmp_fil.fsize;
  if(f_lseek(&tmp_fil, offset) == FR_OK)
    cur = offset;
//  printf("[%s] cur: %d, size: %d\n", __FUNCTION__, cur, tmp_fil.fsize);
  return cur;
}

size_t write(int fd, const void *buf, size_t count)
{
  printf("[%s]\n", __FUNCTION__);
  UINT size = 0;
  f_write(&tmp_fil, buf, count, &size);
  return size;
}

int unlink(const char *pathname)
{
  printf("[%s]\n", __FUNCTION__);
  return 0;
}

char *tempnam(const char *dir, const char *pfx)
{
  printf("[%s]\n", __FUNCTION__);
  return NULL;
}

int rmdir(const char *pathname)
{
  printf("[%s]\n", __FUNCTION__);
  return 0;
}

int mkdir(const char *path, int mode)
{
  printf("[%s]\n", __FUNCTION__);
  return 0;
}

void __assert_func(const char *_file, int _line, const char *_func, const char *_expr)
{
  printf("[%s]\n", __FUNCTION__);
}

static void log_cb(void* ptr, int level, const char* fmt, va_list vl)
{
  if(level > AV_LOG_VERBOSE)
    return;
  vprintf(fmt, vl);
}

//#define SDMMC_READ_SPEED
static void av_task_cb(void *arg)
{
  av_queue_t m;
  int vframe_finished = 0, aframe_finished = 0, audio_playing = 0;
  AVFrame *aframe = av_frame_alloc();

  while(1) {
    xQueueReceive(av_queue, &m, portMAX_DELAY);
#ifndef SDMMC_READ_SPEED
    if(m.pCodecCtx->codec_type == AVMEDIA_TYPE_VIDEO) {
        // Decode video frame
        avcodec_decode_video2(m.pCodecCtx, m.pFrame, &vframe_finished, m.packet);
        // Did we get a video frame?
        if(vframe_finished) {
            ScaleYUV2RGB565(m.pFrame->data[0], m.pFrame->data[1], m.pFrame->data[2],
                            (uint8_t*)lcd_fb_start,
                            m.pCodecCtx->width, m.pCodecCtx->height, LCD_X_SIZE, LCD_Y_SIZE,
                            m.pFrame->linesize[0], m.pFrame->linesize[1], 2*LCD_X_SIZE,
                            (YUVType)m.pCodecCtx->pix_fmt);
    //        BSP_LCD_SetLayerAddress(0, (uint32_t)p->pFrame->data[1]);
            fps++;
        }
    }
    if(m.pCodecCtx->codec_type == AVMEDIA_TYPE_AUDIO) {
        // Decode audio frame
        m.pCodecCtx->request_sample_fmt = AV_SAMPLE_FMT_S16;
        while(avcodec_decode_audio4(m.pCodecCtx, aframe, &aframe_finished, m.packet) > 0) {
            if(aframe_finished) {

              size_t size_in = av_samples_get_buffer_size(NULL, aframe->channels,
                  aframe->nb_samples, m.pCodecCtx->sample_fmt, 0);
              size_t size_out = av_samples_get_buffer_size(NULL, 2,
                  aframe->nb_samples, AV_SAMPLE_FMT_S16, 0);

              uint64_t layout = aframe->channel_layout ? aframe->channel_layout : AV_CH_LAYOUT_MONO;
              struct SwrContext *ctx = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_STEREO,
                  AV_SAMPLE_FMT_S16, aframe->sample_rate, layout,
                  m.pCodecCtx->sample_fmt, aframe->sample_rate, 0, NULL);

              audio_queue_t am;
              am.size = size_out;
              am.data = memalign(4, am.size);

              if(swr_init(ctx) == 0)
                swr_convert(ctx, &am.data, aframe->nb_samples, &aframe->data[0], aframe->nb_samples);

              swr_close(ctx);
              swr_free(&ctx);

                if(!audio_playing) {
                    audio_playing = 1;
                    BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO, 25, AUDIO_FREQUENCY_11K/*aframe->sample_rate*/); // TODO: add sample_rate to AUDIO_FREQ conversion
                    BSP_AUDIO_OUT_Play((uint16_t*)am.data, am.size);
                    // TODO: free am.data by e.g. timer, add locks to RTOS
//                    audio_frame = am.data;
                } else {
                    xQueueSendToBack(audio_queue, &am, portMAX_DELAY);
                }
                break;
            }
        }
    }
    av_free_packet(m.packet);
#endif // SDMMC_READ_SPEED
  }
}

void BSP_AUDIO_OUT_TransferComplete_CallBack(void)
{
  audio_queue_t am;
  if(xQueueReceiveFromISR(audio_queue, &am, NULL)) {
    free(am.data);
  }
}

void BSP_AUDIO_OUT_HalfTransfer_CallBack(void)
{
  audio_queue_t am;
  if(xQueuePeekFromISR(audio_queue, &am)) {
    BSP_AUDIO_OUT_PlayNext((uint16_t*)am.data, am.size);
  }
}

/*
    BSP_LCD_SetLayerAddress(0, (uint32_t)data);
    if(data_prev)
      free(data_prev);
    data_prev = data;

    if(!data) {
      // DMA2D completed
      BSP_LCD_SetLayerAddress(0, frameBufferAddress);
      continue;
    }
    HAL_DMA2D_Start_IT(&Dma2dHandle, (uint32_t)data, frameBufferAddress,
                       LCD_X_SIZE, LCD_Y_SIZE);
    vTaskDelay(LCD_FRAME_DELAY);
*/
static int mount_sdcard()
{
  if(FATFS_LinkDriver(&SD_Driver, SDPath) != FR_OK)
    return -1;
  if(f_mount(&SDFatFs, (TCHAR const*)SDPath, 0) != FR_OK)
    return -2;
  return 0;
}

static void reader_task_cb(void *arg)
{
  int count = 0;

  while(1) {
#ifdef SDMMC_READ_SPEED
    mount_sdcard();
    open("test2.mp4", 0);

    char *buff = memalign(4, 32768);
    printf("buff: %x\n", buff);

    size_t size = 0, sz;
    uint32_t crc = 0;

    uint32_t t0 = xTaskGetTickCount();
    uint32_t sec0 = t0 >> 10;
    while((sz = read(0, buff, 32768)) > 0) {
      size += sz;

      int i;
      for(i = 0; i < sz; i++)
        crc += buff[i];

      uint32_t sec = xTaskGetTickCount() >> 10;
      if(sec0 != sec) {
        sec0 = sec;
        uint32_t time = xTaskGetTickCount() - t0;
        printf("time: %d, %f kB/s\n", time, (float)size/(float)time);
        printf("cpu usage: %d %%\n", osGetCPUUsage());
        size = 0;
        t0 = xTaskGetTickCount();
      }
    }
    printf("crc: %x\n", crc);
    free(buff);
#else
    const char *filenames[3] = {"test6.avi", "H264_test1_480x360.mp4", "MP4_640x360.mp4"};
    const char *filename = filenames[count++];
    if (count > 2)
      count = 0;

//    const char *filename = "test6.avi";
//    const char *filename = "H264_test1_480x360.mp4";
//    const char *filename = "MP4_640x360.mp4";
//    printf("\nEnter filename:\n");
//    char filename[64] = {0};
//    scanf("%s", filename);
    printf("Playing: %s\n", filename);

    AVFormatContext *pFormatCtx = NULL;

    av_log_set_callback(&log_cb);
    av_register_all();

    pFormatCtx = avformat_alloc_context();
    if (!pFormatCtx) {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate context.\n");
        Error_Handler();
    }
    pFormatCtx->interrupt_callback.callback = decode_interrupt_cb;
    pFormatCtx->interrupt_callback.opaque = NULL;
    if (!av_dict_get(format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE)) {
        av_dict_set(&format_opts, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
    }

//    if (!av_dict_get(format_opts, "analyzeduration", NULL, AV_DICT_MATCH_CASE))
//        av_dict_set(&format_opts, "analyzeduration", "1000000", AV_DICT_DONT_OVERWRITE);
//    if (!av_dict_get(format_opts, "probesize", NULL, AV_DICT_MATCH_CASE))
//        av_dict_set(&format_opts, "probesize", "1000000", AV_DICT_DONT_OVERWRITE);

    mount_sdcard();

    if(avformat_open_input(&pFormatCtx, filename, NULL, &format_opts) != 0)
       Error_Handler();

    if(avformat_find_stream_info(pFormatCtx, NULL/*&format_opts*/) < 0)
       Error_Handler();

    av_dump_format(pFormatCtx, 0, filename, 0);

    // Find the first audio and video streams
    int i, videoStream=-1, audioStream=-1;
    for(i = 0; i < pFormatCtx->nb_streams; i++) {
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO
           && videoStream < 0) {
             videoStream = i;
        }
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO
           && audioStream < 0) {
             audioStream = i;
        }
    }

    if(videoStream == -1 || audioStream == -1)
        Error_Handler(); // No video or audio stream found

    // Get pointers to the codec context for audio and video streams
    AVCodecContext *pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;
    AVCodecContext *aCodecCtxOrig = pFormatCtx->streams[audioStream]->codec;

    // Find the decoder for the video stream
    AVCodec *pCodec = avcodec_find_decoder(pCodecCtxOrig->codec_id);
    if(pCodec == NULL) {
      printf("Unsupported video codec!\n");
      Error_Handler(); // Codec not found
    }

    // Find the decoder for the audio stream
    AVCodec *aCodec = avcodec_find_decoder(aCodecCtxOrig->codec_id);
    if(aCodec == NULL) {
      printf("Unsupported audio codec!\n");
      Error_Handler(); // Codec not found
    }

    // Copy context
    AVCodecContext *pCodecCtx = avcodec_alloc_context3(pCodec);
    if(avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
      printf("Couldn't copy video codec context");
      Error_Handler(); // Error copying codec context
    }

    // Copy context
    AVCodecContext *aCodecCtx = avcodec_alloc_context3(aCodec);
    if(avcodec_copy_context(aCodecCtx, aCodecCtxOrig) != 0) {
      printf("Couldn't copy audio codec context");
      Error_Handler(); // Error copying codec context
    }

    // Open codec
    if(avcodec_open2(pCodecCtx, pCodec, &format_opts) < 0 ||
       avcodec_open2(aCodecCtx, aCodec, NULL))
      Error_Handler(); // Could not open codec

    // Allocate video frame
    AVFrame *pFrame = av_frame_alloc();

#ifdef FF_SCALE
    int width = LCD_X_SIZE;
    int height = LCD_Y_SIZE;
    // Allocate an AVFrame structure
    AVFrame *pFrameRGB = av_frame_alloc();
    if(pFrameRGB == NULL)
      Error_Handler();

    uint8_t *buffer = NULL;
    int numBytes;
    // Determine required buffer size and allocate buffer
    numBytes=avpicture_get_size(PIX_FMT_RGB565LE, width, height);
    buffer = (uint8_t*)malloc(numBytes);

    // Assign appropriate parts of buffer to image planes in pFrameRGB
    // Note that pFrameRGB is an AVFrame, but AVFrame is a superset
    // of AVPicture
    avpicture_fill((AVPicture *)pFrameRGB, buffer, PIX_FMT_RGB565LE, width, height);

    struct SwsContext *sws_ctx = NULL;
    // initialize SWS context for software scaling
    sws_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, width, height,
                             PIX_FMT_RGB565LE, SWS_FAST_BILINEAR, NULL, NULL, NULL);

    BSP_LCD_LayerRgb565Init(0, (uint32_t)pFrameRGB->data[0]);
    BSP_LCD_SetLayerAddress(0, (uint32_t)pFrameRGB->data[0]);
    BSP_LCD_DisplayOn();
#endif // FF_SCALE

    AVPacket packet;
    uint32_t sec0 = xTaskGetTickCount() >> 10;

    while(av_read_frame(pFormatCtx, &packet)>=0) {
      // Is this a packet from the video stream?
//      if(packet.stream_index == videoStream)
      {
#if 1
        // Send to av task
        av_queue_t data;
        data.pFrame = pFrame;
        if(packet.stream_index == videoStream) {
          data.pCodecCtx = pCodecCtx;
        } else if(packet.stream_index == audioStream) {
          data.pCodecCtx = aCodecCtx;
        }
        data.packet = &packet;
        xQueueSendToBack(av_queue, &data, portMAX_DELAY);
#else
        int frameFinished;
        // Decode video frame
        avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

        // Did we get a video frame?
        if(frameFinished) {
          // Acquire LCD buffer
//          xSemaphoreTake(lcd_sema, portMAX_DELAY);
          // Switch framebuffer
//          BSP_LCD_SetLayerAddress(0, (uint32_t)lcd_fb_start);
          // Convert the image from its native format to RGB

//          sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data, pFrame->linesize, 0,
//                    pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);

          ScaleYCbCrToRGB565(pFrame->data[0], pFrame->data[1], pFrame->data[2],
                             (uint8_t*)lcd_fb_start,
                             0, 0,
                             pCodecCtx->width, pCodecCtx->height,
                             width, height,
                             pFrame->linesize[0], pFrame->linesize[1], 2*width/*pFrameRGB->linesize[0]*/,
                             pCodecCtx->pix_fmt,
                             FILTER_BILINEAR
                             );
          BSP_LCD_SetLayerAddress(0, (uint32_t)lcd_fb_start);

          // Send the frame
//          BSP_LCD_SetLayerAddress(0, (uint32_t)pFrameRGB->data[0]);
//          HAL_DMA2D_Start_IT(&Dma2dHandle, (uint32_t)pFrame/*RGB*/->data[0], (uint32_t)lcd_fb_start,
//                             LCD_X_SIZE, LCD_Y_SIZE);
//            xQueueSendToBack(lcd_queue, &pFrameRGB->data[0], portMAX_DELAY);
        }
#endif
      }
      // Free the packet that was allocated by av_read_frame
//      av_free_packet(&packet);

      uint32_t sec = xTaskGetTickCount() >> 10;
      if(sec0 != sec) {
        sec0 = sec;
        printf("cpu usage: %d %%, fps: %d\n", osGetCPUUsage(), fps);
        fps = 0;
      }

    }
#ifdef FF_SCALE
    // Free the RGB image
    av_free(buffer);
    av_free(pFrameRGB);
#endif
    // Free the YUV frame
    av_free(pFrame);

    // Close the codecs
    avcodec_close(pCodecCtx);
    avcodec_close(pCodecCtxOrig);

    // Close the video file
    avformat_close_input(&pFormatCtx);
#endif // SDMMC read speed
  }
}

#if 0
static void TransferComplete(DMA2D_HandleTypeDef *hdma2d)
{
  xSemaphoreGiveFromISR(lcd_sema, NULL);
}

static void TransferError(DMA2D_HandleTypeDef *hdma2d)
{
  Error_Handler();
}

static void DMA2D_Config(int32_t x_size, int32_t x_size_orig, uint32_t ColorMode)
{
  /* Configure the DMA2D Mode, Color Mode and output offset */
  Dma2dHandle.Init.Mode         = DMA2D_M2M_PFC; /* DMA2D mode Memory to Memory with Pixel Format Conversion */
  Dma2dHandle.Init.ColorMode    = DMA2D_RGB565; /* DMA2D Output color mode is RGB565 (16 bpp) */
  Dma2dHandle.Init.OutputOffset = (LCD_X_SIZE - x_size) ; /* No offset in output */

  /* DMA2D Callbacks Configuration */
  Dma2dHandle.XferCpltCallback  = TransferComplete;
  Dma2dHandle.XferErrorCallback = TransferError;

  /* Foreground layer Configuration : layer 1 */
  Dma2dHandle.LayerCfg[1].AlphaMode = DMA2D_REPLACE_ALPHA;
  Dma2dHandle.LayerCfg[1].InputAlpha = 0xFF;
  Dma2dHandle.LayerCfg[1].InputColorMode = ColorMode; /* Layer 1 input format */
  Dma2dHandle.LayerCfg[1].InputOffset = x_size_orig - x_size ; /* No offset in input */

   /* Background layer Configuration */
  Dma2dHandle.LayerCfg[0].AlphaMode = DMA2D_REPLACE_ALPHA;
  Dma2dHandle.LayerCfg[0].InputAlpha = 0; /* 127 : semi-transparent */
  Dma2dHandle.LayerCfg[0].InputColorMode = ColorMode;
  Dma2dHandle.LayerCfg[0].InputOffset = (LCD_X_SIZE - x_size) ; /* No offset in input */

  Dma2dHandle.Instance = DMA2D;

  /* DMA2D Initialization */
  if(HAL_DMA2D_Init(&Dma2dHandle) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler();
  }

  if(HAL_DMA2D_ConfigLayer(&Dma2dHandle, 1) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler();
  }

   if(HAL_DMA2D_ConfigLayer(&Dma2dHandle, 0) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler();
  }
}
#endif
/*
typedef struct xTASK_PARAMETERS
{
    TaskFunction_t pvTaskCode;
    const signed char * const pcName;
    unsigned short usStackDepth;
    void *pvParameters;
    UBaseType_t uxPriority;
    portSTACK_TYPE *puxStackBuffer;
    MemoryRegion_t xRegions[ portNUM_CONFIGURABLE_REGIONS ];
} TaskParameters_t;

typedef struct xMEMORY_REGION
{
    void *pvBaseAddress;
    unsigned long ulLengthInBytes;
    unsigned long ulParameters;
} MemoryRegion_t;
*/
TaskParameters_t av_task_param = {
  .pvTaskCode = av_task_cb,
  .pcName = "av_task",
  .usStackDepth = AV_TASK_STACK,
  .uxPriority = AV_TASK_PRIORITY,
  .puxStackBuffer = av_task_stack,
  .xRegions = {NULL}
};

TaskParameters_t reader_task_param = {
  .pvTaskCode = reader_task_cb,
  .pcName = "reader_task",
  .usStackDepth = READER_TASK_STACK,
  .uxPriority = READER_TASK_PRIORITY,
  .puxStackBuffer = reader_task_stack,
  .xRegions = {NULL}//(void*)0x20000000, 0x80000000, portMPU_REGION_READ_WRITE | portMPU_REGION_CACHEABLE}
};

int play_init(void)
{
    // LCD Init
    BSP_LCD_Init();
#if 1
    lcd_fb_start = malloc(FRAME_BUFFER_SIZE);
    ASSERT(lcd_fb_start);

    BSP_LCD_LayerRgb565Init(0, (uint32_t)lcd_fb_start);
//    BSP_LCD_LayerRgb888Init(0, (uint32_t)lcd_fb_start);
    BSP_LCD_DisplayOn();
//    DMA2D_Config(LCD_X_SIZE, LCD_X_SIZE, CM_RGB565);
    BSP_LCD_SetLayerAddress(0, (uint32_t)lcd_fb_start);
#endif
    // Create audio/video task
//    xTaskCreate(av_task_cb, "av_task", AV_TASK_STACK, NULL, AV_TASK_PRIORITY, &av_task);
//    ASSERT(av_task);
    xTaskCreateRestricted(&av_task_param, NULL);

#if 0
    // Create LCD semaphore
    lcd_sema = xSemaphoreCreateBinary();
    ASSERT(lcd_sema);
    xSemaphoreGive(lcd_sema);
#endif
    // Create av queue
    av_queue = xQueueCreate(AV_QUEUE_LENGTH, sizeof(av_queue_t));
    ASSERT(av_queue);

    // Create audio queue
    audio_queue = xQueueCreate(AUDIO_QUEUE_LENGTH, sizeof(audio_queue_t));
    ASSERT(audio_queue);

    // Create reader task
//    xTaskCreate(reader_task_cb, "reader_task", READER_TASK_STACK, NULL, READER_TASK_PRIORITY,
//                &reader_task);
//    ASSERT(reader_task);
    xTaskCreateRestricted(&reader_task_param, NULL);

    int ret = BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO, 25, I2S_AUDIOFREQ_48K);
    ASSERT(ret == 0);
    BSP_AUDIO_OUT_SetAudioFrameSlot(CODEC_AUDIOFRAME_SLOT_02);


    // Create audio task

    return 0;
}
#if 0
void HAL_DMA2D_MspInit(DMA2D_HandleTypeDef *hdma2d)
{
  __HAL_RCC_DMA2D_CLK_ENABLE();
  /* NVIC configuration for DMA2D transfer complete interrupt */
  HAL_NVIC_SetPriority(DMA2D_IRQn, 15, 15);
  HAL_NVIC_EnableIRQ(DMA2D_IRQn);
}
#endif


/**
  * @brief  Clock Config.
  * @param  hsai: might be required to set audio peripheral predivider if any.
  * @param  AudioFreq: Audio frequency used to play the audio stream.
  * @note   This API is called by BSP_AUDIO_OUT_Init() and BSP_AUDIO_OUT_SetFrequency()
  *         Being __weak it can be overwritten by the application
  * @retval None
  */
void BSP_AUDIO_OUT_ClockConfig(SAI_HandleTypeDef *hsai, uint32_t AudioFreq, void *Params)
{
  RCC_PeriphCLKInitTypeDef RCC_ExCLKInitStruct;

  HAL_RCCEx_GetPeriphCLKConfig(&RCC_ExCLKInitStruct);

  /* Set the PLL configuration according to the audio frequency */
  if((AudioFreq == AUDIO_FREQUENCY_11K) || (AudioFreq == AUDIO_FREQUENCY_22K) || (AudioFreq == AUDIO_FREQUENCY_44K))
  {
    /* Configure PLLSAI prescalers */
    /* PLLI2S_VCO: VCO_429M
    SAI_CLK(first level) = PLLI2S_VCO/PLLSAIQ = 429/2 = 214.5 Mhz
    SAI_CLK_x = SAI_CLK(first level)/PLLI2SDivQ = 214.5/19 = 11.289 Mhz */
    RCC_ExCLKInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SAI2;
    RCC_ExCLKInitStruct.Sai2ClockSelection = RCC_SAI2CLKSOURCE_PLLI2S;
    RCC_ExCLKInitStruct.PLLI2S.PLLI2SP = 8;
    RCC_ExCLKInitStruct.PLLI2S.PLLI2SN = 429;
    RCC_ExCLKInitStruct.PLLI2S.PLLI2SQ = 2;
    RCC_ExCLKInitStruct.PLLI2SDivQ = 19;
    HAL_RCCEx_PeriphCLKConfig(&RCC_ExCLKInitStruct);
  }
  else /* AUDIO_FREQUENCY_8K, AUDIO_FREQUENCY_16K, AUDIO_FREQUENCY_48K), AUDIO_FREQUENCY_96K */
  {
    /* SAI clock config
    PLLI2S_VCO: VCO_344M
    SAI_CLK(first level) = PLLI2S_VCO/PLLSAIQ = 344/7 = 49.142 Mhz
    SAI_CLK_x = SAI_CLK(first level)/PLLI2SDivQ = 49.142/1 = 49.142 Mhz */
    RCC_ExCLKInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SAI2;
    RCC_ExCLKInitStruct.Sai2ClockSelection = RCC_SAI2CLKSOURCE_PLLI2S;
//    RCC_ExCLKInitStruct.PLLI2S.PLLI2SP = 8;
    RCC_ExCLKInitStruct.PLLI2S.PLLI2SN = 344;
    RCC_ExCLKInitStruct.PLLI2S.PLLI2SQ = 7;
    RCC_ExCLKInitStruct.PLLI2SDivQ = 1;
    HAL_RCCEx_PeriphCLKConfig(&RCC_ExCLKInitStruct);
  }
}

/**
  * @brief  Clock Config.
  * @param  hltdc: LTDC handle
  * @note   This API is called by BSP_LCD_Init()
  * @retval None
  */
void BSP_LCD_ClockConfig(LTDC_HandleTypeDef *hltdc, void *Params)
{
  static RCC_PeriphCLKInitTypeDef  periph_clk_init_struct;

  /* RK043FN48H LCD clock configuration */
  /* PLLSAI_VCO Input = HSE_VALUE/PLL_M = 1 Mhz */
  /* PLLSAI_VCO Output = PLLSAI_VCO Input * PLLSAIN = 192 Mhz */
  /* PLLLCDCLK = PLLSAI_VCO Output/PLLSAIR = 192/5 = 38.4 Mhz */
  /* LTDC clock frequency = PLLLCDCLK / LTDC_PLLSAI_DIVR_4 = 38.4/4 = 9.6Mhz */
  periph_clk_init_struct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
  periph_clk_init_struct.PLLSAI.PLLSAIN = 192;
  periph_clk_init_struct.PLLSAI.PLLSAIR = RK043FN48H_FREQUENCY_DIVIDER;
  periph_clk_init_struct.PLLSAIDivR = RCC_PLLSAIDIVR_4;
  HAL_RCCEx_PeriphCLKConfig(&periph_clk_init_struct);
}
