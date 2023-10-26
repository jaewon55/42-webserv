#ifndef EVENT_MANAGER_HPP
# define  EVENT_MANAGER_HPP

#include <vector>
#include <sys/event.h>

# define EVENT_COUNT_LIMIT 42

class Transaction;

class EventManager {
private:
	static int kq;
	static EventManager	*instance;

	std::vector<struct kevent> changeVector;
	struct kevent eventArray[EVENT_COUNT_LIMIT];

	EventManager( void );
	EventManager( const EventManager & ref );
	EventManager &operator=( const EventManager & ref );
	static void destroy( void );

public:
	~EventManager( void );

	static EventManager *getInstance( void );

	struct kevent	*getEventArray( void );

	void	addReadEvent( int fd, Transaction *t = NULL );
	void	oneShotReadEvent( int fd, Transaction *t = NULL );
	void	addWriteEvent( int fd, Transaction *t = NULL );
	void	oneShotWriteEvent( int fd, Transaction *t = NULL );
	void	addProcEvent( int pid, Transaction *t = NULL );

	void	onWriteEvent( int fd, Transaction *t = NULL );
	void	offWriteEvent( int fd, Transaction *t = NULL );

	void	delReadEvent( int fd, Transaction *t = NULL );
	void	delWriteEvent( int fd, Transaction *t = NULL );
	void	delProcEvent( int pid, Transaction *t = NULL );

	int		updateEvent( void );

	void	clearChangeVector( void );
};

#endif
