#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#include <uthread.h>
#include <fs.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define test_fs_error(fmt, ...) \
	fprintf(stderr, "%s: "fmt"\n", __func__, ##__VA_ARGS__)

#define die(...)			\
do {					\
	test_fs_error(__VA_ARGS__);	\
	exit(1);			\
} while (0)

#define die_perror(msg)	\
do {			\
	perror(msg);	\
	exit(1);	\
} while (0)


struct thread_arg {
	int argc;
	char **argv;
};


void thread_fs_stat(void *arg)
{
	struct thread_arg *t_arg = arg;
	char *diskname, *filename;
	int fs_fd;
	size_t stat;

	if (t_arg->argc < 2)
		die("need <diskname> <filename>");

	diskname = t_arg->argv[0];
	filename = t_arg->argv[1];

	if (fs_mount(diskname))
		die("Cannot mount diskname");

	fs_fd = fs_open(filename);
	if (fs_fd < 0) {
		fs_umount();
		die("Cannot open file");
	}

	stat = fs_stat(fs_fd);
	if (stat < 0) {
		fs_umount();
		die("Cannot stat file");
	}
	if (!stat) {
		/* Nothing to read, file is empty */
		printf("Empty file\n");
		return;
	}

	if (fs_close(fs_fd)) {
		fs_umount();
		die("Cannot close file");
	}

	if (fs_umount())
		die("cannot unmount diskname");

	printf("Size of file '%s' is %zu bytes\n", filename, stat);
}

void thread_fs_cat(void *arg)
{
	struct thread_arg *t_arg = arg;
	char *diskname, *filename, *buf;
	int fs_fd;
	size_t stat, read;

	if (t_arg->argc < 2)
		die("need <diskname> <filename>");

	diskname = t_arg->argv[0];
	filename = t_arg->argv[1];

	if (fs_mount(diskname))
		die("Cannot mount diskname");

	fs_fd = fs_open(filename);
	if (fs_fd < 0) {
		fs_umount();
		die("Cannot open file");
	}

	stat = fs_stat(fs_fd);
	if (stat < 0) {
		fs_umount();
		die("Cannot stat file");
	}
	if (!stat) {
		/* Nothing to read, file is empty */
		printf("Empty file\n");
		return;
	}
	buf = malloc(stat);
	if (!buf) {
		perror("malloc");
		fs_umount();
		die("Cannot malloc");
	}

	read = fs_read(fs_fd, buf, stat);

	if (fs_close(fs_fd)) {
		fs_umount();
		die("Cannot close file");
	}

	if (fs_umount())
		die("cannot unmount diskname");

	printf("Read file '%s' (%zu/%zu bytes)\n", filename, read, stat);
	printf("Content of the file:\n%s", buf);

	free(buf);
}

void thread_fs_rm(void *arg)
{
	struct thread_arg *t_arg = arg;
	char *diskname, *filename;

	if (t_arg->argc < 2)
		die("need <diskname> <filename>");

	diskname = t_arg->argv[0];
	filename = t_arg->argv[1];

	if (fs_mount(diskname))
		die("Cannot mount diskname");

	if (fs_delete(filename)) {
		fs_umount();
		die("Cannot delete file");
	}

	if (fs_umount())
		die("Cannot unmount diskname");

	printf("Removed file '%s'\n", filename);
}

void thread_fs_add(void *arg)
{
	struct thread_arg *t_arg = arg;
	char *diskname, *filename, *buf;
	int fd, fs_fd;
	struct stat st;
	size_t written;

	if (t_arg->argc < 2)
		die("Usage: <diskname> <host filename>");

	diskname = t_arg->argv[0];
	filename = t_arg->argv[1];

	/* Open file on host computer */
	fd = open(filename, O_RDONLY);
	if (fd < 0)
		die_perror("open");
	if (fstat(fd, &st))
		die_perror("fstat");
	if (!S_ISREG(st.st_mode))
		die("Not a regular file: %s\n", filename);

	/* Map file into buffer */
	buf = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (!buf)
		die_perror("mmap");

	/* Now, deal with our filesystem:
	 * - mount, create a new file, copy content of host file into this new
	 *   file, close the new file, and unmount
	 */
//    printf("before mount Wrote file %s \n", filename);
	if (fs_mount(diskname))
		die("Cannot mount diskname");

//    printf("before create Wrote file %s \n", filename);
	if (fs_create(filename)) {
		fs_umount();
		die("Cannot create file");
	}
//    printf("before open Wrote file %s \n", filename);
	fs_fd = fs_open(filename);
	if (fs_fd < 0) {
		fs_umount();
		die("Cannot open file");
	}

//    printf("before write Wrote file %s \n", filename);
	written = fs_write(fs_fd, buf, st.st_size);
    if (written==-1){
        fs_umount();
        die("Cannot write file");
    }
//    printf("before close Wrote file %s \n", filename);
	if (fs_close(fs_fd)) {
		fs_umount();
		die("Cannot close file");
	}

//    printf("before umount Wrote file %s \n", filename);

	if (fs_umount())
		die("Cannot unmount diskname");

//	printf("Wrote file '%s' (%zu/%zu bytes)\n", filename, written,
//	       st.st_size);

	munmap(buf, st.st_size);
	close(fd);
    fs_ls();
//    printf("Wrote file '%s' (%zu/%zu bytes)\n", filename, written,
//	       st.st_size);
}

void thread_fs_ls(void *arg)
{
	struct thread_arg *t_arg = arg;
	char *diskname;

	if (t_arg->argc < 1)
		die("Usage: <diskname> <filename>");

	diskname = t_arg->argv[0];

	if (fs_mount(diskname))
		die("Cannot mount diskname");

	fs_ls();

	if (fs_umount())
		die("Cannot unmount diskname");
}

void thread_fs_info(void *arg)
{
	struct thread_arg *t_arg = arg;
	char *diskname;

	if (t_arg->argc < 1)
		die("Usage: <diskname>");

	diskname = t_arg->argv[0];

	if (fs_mount(diskname))
		die("Cannot mount diskname");

	fs_info();

	if (fs_umount())
		die("Cannot unmount diskname");
}



void *thread_func_multiuseradd(void *arg){
    struct thread_arg *t_arg = arg;
    t_arg->argv = (char**)malloc(2*sizeof (char *));
    t_arg->argv[0] = (char*)malloc(20*sizeof (char));
    t_arg->argv[1] = (char*)malloc(20*sizeof (char));

    if(t_arg->argc==1){
        t_arg->argv[0]="our_driver";
        t_arg->argv[1]="file1.txt";
    } else if(t_arg->argc==2){
        t_arg->argv[0]="our_driver";
        t_arg->argv[1]="file2.txt";
    } else if(t_arg->argc==3){
        t_arg->argv[0]="our_driver";
        t_arg->argv[1]="file3.txt";
    } else if(t_arg->argc==4){
        t_arg->argv[0]="our_driver";
        t_arg->argv[1]="file4.txt";
    } else if(t_arg->argc==5){
        t_arg->argv[0]="our_driver";
        t_arg->argv[1]="file5.txt";
    }
    t_arg->argc=2;
    uthread_start(thread_fs_add, t_arg);
    return NULL;
}

void thread_fs_multiuseradd(void *arg){
    struct thread_arg *t_arg = arg;
    if (t_arg->argc < 2)
        die("Usage: <diskname> <host filename>");

    struct thread_arg t_arg0 ;
    t_arg0.argc=1;
    struct thread_arg t_arg1 ;
    t_arg1.argc=2;
    struct thread_arg t_arg2 ;
    t_arg2.argc=3;
    struct thread_arg t_arg3 ;
    t_arg3.argc=4;
    struct thread_arg t_arg4 ;
    t_arg4.argc=5;


    pthread_t threads[5];
    pthread_create(&threads[0], NULL, thread_func_multiuseradd, &t_arg0);
    pthread_create(&threads[1], NULL, thread_func_multiuseradd, &t_arg1);
    pthread_create(&threads[2], NULL, thread_func_multiuseradd, &t_arg2);
    pthread_create(&threads[3], NULL, thread_func_multiuseradd, &t_arg3);
    pthread_create(&threads[4], NULL, thread_func_multiuseradd, &t_arg4);

    for (int i = 0; i < 5; ++i) {
        pthread_join(threads[i], NULL);
    }
}

static struct {
	const char *name;
	uthread_func_t func;
} commands[] = {
	{ "info",	thread_fs_info },
	{ "ls",		thread_fs_ls },
	{ "add",	thread_fs_add },
	{ "rm",		thread_fs_rm },
	{ "cat",	thread_fs_cat },
	{ "stat",	thread_fs_stat },
    {"multiuseradd", thread_fs_multiuseradd},
};

void usage(void)
{
	int i;
	fprintf(stderr, "Usage: test-fs <command> [<arg>]\n");
	fprintf(stderr, "Possible commands are:\n");
	for (i = 0; i < ARRAY_SIZE(commands); i++)
		fprintf(stderr, "\t%s\n", commands[i].name);
	exit(1);
}

int main(int argc, char **argv)
{

    int i;
    char *cmd;
    struct thread_arg arg;

    /* Skip argv[0] */
    argc--;
    argv++;

    if (!argc) {
        usage();
    }

    cmd = argv[0];
    arg.argc = --argc;
    arg.argv = &argv[1];

	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (!strcmp(cmd, commands[i].name)) {
            if(i==6){
                thread_fs_multiuseradd(&arg);
            }
            else uthread_start(commands[i].func, &arg);
			break;
		}
	}
	if (i == ARRAY_SIZE(commands)) {
		test_fs_error("invalid command '%s'", cmd);
		usage();
	}

	return 0;
}
