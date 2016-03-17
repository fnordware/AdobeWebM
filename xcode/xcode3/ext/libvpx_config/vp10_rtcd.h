#ifndef VP10_RTCD_H_
#define VP10_RTCD_H_

#ifdef RTCD_C
#define RTCD_EXTERN
#else
#define RTCD_EXTERN extern
#endif

/*
 * VP10
 */

#include "vpx/vpx_integer.h"
#include "vp10/common/common.h"
#include "vp10/common/enums.h"

struct macroblockd;

/* Encoder forward decls */
struct macroblock;
struct vp9_variance_vtable;
struct search_site_config;
struct mv;
union int_mv;
struct yv12_buffer_config;

#ifdef __cplusplus
extern "C" {
#endif

unsigned int vp10_avg_4x4_c(const uint8_t *, int p);
unsigned int vp10_avg_4x4_sse2(const uint8_t *, int p);
#define vp10_avg_4x4 vp10_avg_4x4_sse2

unsigned int vp10_avg_8x8_c(const uint8_t *, int p);
unsigned int vp10_avg_8x8_sse2(const uint8_t *, int p);
#define vp10_avg_8x8 vp10_avg_8x8_sse2

int64_t vp10_block_error_c(const tran_low_t *coeff, const tran_low_t *dqcoeff, intptr_t block_size, int64_t *ssz);
#define vp10_block_error vp10_block_error_c

int vp10_diamond_search_sad_c(const struct macroblock *x, const struct search_site_config *cfg,  struct mv *ref_mv, struct mv *best_mv, int search_param, int sad_per_bit, int *num00, const struct vp9_variance_vtable *fn_ptr, const struct mv *center_mv);
#define vp10_diamond_search_sad vp10_diamond_search_sad_c

void vp10_fdct16x16_c(const int16_t *input, tran_low_t *output, int stride);
void vp10_fdct16x16_sse2(const int16_t *input, tran_low_t *output, int stride);
#define vp10_fdct16x16 vp10_fdct16x16_sse2

void vp10_fdct16x16_1_c(const int16_t *input, tran_low_t *output, int stride);
void vp10_fdct16x16_1_sse2(const int16_t *input, tran_low_t *output, int stride);
#define vp10_fdct16x16_1 vp10_fdct16x16_1_sse2

void vp10_fdct32x32_c(const int16_t *input, tran_low_t *output, int stride);
void vp10_fdct32x32_sse2(const int16_t *input, tran_low_t *output, int stride);
#define vp10_fdct32x32 vp10_fdct32x32_sse2

void vp10_fdct32x32_1_c(const int16_t *input, tran_low_t *output, int stride);
void vp10_fdct32x32_1_sse2(const int16_t *input, tran_low_t *output, int stride);
#define vp10_fdct32x32_1 vp10_fdct32x32_1_sse2

void vp10_fdct32x32_rd_c(const int16_t *input, tran_low_t *output, int stride);
void vp10_fdct32x32_rd_sse2(const int16_t *input, tran_low_t *output, int stride);
#define vp10_fdct32x32_rd vp10_fdct32x32_rd_sse2

void vp10_fdct4x4_c(const int16_t *input, tran_low_t *output, int stride);
void vp10_fdct4x4_sse2(const int16_t *input, tran_low_t *output, int stride);
#define vp10_fdct4x4 vp10_fdct4x4_sse2

void vp10_fdct4x4_1_c(const int16_t *input, tran_low_t *output, int stride);
void vp10_fdct4x4_1_sse2(const int16_t *input, tran_low_t *output, int stride);
#define vp10_fdct4x4_1 vp10_fdct4x4_1_sse2

void vp10_fdct8x8_c(const int16_t *input, tran_low_t *output, int stride);
void vp10_fdct8x8_sse2(const int16_t *input, tran_low_t *output, int stride);
#define vp10_fdct8x8 vp10_fdct8x8_sse2

void vp10_fdct8x8_1_c(const int16_t *input, tran_low_t *output, int stride);
void vp10_fdct8x8_1_sse2(const int16_t *input, tran_low_t *output, int stride);
#define vp10_fdct8x8_1 vp10_fdct8x8_1_sse2

void vp10_fdct8x8_quant_c(const int16_t *input, int stride, tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
#define vp10_fdct8x8_quant vp10_fdct8x8_quant_c

void vp10_fht16x16_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
void vp10_fht16x16_sse2(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define vp10_fht16x16 vp10_fht16x16_sse2

void vp10_fht4x4_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
void vp10_fht4x4_sse2(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define vp10_fht4x4 vp10_fht4x4_sse2

void vp10_fht8x8_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
void vp10_fht8x8_sse2(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define vp10_fht8x8 vp10_fht8x8_sse2

int vp10_full_range_search_c(const struct macroblock *x, const struct search_site_config *cfg, struct mv *ref_mv, struct mv *best_mv, int search_param, int sad_per_bit, int *num00, const struct vp9_variance_vtable *fn_ptr, const struct mv *center_mv);
#define vp10_full_range_search vp10_full_range_search_c

int vp10_full_search_sad_c(const struct macroblock *x, const struct mv *ref_mv, int sad_per_bit, int distance, const struct vp9_variance_vtable *fn_ptr, const struct mv *center_mv, struct mv *best_mv);
int vp10_full_search_sadx3(const struct macroblock *x, const struct mv *ref_mv, int sad_per_bit, int distance, const struct vp9_variance_vtable *fn_ptr, const struct mv *center_mv, struct mv *best_mv);
int vp10_full_search_sadx8(const struct macroblock *x, const struct mv *ref_mv, int sad_per_bit, int distance, const struct vp9_variance_vtable *fn_ptr, const struct mv *center_mv, struct mv *best_mv);
RTCD_EXTERN int (*vp10_full_search_sad)(const struct macroblock *x, const struct mv *ref_mv, int sad_per_bit, int distance, const struct vp9_variance_vtable *fn_ptr, const struct mv *center_mv, struct mv *best_mv);

void vp10_fwht4x4_c(const int16_t *input, tran_low_t *output, int stride);
void vp10_fwht4x4_mmx(const int16_t *input, tran_low_t *output, int stride);
#define vp10_fwht4x4 vp10_fwht4x4_mmx

void vp10_hadamard_16x16_c(int16_t const *src_diff, int src_stride, int16_t *coeff);
void vp10_hadamard_16x16_sse2(int16_t const *src_diff, int src_stride, int16_t *coeff);
#define vp10_hadamard_16x16 vp10_hadamard_16x16_sse2

void vp10_hadamard_8x8_c(int16_t const *src_diff, int src_stride, int16_t *coeff);
void vp10_hadamard_8x8_sse2(int16_t const *src_diff, int src_stride, int16_t *coeff);
void vp10_hadamard_8x8_ssse3(int16_t const *src_diff, int src_stride, int16_t *coeff);
RTCD_EXTERN void (*vp10_hadamard_8x8)(int16_t const *src_diff, int src_stride, int16_t *coeff);

unsigned int vp10_highbd_avg_4x4_c(const uint8_t *, int p);
#define vp10_highbd_avg_4x4 vp10_highbd_avg_4x4_c

unsigned int vp10_highbd_avg_8x8_c(const uint8_t *, int p);
#define vp10_highbd_avg_8x8 vp10_highbd_avg_8x8_c

int64_t vp10_highbd_block_error_c(const tran_low_t *coeff, const tran_low_t *dqcoeff, intptr_t block_size, int64_t *ssz, int bd);
int64_t vp10_highbd_block_error_sse2(const tran_low_t *coeff, const tran_low_t *dqcoeff, intptr_t block_size, int64_t *ssz, int bd);
#define vp10_highbd_block_error vp10_highbd_block_error_sse2

void vp10_highbd_convolve8_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
void vp10_highbd_convolve8_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
#define vp10_highbd_convolve8 vp10_highbd_convolve8_sse2

void vp10_highbd_convolve8_avg_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
void vp10_highbd_convolve8_avg_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
#define vp10_highbd_convolve8_avg vp10_highbd_convolve8_avg_sse2

void vp10_highbd_convolve8_avg_horiz_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
void vp10_highbd_convolve8_avg_horiz_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
#define vp10_highbd_convolve8_avg_horiz vp10_highbd_convolve8_avg_horiz_sse2

void vp10_highbd_convolve8_avg_vert_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
void vp10_highbd_convolve8_avg_vert_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
#define vp10_highbd_convolve8_avg_vert vp10_highbd_convolve8_avg_vert_sse2

void vp10_highbd_convolve8_horiz_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
void vp10_highbd_convolve8_horiz_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
#define vp10_highbd_convolve8_horiz vp10_highbd_convolve8_horiz_sse2

void vp10_highbd_convolve8_vert_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
void vp10_highbd_convolve8_vert_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
#define vp10_highbd_convolve8_vert vp10_highbd_convolve8_vert_sse2

void vp10_highbd_convolve_avg_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
#define vp10_highbd_convolve_avg vp10_highbd_convolve_avg_c

void vp10_highbd_convolve_copy_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
#define vp10_highbd_convolve_copy vp10_highbd_convolve_copy_c

void vp10_highbd_fdct16x16_c(const int16_t *input, tran_low_t *output, int stride);
void vp10_highbd_fdct16x16_sse2(const int16_t *input, tran_low_t *output, int stride);
#define vp10_highbd_fdct16x16 vp10_highbd_fdct16x16_sse2

void vp10_highbd_fdct16x16_1_c(const int16_t *input, tran_low_t *output, int stride);
#define vp10_highbd_fdct16x16_1 vp10_highbd_fdct16x16_1_c

void vp10_highbd_fdct32x32_c(const int16_t *input, tran_low_t *output, int stride);
void vp10_highbd_fdct32x32_sse2(const int16_t *input, tran_low_t *output, int stride);
#define vp10_highbd_fdct32x32 vp10_highbd_fdct32x32_sse2

void vp10_highbd_fdct32x32_1_c(const int16_t *input, tran_low_t *output, int stride);
#define vp10_highbd_fdct32x32_1 vp10_highbd_fdct32x32_1_c

void vp10_highbd_fdct32x32_rd_c(const int16_t *input, tran_low_t *output, int stride);
void vp10_highbd_fdct32x32_rd_sse2(const int16_t *input, tran_low_t *output, int stride);
#define vp10_highbd_fdct32x32_rd vp10_highbd_fdct32x32_rd_sse2

void vp10_highbd_fdct4x4_c(const int16_t *input, tran_low_t *output, int stride);
void vp10_highbd_fdct4x4_sse2(const int16_t *input, tran_low_t *output, int stride);
#define vp10_highbd_fdct4x4 vp10_highbd_fdct4x4_sse2

void vp10_highbd_fdct8x8_c(const int16_t *input, tran_low_t *output, int stride);
void vp10_highbd_fdct8x8_sse2(const int16_t *input, tran_low_t *output, int stride);
#define vp10_highbd_fdct8x8 vp10_highbd_fdct8x8_sse2

void vp10_highbd_fdct8x8_1_c(const int16_t *input, tran_low_t *output, int stride);
#define vp10_highbd_fdct8x8_1 vp10_highbd_fdct8x8_1_c

void vp10_highbd_fht16x16_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define vp10_highbd_fht16x16 vp10_highbd_fht16x16_c

void vp10_highbd_fht4x4_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define vp10_highbd_fht4x4 vp10_highbd_fht4x4_c

void vp10_highbd_fht8x8_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define vp10_highbd_fht8x8 vp10_highbd_fht8x8_c

void vp10_highbd_fwht4x4_c(const int16_t *input, tran_low_t *output, int stride);
#define vp10_highbd_fwht4x4 vp10_highbd_fwht4x4_c

void vp10_highbd_idct16x16_10_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int bd);
void vp10_highbd_idct16x16_10_add_sse2(const tran_low_t *input, uint8_t *dest, int dest_stride, int bd);
#define vp10_highbd_idct16x16_10_add vp10_highbd_idct16x16_10_add_sse2

void vp10_highbd_idct16x16_1_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int bd);
#define vp10_highbd_idct16x16_1_add vp10_highbd_idct16x16_1_add_c

void vp10_highbd_idct16x16_256_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int bd);
void vp10_highbd_idct16x16_256_add_sse2(const tran_low_t *input, uint8_t *dest, int dest_stride, int bd);
#define vp10_highbd_idct16x16_256_add vp10_highbd_idct16x16_256_add_sse2

void vp10_highbd_idct32x32_1024_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int bd);
#define vp10_highbd_idct32x32_1024_add vp10_highbd_idct32x32_1024_add_c

void vp10_highbd_idct32x32_1_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int bd);
#define vp10_highbd_idct32x32_1_add vp10_highbd_idct32x32_1_add_c

