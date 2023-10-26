#include <algorithm>
#include <cstring>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <unistd.h>
#include <vector>
#include "ServerBlock.hpp"
#include "Util.hpp"
#include "webserv.hpp"

#define BODY_LIMIT_MAX 104857600

enum ConfigKey {
	E_LOCATION,
	E_LISTEN,
	E_HOST,
	E_ROOT,
	E_STORAGE,
	E_BODY_LIMIT,
	E_DIRECTORY_LIST,
	E_METHODS,
	E_INDEX,
	E_REDIR,
	E_CGI_PATH
};

static void setLocationRoot(std::vector<std::string> &splitLine, location &newLocation);
static void setLocationMethods(std::vector<std::string> &splitLine, location &newLocation);
static void setLocationIndex(std::vector<std::string> &splitLine, location &newLocation);
static void setLocationRedir(std::vector<std::string> &splitLine, location &newLocation);
static void setLocationCgiPath(std::vector<std::string> &splitLine, location &newLocation);
static void checkLocation(location &loc);

std::string location::getPathWithUrl(const std::string &url) const {
	typedef std::vector<std::string>::const_iterator	iterator;
	std::string path;

	for (iterator iter = this->index.begin(); iter != this->index.end(); ++iter) {
		std::string comparePath(Util::joinPath(this->url, *iter));
		if (Util::compareUrl(comparePath, url)) {
			path = Util::joinPath(this->root, *iter);
			break;
		}
	}
	return (path);
}

ServerBlock::ServerBlock(void) : hostData(0), root("./") {}

ServerBlock::ServerBlock(const ServerBlock & ref) {
	*this = ref;
}

ServerBlock::~ServerBlock(void) {}

ServerBlock &ServerBlock::operator=(const ServerBlock & ref) {
	if (this != &ref) {
		this->port = ref.port;
		memcpy(this->host, ref.host, 4);
		this->hostData = ref.hostData;
		this->root = ref.root;
		this->storage = ref.storage;
		this->bodyLimit = ref.bodyLimit;
		this->directoryList = ref.directoryList;
		this->locations = ref.locations;
	}
	return (*this);
}

void ServerBlock::operator()( location & loc ) {
	updateLocations(loc);
}

static ConfigKey checkConfigKey(std::string key) {
	const std::string keyArr[] = {
	BLOCK_LOCATION,
	KEY_LISTEN,
	KEY_HOST,
	KEY_ROOT,
	KEY_STORAGE,
	KEY_BODY_LIMIT,
	KEY_DIRECTORY_LIST,
	KEY_METHODS,
	KEY_INDEX,
	KEY_REDIR,
	KEY_CGI_PATH
	};
	const int len(static_cast<int>(sizeof(keyArr) / sizeof(std::string)));
	int i(0);

	while (i < len) {
		if (keyArr[i] == key) break;
		++i;
	}
	// 잘못된 key
	if (i == len) throw std::runtime_error("checkConfigKey");
	return (static_cast<ConfigKey>(i));
}

void ServerBlock::parseServerBlock(std::fstream &file) {
	for (std::string line; std::getline(file, line);) {
		std::vector<std::string> splitLine(Util::split(line));

		if (splitLine.empty() || splitLine[0][0] == '#') continue;
		if (splitLine.size() == 1 && splitLine[0] == "}") break;

		switch (checkConfigKey(splitLine[0])) {
			case E_LOCATION: 
				parseLocationBlock(splitLine, file);
				break;
			case E_LISTEN: 
				parseListen(splitLine);
				break;
			case E_HOST: 
				parseHost(splitLine);
				break;
			case E_ROOT: 
				parseRoot(splitLine);
				break;
			case E_STORAGE: 
				parseStorage(splitLine);
				break;
			case E_BODY_LIMIT: 
				parseBodyLimit(splitLine);
				break;
			case E_DIRECTORY_LIST: 
				parseDirectoryList(splitLine);
				break;
			// 잘못된 key
			default: throw std::runtime_error("parseServerBlock");
		}
	}
	updateServerBlock();
	std::for_each(this->locations.begin(), this->locations.end(), *this);
}

