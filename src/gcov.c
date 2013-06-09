#if COVERAGE && !defined(HOSTED)

#include "hal.h"
#include "assert.h"
#include "stdlib.h"
#include "stdio.h"

/*
 * Profiling data types used for gcc 3.4 and above - these are defined by
 * gcc and need to be kept as close to the original definition as possible to
 * remain compatible.
 */
#define GCOV_COUNTERS		5
#define GCOV_DATA_MAGIC		((unsigned int) 0x67636461)
#define GCOV_TAG_FUNCTION	((unsigned int) 0x01000000)
#define GCOV_TAG_COUNTER_BASE	((unsigned int) 0x01a10000)
#define GCOV_TAG_FOR_COUNTER(count)					\
	(GCOV_TAG_COUNTER_BASE + ((unsigned int) (count) << 17))

typedef struct gcov_fn_info {
  uint32_t ident;               /* object file-unique function identifier */
  uint32_t checksum;            /* function checksum */
  uint32_t n_ctrs[0];           /* Number of values per counter type */
} gcov_fn_info_t;

typedef struct gcov_ctr_info {
  uint32_t num;                 /* Number of counter values for this type */
  uint64_t *values;             /* Counter values of this type */
  void (*merge)(uint64_t *, uint32_t); /* merge function - unused */
} gcov_ctr_info_t;

typedef struct gcov_info {
  uint32_t version;             /* GCOV version magic */
  struct gcov_info *next;       /* Intrusive linked list next */
  uint32_t stamp;               /* Timestamp */
  const char *filename;         /* Name of associated GCDA file */
  uint32_t n_functions;         /* Number of functions */
  const struct gcov_fn_info *functions; /* Functions themselves */
  uint32_t ctr_mask;                    /* Enabled counter types mask */
  struct gcov_ctr_info counts[0];       /* Counter data per counter type */
} gcov_info_t;

static gcov_info_t *info_head = NULL;

void __gcov_init(gcov_info_t *info) {
  info->next = info_head;
  info_head = info;
}

void __gcov_flush() {
}

void __gcov_merge_add(uint64_t *counters, uint32_t n_counters) {
}

void __gcov_merge_single(uint64_t *counters, uint32_t n_counters) {
}

void __gcov_merge_delta(uint64_t *counters, uint32_t n_counters) {
}

void __gcov_exit() {
}

static void gcov_dump_set_filename(const char *fn) {
  kprintf("GCOV-%s: ", fn);
}

static void gcov_dump_i32(uint32_t i) {
  kprintf("%08x ", i);
}

static void gcov_dump_i64(uint64_t i) {
  uint32_t l = i & 0xFFFFFFFF;
  uint32_t h = i >> 32;
  kprintf("%08x %08x ", l, h);
}

static void gcov_dump_end() {
  kprintf("\n");
}

static void gcov_dump_translation_unit(gcov_info_t *info) {
  gcov_dump_set_filename(info->filename);

  /* file : int32:magic int32:version int32:stamp record* */
  gcov_dump_i32(GCOV_DATA_MAGIC);
  gcov_dump_i32(info->version);
  gcov_dump_i32(info->stamp);

  /* unit function-data* summary:object summary:program* */
  
  unsigned offsets[GCOV_COUNTERS] = {0};
  unsigned fn_info_offset = 0;

  uint32_t mask = info->ctr_mask;
  unsigned num_types = 0;
  while (mask != 0) {
    if ((mask & 1) == 1)
      ++num_types;
    mask >>= 1;
  }

  for (unsigned i = 0; i < info->n_functions; ++i) {
    const gcov_fn_info_t *fn_info = (const gcov_fn_info_t*)
      ((const char*)info->functions + fn_info_offset);

    fn_info_offset += sizeof(gcov_fn_info_t) + sizeof(uint32_t) * num_types;
    /* function-data: announce_function arc_counts */
    /* announce_function: header int32:ident int32: checksum */
    gcov_dump_i32(GCOV_TAG_FUNCTION);
    gcov_dump_i32(2);           /* Length. 2 x 4 bytes */
    gcov_dump_i32(fn_info->ident);
    gcov_dump_i32(fn_info->checksum);

    uint32_t mask = info->ctr_mask;
    unsigned type = 0;
    while (mask != 0) {
      if ((mask & 1) == 0) {
        mask >>= 1;
        ++type;
        continue;
      }

      gcov_dump_i32(GCOV_TAG_FOR_COUNTER(type));
      gcov_dump_i32(fn_info->n_ctrs[type] * 2);
      for (unsigned ctr = 0; ctr < fn_info->n_ctrs[type]; ++ctr)
        gcov_dump_i64(info->counts[type].values[offsets[type]++]);

      ++type;
      mask >>= 1;
    }
  }
  gcov_dump_end();
}

static int gcov_dump() {
  for (gcov_info_t *i = info_head; i != NULL; i = i->next)
    gcov_dump_translation_unit(i);
  return 0;
}

static prereq_t req[] = { {"console",NULL}, {NULL,NULL} };
static prereq_t load_after[] = { {"hosted/console",NULL},
                                 {"x86/serial",NULL},
                                 {"x86/screen",NULL}, {NULL,NULL} };
static module_t run_on_startup x = {
  .name = "gcov",
  .required = req,
  .load_after = load_after,
  .init = NULL,
  .fini = &gcov_dump
};

#endif
