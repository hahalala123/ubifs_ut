/*
 * Test harness implementation. See ut_support.h for the contract.
 */
#include "ut_support.h"

int ut_checks;
int ut_failures;

/* CRC-32 (poly 0xEDB88320, init/final-xor 0xFFFFFFFF), identical
 * semantics to the kernel's crc32(UBIFS_CRC32_INIT, buf, len). */
static uint32_t ut_crc_table[256];
static int ut_crc_table_ready;

static void ut_crc_init(void)
{
	uint32_t i, j, c;

	for (i = 0; i < 256; i++) {
		c = i;
		for (j = 0; j < 8; j++)
			c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
		ut_crc_table[i] = c;
	}
	ut_crc_table_ready = 1;
}

uint32_t ut_crc32(const void *buf, size_t len)
{
	const uint8_t *p = buf;
	uint32_t crc = 0xFFFFFFFFU;
	size_t i;

	if (!ut_crc_table_ready)
		ut_crc_init();

	for (i = 0; i < len; i++)
		crc = ut_crc_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
	return crc ^ 0xFFFFFFFFU;
}

/*
 * User-space stand-in for fs/ubifs/io.c:ubifs_check_node().
 *
 * The kernel version additionally validates node-type specific length
 * ranges (c->ranges[]) and, for data nodes, the payload CRC. For scanning
 * purposes the header checks below are the part that decides between
 * SCANNED_A_NODE and SCANNED_A_CORRUPT_NODE.
 */
int ubifs_check_node(const struct ubifs_info *c, void *buf, int len, int lnum,
		     int offs, int quiet, int must_chk)
{
	struct ubifs_ch *ch = buf;
	uint32_t crc;
	int node_len;

	(void)c; (void)lnum; (void)offs; (void)quiet; (void)must_chk;

	node_len = (int)le32_to_cpu(ch->len);

	/* Node must at least contain its own common header */
	if (node_len < (int)UBIFS_CH_SZ || node_len > len)
		return 1;

	/* Header CRC covers everything after magic+crc (starts at sqnum) */
	crc = ut_crc32((const uint8_t *)ch + 8, UBIFS_CH_SZ - 8);
	if (crc != le32_to_cpu(ch->crc))
		return 1;

	return 0;
}

/* User-space "flash": the test pre-fills the scan buffer, nothing to do. */
int ubifs_leb_read(const struct ubifs_info *c, int lnum, void *buf, int offs,
		   int len, int check_crc)
{
	(void)c; (void)lnum; (void)buf; (void)offs; (void)len; (void)check_crc;
	return 0;
}

void ut_make_node(void *buf, int node_type, int node_len, uint64_t sqnum)
{
	struct ubifs_ch *ch = buf;

	memset(buf, 0, node_len);
	ch->magic = cpu_to_le32(UBIFS_NODE_MAGIC);
	ch->sqnum = cpu_to_le64(sqnum);
	ch->len = cpu_to_le32(node_len);
	ch->node_type = node_type;
	ch->crc = cpu_to_le32(ut_crc32((const uint8_t *)ch + 8,
				       UBIFS_CH_SZ - 8));
}

void ut_make_pad_node(void *buf, int node_len, int pad_len, uint64_t sqnum)
{
	struct ubifs_pad_node *pad = buf;

	ut_make_node(buf, UBIFS_PAD_NODE, node_len, sqnum);
	pad->pad_len = cpu_to_le32(pad_len);
	/* The pad node's own CRC must be recomputed: pad_len is part of the
	 * header-checked payload only in the real fs, but keep the header
	 * CRC consistent regardless. */
	pad->ch.crc = cpu_to_le32(ut_crc32((const uint8_t *)&pad->ch + 8,
					   UBIFS_CH_SZ - 8));
}
