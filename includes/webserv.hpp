#ifndef WEBSERV_HPP
# define WEBSERV_HPP

//Define http version that our server support -> HTTP/1.1
# define PROTOCOL "HTTP"
# define VERSION 1.1

//Define methods that our server can handle
# define METHOD_NUM 3

//Define key that use in config parsing
# define KEY_KEEPALIVE_TIMEOUT "keepalive_timeout"
# define KEY_DEFAULT_ERROR_PAGE "default_error_page"
# define KEY_MIME_TYPE "mime_type"
# define KEY_SERVER "server"
# define BLOCK_LOCATION "location"
# define KEY_LISTEN "listen"
# define KEY_HOST "host"
# define KEY_ROOT "root"
# define KEY_STORAGE "storage"
# define KEY_BODY_LIMIT "body_limit"
# define KEY_DIRECTORY_LIST "directory_list"
# define KEY_METHODS "methods"
# define KEY_INDEX "index"
# define KEY_REDIR "redir"
# define KEY_CGI_PATH "cgi_path"

//Define status code and status phrase
# define STATUS_OK "200 OK"
# define STATUS_REDIR "302 Found"
# define STATUS_BAD_REQUEST "400 Bad Request"
# define STATUS_NOT_FOUND "404 Not Found"
# define STATUS_INVALID_METHOD "405 Method Not Allowed"
# define STATUS_TOO_LARGE_FILE "413 Content Too Large"
# define STATUS_SERVER_ERROR "500 Internal Server Error"
# define STATUS_NOT_IMPLEMENTED "501 Not Implemented"
# define STATUS_INVALID_VERSION "505 HTTP Version Not Supported"
# define STATUS_TEAPOT "418 I'm a teapot"

# define ERROR_PAGE_400 "./server/error_pages/400.html"
# define ERROR_PAGE_404 "./server/error_pages/404.html"
# define ERROR_PAGE_405 "./server/error_pages/405.html"
# define ERROR_PAGE_413 "./server/error_pages/413.html"
# define ERROR_PAGE_500 "./server/error_pages/500.html"
# define ERROR_PAGE_501 "./server/error_pages/501.html"
# define ERROR_PAGE_505 "./server/error_pages/505.html"
# define ERROR_PAGE_418 "./server/error_pages/418.html"

# define DEFAULT_MIME_TYPE "text/plane"

# define DEFAULT_CONF_FILEPATH "./default.conf" // default configuration file path


class Transaction;

enum RequestDataMethod {
	GET,
	POST,
	DELETE
};

enum RequestDataState {
	START_LINE,
	HEADER,
	BODY,
	COMPLETE
};

enum ResponseDataState {
	RESPONSE_STATE_STATIC,
	RESPONSE_STATE_CGI
};

enum TransactionState {
	CREATE_REQUEST,
	READ_FILE,
	READ_ERROR_FILE,
	READ_PIPE,
	WRITE_PIPE,
	RESPONSE_COMPLETE
};

#endif
