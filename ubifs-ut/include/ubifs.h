/*
 * User-space stub of the kernel's fs/ubifs/ubifs.h + ubifs-media.h subset.
 *
 * Purpose: let the *unmodified* mainline fs/ubifs/scan.c compile and run in
 * user space so that ubifs_scan_a_node() (and friends) can be unit-tested
 * without real flash hardware.
 *
 * Everything here is either:
 *   - copied verbatim from ubifs-media.h (on-flash format must match), or
 *   - a minimal user-space re-implementation of a kernel primitive
 *     (list_head, leXX_to_cpu, kmem wrappers, debug macros...).
 *
 * Baseline: fs/ubifs/scan.c @ 69050f8d6d075dc01af7a5f2f550a8067510366f
 */
#ifndef __UBIFS_USERSPACE_STUB_H__
#define __UBIFS_USERSPACE_STUB_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>

/* ------------------------------------------------------------------ */
/* Little-endian types: host is assumed little-endian (x86/ARM)        */
/* ------------------------------------------------------------------ */
typedef uint8_t  __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint64_t __u64;
typedef uint16_t __le16;
typedef uint32_t __le32;
typedef uint64_t __le64;

#define le16_to_cpu(x) ((__u16)(x))
#define le32_to_cpu(x) ((__u32)(x))
#define le64_to_cpu(x) ((__u64)(x))
#define cpu_to_le16(x) ((__le16)(x))
#define cpu_to_le32(x) ((__le32)(x))
#define cpu_to_le64(x) ((__le64)(x))

#define __packed __attribute__((packed))

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

/* ------------------------------------------------------------------ */
/* Kernel primitives (minimal re-implementations)                      */
/* ------------------------------------------------------------------ */
#define min_t(t, a, b) ((t)((a) < (b) ? (a) : (b)))
#define ALIGN(x, a)    (((x) + (a) - 1) & ~((a) - 1))

#define GFP_NOFS 0
#define kzalloc_obj(type, gfp) calloc(1, sizeof(type))
#define kmalloc_obj(type, gfp) malloc(sizeof(type))
#define kfree(p) free(p)
#define vmalloc(s) malloc(s)
#define vfree(p) free(p)

#define EBADMSG 74
#define ENOMEM  12
#define EINVAL  22
#define EUCLEAN 117

#define ERR_PTR(err) ((void *)(long)(err))
#define PTR_ERR(p)   ((long)(p))
#define IS_ERR(p)    ((unsigned long)(p) >= (unsigned long)-4095)

#define cond_resched() do { } while (0)

#define KERN_DEBUG ""
#define DUMP_PREFIX_OFFSET 0
#define print_hex_dump(a, b, c, d, e, f, g, h) do { } while (0)

/* Minimal list implementation */
struct list_head {
	struct list_head *next, *prev;
};

