#include <stdlib.h>
#include <assert.h>

#include <my_global.h>
#include <my_sys.h>

#include <mysql.h>
#include <m_ctype.h>
#include <m_string.h>

#include <xmmintrin.h>

#define SANITY(x)   assert(x)
#define SSE_ALIGNED __attribute__((aligned (16)))

typedef union {
  int32_t sword[4];
  int64_t dword[2];
} xmm_data;

typedef struct sum_sse_ctx {
  xmm_data sum_buffer SSE_ALIGNED;
  xmm_data cur_buffer SSE_ALIGNED;
  size_t n_filled;
} sum_sse_ctx_t;

static inline
void clear_sum_sse_ctx_cur_buffer(sum_sse_ctx_t *ctx) {
  memset(&ctx->cur_buffer, 0, sizeof(ctx->cur_buffer));
  ctx->n_filled = 0;
}

static inline
void init_sum_sse_ctx(sum_sse_ctx_t *ctx) {
  memset(&ctx->sum_buffer, 0, sizeof(ctx->sum_buffer));
  clear_sum_sse_ctx_cur_buffer(ctx);
}

static inline
void accum_sum_sse_ctx_i32(sum_sse_ctx_t *ctx) {
  __m128i* sum_ptr       = (__m128i*) ctx->sum_buffer.sword;
  const __m128i* cur_ptr = (const __m128i*) ctx->cur_buffer.sword;

  __m128i xmm1 = _mm_load_si128(sum_ptr);
  __m128i xmm2 = _mm_load_si128(cur_ptr);
  __m128i res  = _mm_add_epi32(xmm1, xmm2);

  _mm_store_si128(sum_ptr, res);
}

static inline
void accum_sum_sse_ctx_i64(sum_sse_ctx_t *ctx) {
  __m128i* sum_ptr       = (__m128i*) ctx->sum_buffer.dword;
  const __m128i* cur_ptr = (const __m128i*) ctx->cur_buffer.dword;

  __m128i xmm1 = _mm_load_si128(sum_ptr);
  __m128i xmm2 = _mm_load_si128(cur_ptr);
  __m128i res  = _mm_add_epi64(xmm1, xmm2);

  _mm_store_si128(sum_ptr, res);
}

#ifdef HAVE_DLOPEN

my_bool sum_sse_i32_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void sum_sse_i32_clear(UDF_INIT *initid, char *is_null, char *message);
void sum_sse_i32_add(UDF_INIT *initid, UDF_ARGS *args,
                     char *is_null, char *message);
long long sum_sse_i32(UDF_INIT *initid, UDF_ARGS *args, char *result,
                      unsigned long *length, char *is_null, char *error);
void sum_sse_i32_deinit(UDF_INIT *initid);

my_bool sum_sse_i64_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void sum_sse_i64_clear(UDF_INIT *initid, char *is_null, char *message);
void sum_sse_i64_add(UDF_INIT *initid, UDF_ARGS *args,
                     char *is_null, char *message);
long long sum_sse_i64(UDF_INIT *initid, UDF_ARGS *args, char *result,
                      unsigned long *length, char *is_null, char *error);
void sum_sse_i64_deinit(UDF_INIT *initid);

// ---------------------------------------------------------------------------
// common operations
static my_bool sum_sse_init(UDF_INIT *initid, UDF_ARGS *args, char *message) {
  if (args->arg_count != 1) {
    strcpy(message, "exactly one arg only");
    return 1;
  }

  sum_sse_ctx_t *ctx = (sum_sse_ctx_t*) malloc(sizeof(sum_sse_ctx_t));
  if (!ctx) {
    strcpy(message, "no memory");
    return 1;
  }

  init_sum_sse_ctx(ctx);

  args->arg_type[0]  = INT_RESULT; // TODO(stephentu): remove this casting
  initid->ptr        = (char *) ctx;
  initid->const_item = 0;

  return 0;
}

static inline
void sum_sse_clear(UDF_INIT *initid, char *is_null, char *message) {
  sum_sse_ctx_t *ctx = (sum_sse_ctx_t *) initid->ptr;
  init_sum_sse_ctx(ctx);
}

static inline
void sum_sse_deinit(UDF_INIT *initid) {
  free(initid->ptr);
}

// ---------------------------------------------------------------------------
// sum_sse_i32
my_bool sum_sse_i32_init(UDF_INIT *initid, UDF_ARGS *args, char *message) {
  return sum_sse_init(initid, args, message);
}

void sum_sse_i32_clear(UDF_INIT *initid, char *is_null, char *message) {
  sum_sse_clear(initid, is_null, message);
}

void sum_sse_i32_add(UDF_INIT *initid, UDF_ARGS *args,
                     char *is_null, char *message) {
  sum_sse_ctx_t *ctx = (sum_sse_ctx_t *) initid->ptr;

  SANITY(0 <= ctx->n_filled && ctx->n_filled < 4);
  int32_t value = (int32_t) *((long long*) args->args[0]);
  ctx->cur_buffer.sword[ctx->n_filled++] = value;

  if (ctx->n_filled == 4) {
    accum_sum_sse_ctx_i32(ctx);
    clear_sum_sse_ctx_cur_buffer(ctx);
  }
}

long long sum_sse_i32(UDF_INIT *initid, UDF_ARGS *args, char *result,
                      unsigned long *length, char *is_null, char *error) {
  sum_sse_ctx_t *ctx = (sum_sse_ctx_t *) initid->ptr;

  if (ctx->n_filled != 0) {
    accum_sum_sse_ctx_i32(ctx);
  }

  long long sum = 0;
  sum += ctx->sum_buffer.sword[0];
  sum += ctx->sum_buffer.sword[1];
  sum += ctx->sum_buffer.sword[2];
  sum += ctx->sum_buffer.sword[3];

  return sum;
}

void sum_sse_i32_deinit(UDF_INIT *initid) {
  sum_sse_deinit(initid);
}

// ---------------------------------------------------------------------------
// sum_sse_i64
my_bool sum_sse_i64_init(UDF_INIT *initid, UDF_ARGS *args, char *message) {
  return sum_sse_init(initid, args, message);
}

void sum_sse_i64_clear(UDF_INIT *initid, char *is_null, char *message) {
  sum_sse_clear(initid, is_null, message);
}

void sum_sse_i64_add(UDF_INIT *initid, UDF_ARGS *args,
                     char *is_null, char *message) {
  sum_sse_ctx_t *ctx = (sum_sse_ctx_t *) initid->ptr;

  SANITY(0 <= ctx->n_filled && ctx->n_filled < 2);
  int64_t value = (int64_t) *((long long*) args->args[0]);
  ctx->cur_buffer.dword[ctx->n_filled++] = value;

  if (ctx->n_filled == 2) {
    accum_sum_sse_ctx_i64(ctx);
    clear_sum_sse_ctx_cur_buffer(ctx);
  }
}

long long sum_sse_i64(UDF_INIT *initid, UDF_ARGS *args, char *result,
                      unsigned long *length, char *is_null, char *error) {
  sum_sse_ctx_t *ctx = (sum_sse_ctx_t *) initid->ptr;

  if (ctx->n_filled != 0) {
    accum_sum_sse_ctx_i64(ctx);
  }

  long long sum = 0;
  sum += ctx->sum_buffer.dword[0];
  sum += ctx->sum_buffer.dword[1];

  return sum;
}

void sum_sse_i64_deinit(UDF_INIT *initid) {
  sum_sse_deinit(initid);
}

#endif /* HAVE_DLOPEN */
