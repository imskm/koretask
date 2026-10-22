#include <kore/kore.h>
#include <kore/http.h>

int		page(struct http_request *);

int
page(struct http_request *req)
{
	http_response(req, 200, "OK", 2);

	return (KORE_RESULT_OK);
}

