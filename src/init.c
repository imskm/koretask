#include <kore/kore.h>
#include <kore/http.h>
#include <kore/hooks.h>
#include <kore/seccomp.h>

#include <fsesl/fsesl.h>
#include <fsesl/fsesl_sock.h>

KORE_SECCOMP_FILTER("app",
    KORE_SYSCALL_ALLOW(getdents64),
    KORE_SYSCALL_ALLOW(socketpair),
    KORE_SYSCALL_ALLOW(gettimeofday),
    KORE_SYSCALL_ALLOW(newfstatat),
    KORE_SYSCALL_ALLOW(pipe2),
    KORE_SYSCALL_ALLOW(shutdown),
);

int
init_app(int state)
{
	return (KORE_RESULT_OK);
}







void
kore_worker_configure(void)
{
	setup_kore_task();
}

void
kore_worker_teardown(void)
{
	teardown_kore_task();
}

