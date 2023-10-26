#include <cstdlib>
#include <exception>
#include <fstream>
#include <ios>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <dirent.h>
#include <set>
#include "Util.hpp"
#include "ConfigData.hpp"
#include "ServerBlock.hpp"
#include "webserv.hpp"

static bool compareLocationWithUrl(const location & loc, const std::string & url);
static bool isServerBlockStartLine(std::vector<std::string> &v);
static bool isMimeTypeStartLine(std::vector<std::string> &v);

ConfigData *ConfigData::instance = NULL;

ConfigData::ConfigData(void) {}

ConfigData::ConfigData(const ConfigData &) {}

ConfigData::~ConfigData(void) {}

ConfigData &ConfigData::operator=(const ConfigData &) { return (*this); }

void ConfigData::destroy(void) {
	delete ConfigData::instance;
}

ConfigData *ConfigData::getInstance(void) {
	if (ConfigData::instance == NULL) {
		ConfigData::instance = new ConfigData();
		std::atexit(ConfigData::destroy);
	}
	return (ConfigData::instance);
}

void ConfigData::parseConfig(const char * path ) {
	std::fstream file(path, std::ios_base::in);
	// config파일 open 실패
	if (file.fail()) throw std::runtime_error("fail to open config file");

	for (std::string line; std::getline(file, line);) {
		std::vector<std::string> splitLine(Util::split(line));

		if (splitLine.empty() || splitLine[0][0] == '#') continue;

		if (isServerBlockStartLine(splitLine)) {
			// server 블록 시작
			ServerBlock newBlock;
			newBlock.parseServerBlock(file);
			this->serverBlocks.push_back(newBlock);
		} else if (splitLine[0] == KEY_DEFAULT_ERROR_PAGE) {
			readDefaultErrorPage(splitLine[1]);
		} else if (isMimeTypeStartLine(splitLine)) {
			parseMimeTypeBlock(file);
		} else {
			// Config 파일의 최상위에 설명할 것이 없다면 else 에러
			throw std::runtime_error("fail");
		}
	}
	file.close();
	if (this->defaultErrorPage.empty())
		throw std::runtime_error("need default error page");
	checkServerBlockPort();
}

const std::string &ConfigData::getServerRoot( const int & port ) const {
	return (findServerBlock(port).root);
}

const unsigned char *ConfigData::getHost( const int & port ) const {
	return (findServerBlock(port).host);
}

unsigned int	ConfigData::getHostData( const int & port ) const {
	return (findServerBlock(port).hostData);
}

const std::string &ConfigData::getStorage( const int & port ) const {
	return (findServerBlock(port).storage);
}

const long &ConfigData::getBodyLimit( const int & port ) const {
	return (findServerBlock(port).bodyLimit);
}

const bool &ConfigData::getDirectoryList( const int & port ) const {
	return (findServerBlock(port).directoryList);
}

const location &ConfigData::getLocation( const int & port, const std::string & url ) const {
	typedef std::vector<location>::const_iterator iterator;
	const ServerBlock &sBlock(findServerBlock(port));

	for (iterator iter = sBlock.locations.begin(); iter != sBlock.locations.end(); ++iter) {
		if (compareLocationWithUrl(*iter, url))
			return (*iter);
	}
	throw std::runtime_error(STATUS_NOT_FOUND);
}

const std::string ConfigData::getMimeTypeByExtension(const std::string & key) const {
	std::map<std::string, std::string>::const_iterator	iter = this->mimeType.find(key);

	if (iter == this->mimeType.end()) return (DEFAULT_MIME_TYPE);
	return (iter->second);
}

const std::string &ConfigData::getDefaultErrorPage( void ) const {
	return (this->defaultErrorPage);
}

const std::vector<int> ConfigData::getPorts() const {
	typedef std::vector<ServerBlock>::const_iterator	iter;
	std::vector<int>	ports;

	for (iter it = serverBlocks.begin(); it != serverBlocks.end(); ++it) {
		ports.push_back(it->port);
	}
	return (ports);
}

const ServerBlock &ConfigData::findServerBlock( const int & port ) const {
	typedef std::vector<ServerBlock>::const_iterator	iterator;
	iterator iter = this->serverBlocks.begin();

	for (; iter != this->serverBlocks.end(); ++iter) {
		if (iter->port == port)
			return (*iter);
	}
	throw std::runtime_error(STATUS_NOT_FOUND);
}

