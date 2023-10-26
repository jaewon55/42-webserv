#include <cctype>
#include <cstddef>
#include <exception>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <string>
#include "RequestData.hpp"
#include "webserv.hpp"
#include "Util.hpp"

RequestData::RequestData(void) : state(START_LINE), bodyLength(0) {}

RequestData::RequestData(const RequestData & ref) {
	*this = ref;
}

RequestData::~RequestData(void) {}

RequestData &RequestData::operator=(const RequestData & ref) {
	if ( this != &ref )
	{
		this->state = ref.state;
		this->method = ref.method;
		this->url = ref.url;
		this->query = ref.query;
		this->version = ref.version;
		this->headers = ref.headers;
		this->body = ref.body;
		this->bodyLength = ref.bodyLength;
		this->buff = ref.buff;
		this->newLine = ref.newLine;
	}
	return *this;
}




//----------------getters------------------

int RequestData::getMethod() const { return (this->method); }

std::string RequestData::getMethodString() const {
	std::string res;
	switch (this->method) {
		case GET:
			res = "GET";
			break;
		case POST:
			res = "POST";
			break;
		case DELETE:
			res = "DELETE";
	}
	return (res);
}

const std::string &RequestData::getUrl(void) const { return (this->url); }

const stringMap	&RequestData::getQuery(void) const { return (this->query); }

const std::string	RequestData::getQueryString(void) const { return (this->queryString); }

const std::string &RequestData::getVersion(void) const { return (this->version); }

const std::string &RequestData::getHeaderByKey(std::string key) const {
	stringMap::const_iterator iter(this->headers.find(key));
	if (iter == this->headers.end()) {
		throw std::runtime_error("out of range");
	}
	return (iter->second);
}

const charVector	&RequestData::getBody(void) const { return (this->body); }

long RequestData::getBodyLength(void) const { return (this->bodyLength); }

const charVector	&RequestData::getBuff(void) const { return (this->buff); }

RequestDataState RequestData::getState(void) const { return (this->state); }

//----------------getters------------------




charVectorIter	RequestData::findCRLF(void) {
	for (size_t i = 0; i < this->buff.size(); ++i) {
		if (this->buff[i] == '\r') {
			if (this->buff.size() == (i + 1)) break ; 
			// [CR] is the last element in vector, break.

			if (this->buff[i + 1] == '\n') this->buff.erase(this->buff.begin() + i);
			// [CR][LF] are found, erase the [CR] element from vector.
		}
	}
	charVectorIter	it = std::find(this->buff.begin(), this->buff.end(), '\n');
	return (it);
}

charVectorIter	RequestData::findCRLF(charVectorIter it) {
	std::size_t size = this->buff.end() - it;
	for (size_t i = 0; i < size; ++i) {
		if (*(it + i)== '\r') {
			if (size == (i + 1)) break ; 
			// [CR] is the last element in vector, break.

			if (*(it + i + 1) == '\n') this->buff.erase(it + i);
			// [CR][LF] are found, erase the [CR] element from vector.
		}
	}
	charVectorIter	ret = std::find(it, this->buff.end(), '\n');
	return (ret);
}

void	RequestData::joinBuff(char *data, long len) {
	for (long i = 0; i < len; ++i) {
		this->buff.push_back(data[i]);
	}
	switch (this->state) {
		case START_LINE:
			this->newLine = findCRLF();
			if (this->newLine != this->buff.end()) parseStartLine(this->newLine);
		case HEADER:
			this->newLine = findCRLF();
			while (this->newLine != this->buff.end()) {
				parseHeader(this->newLine);
				if (this->state == BODY) break ;
				this->newLine = findCRLF();
			}
		case BODY:
			parseBody();
		case COMPLETE:
			break;
	}
}

static RequestDataMethod checkHTTPMethod(const std::string method) {
	if (method.empty()) throw std::runtime_error(STATUS_BAD_REQUEST);

	std::string methodArr[METHOD_NUM] = {"GET", "POST", "DELETE"};
	int i(0);

	while (i < METHOD_NUM) {
		if (methodArr[i] == method) break;
		++i;
	}

	if (i == METHOD_NUM) throw std::runtime_error(STATUS_INVALID_METHOD);
	return (static_cast<RequestDataMethod>(i));
}

static const std::string	checkHTTPUrl(const std::string str) {
	if (str.size() == 0) throw std::runtime_error(STATUS_BAD_REQUEST);

	if (str[0] != '/') throw std::runtime_error(STATUS_BAD_REQUEST);

	return (str);
}

static const std::string	checkHTTPVersion(const std::string str) {
	std::size_t	slash = str.find('/');
	if (slash == std::string::npos) throw std::runtime_error(STATUS_BAD_REQUEST);

	std::string	protocol = str.substr(0, slash);
	if (protocol != PROTOCOL) throw std::runtime_error(STATUS_BAD_REQUEST);

	std::string	version = str.substr(slash + 1);
	std::size_t	dot = version.find('.');
	if (dot == std::string::npos) throw std::runtime_error(STATUS_BAD_REQUEST);

	std::string	ver1 = version.substr(0, dot);
	for (size_t i = 0; i < ver1.size(); ++i) {
		if (isdigit(ver1[i]) == 0) throw std::runtime_error(STATUS_BAD_REQUEST);
	}

	std::string ver2 = version.substr(dot + 1);
	for (size_t i = 0; i < ver2.size(); ++i) {
		if (isdigit(ver2[i]) == 0) throw std::runtime_error(STATUS_BAD_REQUEST);
	}
	
	std::stringstream	sstream;
	sstream << VERSION;
	if (version != sstream.str()) throw std::runtime_error(STATUS_INVALID_VERSION);
	
	return (str);
}

