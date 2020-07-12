/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "video.h"
#include "../config/config.h"
#include "../error.h"
#include "rs-build-config.h"
#include <libavutil/avutil.h>
#include <libavutil/dict.h>

/**
 * Arrays for mapping `rsConfig.recordQuality` to seperate quality settings for each
 * encoder.
 */
static const char* encoderNvidiaPreset[] = {"fast", "medium", "slow"};
static const char* encoderVaapiQuality[] = {"30", "20", "10"};
static const char* encoderOpenH264QMin[] = {"30", "20", "10"};
static const char* encoderOpenH264QMax[] = {"40", "30", "20"};
#ifdef RS_CONFIG_X264
static const char* encoderX264Preset[] = {"ultrafast", "medium", "slower"};
#endif

void rsVideoEncoderCreate(RSEncoder* encoder, const RSInput* input) {
   int ret;
   AVDictionary* options = NULL;

   // TODO: all the video encoder settings here use low values. Add some sort of
   //       configuration to control these. Potentially a low/medium/high that translates
   //       to different values for different encoders?

   if (!rsConfig.disableNvidiaEncoder) {
      // Try to create a nVidia encoder. Note that while `nvenc` is technically a hardware
      // based encoder, it acts more like a software one in FFmpeg. It takes a normal
      // pixel format (does not require uploading frames) and does not require a hardware
      // device.
      // TODO: low/medium/high = fast/medium/slow
      av_dict_set(&options, "preset", encoderNvidiaPreset[rsConfig.recordQuality], 0);
      // clang-format off
      if ((ret = rsEncoderCreate(encoder, &(RSEncoderParams){
         .input = input,
         .name = "h264_nvenc",
         .format = AV_PIX_FMT_YUV420P,
         .options = &options
      })) >= 0) return;
      // clang-format on
      av_log(NULL, AV_LOG_WARNING, "Failed to create nVidia encoder: %s\n",
             av_err2str(ret));
   }

   if (!rsConfig.disableVaapiEncoder) {
      // Try to create a VAAPI encoder. The quality here is reveresed such that a higher
      // value equates to a lower quality.
      // TODO: low/medium/high = 30/20/10
      av_dict_set(&options, "global_quality", encoderVaapiQuality[rsConfig.recordQuality],
                  0);
      // clang-format off
      if ((ret = rsEncoderCreate(encoder, &(RSEncoderParams){
         .input = input,
         .name = "h264_vaapi",
         .format = AV_PIX_FMT_NV12,
         .options = &options,
         .hwType = AV_HWDEVICE_TYPE_VAAPI,
         .hwFormat = AV_PIX_FMT_VAAPI
      })) >= 0) return;
      // clang-format on
      av_log(NULL, AV_LOG_WARNING, "Failed to create VAAPI encoder: %s\n",
             av_err2str(ret));
   }

   if (!rsConfig.disableSoftwareEncoder) {
      // Try to create a software encoder.
      if ((ret = rsVideoEncoderCreateSW(encoder, input)) >= 0) return;
      av_log(NULL, AV_LOG_WARNING, "Failed to create software encoder: %s\n",
             av_err2str(ret));
   }

   rsError(AVERROR(ENOSYS));
}

int rsVideoEncoderCreateSW(RSEncoder* encoder, const RSInput* input) {
   int ret;
   AVDictionary* options = NULL;

   // Try to create a x264 encoder.
   // TODO: low/medium/high = ultrafast/medium/slower
   // I really do not like GPL licensing and I want to be safe :)
#ifdef RS_CONFIG_X264
   av_dict_set(&options, "preset", encoderX264Preset[rsConfig.recordQuality], 0);
   // clang-format off
   if ((ret = rsEncoderCreate(encoder, &(RSEncoderParams){
      .input = input,
      .name = "libx264",
      .format = AV_PIX_FMT_YUV420P,
      .options = &options
   })) >= 0) return 0;
   // clang-format on
   av_log(NULL, AV_LOG_WARNING, "Failed to create x264 encoder: %s\n", av_err2str(ret));
#else
   av_log(NULL, AV_LOG_WARNING,
          "Compile with GPL-licensed x264 support for better software encoding\n");
#endif

   // Try to create an OpenH264 encoder.
   // TODO: low/medium/high = 40-50/???/???
   av_dict_set(&options, "qmin", encoderOpenH264QMin[rsConfig.recordQuality], 0);
   av_dict_set(&options, "qmax", encoderOpenH264QMax[rsConfig.recordQuality], 0);
   // clang-format off
   if ((ret = rsEncoderCreate(encoder, &(RSEncoderParams){
      .input = input,
      .name = "libopenh264",
      .format = AV_PIX_FMT_YUV420P,
      .options = &options
   })) >= 0) return 0;
   // clang-format on
   av_log(NULL, AV_LOG_WARNING, "Failed to create OpenH264 encoder: %s\n",
          av_err2str(ret));

   return AVERROR(ENOSYS);
}