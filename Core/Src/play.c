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

#define READER_TASK_PRIORITY  8
#define READER_TASK_STACK     (5*1024)

#define AUDIO_TASK_STACK      (256)
#define AUDIO_TASK_PRIORITY   9
#define VIDEO_TASK_STACK      (256)
#define VIDEO_TASK_PRIORITY   9

#define AUDIO_QUEUE_LENGTH    16
#define VIDEO_QUEUE_LENGTH    2

#define AUDIO_SEMA_TIMEOUT    2000
#define AV_START_DELAY        1

#define countof(a)  (sizeof(a)/sizeof(a[0]))

#pragma location = "__iram"
#pragma data_alignment=32
uint32_t reader_task_stack[READER_TASK_STACK];

#pragma location = "__iram"
#pragma data_alignment=32
uint32_t video_task_stack[VIDEO_TASK_STACK];

typedef struct {
    uint8_t *data;
    size_t size;
} av_queue_t;

static QueueHandle_t        audio_queue;
static QueueHandle_t        video_queue;

static TaskHandle_t         audio_task;
//static TaskHandle_t         video_task;

static SemaphoreHandle_t    audio_sema;
static SemaphoreHandle_t    video_sema;

static TimerHandle_t        video_timer;

DMA2D_HandleTypeDef Dma2dHandle;
volatile char *lcd_fb_start = NULL;
volatile char *lcd_fb_active = NULL;
static int fps = 0;

AVDictionary *format_opts = NULL;
void *_impure_ptr = NULL;

extern void* __iar_dlmemalign(uint32_t, uint32_t);
void *memalign(size_t align, size_t size)
{
  void * p = __iar_dlmemalign(align, size);
  if(!p)
    printf("[%s] Can't allocate %d bytes\n", __FUNCTION__, size);
  return p;
}
#if 0
typedef struct {
  int   tm_sec;
  int   tm_min;
  int   tm_hour;
  int   tm_mday;
  int   tm_mon;
  int   tm_year;
  int   tm_wday;
  int   tm_yday;
  int   tm_isdst;
} ff_tm;

ff_tm *localtime_r(const time_t* t, ff_tm *r)
{
  printf("[%s]\n", __FUNCTION__);
    ff_tm *p = (ff_tm*)localtime(t);
    if (!p)
        return NULL;
    *r = *p;
    return r;
}

ff_tm *gmtime_r(const time_t* t, ff_tm *r)
{
  printf("[%s]\n", __FUNCTION__);
    ff_tm *p = (ff_tm*)gmtime(t);
    if (!p)
        return NULL;
    *r = *p;
    return r;
}

int	__xpg_strerror_r(int a, char *s, size_t len)
{
  printf("[%s]\n", __FUNCTION__);
  return -1;
}

int gettimeofday(struct timeval *p, void *tz)
{
  printf("[%s]\n", __FUNCTION__);
  return -1;
}

int fcntl (int fd, int cmd, int arg)
{
  printf("[%s]\n", __FUNCTION__);
  return -1;
}

int access(const char *path, int amode)
{
  printf("[%s]\n", __FUNCTION__);
  return -1;
}

int	mkstemp(char *s)
{
  printf("[%s]\n", __FUNCTION__);
  return -1;
}

int	isatty(int fildes)
{
  printf("[%s]\n", __FUNCTION__);
  return -1;
}
#endif //0

int __errno(int e)
{
  printf("[%s]: %i\n", __FUNCTION__, e);
  return 0;
}

int __fpclassifyf (float x)
{
//  printf("[%s]\n", __FUNCTION__);
    return fpclassify(x);
}

