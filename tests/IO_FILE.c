typedef int __off_t;
typedef int __off64_t;
typedef int _IO_lock_t;

struct _IO_marker;
struct _IO_codecvt;
struct _IO_wide_data;
struct _IO_FILE;

struct _IO_FILE {
	int _flags; /* High-order word is _IO_MAGIC; rest is flags. */

	/* The following pointers correspond to the C++ streambuf protocol. */
	char *_IO_read_ptr;   /* Current read pointer */
	char *_IO_read_end;   /* End of get area. */
	char *_IO_read_base;  /* Start of putback+get area. */
	char *_IO_write_base; /* Start of put area. */
	char *_IO_write_ptr;  /* Current put pointer. */
	char *_IO_write_end;  /* End of put area. */
	char *_IO_buf_base;   /* Start of reserve area. */
	char *_IO_buf_end;    /* End of reserve area. */

	/* The following fields are used to support backing up and undo. */
	char *_IO_save_base;   /* Pointer to start of non-current get area. */
	char *_IO_backup_base; /* Pointer to first valid character of backup area */
	char *_IO_save_end;    /* Pointer to end of non-current get area. */

	struct _IO_marker *_markers;

	struct _IO_FILE *_chain;

	int _fileno;
	int _flags2 : 24;
	/* Fallback buffer to use when malloc fails to allocate one.  */
	char    _short_backupbuf[1];
	__off_t _old_offset; /* This used to be _offset but it's too small.  */

	/* 1+column number of pbase(); 0 is unknown. */
	unsigned short _cur_column;
	signed char    _vtable_offset;
	char           _shortbuf[1];

	_IO_lock_t *_lock;
#ifdef _IO_USE_OLD_IO_FILE
};

struct _IO_FILE_complete {
	struct _IO_FILE _file;
#endif
	__off64_t _offset;
	/* Wide character stream stuff.  */
	struct _IO_codecvt   *_codecvt;
	struct _IO_wide_data *_wide_data;
	struct _IO_FILE      *_freeres_list;
	void                 *_freeres_buf;
	struct _IO_FILE     **_prevchain;
	int                   _mode;
	/* Make sure we don't get into trouble again.  */
	char _unused2[15 * sizeof(int) - 5 * sizeof(void *)];
};
