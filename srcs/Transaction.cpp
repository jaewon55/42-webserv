
#include "Transaction.hpp"
#include "ConfigData.hpp"
#include "EventManager.hpp"
#include "RequestData.hpp"
#include "ServerBlock.hpp"
#include "webserv.hpp"
#include "Util.hpp"
#include <csignal>
#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <sys/_types/_intptr_t.h>
#include <sys/_types/_pid_t.h>
#include <sys/_types/_ssize_t.h>
#include <sys/event.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

static std::string	errorPage(const char * s);

Transaction::Transaction( void ) : state(CREATE_REQUEST), readFD(-1), writeFD(-1), childPid(0), sendSize(0), errorFlag(false) {}

Transaction::Transaction( const Transaction & src ) {
	*this = src;
}

Transaction::Transaction( int portNum, int fd ) :
	state(CREATE_REQUEST),
	port(portNum),
	acceptedFD(fd),
	readFD(-1),
	writeFD(-1),
	childPid(0),
	sendSize(0),
	errorFlag(false)  {}

Transaction::~Transaction( void ) {
	if (readFD != -1) {
		close(readFD);
		EventManager::getInstance()->delReadEvent(this->readFD);
	}
	if (writeFD != -1) {
		close(writeFD);
		EventManager::getInstance()->delReadEvent(this->writeFD);
	}
	if (childPid != 0) {
		kill(childPid, SIGTERM);
		EventManager::getInstance()->delProcEvent(childPid);
	}
}

Transaction &Transaction::operator=( const Transaction & rhs ) {
	if (this != &rhs) {
		this->state = rhs.state;
		this->acceptedFD = rhs.acceptedFD;
		this->request = rhs.request;
		this->response = rhs.response;
		this->port = rhs.port;
		this->readFD = rhs.readFD;
		this->writeFD = rhs.writeFD;
		this->childPid = rhs.childPid;
		this->message = rhs.message;
		this->sendSize = rhs.sendSize;
		this->errorFlag = rhs.errorFlag;
	}
	return (*this);
}

void	Transaction::errorHandler(const char *code) {
	this->response = ResponseData();
	this->errorFlag = true;
	this->response.setStatus(code);

	std::string	page = errorPage(code);
	this->readFD = open(page.c_str(), O_RDONLY);
	if (this->readFD == -1) {
		ConfigData	&conf = *ConfigData::getInstance();

		std::string defaultPage = conf.getDefaultErrorPage();
		this->response.setStatus(STATUS_SERVER_ERROR);
		this->response.setBody(charVector(defaultPage.begin(), defaultPage.end()));
		this->response.addHeader("Content-Type", "text/html");

		this->state = RESPONSE_COMPLETE;
		EventManager::getInstance()->onWriteEvent(this->acceptedFD, this);
		return;
	}
	EventManager::getInstance()->addReadEvent(this->readFD, this);
	this->state = READ_ERROR_FILE;
}

