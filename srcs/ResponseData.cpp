#include "ResponseData.hpp"
#include "Util.hpp"
#include "webserv.hpp"
#include <stdexcept>

ResponseData::ResponseData(void)
: state(RESPONSE_STATE_STATIC), version("HTTP/1.1"), status(STATUS_TEAPOT) {}

ResponseData::ResponseData(const ResponseData & ref) {
	*this = ref;
}

ResponseData::~ResponseData(void) {}

ResponseData &ResponseData::operator=(const ResponseData & ref) {
	if ( this != &ref ) {
		this->state = ref.state;
		this->version = ref.version;
		this->status = ref.status;
		this->header = ref.header;
		this->body = ref.body;
		this->CGIBuff = ref.CGIBuff;
	}
	return *this;
}

//getter;
ResponseDataState	ResponseData::getState( void ) const { return (this->state); }

//setters;
void	ResponseData::setState( const ResponseDataState s ) {
	this->state = s;
}

void	ResponseData::setStatus( const char * code ) {
	this->status = code;
}

void	ResponseData::addHeader( const std::string & key, const std::string & value ) {
	this->header[key] = value;
}

void	ResponseData::setBody( const charVector & data ) {
	this->body = data;
	addHeader("Content-Length", Util::itos(this->body.size()));
}

void	ResponseData::addBody( const char *data, std::size_t size ) {
	for (std::size_t i = 0; i < size; ++i) {
		this->body.push_back(data[i]);
	}
	addHeader("Content-Length", Util::itos(this->body.size()));
}

void	ResponseData::setCGIBuff( const charVector & buff ) {
	this->CGIBuff.insert(this->CGIBuff.end(), buff.begin(), buff.end());
}

charVector ResponseData::createResponseMessage( void ) const {
	charVector	message;

	if (this->state == RESPONSE_STATE_CGI) {
		typedef charVector::const_iterator	iterator;
		iterator startIter = this->CGIBuff.begin();
		while (startIter != this->CGIBuff.end()) {
			std::string cmpString("Status:");

			if (this->CGIBuff.end() - startIter < 11) {
				startIter = this->CGIBuff.end();
				break;
			}
			if (*startIter == 'S' && cmpString == std::string(startIter, startIter + 7) ) {
				break;
			}
			while (startIter != this->CGIBuff.end()) {
				if (*startIter == '\n') {
					++startIter;
					break;
				}
				++startIter;
			}
		}
		if (startIter == this->CGIBuff.end()) {
			// Status가 없음
			throw std::runtime_error(STATUS_SERVER_ERROR);
		} 
		iterator endIter = startIter;
		for (; endIter != this->CGIBuff.end(); ++endIter) {
			if (*endIter == '\r' || *endIter == '\n') break;
		}
		std::vector<std::string> responseStatus = Util::split(std::string(startIter, endIter), ':');
		responseStatus = Util::split(responseStatus[1]);

		int code;
		try {
			code = Util::stoi(responseStatus[0]);
		} catch (std::exception & e) {
			throw std::runtime_error(STATUS_SERVER_ERROR);
		}
		if (!(200 <= code && code <= 399)) {
			std::string errorStatus;

			for (std::size_t i = 0; i < responseStatus.size(); ++i) {
				errorStatus += responseStatus[i];
			}
			throw std::runtime_error(errorStatus);
		}
		// version
		message.insert(message.end(), this->version.begin(), this->version.end());
		message.push_back(' ');

		for (std::size_t i = 0; i < responseStatus.size(); ++i) {
			message.insert(message.end(), responseStatus[i].begin(), responseStatus[i].end());
			message.push_back(' ');
		}
		message.push_back('\r');
		message.push_back('\n');

		message.insert(message.end(), this->CGIBuff.begin(), this->CGIBuff.end());
	} else {
		//version
		message.insert(message.end(), this->version.begin(), this->version.end());
		message.push_back(' ');

		//status code
		message.insert(message.end(), this->status.begin(), this->status.end());
		message.push_back('\r');
		message.push_back('\n');

		//header
		stringMap::const_iterator iter = header.begin();
		for (; iter != header.end(); ++iter) {
			message.insert(message.end(), iter->first.begin(), iter->first.end());
			message.push_back(':');
			message.push_back(' ');
			message.insert(message.end(), iter->second.begin(), iter->second.end());
			message.push_back('\r');
			message.push_back('\n');
		}
		message.push_back('\r');
		message.push_back('\n');

		//body
		message.insert(message.end(), this->body.begin(), this->body.end());
	}

	return (message);
}
