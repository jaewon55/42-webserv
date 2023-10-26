#ifndef TRANSACTION_HPP
# define TRANSACTION_HPP

#include "RequestData.hpp"
#include "ResponseData.hpp"
#include "webserv.hpp"
# include <iostream>
#include <stdexcept>
# include <string>
# include <map>
# include <vector>

class Transaction {
private:
	TransactionState	state;
	RequestData			request;
	ResponseData		response;
	int					port;
	int					acceptedFD;
	int 				readFD;
	int 				writeFD;
	int					chlidExitState;
	pid_t				childPid;

	std::vector<char>	message;
	ssize_t				sendSize;
	bool				errorFlag;

	void				errorHandler( const char * code );

	void				cgiProcess( void );

	void				parseRequestMessage(struct kevent & kev);

	void				readFileProcess(struct kevent & kev);
	void				readPipeProcess(struct kevent & kev);
	void				readErrorFileProcess(struct kevent & kev);

public:
	Transaction( void );
	Transaction( const Transaction & src );
	Transaction( int portNum, int fd );
	~Transaction( void );
	Transaction &operator=( const Transaction & rhs );

	TransactionState	getState( void ) const;

	void controlWriteEvent( struct kevent & kev );
	void controlReadEvent( struct kevent & kev );
	void controlProcessExitEvent( void );

	void				sendResponseMessage( void );
};

#endif