void vp10_highbd_idct32x32_34_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int bd);
#define vp10_highbd_idct32x32_34_add vp10_highbd_idct32x32_34_add_c

void vp10_highbd_idct4x4_16_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int bd);
void vp10_highbd_idct4x4_16_add_sse2(const tran_low_t *input, uint8_t *dest, int dest_stride, int bd);
#define vp10_highbd_idct4x4_16_add vp10_highbd_idct4x4_16_add_sse2

void vp10_highbd_idct4x4_1_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int bd);
#define vp10_highbd_idct4x4_1_add vp10_highbd_idct4x4_1_add_c

void vp10_highbd_idct8x8_10_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int bd);
void vp10_highbd_idct8x8_10_add_sse2(const tran_low_t *input, uint8_t *dest, int dest_stride, int bd);
#define vp10_highbd_idct8x8_10_add vp10_highbd_idct8x8_10_add_sse2

void vp10_highbd_idct8x8_1_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int bd);
#define vp10_highbd_idct8x8_1_add vp10_highbd_idct8x8_1_add_c

void vp10_highbd_idct8x8_64_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int bd);
void vp10_highbd_idct8x8_64_add_sse2(const tran_low_t *input, uint8_t *dest, int dest_stride, int bd);
#define vp10_highbd_idct8x8_64_add vp10_highbd_idct8x8_64_add_sse2