void	Transaction::parseRequestMessage(struct kevent & kev) {
	charVector	buf(kev.data);
	if (recv(kev.ident, &(buf[0]), kev.data, MSG_DONTWAIT) == -1) {
		throw std::runtime_error(STATUS_SERVER_ERROR);
	}
	try {
		this->request.joinBuff(&(buf[0]), kev.data);
	} catch (std::exception & e) {
		throw std::runtime_error(STATUS_BAD_REQUEST);
	}
	if (this->request.getState() == COMPLETE) {
		ConfigData	&conf = *ConfigData::getInstance();
		
		// limit client body size
		if (this->request.getBodyLength() > conf.getBodyLimit(this->port)) {
			throw std::runtime_error(STATUS_TOO_LARGE_FILE);
		}

		location	loc = conf.getLocation(this->port, this->request.getUrl());

		if (loc.index.empty() == false) {
			if (this->request.getMethod() == GET) {
				std::string	path;
				if (Util::compareUrl(this->request.getUrl(), loc.url) == true) {
					// /home
					for (std::vector<std::string>::iterator iter = loc.index.begin(); iter != loc.index.end(); ++iter) {
						if (access(Util::joinPath(loc.root, *iter).c_str(), F_OK) == 0) {
							path = Util::joinPath(loc.root, *iter);
							break;
						}
					}
				} else {
					path = loc.getPathWithUrl(this->request.getUrl());
					// /home/index.html
				}
				if (path.empty() == true) throw std::runtime_error(STATUS_NOT_FOUND);
				this->readFD = open(path.c_str(), O_RDONLY);
				if (this->readFD == -1) throw std::runtime_error(STATUS_SERVER_ERROR);
				// path의 확장자를 보고 mime-type 설정
				this->response.addHeader("Content-Type",
					conf.getMimeTypeByExtension(Util::extensionFinder(path)));
				this->state = READ_FILE;
				EventManager::getInstance()->addReadEvent(this->readFD, this);
			} else throw std::runtime_error(STATUS_INVALID_METHOD);
		} else if (loc.redirPath.empty() == false) {
			if (this->request.getMethod() == GET) {
				this->response.setStatus(STATUS_REDIR);
				this->response.addHeader("Location", loc.redirPath);
				this->response.addHeader("Content-Length", "0");
				this->state = RESPONSE_COMPLETE;
				EventManager::getInstance()->onWriteEvent(this->acceptedFD, this);
			} else throw std::runtime_error(STATUS_INVALID_METHOD);
		} else {
			
			// cgi
			typedef std::vector<RequestDataMethod>::iterator	iterator;
			iterator it = loc.methods.begin();
			for (; it != loc.methods.end(); ++it) {
				if (*it == this->request.getMethod()) break;
			}
			if (it == loc.methods.end()) throw std::runtime_error(STATUS_INVALID_METHOD);
		
			// pipe 2개 열기
			int fdsRequest[2];
			int fdsResponse[2];

			if (pipe(fdsRequest) == -1)
				throw std::runtime_error(STATUS_SERVER_ERROR);
			if (pipe(fdsResponse) == -1) {
				close(fdsRequest[0]);
				close(fdsRequest[1]);
				throw std::runtime_error(STATUS_SERVER_ERROR);
			}
		
			pid_t pid;
			pid = fork();
			if (pid < 0) {
				close(fdsResponse[0]);
				close(fdsResponse[1]);
				close(fdsRequest[0]);
				close(fdsRequest[1]);
				throw std::runtime_error(STATUS_SERVER_ERROR);

			} else if (pid == 0) {
				
				close(fdsRequest[1]);
				close(fdsResponse[0]);
				dup2(fdsResponse[1], STDOUT_FILENO);
				dup2(fdsRequest[0], STDIN_FILENO);

				ConfigData	&conf = *ConfigData::getInstance();
				location loc;

				try {
					loc = conf.getLocation(this->port, this->request.getUrl());
				} catch (std::exception & e) {
					exit(1);
				}

				std::vector<char*> argv;
				argv.push_back(const_cast<char*>(Util::joinPath(loc.root, loc.cgiPath).c_str()));
				argv.push_back(NULL);

				std::vector<char*> envp;

				std::string	requestMethod = "REQUEST_METHOD=" + this->request.getMethodString();
				std::string	queryString = "QUERY_STRING=" + this->request.getQueryString();
				std::string	pathInfo = "PATH_INFO=" + this->request.getUrl();
				std::string	contentType;
				std::string	cookie;
				try {
					contentType = "CONTENT_TYPE=" + this->request.getHeaderByKey("Content-Type");
				} catch (std::exception & e) {
					contentType = "CONTENT_TYPE=";
				}
				try {
					cookie = "HTTP_COOKIE=" + this->request.getHeaderByKey("Cookie");
				} catch (std::exception & e) {
					cookie = "HTTP_COOKIE=";
				}
				char* requestMethodBuf = new char[requestMethod.size() + 1];
				std::strcpy(requestMethodBuf, requestMethod.c_str());

				char* queryStringBuf = new char[queryString.size() + 1];
				std::strcpy(queryStringBuf, queryString.c_str());

				char* pathInfoBuf = new char[pathInfo.size() + 1];
				std::strcpy(pathInfoBuf, pathInfo.c_str());

				char* contentTypeBuf = new char[contentType.size() + 1];
				std::strcpy(contentTypeBuf, contentType.c_str());

				char* cookieBuf = new char[cookie.size() + 1];
				std::strcpy(cookieBuf, cookie.c_str());

				envp.push_back(requestMethodBuf);
				envp.push_back(queryStringBuf);
				envp.push_back(pathInfoBuf);
				envp.push_back(contentTypeBuf);
				envp.push_back(cookieBuf);
				envp.push_back(NULL); // envp 배열의 끝을 나타내는 NULL 추가
				
				execve(Util::joinPath(loc.root, loc.cgiPath).c_str(), &(argv[0]), &(envp[0]));
				exit(1);

			} else {

				close(fdsRequest[0]);
				close(fdsResponse[1]);
				this->readFD = fdsResponse[0];
				this->writeFD = fdsRequest[1];

				

				EventManager::getInstance()->oneShotWriteEvent(this->writeFD, this);
				this->childPid = pid;
				this->state = WRITE_PIPE;

			}
		}
	}
}

void	Transaction::readFileProcess(struct kevent & kev) {
	charVector buff(kev.data);
	ConfigData	&conf = *ConfigData::getInstance();

	int	readLength = read(kev.ident, &buff[0], kev.data);
	if (readLength == -1)
		throw std::runtime_error(STATUS_SERVER_ERROR);
	
	location loc = conf.getLocation(this->port, this->request.getUrl());
	
	if (loc.directoryPage == "" || !Util::compareUrl(this->request.getUrl(), loc.url)) {
		this->response.setBody(buff);
	} else {
		// dir list
		this->response.addBody(loc.directoryPage.c_str(), loc.directoryPage.size());
		this->response.addHeader("Content-Type", "text/html");
	}

	this->response.setStatus(STATUS_OK);
	this->response.addHeader("Connection", "keep-alive" );
	this->response.addHeader("accepted-fd", Util::itos(static_cast<long>(this->acceptedFD)));
	// 추가로 더 넣어야할 헤더에 대해서는 필요하면 추가하기
	this->state = RESPONSE_COMPLETE;
	EventManager::getInstance()->delReadEvent(this->readFD, this);
	EventManager::getInstance()->onWriteEvent(this->acceptedFD, this);
}

