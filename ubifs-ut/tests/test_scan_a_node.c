/*
 * Unit tests for fs/ubifs/scan.c:ubifs_scan_a_node().
 *
 * Focus (interview question 1): the padding-node validation branch of
 * ubifs_scan_a_node() - the highest-traffic decision point when scanning
 * journal/bud LEBs during mount and GC. The remaining branches (empty
 * space, raw padding bytes, short buffer, corrupt header, plain valid
 * node) are covered as well so the whole function is regression-proof.
 *
 * A small ubifs_scan() integration test at the end demonstrates how the
 * same harness extends to question 2 (journal replay input construction).
 */
#include "ut_support.h"

/* Prototypes from the real scan.c under test */
int ubifs_scan_a_node(const struct ubifs_info *c, void *buf, int len,
		      int lnum, int offs, int quiet);
struct ubifs_scan_leb *ubifs_scan(const struct ubifs_info *c, int lnum,
				  int offs, void *sbuf, int quiet);
void ubifs_scan_destroy(struct ubifs_scan_leb *sleb);

#define TEST_LEB_SIZE (128 * 1024)

static struct ubifs_info c;
static uint8_t buf[TEST_LEB_SIZE];

static void setup(void)
{
	c.leb_size = TEST_LEB_SIZE;
	c.min_io_size = 8;
	memset(buf, 0xFF, sizeof(buf));
}

/* --- empty space ------------------------------------------------------ */
static void test_empty_space(void)
{
	setup();
	CHECK_EQ(ubifs_scan_a_node(&c, buf, sizeof(buf), 3, 0x40, 1),
		 SCANNED_EMPTY_SPACE);
}

/* --- raw padding bytes (no node header) ------------------------------- */
static void test_raw_padding_bytes(void)
{
	setup();
	memset(buf, UBIFS_PADDING_BYTE, 16);
	CHECK_EQ(ubifs_scan_a_node(&c, buf, sizeof(buf), 3, 0, 1), 16);
}

/* padding that does not end on an 8-byte boundary is garbage */
static void test_raw_padding_misaligned(void)
{
	setup();
	memset(buf, UBIFS_PADDING_BYTE, 10);
	CHECK_EQ(ubifs_scan_a_node(&c, buf, sizeof(buf), 3, 0, 1),
		 SCANNED_GARBAGE);
}

/* neither empty space, nor magic, nor padding bytes: garbage */
static void test_garbage(void)
{
	setup();
	buf[0] = 0x12; buf[1] = 0x34; buf[2] = 0x56; buf[3] = 0x78;
	CHECK_EQ(ubifs_scan_a_node(&c, buf, sizeof(buf), 3, 0, 1),
		 SCANNED_GARBAGE);
}

/* --- buffer shorter than a common header ------------------------------ */
static void test_len_too_small(void)
{
	setup();
	/* magic is valid but there is no room for the common header */
	struct ubifs_ch *ch = (void *)buf;
	ch->magic = cpu_to_le32(UBIFS_NODE_MAGIC);
	CHECK_EQ(ubifs_scan_a_node(&c, buf, UBIFS_CH_SZ - 1, 3, 0, 1),
		 SCANNED_GARBAGE);
}

/* ================= padding node validation (the focus) ================= */

/* A well-formed pad node: node_len + pad_len lands on an 8-byte boundary
 * and stays inside the LEB. The function returns the total skip length. */
static void test_valid_pad_node(void)
{
	int offs = 0x100;

	setup();
	ut_make_pad_node(buf, UBIFS_PAD_NODE_SZ, 36, 5);
	CHECK_EQ(ubifs_scan_a_node(&c, buf, sizeof(buf), 3, offs, 1),
		 UBIFS_PAD_NODE_SZ + 36);
}

/* pad_len so large that node + padding runs past the end of the LEB */
static void test_pad_node_overruns_leb(void)
{
	setup();
	ut_make_pad_node(buf, UBIFS_PAD_NODE_SZ, TEST_LEB_SIZE, 5);
	CHECK_EQ(ubifs_scan_a_node(&c, buf, sizeof(buf), 3, 0x100, 1),
		 SCANNED_A_BAD_PAD_NODE);
}

/* pad_len whose signed interpretation is negative (high bit set) */
static void test_pad_node_negative_pad_len(void)
{
	setup();
	ut_make_pad_node(buf, UBIFS_PAD_NODE_SZ, 0, 5);
	((struct ubifs_pad_node *)buf)->pad_len = cpu_to_le32(0x80000000);
	CHECK_EQ(ubifs_scan_a_node(&c, buf, sizeof(buf), 3, 0x100, 1),
		 SCANNED_A_BAD_PAD_NODE);
}

/* node_len + pad_len not aligned to 8 bytes */
static void test_pad_node_misaligned_total(void)
{
	setup();
	/* UBIFS_PAD_NODE_SZ (28) + 2 == 30, 30 % 8 != 0 */
	ut_make_pad_node(buf, UBIFS_PAD_NODE_SZ, 2, 5);
	CHECK_EQ(ubifs_scan_a_node(&c, buf, sizeof(buf), 3, 0x100, 1),
		 SCANNED_A_BAD_PAD_NODE);
}

