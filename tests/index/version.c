#include "clar_libgit2.h"
#include "index.h"

static git_repository *g_repo = NULL;

void test_index_version__cleanup(void)
{
        cl_git_sandbox_cleanup();
        g_repo = NULL;
}

void test_index_version__can_read_v4(void)
{
	const char *paths[] = {
	    "file.tx", "file.txt", "file.txz", "foo", "zzz",
	};
	git_index *index;
	size_t i;

	g_repo = cl_git_sandbox_init("indexv4");

	cl_git_pass(git_repository_index(&index, g_repo));
	cl_assert_equal_sz(git_index_entrycount(index), 5);

	for (i = 0; i < ARRAY_SIZE(paths); i++) {
		const git_index_entry *entry =
		    git_index_get_bypath(index, paths[i], GIT_INDEX_STAGE_NORMAL);

		cl_assert(entry != NULL);
	}

	git_index_free(index);
}

void test_index_version__can_write_v4(void)
{
	git_index *index;
	const git_index_entry *entry;

	g_repo = cl_git_sandbox_init("filemodes");
	cl_git_pass(git_repository_index(&index, g_repo));

	cl_assert(index->on_disk);
	cl_assert(git_index_version(index) == 2);

	cl_assert(git_index_entrycount(index) == 6);

	cl_git_pass(git_index_set_version(index, 4));

	cl_git_pass(git_index_write(index));
	git_index_free(index);

	cl_git_pass(git_repository_index(&index, g_repo));
	cl_assert(git_index_version(index) == 4);

	entry = git_index_get_bypath(index, "exec_off", 0);
	cl_assert(entry);
	entry = git_index_get_bypath(index, "exec_off2on_staged", 0);
	cl_assert(entry);
	entry = git_index_get_bypath(index, "exec_on", 0);
	cl_assert(entry);

	git_index_free(index);
}

void test_index_version__v4_uses_path_compression(void)
{
	git_index_entry entry;
	git_index *index;
	char path[250], buf[1];
	struct stat st;
	char i, j;

	memset(path, 'a', sizeof(path));
	memset(buf, 'a', sizeof(buf));

	memset(&entry, 0, sizeof(entry));
	entry.path = path;
	entry.mode = GIT_FILEMODE_BLOB;

	g_repo = cl_git_sandbox_init("indexv4");
	cl_git_pass(git_repository_index(&index, g_repo));

	/* write 676 paths of 250 bytes length */
	for (i = 'a'; i <= 'z'; i++) {
		for (j = 'a'; j < 'z'; j++) {
			path[ARRAY_SIZE(path) - 3] = i;
			path[ARRAY_SIZE(path) - 2] = j;
			path[ARRAY_SIZE(path) - 1] = '\0';
			cl_git_pass(git_index_add_frombuffer(index, &entry, buf, sizeof(buf)));
		}
	}

	cl_git_pass(git_index_write(index));
	cl_git_pass(p_stat(git_index_path(index), &st));

	/*
	 * Without path compression, the written paths would at
	 * least take
	 *
	 *    (entries * pathlen) = len
	 *    (676 * 250) = 169000
	 *
	 *  bytes. As index v4 uses suffix-compression and our
	 *  written paths only differ in the last two entries,
	 *  this number will be much smaller, e.g.
	 *
	 *    (1 * pathlen) + (675 * 2) = len
	 *    676 + 1350 = 2026
	 *
	 *    bytes.
	 *
	 *    Note that the above calculations do not include
	 *    additional metadata of the index, e.g. OIDs or
	 *    index extensions. Including those we get an index
	 *    of approx. 200kB without compression and 40kB with
	 *    compression. As this is a lot smaller than without
	 *    compression, we can verify that path compression is
	 *    used.
	 */
	cl_assert_(st.st_size < 75000, "path compression not enabled");

	git_index_free(index);
}