void	Transaction::readPipeProcess(struct kevent & kev) {

	charVector buff(kev.data);
	int	readLength = read(kev.ident, &buff[0], kev.data);

	if (readLength == -1)
		throw std::runtime_error(STATUS_SERVER_ERROR);

	this->response.setCGIBuff(buff);
	if (kev.flags & EV_EOF) {
		this->response.setState(RESPONSE_STATE_CGI);
		this->state = RESPONSE_COMPLETE;
		EventManager::getInstance()->addProcEvent(this->childPid, this);
	}
}

void	Transaction::readErrorFileProcess(struct kevent & kev) {
	charVector data(kev.data);
	
	int	readLength = read(kev.ident, &data[0], kev.data);
	if (readLength == -1) {
		ConfigData	&conf = *ConfigData::getInstance();

		std::string defaultPage = conf.getDefaultErrorPage();
		this->response.setStatus(STATUS_SERVER_ERROR);
		this->response.setBody(charVector(defaultPage.begin(), defaultPage.end()));
	} else {
		this->response.setBody(data);
	}
	this->response.addHeader("Content-Type", "text/html");
	// 추가로 더 넣어야할 헤더에 대해서는 필요하면 추가하기
	this->state = RESPONSE_COMPLETE;
	EventManager::getInstance()->delReadEvent(this->readFD, this);
	EventManager::getInstance()->onWriteEvent(this->acceptedFD, this);
}

TransactionState Transaction::getState(void) const { return (this->state); }

void Transaction::controlWriteEvent( struct kevent & ) {
	try {
		switch (this->state) {
		case WRITE_PIPE:
			if (write(this->writeFD, &(this->request.getBody()[0]), this->request.getBodyLength()) < 0) {
				throw std::runtime_error(STATUS_SERVER_ERROR);
			}
			close(this->writeFD);
			this->writeFD = -1;
			EventManager::getInstance()->addReadEvent(this->readFD, this);
			this->state = READ_PIPE;
			break;
		case RESPONSE_COMPLETE:
			// socket에 쓰기
			sendResponseMessage();
			break;
		default:
			break;
		}
	} catch(std::exception &e) {
		if (this->errorFlag)
			throw std::runtime_error("remove client");
		errorHandler(e.what());
	}
}

void Transaction::controlReadEvent( struct kevent & kev ) {
	try {
		switch (this->state) {
		case CREATE_REQUEST:
			parseRequestMessage(kev);
			break;
		case READ_FILE:
			// file읽기
			readFileProcess(kev);
			break;
		case READ_PIPE:
			readPipeProcess(kev);
			break;
		case READ_ERROR_FILE:
			// error_file 읽기
			readErrorFileProcess(kev);
			break;
		default:
			break;
		}
	}
	catch(const std::exception& e) {
		// error 핸들링
		errorHandler(e.what());
	}
	
}

void	Transaction::controlProcessExitEvent( void ) {
	waitpid(this->childPid, 0, 0);
	EventManager::getInstance()->delReadEvent(this->readFD, this);
	EventManager::getInstance()->onWriteEvent(this->acceptedFD, this);
}

void	Transaction::sendResponseMessage(void) {
	try {
		this->message = this->response.createResponseMessage();
	} catch (std::exception & e) {
		errorHandler(e.what());
		return;
	}

	ssize_t sendResult = send(this->acceptedFD, &(this->message[this->sendSize]), this->message.size() - this->sendSize, MSG_DONTWAIT);
	if (sendResult > 0) {
		this->sendSize += sendResult;
	} else if (sendResult < 0) {
		this->errorFlag = true;
	}

	if (this->errorFlag) {
		throw std::runtime_error("remove client");
	}

	if (this->sendSize == static_cast<ssize_t>(this->message.size())) {
		if (this->readFD != -1) {
			close(this->readFD);
			this->readFD = -1;
		}
		if (this->writeFD != -1) {
			close(this->writeFD);
			this->writeFD = -1;
		}
		*this = Transaction(this->port, this->acceptedFD);
		EventManager::getInstance()->offWriteEvent(this->acceptedFD, this);
	}
}


static std::string	errorPage(const char * s) {
	int code = Util::stoi(std::string(s, 3));

	switch (code) {
		case 400:
			return (ERROR_PAGE_400);
		case 404:
			return (ERROR_PAGE_404);
		case 405:
			return (ERROR_PAGE_405);
		case 413:
			return (ERROR_PAGE_413);
		case 500:
			return (ERROR_PAGE_500);
		case 501:
			return (ERROR_PAGE_501);
		case 505:
			return (ERROR_PAGE_505);
		default:
			return (ERROR_PAGE_418);
	}
}