int __fpclassifyd (double x)
{
//  printf("[%s]\n", __FUNCTION__);
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
//  printf("[%s]\n", __FUNCTION__);
  static int cur = 0;
  if(whence == SEEK_CUR)
    offset += cur;
  else if (whence == SEEK_END)
    offset += tmp_fil.fsize;
  if(f_lseek(&tmp_fil, offset) == FR_OK)
    cur = offset;
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

static void audio_task_cb(void *arg)
{
  av_queue_t m[2] = {0};
  vTaskDelay(AV_START_DELAY);

  while(1) {
    free(m[0].data);
    xQueueReceive(audio_queue, &m[0], portMAX_DELAY);
//    BSP_AUDIO_OUT_PlayNext((uint16_t*)m[0].data, m[0].size);
//    xSemaphoreTake(audio_sema, AUDIO_SEMA_TIMEOUT);
    free(m[1].data);
    xQueueReceive(audio_queue, &m[1], portMAX_DELAY);
//    BSP_AUDIO_OUT_PlayNext((uint16_t*)m[1].data, m[1].size);
//    xSemaphoreTake(audio_sema, AUDIO_SEMA_TIMEOUT);
  }
}

void BSP_AUDIO_OUT_HalfTransfer_CallBack(void)
{
  xSemaphoreGiveFromISR(audio_sema, NULL);
}

static void video_timer_cb(TimerHandle_t pxTimer)
{
  xSemaphoreGiveFromISR(video_sema, NULL);
}

static void video_task_cb(void *arg)
{
  av_queue_t m;
  void *data_prev = NULL;
//  vTaskDelay(AV_START_DELAY);

  BSP_LCD_LayerRgb565Init(0, (uint32_t)0);
  BSP_LCD_DisplayOn();

  while(1) {
    xQueueReceive(video_queue, &m, portMAX_DELAY);
    BSP_LCD_SetLayerAddress(0, (uint32_t)m.data);
    free(data_prev);
    data_prev = m.data;
    xSemaphoreTake(video_sema, portMAX_DELAY);
    fps++;
  }
}

const uint32_t audio_freq[] = {
        AUDIO_FREQUENCY_192K, AUDIO_FREQUENCY_96K, AUDIO_FREQUENCY_48K,
        AUDIO_FREQUENCY_44K, AUDIO_FREQUENCY_32K, AUDIO_FREQUENCY_22K,
        AUDIO_FREQUENCY_16K, AUDIO_FREQUENCY_11K, AUDIO_FREQUENCY_8K
};

static uint32_t get_audio_freq(uint32_t sample_rate)
{
  uint32_t delta = -1;
  uint32_t index = 0;
  for(int i = 0; i < countof(audio_freq); i++) {
    uint32_t d = abs(sample_rate - audio_freq[i]);
    if(d ==0) {
      return audio_freq[i];
    } else if(d < delta) {
      index = i;
      delta = d;
    }
  }
  return audio_freq[index];
}

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

    char *buff = memalign(CACHE_LINE, 32768);
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
    const char *filenames[3] = {"MP4_640x360.mp4", "test6.avi", "H264_test1_480x360.mp4"};
    const char *filename = filenames[count++];
    if (count > 2)
      count = 0;

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
#if 0
    if (!av_dict_get(format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE)) {
        av_dict_set(&format_opts, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
    }
#endif
    mount_sdcard();

    if(avformat_open_input(&pFormatCtx, filename, NULL, NULL/*&format_opts*/) != 0)
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
    AVCodecContext *vCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;
    AVCodecContext *aCodecCtxOrig = pFormatCtx->streams[audioStream]->codec;

    // Find the decoder for the video stream
    AVCodec *vCodec = avcodec_find_decoder(vCodecCtxOrig->codec_id);
    if(vCodec == NULL) {
      printf("Unsupported video codec!\n");
      Error_Handler(); // Codec not found
    }

    // Find the decoder for the audio stream
    AVCodec *aCodec = avcodec_find_decoder(aCodecCtxOrig->codec_id);
    if(aCodec == NULL) {
      printf("Unsupported audio codec!\n");
      Error_Handler(); // Codec not found
    }

    // Copy video context
    AVCodecContext *vCodecCtx = avcodec_alloc_context3(vCodec);
    if(avcodec_copy_context(vCodecCtx, vCodecCtxOrig) != 0) {
      printf("Couldn't copy video codec context");
      Error_Handler(); // Error copying codec context
    }

    // Copy audio context
    AVCodecContext *aCodecCtx = avcodec_alloc_context3(aCodec);
    if(avcodec_copy_context(aCodecCtx, aCodecCtxOrig) != 0) {
      printf("Couldn't copy audio codec context");
      Error_Handler(); // Error copying codec context
    }

    // Open codec
    if(avcodec_open2(vCodecCtx, vCodec, NULL/*&format_opts*/) < 0 ||
       avcodec_open2(aCodecCtx, aCodec, NULL))
      Error_Handler(); // Could not open codec

    // Allocate video and audio frames
    AVFrame *vFrame = av_frame_alloc();
    AVFrame *aFrame = av_frame_alloc();

    AVPacket packet;
    int vframe_finished = 0, aframe_finished = 0;
    av_queue_t m;

    uint32_t sec0 = xTaskGetTickCount() >> 10;

    BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO, 25, get_audio_freq(aCodecCtx->sample_rate));

    int framerate = vCodecCtx->framerate.num ?
      vCodecCtx->framerate.den * 1000 / vCodecCtx->framerate.num :
      vCodecCtx->codec_id == CODEC_ID_H264 && vCodecCtx->time_base.num ?
      vCodecCtx->time_base.num * 50000 / vCodecCtx->time_base.den :
      vCodecCtx->pkt_timebase.den/vCodecCtx->pkt_timebase.num;

    xTimerChangePeriod(video_timer, framerate, 0);
    xTimerStart(video_timer, 0);

    while(av_read_frame(pFormatCtx, &packet) >= 0) {
      // Decode video packet
      if(packet.stream_index == videoStream) {
        avcodec_decode_video2(vCodecCtx, vFrame, &vframe_finished, &packet);
        if(vframe_finished) {
          while(!(m.data = memalign(4, FRAME_BUFFER_SIZE)))
            vTaskDelay(10);
          m.size = FRAME_BUFFER_SIZE;
          ScaleYUV2RGB565(vFrame->data[0], vFrame->data[1], vFrame->data[2], m.data,
                          vCodecCtx->width, vCodecCtx->height, LCD_X_SIZE, LCD_Y_SIZE,
                          vFrame->linesize[0], vFrame->linesize[1], 2*LCD_X_SIZE,
                          (YUVType)vCodecCtx->pix_fmt);
          xQueueSendToBack(video_queue, &m, portMAX_DELAY);
        }
      } else
        // Decode audio packet
        if(packet.stream_index == audioStream) {
        aCodecCtx->request_sample_fmt = AV_SAMPLE_FMT_S16;
        while(avcodec_decode_audio4(aCodecCtx, aFrame, &aframe_finished, &packet) > 0) {
            if(aframe_finished) {

              size_t size_in = av_samples_get_buffer_size(NULL, aFrame->channels,
                  aFrame->nb_samples, aCodecCtx->sample_fmt, 0);
              size_t size_out = av_samples_get_buffer_size(NULL, 2,
                  aFrame->nb_samples, AV_SAMPLE_FMT_S16, 0);

              uint64_t layout = aFrame->channel_layout ? aFrame->channel_layout : AV_CH_LAYOUT_MONO;
              struct SwrContext *ctx = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_STEREO,
                  AV_SAMPLE_FMT_S16, aFrame->sample_rate, layout,
                  aCodecCtx->sample_fmt, aFrame->sample_rate, 0, NULL);

              m.size = size_out;
              while(!(m.data = memalign(4, size_out)))
                vTaskDelay(10);

              if(swr_init(ctx) == 0)
                swr_convert(ctx, &m.data, aFrame->nb_samples, (const uint8_t**)&aFrame->data[0],
                            aFrame->nb_samples);

              xQueueSendToBack(audio_queue, &m, portMAX_DELAY);
              swr_close(ctx);
              swr_free(&ctx);
              break;
            }
        }
      }

      av_free_packet(&packet);

      uint32_t sec = xTaskGetTickCount() >> 10;
      if(sec0 != sec) {
        sec0 = sec;
        printf("cpu usage: %d %%, fps: %d\n", osGetCPUUsage(), fps);
        fps = 0;
      }
    }

    // Free the YUV frame
    av_free(vFrame);
    av_free(aFrame);

    // Close the codecs
    avcodec_close(vCodecCtx);
    avcodec_close(vCodecCtxOrig);
    avcodec_close(aCodecCtx);
    avcodec_close(aCodecCtxOrig);

    // Close the video file
    avformat_close_input(&pFormatCtx);
