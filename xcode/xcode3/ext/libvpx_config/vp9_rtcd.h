#ifndef VP9_RTCD_H_
#define VP9_RTCD_H_

#ifdef RTCD_C
#define RTCD_EXTERN
#else
#define RTCD_EXTERN extern
#endif

/*
 * VP9
 */

#include "vpx/vpx_integer.h"

struct loop_filter_info;
struct blockd;
struct macroblockd;
struct loop_filter_info;

/* Encoder forward decls */
struct block;
struct macroblock;
struct vp9_variance_vtable;

#define DEC_MVCOSTS int *mvjcost, int *mvcost[2]
union int_mv;
struct yv12_buffer_config;

void vp9_dequant_idct_add_y_block_8x8_c(int16_t *q, const int16_t *dq, uint8_t *pre, uint8_t *dst, int stride, struct macroblockd *xd);
#define vp9_dequant_idct_add_y_block_8x8 vp9_dequant_idct_add_y_block_8x8_c

void vp9_dequant_idct_add_uv_block_8x8_c(int16_t *q, const int16_t *dq, uint8_t *pre, uint8_t *dstu, uint8_t *dstv, int stride, struct macroblockd *xd);
#define vp9_dequant_idct_add_uv_block_8x8 vp9_dequant_idct_add_uv_block_8x8_c

void vp9_dequant_idct_add_16x16_c(int16_t *input, const int16_t *dq, uint8_t *pred, uint8_t *dest, int pitch, int stride, int eob);
#define vp9_dequant_idct_add_16x16 vp9_dequant_idct_add_16x16_c

void vp9_dequant_idct_add_8x8_c(int16_t *input, const int16_t *dq, uint8_t *pred, uint8_t *dest, int pitch, int stride, int eob);
#define vp9_dequant_idct_add_8x8 vp9_dequant_idct_add_8x8_c

void vp9_dequant_idct_add_c(int16_t *input, const int16_t *dq, uint8_t *pred, uint8_t *dest, int pitch, int stride, int eob);
#define vp9_dequant_idct_add vp9_dequant_idct_add_c

void vp9_dequant_idct_add_y_block_c(int16_t *q, const int16_t *dq, uint8_t *pre, uint8_t *dst, int stride, struct macroblockd *xd);
#define vp9_dequant_idct_add_y_block vp9_dequant_idct_add_y_block_c

void vp9_dequant_idct_add_uv_block_c(int16_t *q, const int16_t *dq, uint8_t *pre, uint8_t *dstu, uint8_t *dstv, int stride, struct macroblockd *xd);
#define vp9_dequant_idct_add_uv_block vp9_dequant_idct_add_uv_block_c

void vp9_dequant_idct_add_32x32_c(int16_t *q, const int16_t *dq, uint8_t *pre, uint8_t *dst, int pitch, int stride, int eob);
#define vp9_dequant_idct_add_32x32 vp9_dequant_idct_add_32x32_c

void vp9_dequant_idct_add_uv_block_16x16_c(int16_t *q, const int16_t *dq, uint8_t *dstu, uint8_t *dstv, int stride, struct macroblockd *xd);
#define vp9_dequant_idct_add_uv_block_16x16 vp9_dequant_idct_add_uv_block_16x16_c

void vp9_copy_mem16x16_c(const uint8_t *src, int src_pitch, uint8_t *dst, int dst_pitch);
void vp9_copy_mem16x16_mmx(const uint8_t *src, int src_pitch, uint8_t *dst, int dst_pitch);
void vp9_copy_mem16x16_sse2(const uint8_t *src, int src_pitch, uint8_t *dst, int dst_pitch);
#define vp9_copy_mem16x16 vp9_copy_mem16x16_sse2

void vp9_copy_mem8x8_c(const uint8_t *src, int src_pitch, uint8_t *dst, int dst_pitch);
void vp9_copy_mem8x8_mmx(const uint8_t *src, int src_pitch, uint8_t *dst, int dst_pitch);
#define vp9_copy_mem8x8 vp9_copy_mem8x8_mmx

void vp9_copy_mem8x4_c(const uint8_t *src, int src_pitch, uint8_t *dst, int dst_pitch);
void vp9_copy_mem8x4_mmx(const uint8_t *src, int src_pitch, uint8_t *dst, int dst_pitch);
#define vp9_copy_mem8x4 vp9_copy_mem8x4_mmx

void vp9_recon_b_c(uint8_t *pred_ptr, int16_t *diff_ptr, uint8_t *dst_ptr, int stride);
#define vp9_recon_b vp9_recon_b_c

void vp9_recon_uv_b_c(uint8_t *pred_ptr, int16_t *diff_ptr, uint8_t *dst_ptr, int stride);
#define vp9_recon_uv_b vp9_recon_uv_b_c

void vp9_recon2b_c(uint8_t *pred_ptr, int16_t *diff_ptr, uint8_t *dst_ptr, int stride);
void vp9_recon2b_sse2(uint8_t *pred_ptr, int16_t *diff_ptr, uint8_t *dst_ptr, int stride);
#define vp9_recon2b vp9_recon2b_sse2

void vp9_recon4b_c(uint8_t *pred_ptr, int16_t *diff_ptr, uint8_t *dst_ptr, int stride);
void vp9_recon4b_sse2(uint8_t *pred_ptr, int16_t *diff_ptr, uint8_t *dst_ptr, int stride);
#define vp9_recon4b vp9_recon4b_sse2

void vp9_recon_mb_c(struct macroblockd *x);
#define vp9_recon_mb vp9_recon_mb_c

void vp9_recon_mby_c(struct macroblockd *x);
#define vp9_recon_mby vp9_recon_mby_c

void vp9_recon_mby_s_c(struct macroblockd *x, uint8_t *dst);
#define vp9_recon_mby_s vp9_recon_mby_s_c

void vp9_recon_mbuv_s_c(struct macroblockd *x, uint8_t *udst, uint8_t *vdst);
#define vp9_recon_mbuv_s vp9_recon_mbuv_s_c

void vp9_recon_sby_s_c(struct macroblockd *x, uint8_t *dst);
#define vp9_recon_sby_s vp9_recon_sby_s_c

void vp9_recon_sbuv_s_c(struct macroblockd *x, uint8_t *udst, uint8_t *vdst);
#define vp9_recon_sbuv_s vp9_recon_sbuv_s_c

void vp9_recon_sb64y_s_c(struct macroblockd *x, uint8_t *dst);
#define vp9_recon_sb64y_s vp9_recon_sb64y_s_c

void vp9_recon_sb64uv_s_c(struct macroblockd *x, uint8_t *udst, uint8_t *vdst);
#define vp9_recon_sb64uv_s vp9_recon_sb64uv_s_c

void vp9_build_intra_predictors_mby_s_c(struct macroblockd *x);
#define vp9_build_intra_predictors_mby_s vp9_build_intra_predictors_mby_s_c

void vp9_build_intra_predictors_sby_s_c(struct macroblockd *x);
#define vp9_build_intra_predictors_sby_s vp9_build_intra_predictors_sby_s_c

void vp9_build_intra_predictors_sbuv_s_c(struct macroblockd *x);
#define vp9_build_intra_predictors_sbuv_s vp9_build_intra_predictors_sbuv_s_c

void vp9_build_intra_predictors_mby_c(struct macroblockd *x);
#define vp9_build_intra_predictors_mby vp9_build_intra_predictors_mby_c

void vp9_build_intra_predictors_mby_s_c(struct macroblockd *x);
#define vp9_build_intra_predictors_mby_s vp9_build_intra_predictors_mby_s_c

void vp9_build_intra_predictors_mbuv_c(struct macroblockd *x);
#define vp9_build_intra_predictors_mbuv vp9_build_intra_predictors_mbuv_c

void vp9_build_intra_predictors_mbuv_s_c(struct macroblockd *x);
#define vp9_build_intra_predictors_mbuv_s vp9_build_intra_predictors_mbuv_s_c

void vp9_build_intra_predictors_sb64y_s_c(struct macroblockd *x);
#define vp9_build_intra_predictors_sb64y_s vp9_build_intra_predictors_sb64y_s_c

void vp9_build_intra_predictors_sb64uv_s_c(struct macroblockd *x);
#define vp9_build_intra_predictors_sb64uv_s vp9_build_intra_predictors_sb64uv_s_c

void vp9_intra4x4_predict_c(struct macroblockd *xd, struct blockd *x, int b_mode, uint8_t *predictor);
#define vp9_intra4x4_predict vp9_intra4x4_predict_c

void vp9_intra8x8_predict_c(struct macroblockd *xd, struct blockd *x, int b_mode, uint8_t *predictor);
#define vp9_intra8x8_predict vp9_intra8x8_predict_c

void vp9_intra_uv4x4_predict_c(struct macroblockd *xd, struct blockd *x, int b_mode, uint8_t *predictor);
#define vp9_intra_uv4x4_predict vp9_intra_uv4x4_predict_c

void vp9_add_residual_4x4_c(const int16_t *diff, const uint8_t *pred, int pitch, uint8_t *dest, int stride);
void vp9_add_residual_4x4_sse2(const int16_t *diff, const uint8_t *pred, int pitch, uint8_t *dest, int stride);
#define vp9_add_residual_4x4 vp9_add_residual_4x4_sse2

void vp9_add_residual_8x8_c(const int16_t *diff, const uint8_t *pred, int pitch, uint8_t *dest, int stride);
void vp9_add_residual_8x8_sse2(const int16_t *diff, const uint8_t *pred, int pitch, uint8_t *dest, int stride);
#define vp9_add_residual_8x8 vp9_add_residual_8x8_sse2

void vp9_add_residual_16x16_c(const int16_t *diff, const uint8_t *pred, int pitch, uint8_t *dest, int stride);
void vp9_add_residual_16x16_sse2(const int16_t *diff, const uint8_t *pred, int pitch, uint8_t *dest, int stride);
#define vp9_add_residual_16x16 vp9_add_residual_16x16_sse2

void vp9_add_residual_32x32_c(const int16_t *diff, const uint8_t *pred, int pitch, uint8_t *dest, int stride);
void vp9_add_residual_32x32_sse2(const int16_t *diff, const uint8_t *pred, int pitch, uint8_t *dest, int stride);
#define vp9_add_residual_32x32 vp9_add_residual_32x32_sse2

void vp9_add_constant_residual_8x8_c(const int16_t diff, const uint8_t *pred, int pitch, uint8_t *dest, int stride);
void vp9_add_constant_residual_8x8_sse2(const int16_t diff, const uint8_t *pred, int pitch, uint8_t *dest, int stride);
#define vp9_add_constant_residual_8x8 vp9_add_constant_residual_8x8_sse2

void vp9_add_constant_residual_16x16_c(const int16_t diff, const uint8_t *pred, int pitch, uint8_t *dest, int stride);
void vp9_add_constant_residual_16x16_sse2(const int16_t diff, const uint8_t *pred, int pitch, uint8_t *dest, int stride);
#define vp9_add_constant_residual_16x16 vp9_add_constant_residual_16x16_sse2

void vp9_add_constant_residual_32x32_c(const int16_t diff, const uint8_t *pred, int pitch, uint8_t *dest, int stride);
void vp9_add_constant_residual_32x32_sse2(const int16_t diff, const uint8_t *pred, int pitch, uint8_t *dest, int stride);
#define vp9_add_constant_residual_32x32 vp9_add_constant_residual_32x32_sse2

void vp9_loop_filter_mbv_c(uint8_t *y, uint8_t *u, uint8_t *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
void vp9_loop_filter_mbv_sse2(uint8_t *y, uint8_t *u, uint8_t *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
#define vp9_loop_filter_mbv vp9_loop_filter_mbv_sse2

void vp9_loop_filter_bv_c(uint8_t *y, uint8_t *u, uint8_t *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
void vp9_loop_filter_bv_sse2(uint8_t *y, uint8_t *u, uint8_t *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
#define vp9_loop_filter_bv vp9_loop_filter_bv_sse2

void vp9_loop_filter_bv8x8_c(uint8_t *y, uint8_t *u, uint8_t *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
void vp9_loop_filter_bv8x8_sse2(uint8_t *y, uint8_t *u, uint8_t *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
#define vp9_loop_filter_bv8x8 vp9_loop_filter_bv8x8_sse2

void vp9_loop_filter_mbh_c(uint8_t *y, uint8_t *u, uint8_t *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
void vp9_loop_filter_mbh_sse2(uint8_t *y, uint8_t *u, uint8_t *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
#define vp9_loop_filter_mbh vp9_loop_filter_mbh_sse2

void vp9_loop_filter_bh_c(uint8_t *y, uint8_t *u, uint8_t *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
void vp9_loop_filter_bh_sse2(uint8_t *y, uint8_t *u, uint8_t *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
#define vp9_loop_filter_bh vp9_loop_filter_bh_sse2

void vp9_loop_filter_bh8x8_c(uint8_t *y, uint8_t *u, uint8_t *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
void vp9_loop_filter_bh8x8_sse2(uint8_t *y, uint8_t *u, uint8_t *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
#define vp9_loop_filter_bh8x8 vp9_loop_filter_bh8x8_sse2

void vp9_loop_filter_simple_vertical_edge_c(uint8_t *y, int ystride, const uint8_t *blimit);
void vp9_loop_filter_simple_vertical_edge_mmx(uint8_t *y, int ystride, const uint8_t *blimit);
void vp9_loop_filter_simple_vertical_edge_sse2(uint8_t *y, int ystride, const uint8_t *blimit);
#define vp9_loop_filter_simple_mbv vp9_loop_filter_simple_vertical_edge_sse2

void vp9_loop_filter_simple_horizontal_edge_c(uint8_t *y, int ystride, const uint8_t *blimit);
void vp9_loop_filter_simple_horizontal_edge_mmx(uint8_t *y, int ystride, const uint8_t *blimit);
void vp9_loop_filter_simple_horizontal_edge_sse2(uint8_t *y, int ystride, const uint8_t *blimit);
#define vp9_loop_filter_simple_mbh vp9_loop_filter_simple_horizontal_edge_sse2

void vp9_loop_filter_bvs_c(uint8_t *y, int ystride, const uint8_t *blimit);
void vp9_loop_filter_bvs_mmx(uint8_t *y, int ystride, const uint8_t *blimit);
void vp9_loop_filter_bvs_sse2(uint8_t *y, int ystride, const uint8_t *blimit);
#define vp9_loop_filter_simple_bv vp9_loop_filter_bvs_sse2

void vp9_loop_filter_bhs_c(uint8_t *y, int ystride, const uint8_t *blimit);
void vp9_loop_filter_bhs_mmx(uint8_t *y, int ystride, const uint8_t *blimit);
void vp9_loop_filter_bhs_sse2(uint8_t *y, int ystride, const uint8_t *blimit);
#define vp9_loop_filter_simple_bh vp9_loop_filter_bhs_sse2

void vp9_lpf_mbh_w_c(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr, int y_stride, int uv_stride, struct loop_filter_info *lfi);
void vp9_lpf_mbh_w_sse2(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr, int y_stride, int uv_stride, struct loop_filter_info *lfi);
#define vp9_lpf_mbh_w vp9_lpf_mbh_w_sse2

void vp9_lpf_mbv_w_c(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr, int y_stride, int uv_stride, struct loop_filter_info *lfi);
void vp9_lpf_mbv_w_sse2(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr, int y_stride, int uv_stride, struct loop_filter_info *lfi);
#define vp9_lpf_mbv_w vp9_lpf_mbv_w_sse2

void vp9_mbpost_proc_down_c(uint8_t *dst, int pitch, int rows, int cols, int flimit);
void vp9_mbpost_proc_down_mmx(uint8_t *dst, int pitch, int rows, int cols, int flimit);
void vp9_mbpost_proc_down_xmm(uint8_t *dst, int pitch, int rows, int cols, int flimit);
#define vp9_mbpost_proc_down vp9_mbpost_proc_down_xmm

void vp9_mbpost_proc_across_ip_c(uint8_t *src, int pitch, int rows, int cols, int flimit);
void vp9_mbpost_proc_across_ip_xmm(uint8_t *src, int pitch, int rows, int cols, int flimit);
#define vp9_mbpost_proc_across_ip vp9_mbpost_proc_across_ip_xmm

void vp9_post_proc_down_and_across_c(uint8_t *src_ptr, uint8_t *dst_ptr, int src_pixels_per_line, int dst_pixels_per_line, int rows, int cols, int flimit);
void vp9_post_proc_down_and_across_mmx(uint8_t *src_ptr, uint8_t *dst_ptr, int src_pixels_per_line, int dst_pixels_per_line, int rows, int cols, int flimit);
void vp9_post_proc_down_and_across_xmm(uint8_t *src_ptr, uint8_t *dst_ptr, int src_pixels_per_line, int dst_pixels_per_line, int rows, int cols, int flimit);
#define vp9_post_proc_down_and_across vp9_post_proc_down_and_across_xmm

void vp9_plane_add_noise_c(uint8_t *Start, char *noise, char blackclamp[16], char whiteclamp[16], char bothclamp[16], unsigned int Width, unsigned int Height, int Pitch);
void vp9_plane_add_noise_mmx(uint8_t *Start, char *noise, char blackclamp[16], char whiteclamp[16], char bothclamp[16], unsigned int Width, unsigned int Height, int Pitch);
void vp9_plane_add_noise_wmt(uint8_t *Start, char *noise, char blackclamp[16], char whiteclamp[16], char bothclamp[16], unsigned int Width, unsigned int Height, int Pitch);
#define vp9_plane_add_noise vp9_plane_add_noise_wmt

void vp9_blend_mb_inner_c(uint8_t *y, uint8_t *u, uint8_t *v, int y1, int u1, int v1, int alpha, int stride);
#define vp9_blend_mb_inner vp9_blend_mb_inner_c

void vp9_blend_mb_outer_c(uint8_t *y, uint8_t *u, uint8_t *v, int y1, int u1, int v1, int alpha, int stride);
#define vp9_blend_mb_outer vp9_blend_mb_outer_c

void vp9_blend_b_c(uint8_t *y, uint8_t *u, uint8_t *v, int y1, int u1, int v1, int alpha, int stride);
#define vp9_blend_b vp9_blend_b_c

unsigned int vp9_sad16x3_c(const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vp9_sad16x3_sse2(const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int ref_stride);
#define vp9_sad16x3 vp9_sad16x3_sse2

unsigned int vp9_sad3x16_c(const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vp9_sad3x16_sse2(const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int ref_stride);
#define vp9_sad3x16 vp9_sad3x16_sse2

unsigned int vp9_sub_pixel_variance16x2_c(const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance16x2_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vp9_sub_pixel_variance16x2 vp9_sub_pixel_variance16x2_sse2

void vp9_convolve8_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve8_ssse3(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
RTCD_EXTERN void (*vp9_convolve8)(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);

void vp9_convolve8_horiz_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve8_horiz_ssse3(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
RTCD_EXTERN void (*vp9_convolve8_horiz)(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);

void vp9_convolve8_vert_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve8_vert_ssse3(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
RTCD_EXTERN void (*vp9_convolve8_vert)(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);

void vp9_convolve8_avg_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve8_avg_ssse3(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
RTCD_EXTERN void (*vp9_convolve8_avg)(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);

void vp9_convolve8_avg_horiz_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve8_avg_horiz_ssse3(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
RTCD_EXTERN void (*vp9_convolve8_avg_horiz)(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);

void vp9_convolve8_avg_vert_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve8_avg_vert_ssse3(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
RTCD_EXTERN void (*vp9_convolve8_avg_vert)(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);

void vp9_convolve8_1by8_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_1by8 vp9_convolve8_1by8_c

void vp9_convolve8_qtr_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_qtr vp9_convolve8_qtr_c

void vp9_convolve8_3by8_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_3by8 vp9_convolve8_3by8_c

void vp9_convolve8_5by8_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_5by8 vp9_convolve8_5by8_c

void vp9_convolve8_3qtr_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_3qtr vp9_convolve8_3qtr_c

void vp9_convolve8_7by8_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_7by8 vp9_convolve8_7by8_c

void vp9_convolve8_1by8_horiz_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_1by8_horiz vp9_convolve8_1by8_horiz_c

void vp9_convolve8_qtr_horiz_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_qtr_horiz vp9_convolve8_qtr_horiz_c

void vp9_convolve8_3by8_horiz_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_3by8_horiz vp9_convolve8_3by8_horiz_c

void vp9_convolve8_5by8_horiz_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_5by8_horiz vp9_convolve8_5by8_horiz_c

void vp9_convolve8_3qtr_horiz_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_3qtr_horiz vp9_convolve8_3qtr_horiz_c

void vp9_convolve8_7by8_horiz_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_7by8_horiz vp9_convolve8_7by8_horiz_c

void vp9_convolve8_1by8_vert_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_1by8_vert vp9_convolve8_1by8_vert_c

void vp9_convolve8_qtr_vert_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_qtr_vert vp9_convolve8_qtr_vert_c

void vp9_convolve8_3by8_vert_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_3by8_vert vp9_convolve8_3by8_vert_c

void vp9_convolve8_5by8_vert_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_5by8_vert vp9_convolve8_5by8_vert_c

void vp9_convolve8_3qtr_vert_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_3qtr_vert vp9_convolve8_3qtr_vert_c

void vp9_convolve8_7by8_vert_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_7by8_vert vp9_convolve8_7by8_vert_c

void vp9_short_idct4x4_1_c(int16_t *input, int16_t *output, int pitch);
#define vp9_short_idct4x4_1 vp9_short_idct4x4_1_c

void vp9_short_idct4x4_c(int16_t *input, int16_t *output, int pitch);
void vp9_short_idct4x4_sse2(int16_t *input, int16_t *output, int pitch);
#define vp9_short_idct4x4 vp9_short_idct4x4_sse2

void vp9_short_idct8x8_c(int16_t *input, int16_t *output, int pitch);
void vp9_short_idct8x8_sse2(int16_t *input, int16_t *output, int pitch);
#define vp9_short_idct8x8 vp9_short_idct8x8_sse2

void vp9_short_idct10_8x8_c(int16_t *input, int16_t *output, int pitch);
void vp9_short_idct10_8x8_sse2(int16_t *input, int16_t *output, int pitch);
#define vp9_short_idct10_8x8 vp9_short_idct10_8x8_sse2

void vp9_short_idct1_8x8_c(int16_t *input, int16_t *output);
#define vp9_short_idct1_8x8 vp9_short_idct1_8x8_c

void vp9_short_idct16x16_c(int16_t *input, int16_t *output, int pitch);
void vp9_short_idct16x16_sse2(int16_t *input, int16_t *output, int pitch);
#define vp9_short_idct16x16 vp9_short_idct16x16_sse2

void vp9_short_idct10_16x16_c(int16_t *input, int16_t *output, int pitch);
void vp9_short_idct10_16x16_sse2(int16_t *input, int16_t *output, int pitch);
#define vp9_short_idct10_16x16 vp9_short_idct10_16x16_sse2

void vp9_short_idct1_16x16_c(int16_t *input, int16_t *output);
#define vp9_short_idct1_16x16 vp9_short_idct1_16x16_c

void vp9_short_idct32x32_c(int16_t *input, int16_t *output, int pitch);
void vp9_short_idct32x32_sse2(int16_t *input, int16_t *output, int pitch);
#define vp9_short_idct32x32 vp9_short_idct32x32_sse2

void vp9_short_idct1_32x32_c(int16_t *input, int16_t *output);
#define vp9_short_idct1_32x32 vp9_short_idct1_32x32_c

void vp9_short_idct10_32x32_c(int16_t *input, int16_t *output, int pitch);
#define vp9_short_idct10_32x32 vp9_short_idct10_32x32_c

void vp9_short_iht8x8_c(int16_t *input, int16_t *output, int pitch, int tx_type);
#define vp9_short_iht8x8 vp9_short_iht8x8_c

void vp9_short_iht4x4_c(int16_t *input, int16_t *output, int pitch, int tx_type);
#define vp9_short_iht4x4 vp9_short_iht4x4_c

void vp9_short_iht16x16_c(int16_t *input, int16_t *output, int pitch, int tx_type);
#define vp9_short_iht16x16 vp9_short_iht16x16_c

void vp9_idct4_1d_c(int16_t *input, int16_t *output);
void vp9_idct4_1d_sse2(int16_t *input, int16_t *output);
#define vp9_idct4_1d vp9_idct4_1d_sse2

void vp9_dc_only_idct_add_c(int input_dc, uint8_t *pred_ptr, uint8_t *dst_ptr, int pitch, int stride);
void vp9_dc_only_idct_add_sse2(int input_dc, uint8_t *pred_ptr, uint8_t *dst_ptr, int pitch, int stride);
#define vp9_dc_only_idct_add vp9_dc_only_idct_add_sse2

void vp9_short_iwalsh4x4_1_c(int16_t *input, int16_t *output, int pitch);
#define vp9_short_iwalsh4x4_1 vp9_short_iwalsh4x4_1_c

void vp9_short_iwalsh4x4_c(int16_t *input, int16_t *output, int pitch);
#define vp9_short_iwalsh4x4 vp9_short_iwalsh4x4_c

void vp9_dc_only_inv_walsh_add_c(int input_dc, uint8_t *pred_ptr, uint8_t *dst_ptr, int pitch, int stride);
#define vp9_dc_only_inv_walsh_add vp9_dc_only_inv_walsh_add_c

unsigned int vp9_sad32x3_c(const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int ref_stride, int max_sad);
#define vp9_sad32x3 vp9_sad32x3_c

unsigned int vp9_sad3x32_c(const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int ref_stride, int max_sad);
#define vp9_sad3x32 vp9_sad3x32_c

unsigned int vp9_variance32x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vp9_variance32x32 vp9_variance32x32_c

unsigned int vp9_variance64x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vp9_variance64x64 vp9_variance64x64_c

unsigned int vp9_variance16x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance16x16_mmx(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance16x16_wmt(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vp9_variance16x16 vp9_variance16x16_wmt

unsigned int vp9_variance16x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance16x8_mmx(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance16x8_wmt(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vp9_variance16x8 vp9_variance16x8_wmt

unsigned int vp9_variance8x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance8x16_mmx(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance8x16_wmt(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vp9_variance8x16 vp9_variance8x16_wmt

unsigned int vp9_variance8x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance8x8_mmx(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance8x8_wmt(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vp9_variance8x8 vp9_variance8x8_wmt

unsigned int vp9_variance4x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance4x4_mmx(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance4x4_wmt(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vp9_variance4x4 vp9_variance4x4_wmt

unsigned int vp9_sub_pixel_variance64x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance64x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vp9_sub_pixel_variance64x64 vp9_sub_pixel_variance64x64_sse2

unsigned int vp9_sub_pixel_variance32x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance32x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vp9_sub_pixel_variance32x32 vp9_sub_pixel_variance32x32_sse2

unsigned int vp9_sub_pixel_variance16x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance16x16_mmx(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance16x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance16x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_variance16x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vp9_sub_pixel_variance8x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance8x16_mmx(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance8x16_wmt(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vp9_sub_pixel_variance8x16 vp9_sub_pixel_variance8x16_wmt

unsigned int vp9_sub_pixel_variance16x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance16x8_mmx(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance16x8_wmt(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance16x8_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_variance16x8)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vp9_sub_pixel_variance8x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance8x8_mmx(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance8x8_wmt(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vp9_sub_pixel_variance8x8 vp9_sub_pixel_variance8x8_wmt

unsigned int vp9_sub_pixel_variance4x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance4x4_mmx(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance4x4_wmt(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vp9_sub_pixel_variance4x4 vp9_sub_pixel_variance4x4_wmt

unsigned int vp9_sad64x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int max_sad);
unsigned int vp9_sad64x64_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int max_sad);
#define vp9_sad64x64 vp9_sad64x64_sse2

unsigned int vp9_sad32x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int max_sad);
unsigned int vp9_sad32x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int max_sad);
#define vp9_sad32x32 vp9_sad32x32_sse2

unsigned int vp9_sad16x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int max_sad);
unsigned int vp9_sad16x16_mmx(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int max_sad);
unsigned int vp9_sad16x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int max_sad);
#define vp9_sad16x16 vp9_sad16x16_sse2

unsigned int vp9_sad16x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int max_sad);
unsigned int vp9_sad16x8_mmx(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int max_sad);
unsigned int vp9_sad16x8_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int max_sad);
#define vp9_sad16x8 vp9_sad16x8_sse2

unsigned int vp9_sad8x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int max_sad);
unsigned int vp9_sad8x16_mmx(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int max_sad);
unsigned int vp9_sad8x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int max_sad);
#define vp9_sad8x16 vp9_sad8x16_sse2

unsigned int vp9_sad8x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int max_sad);
unsigned int vp9_sad8x8_mmx(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int max_sad);
unsigned int vp9_sad8x8_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int max_sad);
#define vp9_sad8x8 vp9_sad8x8_sse2

unsigned int vp9_sad4x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int max_sad);
unsigned int vp9_sad4x4_mmx(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int max_sad);
unsigned int vp9_sad4x4_sse(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int max_sad);
#define vp9_sad4x4 vp9_sad4x4_sse

unsigned int vp9_variance_halfpixvar16x16_h_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance_halfpixvar16x16_h_mmx(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance_halfpixvar16x16_h_wmt(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vp9_variance_halfpixvar16x16_h vp9_variance_halfpixvar16x16_h_wmt

unsigned int vp9_variance_halfpixvar16x16_v_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance_halfpixvar16x16_v_mmx(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance_halfpixvar16x16_v_wmt(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vp9_variance_halfpixvar16x16_v vp9_variance_halfpixvar16x16_v_wmt

unsigned int vp9_variance_halfpixvar16x16_hv_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance_halfpixvar16x16_hv_mmx(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance_halfpixvar16x16_hv_wmt(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vp9_variance_halfpixvar16x16_hv vp9_variance_halfpixvar16x16_hv_wmt

unsigned int vp9_variance_halfpixvar64x64_h_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vp9_variance_halfpixvar64x64_h vp9_variance_halfpixvar64x64_h_c

unsigned int vp9_variance_halfpixvar64x64_v_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vp9_variance_halfpixvar64x64_v vp9_variance_halfpixvar64x64_v_c

unsigned int vp9_variance_halfpixvar64x64_hv_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vp9_variance_halfpixvar64x64_hv vp9_variance_halfpixvar64x64_hv_c

unsigned int vp9_variance_halfpixvar32x32_h_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vp9_variance_halfpixvar32x32_h vp9_variance_halfpixvar32x32_h_c

unsigned int vp9_variance_halfpixvar32x32_v_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vp9_variance_halfpixvar32x32_v vp9_variance_halfpixvar32x32_v_c

unsigned int vp9_variance_halfpixvar32x32_hv_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vp9_variance_halfpixvar32x32_hv vp9_variance_halfpixvar32x32_hv_c

void vp9_sad64x64x3_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
#define vp9_sad64x64x3 vp9_sad64x64x3_c

void vp9_sad32x32x3_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
#define vp9_sad32x32x3 vp9_sad32x32x3_c

void vp9_sad16x16x3_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
void vp9_sad16x16x3_sse3(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
void vp9_sad16x16x3_ssse3(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp9_sad16x16x3)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);

void vp9_sad16x8x3_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
void vp9_sad16x8x3_sse3(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
void vp9_sad16x8x3_ssse3(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp9_sad16x8x3)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);

void vp9_sad8x16x3_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
void vp9_sad8x16x3_sse3(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp9_sad8x16x3)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);

void vp9_sad8x8x3_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
void vp9_sad8x8x3_sse3(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp9_sad8x8x3)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);

void vp9_sad4x4x3_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
void vp9_sad4x4x3_sse3(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp9_sad4x4x3)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);

void vp9_sad64x64x8_c(const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int  ref_stride, uint32_t *sad_array);
#define vp9_sad64x64x8 vp9_sad64x64x8_c

void vp9_sad32x32x8_c(const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int  ref_stride, uint32_t *sad_array);
#define vp9_sad32x32x8 vp9_sad32x32x8_c

void vp9_sad16x16x8_c(const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int  ref_stride, uint32_t *sad_array);
#define vp9_sad16x16x8 vp9_sad16x16x8_c

void vp9_sad16x8x8_c(const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int  ref_stride, uint32_t *sad_array);
#define vp9_sad16x8x8 vp9_sad16x8x8_c

void vp9_sad8x16x8_c(const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int  ref_stride, uint32_t *sad_array);
#define vp9_sad8x16x8 vp9_sad8x16x8_c

void vp9_sad8x8x8_c(const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int  ref_stride, uint32_t *sad_array);
#define vp9_sad8x8x8 vp9_sad8x8x8_c

void vp9_sad4x4x8_c(const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int  ref_stride, uint32_t *sad_array);
#define vp9_sad4x4x8 vp9_sad4x4x8_c

void vp9_sad64x64x4d_c(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
void vp9_sad64x64x4d_sse2(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
#define vp9_sad64x64x4d vp9_sad64x64x4d_sse2

void vp9_sad32x32x4d_c(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
void vp9_sad32x32x4d_sse2(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
#define vp9_sad32x32x4d vp9_sad32x32x4d_sse2

void vp9_sad16x16x4d_c(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
void vp9_sad16x16x4d_sse2(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
#define vp9_sad16x16x4d vp9_sad16x16x4d_sse2

void vp9_sad16x8x4d_c(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
void vp9_sad16x8x4d_sse2(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
#define vp9_sad16x8x4d vp9_sad16x8x4d_sse2

void vp9_sad8x16x4d_c(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
void vp9_sad8x16x4d_sse2(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
#define vp9_sad8x16x4d vp9_sad8x16x4d_sse2

void vp9_sad8x8x4d_c(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
void vp9_sad8x8x4d_sse2(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
#define vp9_sad8x8x4d vp9_sad8x8x4d_sse2

void vp9_sad4x4x4d_c(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
void vp9_sad4x4x4d_sse(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
#define vp9_sad4x4x4d vp9_sad4x4x4d_sse

unsigned int vp9_sub_pixel_mse16x16_c(const uint8_t *src_ptr, int  src_pixels_per_line, int  xoffset, int  yoffset, const uint8_t *dst_ptr, int dst_pixels_per_line, unsigned int *sse);
unsigned int vp9_sub_pixel_mse16x16_mmx(const uint8_t *src_ptr, int  src_pixels_per_line, int  xoffset, int  yoffset, const uint8_t *dst_ptr, int dst_pixels_per_line, unsigned int *sse);
unsigned int vp9_sub_pixel_mse16x16_sse2(const uint8_t *src_ptr, int  src_pixels_per_line, int  xoffset, int  yoffset, const uint8_t *dst_ptr, int dst_pixels_per_line, unsigned int *sse);
#define vp9_sub_pixel_mse16x16 vp9_sub_pixel_mse16x16_sse2

unsigned int vp9_mse16x16_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int vp9_mse16x16_mmx(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int vp9_mse16x16_wmt(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
#define vp9_mse16x16 vp9_mse16x16_wmt

unsigned int vp9_sub_pixel_mse64x64_c(const uint8_t *src_ptr, int  source_stride, int  xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vp9_sub_pixel_mse64x64 vp9_sub_pixel_mse64x64_c

unsigned int vp9_sub_pixel_mse32x32_c(const uint8_t *src_ptr, int  source_stride, int  xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vp9_sub_pixel_mse32x32 vp9_sub_pixel_mse32x32_c

unsigned int vp9_get_mb_ss_c(const int16_t *);
unsigned int vp9_get_mb_ss_mmx(const int16_t *);
unsigned int vp9_get_mb_ss_sse2(const int16_t *);
#define vp9_get_mb_ss vp9_get_mb_ss_sse2

int vp9_mbblock_error_c(struct macroblock *mb);
int vp9_mbblock_error_mmx(struct macroblock *mb);
int vp9_mbblock_error_xmm(struct macroblock *mb);
#define vp9_mbblock_error vp9_mbblock_error_xmm

int vp9_block_error_c(int16_t *coeff, int16_t *dqcoeff, int block_size);
int vp9_block_error_mmx(int16_t *coeff, int16_t *dqcoeff, int block_size);
int vp9_block_error_xmm(int16_t *coeff, int16_t *dqcoeff, int block_size);
#define vp9_block_error vp9_block_error_xmm

void vp9_subtract_b_c(struct block *be, struct blockd *bd, int pitch);
void vp9_subtract_b_mmx(struct block *be, struct blockd *bd, int pitch);
void vp9_subtract_b_sse2(struct block *be, struct blockd *bd, int pitch);
#define vp9_subtract_b vp9_subtract_b_sse2

int vp9_mbuverror_c(struct macroblock *mb);
int vp9_mbuverror_mmx(struct macroblock *mb);
int vp9_mbuverror_xmm(struct macroblock *mb);
#define vp9_mbuverror vp9_mbuverror_xmm

void vp9_subtract_b_c(struct block *be, struct blockd *bd, int pitch);
void vp9_subtract_b_mmx(struct block *be, struct blockd *bd, int pitch);
void vp9_subtract_b_sse2(struct block *be, struct blockd *bd, int pitch);
#define vp9_subtract_b vp9_subtract_b_sse2

void vp9_subtract_mby_c(int16_t *diff, uint8_t *src, uint8_t *pred, int stride);
void vp9_subtract_mby_mmx(int16_t *diff, uint8_t *src, uint8_t *pred, int stride);
void vp9_subtract_mby_sse2(int16_t *diff, uint8_t *src, uint8_t *pred, int stride);
#define vp9_subtract_mby vp9_subtract_mby_sse2

void vp9_subtract_mbuv_c(int16_t *diff, uint8_t *usrc, uint8_t *vsrc, uint8_t *pred, int stride);
void vp9_subtract_mbuv_mmx(int16_t *diff, uint8_t *usrc, uint8_t *vsrc, uint8_t *pred, int stride);
void vp9_subtract_mbuv_sse2(int16_t *diff, uint8_t *usrc, uint8_t *vsrc, uint8_t *pred, int stride);
#define vp9_subtract_mbuv vp9_subtract_mbuv_sse2

void vp9_short_fht4x4_c(int16_t *InputData, int16_t *OutputData, int pitch, int tx_type);
#define vp9_short_fht4x4 vp9_short_fht4x4_c

void vp9_short_fht8x8_c(int16_t *InputData, int16_t *OutputData, int pitch, int tx_type);
#define vp9_short_fht8x8 vp9_short_fht8x8_c

void vp9_short_fht16x16_c(int16_t *InputData, int16_t *OutputData, int pitch, int tx_type);
#define vp9_short_fht16x16 vp9_short_fht16x16_c

void vp9_short_fdct8x8_c(int16_t *InputData, int16_t *OutputData, int pitch);
void vp9_short_fdct8x8_sse2(int16_t *InputData, int16_t *OutputData, int pitch);
#define vp9_short_fdct8x8 vp9_short_fdct8x8_sse2

void vp9_short_fdct4x4_c(int16_t *InputData, int16_t *OutputData, int pitch);
void vp9_short_fdct4x4_sse2(int16_t *InputData, int16_t *OutputData, int pitch);
#define vp9_short_fdct4x4 vp9_short_fdct4x4_sse2

void vp9_short_fdct8x4_c(int16_t *InputData, int16_t *OutputData, int pitch);
void vp9_short_fdct8x4_sse2(int16_t *InputData, int16_t *OutputData, int pitch);
#define vp9_short_fdct8x4 vp9_short_fdct8x4_sse2

void vp9_short_fdct32x32_c(int16_t *InputData, int16_t *OutputData, int pitch);
#define vp9_short_fdct32x32 vp9_short_fdct32x32_c

void vp9_short_fdct16x16_c(int16_t *InputData, int16_t *OutputData, int pitch);
void vp9_short_fdct16x16_sse2(int16_t *InputData, int16_t *OutputData, int pitch);
#define vp9_short_fdct16x16 vp9_short_fdct16x16_sse2

void vp9_short_walsh4x4_c(int16_t *InputData, int16_t *OutputData, int pitch);
#define vp9_short_walsh4x4 vp9_short_walsh4x4_c

void vp9_short_walsh8x4_c(int16_t *InputData, int16_t *OutputData, int pitch);
#define vp9_short_walsh8x4 vp9_short_walsh8x4_c

int vp9_full_search_sad_c(struct macroblock *x, struct block *b, struct blockd *d, union int_mv *ref_mv, int sad_per_bit, int distance, struct vp9_variance_vtable *fn_ptr, DEC_MVCOSTS, union int_mv *center_mv);
int vp9_full_search_sadx3(struct macroblock *x, struct block *b, struct blockd *d, union int_mv *ref_mv, int sad_per_bit, int distance, struct vp9_variance_vtable *fn_ptr, DEC_MVCOSTS, union int_mv *center_mv);
int vp9_full_search_sadx8(struct macroblock *x, struct block *b, struct blockd *d, union int_mv *ref_mv, int sad_per_bit, int distance, struct vp9_variance_vtable *fn_ptr, DEC_MVCOSTS, union int_mv *center_mv);
RTCD_EXTERN int (*vp9_full_search_sad)(struct macroblock *x, struct block *b, struct blockd *d, union int_mv *ref_mv, int sad_per_bit, int distance, struct vp9_variance_vtable *fn_ptr, DEC_MVCOSTS, union int_mv *center_mv);

int vp9_refining_search_sad_c(struct macroblock *x, struct block *b, struct blockd *d, union int_mv *ref_mv, int sad_per_bit, int distance, struct vp9_variance_vtable *fn_ptr, DEC_MVCOSTS, union int_mv *center_mv);
int vp9_refining_search_sadx4(struct macroblock *x, struct block *b, struct blockd *d, union int_mv *ref_mv, int sad_per_bit, int distance, struct vp9_variance_vtable *fn_ptr, DEC_MVCOSTS, union int_mv *center_mv);
RTCD_EXTERN int (*vp9_refining_search_sad)(struct macroblock *x, struct block *b, struct blockd *d, union int_mv *ref_mv, int sad_per_bit, int distance, struct vp9_variance_vtable *fn_ptr, DEC_MVCOSTS, union int_mv *center_mv);

int vp9_diamond_search_sad_c(struct macroblock *x, struct block *b, struct blockd *d, union int_mv *ref_mv, union int_mv *best_mv, int search_param, int sad_per_bit, int *num00, struct vp9_variance_vtable *fn_ptr, DEC_MVCOSTS, union int_mv *center_mv);
int vp9_diamond_search_sadx4(struct macroblock *x, struct block *b, struct blockd *d, union int_mv *ref_mv, union int_mv *best_mv, int search_param, int sad_per_bit, int *num00, struct vp9_variance_vtable *fn_ptr, DEC_MVCOSTS, union int_mv *center_mv);
RTCD_EXTERN int (*vp9_diamond_search_sad)(struct macroblock *x, struct block *b, struct blockd *d, union int_mv *ref_mv, union int_mv *best_mv, int search_param, int sad_per_bit, int *num00, struct vp9_variance_vtable *fn_ptr, DEC_MVCOSTS, union int_mv *center_mv);

void vp9_temporal_filter_apply_c(uint8_t *frame1, unsigned int stride, uint8_t *frame2, unsigned int block_size, int strength, int filter_weight, unsigned int *accumulator, uint16_t *count);
void vp9_temporal_filter_apply_sse2(uint8_t *frame1, unsigned int stride, uint8_t *frame2, unsigned int block_size, int strength, int filter_weight, unsigned int *accumulator, uint16_t *count);
#define vp9_temporal_filter_apply vp9_temporal_filter_apply_sse2

void vp9_yv12_copy_partial_frame_c(struct yv12_buffer_config *src_ybc, struct yv12_buffer_config *dst_ybc, int fraction);
#define vp9_yv12_copy_partial_frame vp9_yv12_copy_partial_frame_c

void vp9_rtcd(void);

#ifdef RTCD_C
#include "vpx_ports/x86.h"
static void setup_rtcd_internal(void)
{
    int flags = x86_simd_caps();

    (void)flags;


































































    vp9_convolve8 = vp9_convolve8_c;
    if (flags & HAS_SSSE3) vp9_convolve8 = vp9_convolve8_ssse3;

    vp9_convolve8_horiz = vp9_convolve8_horiz_c;
    if (flags & HAS_SSSE3) vp9_convolve8_horiz = vp9_convolve8_horiz_ssse3;

    vp9_convolve8_vert = vp9_convolve8_vert_c;
    if (flags & HAS_SSSE3) vp9_convolve8_vert = vp9_convolve8_vert_ssse3;

    vp9_convolve8_avg = vp9_convolve8_avg_c;
    if (flags & HAS_SSSE3) vp9_convolve8_avg = vp9_convolve8_avg_ssse3;

    vp9_convolve8_avg_horiz = vp9_convolve8_avg_horiz_c;
    if (flags & HAS_SSSE3) vp9_convolve8_avg_horiz = vp9_convolve8_avg_horiz_ssse3;

    vp9_convolve8_avg_vert = vp9_convolve8_avg_vert_c;
    if (flags & HAS_SSSE3) vp9_convolve8_avg_vert = vp9_convolve8_avg_vert_ssse3;

















































    vp9_sub_pixel_variance16x16 = vp9_sub_pixel_variance16x16_sse2;
    if (flags & HAS_SSSE3) vp9_sub_pixel_variance16x16 = vp9_sub_pixel_variance16x16_ssse3;


    vp9_sub_pixel_variance16x8 = vp9_sub_pixel_variance16x8_wmt;
    if (flags & HAS_SSSE3) vp9_sub_pixel_variance16x8 = vp9_sub_pixel_variance16x8_ssse3;





















    vp9_sad16x16x3 = vp9_sad16x16x3_c;
    if (flags & HAS_SSE3) vp9_sad16x16x3 = vp9_sad16x16x3_sse3;
    if (flags & HAS_SSSE3) vp9_sad16x16x3 = vp9_sad16x16x3_ssse3;

    vp9_sad16x8x3 = vp9_sad16x8x3_c;
    if (flags & HAS_SSE3) vp9_sad16x8x3 = vp9_sad16x8x3_sse3;
    if (flags & HAS_SSSE3) vp9_sad16x8x3 = vp9_sad16x8x3_ssse3;

    vp9_sad8x16x3 = vp9_sad8x16x3_c;
    if (flags & HAS_SSE3) vp9_sad8x16x3 = vp9_sad8x16x3_sse3;

    vp9_sad8x8x3 = vp9_sad8x8x3_c;
    if (flags & HAS_SSE3) vp9_sad8x8x3 = vp9_sad8x8x3_sse3;

    vp9_sad4x4x3 = vp9_sad4x4x3_c;
    if (flags & HAS_SSE3) vp9_sad4x4x3 = vp9_sad4x4x3_sse3;





































    vp9_full_search_sad = vp9_full_search_sad_c;
    if (flags & HAS_SSE3) vp9_full_search_sad = vp9_full_search_sadx3;
    if (flags & HAS_SSE4_1) vp9_full_search_sad = vp9_full_search_sadx8;

    vp9_refining_search_sad = vp9_refining_search_sad_c;
    if (flags & HAS_SSE3) vp9_refining_search_sad = vp9_refining_search_sadx4;

    vp9_diamond_search_sad = vp9_diamond_search_sad_c;
    if (flags & HAS_SSE3) vp9_diamond_search_sad = vp9_diamond_search_sadx4;
}
#endif
#endif