void ServerBlock::parseLocationBlock(std::vector<std::string> &startLine, std::fstream & file) {
	location newLocation;

	if (startLine.size() != 3 || startLine[1][0] != '/' || startLine[2] != "{")
		throw std::runtime_error("parseLocationBlock");

	newLocation.url = startLine[1];

	for (std::string line; std::getline(file, line);) {
		std::vector<std::string> splitLine(Util::split(line));

		if (splitLine.empty() || splitLine[0][0] == '#') continue;
		if (splitLine.size() == 1 && splitLine[0] == "}") break;

		switch (checkConfigKey(splitLine[0])) {
			case E_ROOT: 
				setLocationRoot(splitLine, newLocation);
				break;
			case E_METHODS: 
				setLocationMethods(splitLine, newLocation);
				break;
			case E_INDEX: 
				setLocationIndex(splitLine, newLocation);
				break;
			case E_REDIR: 
				setLocationRedir(splitLine, newLocation);
				break;
			case E_CGI_PATH: 
				setLocationCgiPath(splitLine, newLocation);
				break;
			// 잘못된 key
			default: throw std::runtime_error("parseLocationBlock");
		}
	}
	checkLocation(newLocation);
	locations.push_back(newLocation);
}

void ServerBlock::parseListen(std::vector<std::string> & splitLine) {
	if (splitLine.size() != 2) throw std::runtime_error("listen");

	this->port = Util::stoi(splitLine[1]);
}

void ServerBlock::parseHost(std::vector<std::string> & splitLine) {
	if (splitLine.size() != 2) throw std::runtime_error("host");

	Util::stringVector v(Util::split(splitLine[1], '.'));
	
	if (v.size() != 4) throw std::runtime_error("host");

	for (int i = 0; i < 4; ++i) {
		unsigned int n = Util::stoi(v[i]);
		if (n < 0 || 255 < n) throw std::runtime_error("host");
		this->hostData += (n << (i * 8));
		this->host[i] = static_cast<unsigned char>(n);
	}
}

void ServerBlock::parseRoot(std::vector<std::string> & splitLine) {
	if (splitLine.size() != 2) throw std::runtime_error("root");

	if (access(splitLine[1].c_str(), F_OK) < 0) throw std::runtime_error("root");
	this->root = splitLine[1];
}

void ServerBlock::parseStorage(std::vector<std::string> & splitLine) {
	if (splitLine.size() != 2) throw std::runtime_error("storage");

	this->storage = splitLine[1];
}

void ServerBlock::parseBodyLimit(std::vector<std::string> & splitLine) {
	if (splitLine.size() != 2) throw std::runtime_error("body_limit");

	this->bodyLimit = Util::stoi(splitLine[1]);
}

void ServerBlock::parseDirectoryList(std::vector<std::string> & splitLine) {
	if (splitLine.size() != 2) throw std::runtime_error("directory_list");

	if (splitLine[1] == "on") {
		this->directoryList = true;
	} else if (splitLine[1] == "off") {
		this->directoryList = false;
	} else {
		throw std::runtime_error("directory_list");
	}
}

void ServerBlock::updateServerBlock(void) {
	unsigned char cmp[4];
	memset(cmp, 0, 4);
	if (this->port == 0 || memcmp(this->host, cmp, 4) == 0)
		throw std::runtime_error("updateServerBlock");
	if (this->storage.empty())
		throw std::runtime_error("updateServerBlock");
	if (this->bodyLimit < 1 || this->bodyLimit > BODY_LIMIT_MAX)
		throw std::runtime_error("updateServerBlock");

	std::string path(Util::joinPath(this->root, this->storage));
	if (access(path.c_str(), F_OK) < 0)
		throw std::runtime_error("updateServerBlock");
	this->storage = path;
}

void ServerBlock::updateLocations(location &loc) {
	if (loc.root.empty()) loc.root = Util::joinPath(this->root, loc.url);

	if (!loc.cgiPath.empty()) {
		std::string path(Util::joinPath(loc.root, loc.cgiPath));
		if (access(path.c_str(), F_OK) < 0) throw std::runtime_error("update");
	}
}

