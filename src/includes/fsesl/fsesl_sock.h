#ifndef _FSESL_SOCK_H
#define _FSESL_SOCK_H

#include <kore/kore.h>
#include <kore/tasks.h>
#include <kore/hooks.h>

int 	handle_task(struct kore_task *task);
void 	setup_kore_task(void);
void	teardown_kore_task(void);

#endif