/* 28 + 8 == 36: aligned to 4 bytes but NOT to 8 - distinguishes the
 * 8-byte alignment check from a weaker "& 3"-style check */
static void test_pad_node_36_bytes_is_misaligned(void)
{
	setup();
	ut_make_pad_node(buf, UBIFS_PAD_NODE_SZ, 8, 5);
	CHECK_EQ(ubifs_scan_a_node(&c, buf, sizeof(buf), 3, 0x100, 1),
		 SCANNED_A_BAD_PAD_NODE);
}

/* a pad node may end exactly at the LEB end: offs + node_len + pad_len
 * == leb_size must be accepted (the check is '>', not '>=') */
static void test_pad_node_ends_exactly_at_leb_end(void)
{
	int offs = TEST_LEB_SIZE - (UBIFS_PAD_NODE_SZ + 36);

	setup();
	ut_make_pad_node(buf, UBIFS_PAD_NODE_SZ, 36, 5);
	CHECK_EQ(ubifs_scan_a_node(&c, buf, sizeof(buf), 3, offs, 1),
		 UBIFS_PAD_NODE_SZ + 36);
}

/* ================= header corruption / plain nodes ===================== */

/* header CRC mismatch must be reported as a corrupt node */
static void test_corrupt_header_crc(void)
{
	setup();
	ut_make_node(buf, UBIFS_CS_NODE, UBIFS_CS_NODE_SZ, 7);
	((struct ubifs_ch *)buf)->crc = cpu_to_le32(0xDEADBEEF);
	CHECK_EQ(ubifs_scan_a_node(&c, buf, sizeof(buf), 3, 0x200, 1),
		 SCANNED_A_CORRUPT_NODE);
}

/* any non-pad node with a valid header scans as SCANNED_A_NODE */
static void test_valid_plain_node(void)
{
	setup();
	ut_make_node(buf, UBIFS_CS_NODE, UBIFS_CS_NODE_SZ, 7);
	CHECK_EQ(ubifs_scan_a_node(&c, buf, sizeof(buf), 3, 0x200, 1),
		 SCANNED_A_NODE);
}

/* --- integration: ubifs_scan() over a hand-built LEB ------------------- */
static void test_ubifs_scan_collects_nodes(void)
{
	struct ubifs_scan_leb *sleb;
	struct ubifs_scan_node *snod;
	int lnum = 5;
	int leb_size = 4096;
	void *sbuf = malloc(leb_size);
	int offs = 0;

	c.leb_size = leb_size;
	c.min_io_size = 8;
	memset(sbuf, 0xFF, leb_size);

	/* node 1: commit start node at offset 0 */
	ut_make_node(sbuf + offs, UBIFS_CS_NODE, UBIFS_CS_NODE_SZ, 11);
	offs += UBIFS_CS_NODE_SZ;
	/* pad node at 32: 28-byte node + 4 pad bytes = 32, lands on offset 64 */
	ut_make_pad_node(sbuf + offs, UBIFS_PAD_NODE_SZ, 4, 12);
	offs += UBIFS_PAD_NODE_SZ + 4;
	/* node 2: another commit start node at offset 64 */
	ut_make_node(sbuf + offs, UBIFS_CS_NODE, UBIFS_CS_NODE_SZ, 13);

	sleb = ubifs_scan(&c, lnum, 0, sbuf, 1);
	CHECK(!IS_ERR(sleb));
	if (IS_ERR(sleb)) {
		free(sbuf);
		return;
	}
	CHECK_EQ(sleb->lnum, lnum);
	CHECK_EQ(sleb->nodes_cnt, 2);
	snod = list_entry(sleb->nodes.next, struct ubifs_scan_node, list);
	CHECK_EQ(snod->type, UBIFS_CS_NODE);
	CHECK_EQ(snod->offs, 0);
	CHECK_EQ(snod->sqnum, 11);
	snod = list_entry(snod->list.next, struct ubifs_scan_node, list);
	CHECK_EQ(snod->offs, 64);
	CHECK_EQ(snod->sqnum, 13);
	CHECK_EQ(sleb->endpt, 64 + UBIFS_CS_NODE_SZ);
	ubifs_scan_destroy(sleb);
	free(sbuf);
}

int main(void)
{
	test_empty_space();
	test_raw_padding_bytes();
	test_raw_padding_misaligned();
	test_garbage();
	test_len_too_small();
	test_valid_pad_node();
	test_pad_node_overruns_leb();
	test_pad_node_negative_pad_len();
	test_pad_node_misaligned_total();
	test_pad_node_36_bytes_is_misaligned();
	test_pad_node_ends_exactly_at_leb_end();
	test_corrupt_header_crc();
	test_valid_plain_node();
	test_ubifs_scan_collects_nodes();

	printf("ubifs_scan_a_node: %d checks, %d failures\n",
	       ut_checks, ut_failures);
	return ut_failures ? 1 : 0;
}
