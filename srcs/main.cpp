// socket
#include <sys/_types/_socklen_t.h>
#include <sys/socket.h>

// kqueue, kevent
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>

#include <vector>
#include <list>
#include <iostream>
#include <map>
#include "RequestData.hpp"
#include "Util.hpp"
#include "webserv.hpp"
#include "Transaction.hpp"
#include "EventManager.hpp"
#include "ConfigData.hpp"
#include "signal.h"

int main(int argc, char **argv) {
	int socketFD;
	signal(SIGPIPE, SIG_IGN);
	if (argc > 2) {
		std::cerr << "Error: Too many arguments" << std::endl;
		return (1);
	}
	ConfigData &config(*ConfigData::getInstance());
	try {
		if (argc == 1) {
			config.parseConfig(DEFAULT_CONF_FILEPATH);
		} else {
			config.parseConfig(argv[1]);
		}
		config.makeDirectoryPage();
	} catch (std::exception &e) {
		std::cerr << "Error: invalid configuration file: " << e.what() << std::endl;
		return (1);
	}

	socklen_t			size = sizeof(struct sockaddr_in);
	std::vector<int>	ports = config.getPorts();
	typedef std::vector<int>::iterator	intVecIter;

	std::map<int, struct sockaddr_in>	socketInfo;

	for (intVecIter it = ports.begin(); it != ports.end(); ++it) {
		// TCP/IP 상에서의 통신이기 때문에 IPv4용 구조체인 sockaddr_in을 사용
		struct sockaddr_in	host_addr;

		// IPv4, TCP, 프로토콜(자동)로 소켓 설정
		socketFD = socket(PF_INET, SOCK_STREAM, 0);

		int optionValue = 1;
		setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &optionValue, sizeof(optionValue));

		host_addr.sin_family = AF_INET;
		host_addr.sin_port = htons(*it);
		host_addr.sin_addr.s_addr = config.getHostData(*it);
		memset(&(host_addr.sin_zero), 0, 8);

		// 소켓과 프로세스를 바인딩
		// sockaddr 형변환 시 reinterpret_cast 사용이 가능할 정도로 두 구조체는 비슷한 형식을 가지고 있다. 구조체의 크기가 같다.
		int	check = bind(socketFD, (struct sockaddr *)&host_addr, sizeof(struct sockaddr));
		const unsigned char *IP = config.getHost(*it);
		if (check == -1) {
			std::cerr << "bind error!";
			std::cerr << static_cast<int>(IP[0]) << ".";
			std::cerr << static_cast<int>(IP[1]) << ".";
			std::cerr << static_cast<int>(IP[2]) << ".";
			std::cerr << static_cast<int>(IP[3]) << ":";
			std::cerr << ntohs(host_addr.sin_port) << std::endl;
			return (1);
		}
		std::cout << "connected! " << "http://";
		std::cout << static_cast<int>(IP[0]) << ".";
		std::cout << static_cast<int>(IP[1]) << ".";
		std::cout << static_cast<int>(IP[2]) << ".";
		std::cout << static_cast<int>(IP[3]) << ":";
		std::cout << ntohs(host_addr.sin_port) << std::endl;
		
		listen(socketFD, 300);
		
		// 파일의 속성을 non-blocking으로 변경
		fcntl(socketFD, F_SETFL, O_NONBLOCK);

		socketInfo[socketFD] = host_addr;
	}


	int event_count;
	std::list<Transaction>	clients;

	EventManager & eventManager = *(EventManager::getInstance());

	typedef std::map<int, struct sockaddr_in>::iterator	socketMapIter;
	for (socketMapIter it = socketInfo.begin(); it != socketInfo.end(); ++it) {
		eventManager.addReadEvent(it->first);
	}
	struct kevent *eventArray = eventManager.getEventArray();


	struct linger	_linger;
	_linger.l_onoff = 1;
	_linger.l_linger = 0;

	while (1) {
		event_count = eventManager.updateEvent();
		eventManager.clearChangeVector();

		// 반복문 돌면서 이벤트 처리[0 1 2 3 4 ...]
		for (int i = 0; i < event_count; ++i) {
			Transaction *transaction = reinterpret_cast<Transaction *>(eventArray[i].udata);

			// 맵 순회하면서 ident가 socketFD인지 찾는 함수

				// 연결 요청에 대한 이벤트가 들어왔을 경우
				// 연결 수락
			socketMapIter	it = socketInfo.find(eventArray[i].ident);
			if (it != socketInfo.end()) {

				struct sockaddr_in	client_addr;

				int acceptedFD = accept(it->first, (struct sockaddr *)&client_addr, &size);
				fcntl(acceptedFD, F_SETFL, O_NONBLOCK);
				setsockopt(acceptedFD, SOL_SOCKET, SO_LINGER, &_linger, sizeof(_linger));

				// list에 새로운 소켓 fd를 등록
				clients.push_back(Transaction(ntohs(it->second.sin_port), acceptedFD));
				eventManager.addReadEvent(acceptedFD, &(clients.back()));
				eventManager.addWriteEvent(acceptedFD, &(clients.back()));
				
			} else if (transaction == NULL) {
				continue;
			} else if ((transaction->getState() != READ_PIPE) && (eventArray[i].filter == EVFILT_WRITE || eventArray[i].filter == EVFILT_READ) && eventArray[i].flags & EV_EOF) {
				typedef std::list<Transaction>::iterator	iter;
				for (int j = i + 1; j < event_count; ++j) {
					if (transaction == reinterpret_cast<Transaction *>(eventArray[j].udata)) {
						eventArray[j].udata = NULL;
					}
				}
				for (iter it = clients.begin(); it != clients.end(); ++it) {
					if (&(*it) == transaction) {
						clients.erase(it);
						close(eventArray[i].ident);
						break ;
					}
				}				
			} else if (eventArray[i].filter == EVFILT_WRITE) {
				// write 이벤트 처리
				// socket에 대한 write or local file에 대한 write
				try {
					transaction->controlWriteEvent(eventArray[i]);
				} catch(std::exception &e) {
					// std::cerr << e.what() << std::endl;
					typedef std::list<Transaction>::iterator	iter;
					for (int j = i + 1; j < event_count; ++j) {
						if (transaction == reinterpret_cast<Transaction *>(eventArray[j].udata)) {
							eventArray[j].udata = NULL;
						}
					}
					for (iter it = clients.begin(); it != clients.end(); ++it) {
						if (&(*it) == transaction) {
							clients.erase(it);
							close(eventArray[i].ident);
							break ;
						}
					}
				}
			} else if (eventArray[i].filter == EVFILT_READ) {
				// read 이벤트 처리
				// socket에 대한 read or local file에 대한 read
				transaction->controlReadEvent(eventArray[i]);
			} else if (eventArray[i].filter == EVFILT_PROC) {
				// cgi 끝남
				transaction->controlProcessExitEvent();
			}
		}
	}
	return (0);
}

/*

	struct kevent {
		uintptr_t ident;
		int16_t   filter;
		uint16_t  flags;
		uint32_t  fflags;
		intptr_t  data;
		void      *udata;
	};

*/