void vp10_highbd_iht16x16_256_add_c(const tran_low_t *input, uint8_t *output, int pitch, int tx_type, int bd);
#define vp10_highbd_iht16x16_256_add vp10_highbd_iht16x16_256_add_c

void vp10_highbd_iht4x4_16_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type, int bd);
#define vp10_highbd_iht4x4_16_add vp10_highbd_iht4x4_16_add_c

void vp10_highbd_iht8x8_64_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type, int bd);
#define vp10_highbd_iht8x8_64_add vp10_highbd_iht8x8_64_add_c

void vp10_highbd_iwht4x4_16_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int bd);
#define vp10_highbd_iwht4x4_16_add vp10_highbd_iwht4x4_16_add_c

void vp10_highbd_iwht4x4_1_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int bd);
#define vp10_highbd_iwht4x4_1_add vp10_highbd_iwht4x4_1_add_c

void vp10_highbd_minmax_8x8_c(const uint8_t *s, int p, const uint8_t *d, int dp, int *min, int *max);
#define vp10_highbd_minmax_8x8 vp10_highbd_minmax_8x8_c

void vp10_highbd_quantize_fp_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
#define vp10_highbd_quantize_fp vp10_highbd_quantize_fp_c

void vp10_highbd_quantize_fp_32x32_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
#define vp10_highbd_quantize_fp_32x32 vp10_highbd_quantize_fp_32x32_c