void ConfigData::parseMimeTypeBlock(std::fstream & file) {
	for (std::string line; std::getline(file, line);) {
		std::vector<std::string> splitLine(Util::split(line));

		if (splitLine.empty() || splitLine[0][0] == '#') continue;
		if (splitLine.size() == 1 && splitLine[0] == "}") break;

		if (splitLine.size() != 2) throw std::runtime_error("fail");

		this->mimeType[splitLine[0]] = splitLine[1];
	}
}

void ConfigData::makeDirectoryPage( void ) {
	typedef std::vector<ServerBlock>::iterator	iter;
	for (iter it = serverBlocks.begin(); it != serverBlocks.end(); ++it) {
		if (it->directoryList) {

			typedef std::vector<location>::iterator	locationIter;
			for (locationIter lit = it->locations.begin(); lit != it->locations.end(); ++lit) {
				if (lit->index.empty())
					continue;

				std::string page =
					"<!DOCTYPE html>"
					"<html lang='en'>"
					"<head>"
					"<meta charset='UTF-8'>"
					"<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
					"<title>Directory Listing</title>"
					"<style>"
					"	body {"
					"	font-family: Arial, sans-serif;"
					"	}"
					"	ul {"
					"	list-style-type: none;"
					"	padding: 0;"
					"	}"
					"	li {"
					"	margin-bottom: 8px;"
					"	}"
					"	a {"
					"	color: #1a0dab;"
					"	text-decoration: none;"
					"	}"
					"	a:hover {"
					"	text-decoration: underline;"
					"	}"
					"</style>"
					"</head>"
					"<body>"
					"<h1>Directory Listing</h1>"
					"<ul>";
				
				DIR *dir;
				struct dirent *diread;
				
				dir = opendir(lit->root.c_str());
				
				if (dir != NULL) {
					while ((diread = readdir(dir)) != NULL) {
						page += "<li>" + std::string(diread->d_name) + "</li>";
					}
					closedir (dir);
				} else {
					throw std::runtime_error("dir error");
				}

				page += "</ul></body></html>";
				lit->directoryPage = page;
			}
		}
	}
}
	
void ConfigData::readDefaultErrorPage( std::string & errorPage ) {
	std::fstream file(errorPage, std::ios_base::in);

	if (file.fail()) throw std::runtime_error("fail to open default error page file");

	file.seekg(0, std::ios::end);
	int size = file.tellg();
	file.seekg(0, std::ios::beg);
	this->defaultErrorPage.resize(size);
	file.read(&(this->defaultErrorPage[0]), size);

	if (file.fail()) throw std::runtime_error("fail to read default error page file");
	file.close();
}

void ConfigData::checkServerBlockPort(void) {
	typedef std::vector<ServerBlock>::iterator iter;

	std::set<int> ports;
	iter i = this->serverBlocks.begin();

	while (i != this->serverBlocks.end()) {
		if (ports.find(i->port) == ports.end()) {
			ports.insert(i->port);
			++i;
		} else {
			throw std::runtime_error("duplicated ports");
		}
	}
}

static bool compareLocationWithUrl(const location & loc, const std::string & url) {
	if (!loc.index.empty()) {
		// index
		typedef std::vector<std::string>::const_iterator iterator;
		if (Util::compareUrl(loc.url, url)) return (true);

		for (iterator iter = loc.index.begin(); iter != loc.index.end(); ++iter) {
			std::string comparePath(Util::joinPath(loc.url, *iter));
			if (Util::compareUrl(comparePath, url)) return (true);
		}
		return (false);
	}
	return (Util::compareUrl(loc.url, url));
}

static bool isServerBlockStartLine(std::vector<std::string> &v) {
	return (v[0] == KEY_SERVER && v[1] == "{");
}

static bool isMimeTypeStartLine(std::vector<std::string> &v) {
	return (v[0] == KEY_MIME_TYPE && v[1] == "{");
}

std::ostream &operator<<( std::ostream & o, ConfigData const & i ) {
	typedef std::vector<ServerBlock>::const_iterator serverIter;
	for (serverIter iter = i.serverBlocks.begin(); iter != i.serverBlocks.end(); ++iter) {
		o << *iter;
	}
	return (o);
}