static void setLocationRoot(std::vector<std::string> &splitLine, location &newLocation) {
	if (splitLine.size() != 2) throw std::runtime_error("setLocationRoot");

	newLocation.root = splitLine[1];
}

static RequestDataMethod checkHTTPMethod(std::string method) {
	std::string methodArr[3] = {"GET", "POST", "DELETE"};
	int i(0);

	while (i < 3) {
		if (methodArr[i] == method) break;
		++i;
	}

	if (i == 3) throw std::runtime_error("invalid HTTP method!");
	return (static_cast<RequestDataMethod>(i));
}

static void setLocationMethods(std::vector<std::string> &splitLine, location &newLocation) {
	typedef std::vector<std::string>::iterator iterator;

	for (iterator iter = splitLine.begin(); iter != splitLine.end(); ++iter) {
		if (iter == splitLine.begin()) continue;
		newLocation.methods.push_back(checkHTTPMethod(*iter));
	}
}

static void setLocationIndex(std::vector<std::string> &splitLine, location &newLocation) {
	typedef std::vector<std::string>::iterator iterator;

	if (splitLine.size() < 2 || !newLocation.cgiPath.empty() || !newLocation.redirPath.empty())
		throw std::runtime_error("setLocationIndex");
	for (iterator iter = ++(splitLine.begin()); iter != splitLine.end(); ++iter) {
		newLocation.index.push_back(*iter);
	}
}

static void setLocationRedir(std::vector<std::string> &splitLine, location &newLocation) {
	if (splitLine.size() != 2 || !newLocation.cgiPath.empty() || !newLocation.index.empty())
		throw std::runtime_error("setLocationRedir");

	newLocation.redirPath = splitLine[1];
}

static void setLocationCgiPath(std::vector<std::string> &splitLine, location &newLocation) {
	if (splitLine.size() != 2 || !newLocation.index.empty() || !newLocation.redirPath.empty())
		throw std::runtime_error("setLocationCgiPath");

	newLocation.cgiPath = splitLine[1];
}

static void checkLocation(location &loc) {
	if (loc.cgiPath.empty() && loc.redirPath.empty() && loc.index.empty())
		throw std::runtime_error("checkLocation");
}

std::ostream &operator<<( std::ostream & o, location const & loc ) {
	typedef std::vector<RequestDataMethod>::const_iterator methodIter;
	typedef std::vector<std::string>::const_iterator stringIter;

	o << "\tlocation " << loc.url << "\n";
	o << "\t\troot " << loc.root << "\n";
	o << "\t\tmethods";
	for (methodIter iter = loc.methods.begin(); iter != loc.methods.end(); ++iter) {
		std::string m;
		switch (*iter) {
			case GET: 
				m = "GET";
				break;
			case POST: 
				m = "POST";
				break;
			case DELETE: 
				m = "DELETE";
				break;
		}
		o << " " << m;
	}
	o << "\n";
	if (!loc.index.empty()) {
		// index
		o << "\t\tindex";
		for (stringIter iter = loc.index.begin(); iter != loc.index.end(); ++iter) {
			o << " " << *iter;
		}
	} else if (!loc.redirPath.empty()) {
		// redir
		o << "\t\tredir " << loc.redirPath;
	} else {
		// cgi_path
		o << "\t\tcgi_path" << loc.cgiPath;
	}
	o << "\n\n";
	return (o);
}

std::ostream &operator<<( std::ostream & o, ServerBlock const & server ) {
	o << "server" << "\n";
	o << "\tlisten " << server.port << "\n\n";
	o << "\thost ";
	for (int i = 0; i < 4; ++i) {
		o << static_cast<int>(server.host[i]);
		if (i != 3) o << ".";
	}
	o << "\n\n";
	o << "\troot " << server.root << "\n\n";
	o << "\tstorage " << server.storage << "\n\n";
	o << "\tbody_limit " << server.bodyLimit << "\n\n";
	o << "\tdirectory_list ";
	if (server.directoryList)
		o << "on\n\n";
	else
		o << "off\n\n";
	typedef std::vector<location>::const_iterator locIter;
	for (locIter iter = server.locations.begin(); iter != server.locations.end(); ++iter) {
		o << *iter;
	}
	return (o);
}