#endif // SDMMC read speed
  }
}

TaskParameters_t reader_task_param = {
  .pvTaskCode = reader_task_cb,
  .pcName = "reader_task",
  .usStackDepth = READER_TASK_STACK,
  .uxPriority = READER_TASK_PRIORITY,
  .puxStackBuffer = reader_task_stack,
  .xRegions = {NULL}
};

TaskParameters_t video_task_param = {
  .pvTaskCode = video_task_cb,
  .pcName = "video_task",
  .usStackDepth = VIDEO_TASK_STACK,
  .uxPriority = VIDEO_TASK_PRIORITY,
  .puxStackBuffer = video_task_stack,
  .xRegions = {NULL}
};

int play_init(void)
{
    // LCD Init
    BSP_LCD_Init();

    // Create audio queue
    audio_queue = xQueueCreate(AUDIO_QUEUE_LENGTH, sizeof(av_queue_t));
    vQueueAddToRegistry(audio_queue, "audio_queue");
    ASSERT(audio_queue);

    // Create video queue
    video_queue = xQueueCreate(VIDEO_QUEUE_LENGTH, sizeof(av_queue_t));
    vQueueAddToRegistry(video_queue, "video_queue");
    ASSERT(video_queue);

    // Create audio task
    xTaskCreate(audio_task_cb, "audio_task", AUDIO_TASK_STACK, NULL,
                AUDIO_TASK_PRIORITY, &audio_task);
    ASSERT(audio_task);

    // Create video task
//    xTaskCreate(video_task_cb, "video_task", VIDEO_TASK_STACK, NULL,
//                VIDEO_TASK_PRIORITY, &video_task);
//    ASSERT(video_task);
    xTaskCreateRestricted(&video_task_param, NULL);

    // Create video task timer
    video_timer = xTimerCreate("video_timer", 10, pdTRUE, NULL, video_timer_cb);

    // Create audio and video semaphores
    audio_sema = xSemaphoreCreateBinary();
    ASSERT(audio_sema);
    video_sema = xSemaphoreCreateBinary();
    ASSERT(video_sema);

    // Create reader task
    xTaskCreateRestricted(&reader_task_param, NULL);

    BSP_AUDIO_OUT_SetAudioFrameSlot(CODEC_AUDIOFRAME_SLOT_02);

    return 0;
}

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

void Error_Handler(void)
{
  BSP_LED_Init(LED1);
  BSP_LED_On(LED1);
  while(1){}
}
