#include <fsesl/fsesl_sock.h>
#include <stdbool.h>

#include <poll.h>

static int handle_success_event(int sockfd);
static void handle_event(const char *buf, size_t len);


static void handle_task_channel_data_available(struct kore_task *task);
static int tcp_connect(const char *ip, int port);

static struct kore_task fsesl_task;

void
setup_kore_task(void)
{
	if (worker->id != 2)
		return;

	kore_task_create(&fsesl_task, handle_task);
	kore_task_bind_callback(&fsesl_task, handle_task_channel_data_available);

	kore_task_run(&fsesl_task);
}

void
teardown_kore_task(void)
{
	kore_task_destroy(&fsesl_task);
}

int
handle_task(struct kore_task *task)
{
	int port = 3939;
	int sockfd, ret;
	const char *host = "127.0.0.1";
	struct pollfd pollfds[1];
	bool running;


reconnect:
	sockfd = tcp_connect(host, port);
	if (sockfd == -1) {
		kore_log(LOG_ERR, "%s: tcp_connect failed", __func__);
		return -1;
	}

	pollfds[0].fd = sockfd;
	pollfds[0].events = POLLIN;

	running = true;
	while (running) {
		int nready = poll(pollfds, 1, -1);
		if (nready == -1) {
			kore_log(LOG_ERR, "%s: poll error: %s", __func__, errno_s);
			perror("Poll error");
			continue;
		}

		ret = handle_success_event(pollfds[0].fd);
		if (ret == -1) {
			kore_log(LOG_ERR, "%s: handle_success_event error: disconnecting..", __func__);
			running = false;
		}
	}

	close(pollfds[0].fd);
	kore_log(LOG_INFO, "%s: disconnected.", __func__);
	// @TODO Wait before reconnecting
	goto reconnect;

	return KORE_RESULT_OK;
}

int
handle_success_event(int sockfd)
{
	size_t buflen = BUFSIZ * 4;
	char *buf;
	ssize_t nbytes;

	buf = kore_malloc(buflen);
	nbytes = read(sockfd, buf, buflen);
	if (nbytes == -1) {
		kore_log(LOG_ERR, "%s: read error: %s", __func__, errno_s);
		kore_free(buf);
		return -1;
	} else if (nbytes == 0) {
		kore_log(LOG_INFO, "%s: Server died.", __func__);
		kore_free(buf);
		return -1;
	}

	fprintf(stderr, "\n\n==============[ Event Received ]==============\n\n");
	kore_task_channel_write(&fsesl_task, buf, nbytes);
	kore_free(buf);

	return 0;
}

static
void handle_task_channel_data_available(struct kore_task *task)
{
	char *buf = NULL;
	size_t buflen = BUFSIZ * 4, len;

	kore_log(LOG_DEBUG, "Task channel data available");

	if (kore_task_finished(&fsesl_task)) {
		kore_log(LOG_INFO, "fsels task finished.");
		goto cleanup;
	}

	buf = kore_malloc(buflen);
	if (buf == NULL) {
		kore_log(LOG_ERR, "kore_malloc error: failed to malloc: %s", errno_s);
		goto cleanup;
	}

	len = kore_task_channel_read(&fsesl_task, buf, buflen);
	if (len > buflen) {
		kore_log(LOG_WARNING, "Read truncation happened. Cannot parse JSON data");
		goto cleanup;
	}

	kore_log(LOG_DEBUG, "handle_task_channel_data_available: going to call handle_event()");
	handle_event(buf, len);

cleanup:
	if (buf) kore_free(buf);
}

static void
handle_event(const char *data, size_t len)
{
	kore_log(LOG_DEBUG, "%s: event handled: data received:", __func__);
	kore_log(LOG_DEBUG, "%.*s", (int) len, data);
}


static int
tcp_connect(const char *ip, int port)
{
	int ret;
	int sockfd;
	struct sockaddr_in addr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("socket error");
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	ret = inet_pton(AF_INET, ip, &addr.sin_addr.s_addr);
	if (ret <= 0) {
		fprintf(stderr, "Invalid ip [%s]\n", ip);
		return -1;
	}

	ret = connect(sockfd, (struct sockaddr *) &addr, sizeof(addr));
	if (ret == -1) {
		perror("connect error");
		return -1;
	}

	fprintf(stderr, "Connected successfully!\n");

	return sockfd;
}