static inline void INIT_LIST_HEAD(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

static inline void __list_add(struct list_head *new, struct list_head *prev,
			      struct list_head *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
}

static inline void list_del(struct list_head *entry)
{
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
}

static inline int list_empty(const struct list_head *head)
{
	return head->next == head;
}

#define container_of(ptr, type, member) \
	((type *)((char *)(ptr) - offsetof(type, member)))

#define list_entry(ptr, type, member) container_of(ptr, type, member)

/* Debug / error output */
#define ubifs_err(c, fmt, ...) fprintf(stderr, "UBIFS err: " fmt "\n", ##__VA_ARGS__)
#define ubifs_warn(c, fmt, ...) fprintf(stderr, "UBIFS warn: " fmt "\n", ##__VA_ARGS__)
#define ubifs_msg(c, fmt, ...) fprintf(stderr, "UBIFS: " fmt "\n", ##__VA_ARGS__)
#define ubifs_assert(c, expr) assert(expr)
#define dbg_scan(fmt, ...) do { } while (0)
#define ubifs_dump_node(c, node, len) do { } while (0)

/* Keys: not exercised by the scan tests, keep as inert placeholders */
struct ubifs_info;

union ubifs_key {
	__u8 k[16];
	__u64 junk;
};

static inline void key_read(const struct ubifs_info *c, const void *from,
			    union ubifs_key *to)
{
	(void)c;
	memcpy(to, from, sizeof(*to));
}

static inline void invalid_key_init(const struct ubifs_info *c,
				    union ubifs_key *key)
{
	(void)c;
	memset(key, 0, sizeof(*key));
}

/* ------------------------------------------------------------------ */
/* On-flash format: copied from ubifs-media.h                          */
/* ------------------------------------------------------------------ */
#define UBIFS_NODE_MAGIC  0x06101831
#define UBIFS_PADDING_BYTE 0xCE

enum {
	UBIFS_INO_NODE,
	UBIFS_DATA_NODE,
	UBIFS_DENT_NODE,
	UBIFS_XENT_NODE,
	UBIFS_TRUN_NODE,
	UBIFS_PAD_NODE,
	UBIFS_SB_NODE,
	UBIFS_MST_NODE,
	UBIFS_REF_NODE,
	UBIFS_IDX_NODE,
	UBIFS_CS_NODE,
	UBIFS_ORPH_NODE,
	UBIFS_AUTH_NODE,
	UBIFS_SIG_NODE,
	UBIFS_NODE_TYPES_CNT,
};

/* Node sizes (N.B. these are guaranteed to be multiples of 8) */
#define UBIFS_CH_SZ        sizeof(struct ubifs_ch)
#define UBIFS_INO_NODE_SZ  sizeof(struct ubifs_ino_node)
#define UBIFS_PAD_NODE_SZ  sizeof(struct ubifs_pad_node)
#define UBIFS_CS_NODE_SZ   sizeof(struct ubifs_cs_node)

struct ubifs_ch {
	__le32 magic;
	__le32 crc;
	__le64 sqnum;
	__le32 len;
	__u8 node_type;
	__u8 group_type;
	__u8 padding[2];
} __packed;

#define UBIFS_MAX_KEY_LEN 16

struct ubifs_ino_node {
	struct ubifs_ch ch;
	__u8 key[UBIFS_MAX_KEY_LEN];
	__le64 creat_sqnum;
	__le64 size;
	__le64 atime_sec;
	__le64 ctime_sec;
	__le64 mtime_sec;
	__le32 atime_nsec;
	__le32 ctime_nsec;
	__le32 mtime_nsec;
	__le32 nlink;
	__le32 uid;
	__le32 gid;
	__le32 mode;
	__le32 flags;
	__le32 data_len;
	__le32 xattr_cnt;
	__le32 xattr_size;
	__u8 padding1[4];
	__le32 xattr_names;
	__le16 compr_type;
	__u8 padding2[26];
	__u8 data[];
} __packed;

struct ubifs_pad_node {
	struct ubifs_ch ch;
	__le32 pad_len;
} __packed;

struct ubifs_cs_node {
	struct ubifs_ch ch;
	__le64 cmt_no;
} __packed;

static inline const char *dbg_ntype(int type)
{
	static const char * const names[UBIFS_NODE_TYPES_CNT] = {
		"ino", "data", "dent", "xent", "trun", "pad", "sb",
		"mst", "ref", "idx", "cs", "orph", "auth", "sig",
	};
	if (type < 0 || type >= UBIFS_NODE_TYPES_CNT)
		return "unknown";
	return names[type];
}

/* ------------------------------------------------------------------ */
/* In-memory data structures (mirrors fs/ubifs/ubifs.h)                */
/* ------------------------------------------------------------------ */
struct ubifs_scan_node {
	struct list_head list;
	union ubifs_key key;
	__u64 sqnum;
	int type;
	int offs;
	int len;
	void *node;
};

struct ubifs_scan_leb {
	struct list_head nodes;
	__u64 sqnum;
	void *buf;
	int lnum;
	int nodes_cnt;
	int endpt;
};

/* Only the fields actually touched by scan.c */
struct ubifs_info {
	int leb_size;
	int min_io_size;
};

/* Return codes of ubifs_scan_a_node(): >0 means "scanned that many padding
 * bytes", the node codes are <= 0 (verbatim from the kernel ubifs.h). */
enum {
	SCANNED_GARBAGE        = 0,
	SCANNED_EMPTY_SPACE    = -1,
	SCANNED_A_NODE         = -2,
	SCANNED_A_CORRUPT_NODE = -3,
	SCANNED_A_BAD_PAD_NODE = -4,
};

/* ------------------------------------------------------------------ */
/* Functions provided by the test harness (tests/ut_support.c)         */
/* ------------------------------------------------------------------ */
int ubifs_check_node(const struct ubifs_info *c, void *buf, int len, int lnum,
		     int offs, int quiet, int must_chk);
int ubifs_leb_read(const struct ubifs_info *c, int lnum, void *buf, int offs,
		   int len, int check_crc);

/* Functions defined by scan.c (declared here as in the kernel ubifs.h) */
struct ubifs_scan_leb *ubifs_start_scan(const struct ubifs_info *c, int lnum,
					int offs, void *sbuf);
void ubifs_end_scan(const struct ubifs_info *c, struct ubifs_scan_leb *sleb,
		    int lnum, int offs);
int ubifs_add_snod(const struct ubifs_info *c, struct ubifs_scan_leb *sleb,
		   void *buf, int offs);
void ubifs_scanned_corruption(const struct ubifs_info *c, int lnum, int offs,
			      void *buf);
struct ubifs_scan_leb *ubifs_scan(const struct ubifs_info *c, int lnum,
				  int offs, void *sbuf, int quiet);
void ubifs_scan_destroy(struct ubifs_scan_leb *sleb);

/* CRC helper shared by the harness and the tests */
uint32_t ut_crc32(const void *buf, size_t len);

#endif /* __UBIFS_USERSPACE_STUB_H__ */
