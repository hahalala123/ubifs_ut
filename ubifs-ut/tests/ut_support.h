/*
 * Test harness helpers for the UBIFS user-space unit tests.
 *
 * Provides:
 *   - a table-driven CRC32 (same polynomial/semantics as the kernel's
 *     crc32(UBIFS_CRC32_INIT, buf, len), i.e. plain CRC-32);
 *   - a user-space stand-in for ubifs_check_node() (fs/ubifs/io.c) that
 *     performs the same header validation that matters for scanning:
 *     node length sanity + common-header CRC check;
 *   - a stub for ubifs_leb_read() (user-space "flash" is just memory);
 *   - tiny CHECK() test-framework macros.
 */
#ifndef __UT_SUPPORT_H__
#define __UT_SUPPORT_H__

#include "ubifs.h"

/* --- CRC32 ------------------------------------------------------------ */
uint32_t ut_crc32(const void *buf, size_t len);

/* --- Stubs for kernel functions used by scan.c ------------------------ */
int ubifs_check_node(const struct ubifs_info *c, void *buf, int len, int lnum,
		     int offs, int quiet, int must_chk);
int ubifs_leb_read(const struct ubifs_info *c, int lnum, void *buf, int offs,
		   int len, int check_crc);

/* --- Node-building helpers --------------------------------------------
 * Fill in a node's common header and compute its header CRC exactly like
 * the kernel does (crc over the header minus magic/crc, i.e. from sqnum).
 */
void ut_make_node(void *buf, int node_type, int node_len, uint64_t sqnum);
void ut_make_pad_node(void *buf, int node_len, int pad_len, uint64_t sqnum);

/* --- Minimal test framework ------------------------------------------- */
extern int ut_checks;
extern int ut_failures;

#define CHECK(cond) do {                                              \
	ut_checks++;                                                  \
	if (!(cond)) {                                                \
		ut_failures++;                                        \
		fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, \
			#cond);                                       \
	}                                                             \
} while (0)

#define CHECK_EQ(got, want) do {                                      \
	ut_checks++;                                                  \
	long _g = (long)(got), _w = (long)(want);                     \
	if (_g != _w) {                                               \
		ut_failures++;                                        \
		fprintf(stderr, "FAIL %s:%d: %s == %ld, want %ld\n",  \
			__FILE__, __LINE__, #got, _g, _w);            \
	}                                                             \
} while (0)

#endif /* __UT_SUPPORT_H__ */
