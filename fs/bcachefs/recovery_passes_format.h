/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _BCACHEFS_RECOVERY_PASSES_FORMAT_H
#define _BCACHEFS_RECOVERY_PASSES_FORMAT_H

#define PASS_SILENT		BIT(0)
#define PASS_FSCK		BIT(1)
#define PASS_UNCLEAN		BIT(2)
#define PASS_ALWAYS		BIT(3)
#define PASS_ONLINE		BIT(4)
#define PASS_ALLOC		BIT(5)
#define PASS_FSCK_ALLOC		(PASS_FSCK|PASS_ALLOC)

#ifdef CONFIG_BCACHEFS_DEBUG
#define PASS_FSCK_DEBUG		BIT(1)
#else
#define PASS_FSCK_DEBUG		0
#endif

/*
 * Passes may be reordered, but the second field is a persistent identifier and
 * must never change:
 */
#define BCH_RECOVERY_PASSES()								\
	x(recovery_pass_empty,			41, PASS_SILENT)			\
	x(scan_for_btree_nodes,			37, 0)					\
	x(check_topology,			 4, 0)					\
	x(accounting_read,			39, PASS_ALWAYS)			\
	x(alloc_read,				 0, PASS_ALWAYS)			\
	x(stripes_read,				 1, 0)					\
	x(initialize_subvolumes,		 2, 0)					\
	x(snapshots_read,			 3, PASS_ALWAYS)			\
	x(check_allocations,			 5, PASS_FSCK_ALLOC)			\
	x(trans_mark_dev_sbs,			 6, PASS_ALWAYS|PASS_SILENT|PASS_ALLOC)	\
	x(fs_journal_alloc,			 7, PASS_ALWAYS|PASS_SILENT|PASS_ALLOC)	\
	x(set_may_go_rw,			 8, PASS_ALWAYS|PASS_SILENT)		\
	x(journal_replay,			 9, PASS_ALWAYS)			\
	x(check_alloc_info,			10, PASS_ONLINE|PASS_FSCK_ALLOC)	\
	x(check_lrus,				11, PASS_ONLINE|PASS_FSCK_ALLOC)	\
	x(check_btree_backpointers,		12, PASS_ONLINE|PASS_FSCK_ALLOC)	\
	x(check_backpointers_to_extents,	13, PASS_ONLINE|PASS_FSCK_DEBUG)	\
	x(check_extents_to_backpointers,	14, PASS_ONLINE|PASS_FSCK_ALLOC)	\
	x(check_alloc_to_lru_refs,		15, PASS_ONLINE|PASS_FSCK_ALLOC)	\
	x(fs_freespace_init,			16, PASS_ALWAYS|PASS_SILENT)		\
	x(bucket_gens_init,			17, 0)					\
	x(reconstruct_snapshots,		38, 0)					\
	x(check_snapshot_trees,			18, PASS_ONLINE|PASS_FSCK)		\
	x(check_snapshots,			19, PASS_ONLINE|PASS_FSCK)		\
	x(check_subvols,			20, PASS_ONLINE|PASS_FSCK)		\
	x(check_subvol_children,		35, PASS_ONLINE|PASS_FSCK)		\
	x(delete_dead_snapshots,		21, PASS_ONLINE|PASS_FSCK)		\
	x(fs_upgrade_for_subvolumes,		22, 0)					\
	x(check_inodes,				24, PASS_FSCK)				\
	x(check_extents,			25, PASS_FSCK)				\
	x(check_indirect_extents,		26, PASS_ONLINE|PASS_FSCK)		\
	x(check_dirents,			27, PASS_FSCK)				\
	x(check_xattrs,				28, PASS_FSCK)				\
	x(check_root,				29, PASS_ONLINE|PASS_FSCK)		\
	x(check_unreachable_inodes,		40, PASS_FSCK)				\
	x(check_subvolume_structure,		36, PASS_ONLINE|PASS_FSCK)		\
	x(check_directory_structure,		30, PASS_ONLINE|PASS_FSCK)		\
	x(check_nlinks,				31, PASS_FSCK)				\
	x(check_rebalance_work,			43, PASS_ONLINE|PASS_FSCK)		\
	x(resume_logged_ops,			23, PASS_ALWAYS)			\
	x(delete_dead_inodes,			32, PASS_ALWAYS)			\
	x(fix_reflink_p,			33, 0)					\
	x(set_fs_needs_rebalance,		34, 0)					\
	x(lookup_root_inode,			42, PASS_ALWAYS|PASS_SILENT)

/* We normally enumerate recovery passes in the order we run them: */
enum bch_recovery_pass {
#define x(n, id, when)	BCH_RECOVERY_PASS_##n,
	BCH_RECOVERY_PASSES()
#undef x
	BCH_RECOVERY_PASS_NR
};

/* But we also need stable identifiers that can be used in the superblock */
enum bch_recovery_pass_stable {
#define x(n, id, when)	BCH_RECOVERY_PASS_STABLE_##n = id,
	BCH_RECOVERY_PASSES()
#undef x
};

struct recovery_pass_entry {
	__le64			last_run;
	__le32			last_runtime;
	__le32			flags;
};

LE32_BITMASK(BCH_RECOVERY_PASS_NO_RATELIMIT,	struct recovery_pass_entry, flags, 0, 1)

struct bch_sb_field_recovery_passes {
	struct bch_sb_field	field;
	struct recovery_pass_entry start[];
};

static inline unsigned
recovery_passes_nr_entries(struct bch_sb_field_recovery_passes *r)
{
	return r
		? ((vstruct_end(&r->field) - (void *) &r->start[0]) /
		   sizeof(struct recovery_pass_entry))
		: 0;
}

#endif /* _BCACHEFS_RECOVERY_PASSES_FORMAT_H */
