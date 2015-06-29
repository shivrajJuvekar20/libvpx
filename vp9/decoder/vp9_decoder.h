/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_DECODER_VP9_DECODER_H_
#define VP9_DECODER_VP9_DECODER_H_

#include "./vpx_config.h"

#include "vpx/vpx_codec.h"
#include "vpx_scale/yv12config.h"
#include "vp9/common/vp9_thread_common.h"
#include "vp9/common/vp9_onyxc_int.h"
#include "vp9/common/vp9_ppflags.h"
#include "vp9/common/vp9_thread.h"
#include "vp9/decoder/vp9_dthread.h"
#include "vp9/decoder/vp9_reader.h"

#ifdef __cplusplus
extern "C" {
#endif

// TODO(hkuang): combine this with TileWorkerData.
typedef struct TileData {
  VP9_COMMON *cm;
  vp9_reader bit_reader;
  DECLARE_ALIGNED(16, MACROBLOCKD, xd);
} TileData;

typedef struct TileWorkerData {
  struct VP9Decoder *pbi;
  vp9_reader bit_reader;
  FRAME_COUNTS counts;
  DECLARE_ALIGNED(16, MACROBLOCKD, xd);
  struct vpx_internal_error_info error_info;
} TileWorkerData;

typedef struct VP9Decoder {
  DECLARE_ALIGNED(16, MACROBLOCKD, mb);

  DECLARE_ALIGNED(16, VP9_COMMON, common);

  int ready_for_new_data;

  int refresh_frame_flags;

  int frame_parallel_decode;  // frame-based threading.

  // TODO(hkuang): Combine this with cur_buf in macroblockd as they are
  // the same.
  RefCntBuffer *cur_buf;   //  Current decoding frame buffer.

  VP9Worker *frame_worker_owner;   // frame_worker that owns this pbi.
  VP9Worker lf_worker;
  VP9Worker *tile_workers;
  TileWorkerData *tile_worker_data;
  TileInfo *tile_worker_info;
  int num_tile_workers;

  TileData *tile_data;
  int total_tiles;

  VP9LfSync lf_row_sync;

  vpx_decrypt_cb decrypt_cb;
  void *decrypt_state;

  int max_threads;
  int inv_tile_order;
  int need_resync;  // wait for key/intra-only frame.
  int hold_ref_buf;  // hold the reference buffer.

#if CONFIG_INTERNAL_STATS
  // total blocks in unit of 8x8 inside the frame.
  int total_block_in_8x8;
  // number of blocks using 1-D sub-pixel filtering for motion compensated
  // prediction. if a block is using sub-pixel filter in both vertical and
  // horizontal directions, it counts as using 1-D sub-pixel filter twice.
  int subpel_mc_block_in_4x4;
  // nubmer of blocks using intra prediction mode.
  int intra_block_in_8x8;
#endif
} VP9Decoder;

int vp9_receive_compressed_data(struct VP9Decoder *pbi,
                                size_t size, const uint8_t **dest);

int vp9_get_raw_frame(struct VP9Decoder *pbi, YV12_BUFFER_CONFIG *sd,
                      vp9_ppflags_t *flags);

vpx_codec_err_t vp9_copy_reference_dec(struct VP9Decoder *pbi,
                                       VP9_REFFRAME ref_frame_flag,
                                       YV12_BUFFER_CONFIG *sd);

vpx_codec_err_t vp9_set_reference_dec(VP9_COMMON *cm,
                                      VP9_REFFRAME ref_frame_flag,
                                      YV12_BUFFER_CONFIG *sd);

static INLINE uint8_t read_marker(vpx_decrypt_cb decrypt_cb,
                                  void *decrypt_state,
                                  const uint8_t *data) {
  if (decrypt_cb) {
    uint8_t marker;
    decrypt_cb(decrypt_state, data, &marker, 1);
    return marker;
  }
  return *data;
}

// This function is exposed for use in tests, as well as the inlined function
// "read_marker".
vpx_codec_err_t vp9_parse_superframe_index(const uint8_t *data,
                                           size_t data_sz,
                                           uint32_t sizes[8], int *count,
                                           vpx_decrypt_cb decrypt_cb,
                                           void *decrypt_state);

struct VP9Decoder *vp9_decoder_create(BufferPool *const pool);

void vp9_decoder_remove(struct VP9Decoder *pbi);

static INLINE void decrease_ref_count(int idx, RefCntBuffer *const frame_bufs,
                                      BufferPool *const pool) {
  if (idx >= 0) {
    --frame_bufs[idx].ref_count;
    // A worker may only get a free framebuffer index when calling get_free_fb.
    // But the private buffer is not set up until finish decoding header.
    // So any error happens during decoding header, the frame_bufs will not
    // have valid priv buffer.
    if (frame_bufs[idx].ref_count == 0 &&
        frame_bufs[idx].raw_frame_buffer.priv) {
      pool->release_fb_cb(pool->cb_priv, &frame_bufs[idx].raw_frame_buffer);
    }
  }
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_DECODER_VP9_DECODER_H_