void	RequestData::parseStartLine(charVectorIter newLine) {
	Util::stringVector	line = Util::split(std::string(this->buff.begin(), newLine));

	if (line.size() != 3) throw std::runtime_error(STATUS_BAD_REQUEST);

	this->method = checkHTTPMethod(line[0]);

	this->url = checkHTTPUrl(line[1]);

	this->version = checkHTTPVersion(line[2]);

	this->buff.erase(this->buff.begin(), newLine + 1);
	parseUrlQuery();
	this->state = HEADER;
}

void	RequestData::parseUrlQuery(void) {
	std::size_t start = this->url.find('?');
	if (start == std::string::npos) return ;

	std::string	urlWithoutQuery = this->url.substr(0, start);

	std::string	qry = this->url.substr(start + 1);
	this->queryString = qry;
	qry += "&";

	this->url = urlWithoutQuery;

	std::size_t	pos = qry.find('=');
	if (pos == std::string::npos) throw std::runtime_error(STATUS_BAD_REQUEST);
	std::size_t	end = qry.find('&');
	if (pos > end) throw std::runtime_error(STATUS_BAD_REQUEST);

	std::string	key = qry.substr(0, pos);

	this->query[key] = qry.substr(pos + 1, end);
	
	while (qry.size() != 0) {
		std::size_t	pos = qry.find('=');
		if (pos == std::string::npos) throw std::runtime_error(STATUS_BAD_REQUEST);
		std::size_t	end = qry.find('&');
		if (pos >= end) throw std::runtime_error(STATUS_BAD_REQUEST);

		std::string	key = qry.substr(0, pos);

		this->query[key] = qry.substr(pos + 1, end - (pos + 1));

		qry = qry.substr(end + 1);
	}
}

void	RequestData::parseHeader(charVectorIter newLine) {
	std::string	line(buff.begin(), newLine);
	
	if (line == "") { // empty line with CRLF -> header part end
		this->state = BODY;
		this->buff.erase(this->buff.begin(), newLine + 1);
		return ;
	}
	charVectorIter	it = std::find(this->buff.begin(), newLine, ':');
	if (it == this->buff.end())
		throw std::runtime_error(STATUS_BAD_REQUEST);

	std::string	key(this->buff.begin(), it);
	if (key == "") throw std::runtime_error(STATUS_BAD_REQUEST);

	std::string	value(it + 1, newLine);
	std::size_t	i;
	for (i = 0; i < value.size(); ++i) {
		if (value[i] != ' ') break ;
	}
	if (i < value.size()) value = value.substr(i);

	this->headers[key] = value;

	this->buff.erase(this->buff.begin(), newLine + 1);
}

void	RequestData::parseBody(void) {
	if (headers.find("Transfer-Encoding") == headers.end()) {
		stringMapIter	contentLength = headers.find("Content-Length");
		if (contentLength == headers.end()) {
			this->state = COMPLETE;
			return ;
		}
		else { // Content-Length header exist
			try {
				this->bodyLength = Util::stoi(contentLength->second);
			} catch (std::exception & e) {
				this->bodyLength = 0;
				this->state = COMPLETE;
				return ;
			}
			if (this->buff.size() >= static_cast<size_t>(this->bodyLength)) {
				this->buff.resize(this->bodyLength);
				this->body = this->buff;
				this->buff.clear();
				charVector().swap(this->buff); // deallocate the this->buff memory
				this->state = COMPLETE;
				return ;
			}
		}
	}
	else { // Chunked body data parsing
		if (headers["Transfer-Encoding"] == "chunked") {
			charVectorIter	firstIter = findCRLF();
			while (firstIter != this->buff.end()) {
				std::string hex(this->buff.begin(), firstIter);
				long chunkLength = Util::hexstol(hex);

				charVectorIter	secondIter = findCRLF(firstIter + 1);
				if (secondIter == this->buff.end()) break ;

				if (chunkLength != (secondIter - (firstIter + 1)))
					throw std::runtime_error(STATUS_BAD_REQUEST);
				
				if (chunkLength == 0) {
					this->buff.clear();
					charVector().swap(this->buff);
					this->bodyLength = this->body.size();
					this->state = COMPLETE;
					return ;
				}
				this->body.insert(this->body.end(), (firstIter + 1), secondIter);
				this->buff.erase(this->buff.begin(), secondIter + 1);

				firstIter = findCRLF();
				if (firstIter == this->buff.end()) break ;
			}
		}
		else throw std::runtime_error(STATUS_BAD_REQUEST);
	}
}

std::ostream &operator<<( std::ostream & o, RequestData const &m )
{
	std::string meth;
	switch (m.method) {
		case GET:
			meth = "GET";
			break;
		case POST:
			meth = "POST";
			break;
		case DELETE:
			meth = "DELETE";
	}
	std::map<std::string, std::string>::const_iterator iter = m.headers.begin();

	o << "state : " << m.state << "\n";
	o << meth << " " << m.url << " " << m.version << "\n";
	for (; iter != m.headers.end(); ++iter) {
		o << iter->first << ": " << iter->second << "\n";
	}
	o << "\n";
	std::string b(m.body.begin(), m.body.end());
	o << b << std::endl;
	return o;
}