void vp10_highbd_temporal_filter_apply_c(uint8_t *frame1, unsigned int stride, uint8_t *frame2, unsigned int block_width, unsigned int block_height, int strength, int filter_weight, unsigned int *accumulator, uint16_t *count);
#define vp10_highbd_temporal_filter_apply vp10_highbd_temporal_filter_apply_c

void vp10_idct16x16_10_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride);
#define vp10_idct16x16_10_add vp10_idct16x16_10_add_c

void vp10_idct16x16_1_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride);
#define vp10_idct16x16_1_add vp10_idct16x16_1_add_c

void vp10_idct16x16_256_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride);
#define vp10_idct16x16_256_add vp10_idct16x16_256_add_c

void vp10_idct32x32_1024_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride);
#define vp10_idct32x32_1024_add vp10_idct32x32_1024_add_c

void vp10_idct32x32_1_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride);
#define vp10_idct32x32_1_add vp10_idct32x32_1_add_c

void vp10_idct32x32_34_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride);
#define vp10_idct32x32_34_add vp10_idct32x32_34_add_c

void vp10_idct4x4_16_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride);
#define vp10_idct4x4_16_add vp10_idct4x4_16_add_c

void vp10_idct4x4_1_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride);
#define vp10_idct4x4_1_add vp10_idct4x4_1_add_c

