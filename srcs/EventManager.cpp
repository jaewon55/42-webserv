#include <sys/event.h>
#include "EventManager.hpp"
#include "Transaction.hpp"

int	EventManager::kq = kqueue();

EventManager	*EventManager::instance = NULL;

EventManager::EventManager(void) {}

EventManager::EventManager(const EventManager & ref) {
	*this = ref;
}

EventManager::~EventManager(void) {}

EventManager &EventManager::operator=(const EventManager & ref) {
	if (this != &ref) {
		this->changeVector = ref.changeVector;
		for (int i = 0; i < EVENT_COUNT_LIMIT; ++i) {
			this->eventArray[i] = ref.eventArray[i];
		}
	}
	return (*this);
}

void EventManager::destroy(void) {
	delete EventManager::instance;
}

EventManager *EventManager::getInstance(void) {
	if (EventManager::instance == NULL) {
		EventManager::instance = new EventManager();
		std::atexit(EventManager::destroy);
	}
	return (EventManager::instance);
}

struct kevent	*EventManager::getEventArray(void) { return (this->eventArray); }

void	EventManager::addReadEvent(const int fd, Transaction *t) {
	struct kevent kev;

	EV_SET(&kev, fd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_EOF, 0, 0, NULL);
	kev.udata = static_cast<void *>(t);
	this->changeVector.push_back(kev);
}

void	EventManager::oneShotReadEvent( int fd, Transaction *t) {
	struct kevent kev;

	EV_SET(&kev, fd, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, NULL);
	kev.udata = static_cast<void *>(t);
	this->changeVector.push_back(kev);
}

void	EventManager::addWriteEvent(int fd, Transaction *t) {
	struct kevent kev;

	EV_SET(&kev, fd, EVFILT_WRITE, EV_ADD | EV_DISABLE | EV_EOF, 0, 0, NULL);
	kev.udata = static_cast<void *>(t);
	this->changeVector.push_back(kev);
}

void	EventManager::oneShotWriteEvent( int fd, Transaction *t) {
	struct kevent kev;

	EV_SET(&kev, fd, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, NULL);
	kev.udata = static_cast<void *>(t);
	this->changeVector.push_back(kev);
}

void	EventManager::addProcEvent(int pid, Transaction *t) {
	struct kevent kev;

	EV_SET(&kev, pid, EVFILT_PROC, EV_ADD | EV_ENABLE, NOTE_EXIT, 0, NULL);
	kev.udata = static_cast<void *>(t);
	this->changeVector.push_back(kev);
}

void	EventManager::onWriteEvent(int fd, Transaction *t) {
	struct kevent kev;

	EV_SET(&kev, fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL);
	kev.udata = static_cast<void *>(t);
	this->changeVector.push_back(kev);
}

void	EventManager::offWriteEvent(int fd, Transaction *t) {
	struct kevent kev;

	EV_SET(&kev, fd, EVFILT_WRITE, EV_ADD | EV_DISABLE, 0, 0, NULL);
	kev.udata = static_cast<void *>(t);
	this->changeVector.push_back(kev);
}


void	EventManager::delReadEvent(int fd, Transaction *t) {
	struct kevent kev;

	EV_SET(&kev, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
	kev.udata = static_cast<void *>(t);
	this->changeVector.push_back(kev);
}

void	EventManager::delWriteEvent(int fd, Transaction *t) {
	struct kevent kev;

	EV_SET(&kev, fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
	kev.udata = static_cast<void *>(t);
	this->changeVector.push_back(kev);
}

void	EventManager::delProcEvent(int pid, Transaction *t) {
	struct kevent kev;

	EV_SET(&kev, pid, EVFILT_PROC, EV_DELETE, NOTE_EXIT, 0, NULL);
	kev.udata = static_cast<void *>(t);
	this->changeVector.push_back(kev);
}

int		EventManager::updateEvent(void) {
	return (
		kevent(
			kq,
			&(changeVector[0]),
			changeVector.size(),
			eventArray,
			EVENT_COUNT_LIMIT,
			NULL
		)
	);
}

void	EventManager::clearChangeVector(void) {
	this->changeVector.clear();
}