void vp10_idct8x8_12_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride);
#define vp10_idct8x8_12_add vp10_idct8x8_12_add_c

void vp10_idct8x8_1_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride);
#define vp10_idct8x8_1_add vp10_idct8x8_1_add_c

void vp10_idct8x8_64_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride);
#define vp10_idct8x8_64_add vp10_idct8x8_64_add_c

void vp10_iht16x16_256_add_c(const tran_low_t *input, uint8_t *output, int pitch, int tx_type);
#define vp10_iht16x16_256_add vp10_iht16x16_256_add_c

void vp10_iht4x4_16_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type);
void vp10_iht4x4_16_add_sse2(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type);
#define vp10_iht4x4_16_add vp10_iht4x4_16_add_sse2

void vp10_iht8x8_64_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type);
void vp10_iht8x8_64_add_sse2(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type);
#define vp10_iht8x8_64_add vp10_iht8x8_64_add_sse2

int16_t vp10_int_pro_col_c(uint8_t const *ref, const int width);
int16_t vp10_int_pro_col_sse2(uint8_t const *ref, const int width);
#define vp10_int_pro_col vp10_int_pro_col_sse2

void vp10_int_pro_row_c(int16_t *hbuf, uint8_t const *ref, const int ref_stride, const int height);
void vp10_int_pro_row_sse2(int16_t *hbuf, uint8_t const *ref, const int ref_stride, const int height);
#define vp10_int_pro_row vp10_int_pro_row_sse2

void vp10_iwht4x4_16_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride);
#define vp10_iwht4x4_16_add vp10_iwht4x4_16_add_c

void vp10_iwht4x4_1_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride);
#define vp10_iwht4x4_1_add vp10_iwht4x4_1_add_c

void vp10_minmax_8x8_c(const uint8_t *s, int p, const uint8_t *d, int dp, int *min, int *max);
void vp10_minmax_8x8_sse2(const uint8_t *s, int p, const uint8_t *d, int dp, int *min, int *max);
#define vp10_minmax_8x8 vp10_minmax_8x8_sse2

void vp10_quantize_fp_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
#define vp10_quantize_fp vp10_quantize_fp_c

void vp10_quantize_fp_32x32_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
#define vp10_quantize_fp_32x32 vp10_quantize_fp_32x32_c

int16_t vp10_satd_c(const int16_t *coeff, int length);
int16_t vp10_satd_sse2(const int16_t *coeff, int length);
#define vp10_satd vp10_satd_sse2

void vp10_temporal_filter_apply_c(uint8_t *frame1, unsigned int stride, uint8_t *frame2, unsigned int block_width, unsigned int block_height, int strength, int filter_weight, unsigned int *accumulator, uint16_t *count);
void vp10_temporal_filter_apply_sse2(uint8_t *frame1, unsigned int stride, uint8_t *frame2, unsigned int block_width, unsigned int block_height, int strength, int filter_weight, unsigned int *accumulator, uint16_t *count);
#define vp10_temporal_filter_apply vp10_temporal_filter_apply_sse2

int vp10_vector_var_c(int16_t const *ref, int16_t const *src, const int bwl);
int vp10_vector_var_sse2(int16_t const *ref, int16_t const *src, const int bwl);
#define vp10_vector_var vp10_vector_var_sse2

void vp10_rtcd(void);

#ifdef RTCD_C
#include "vpx_ports/x86.h"
static void setup_rtcd_internal(void)
{
    int flags = x86_simd_caps();

    (void)flags;

    vp10_full_search_sad = vp10_full_search_sad_c;
    if (flags & HAS_SSE3) vp10_full_search_sad = vp10_full_search_sadx3;
    if (flags & HAS_SSE4_1) vp10_full_search_sad = vp10_full_search_sadx8;
    vp10_hadamard_8x8 = vp10_hadamard_8x8_sse2;
    if (flags & HAS_SSSE3) vp10_hadamard_8x8 = vp10_hadamard_8x8_ssse3;
}
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif
