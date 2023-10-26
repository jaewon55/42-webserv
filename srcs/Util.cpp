#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "Util.hpp"

Util::Util(void) {}

Util::Util(const Util &) {}

Util &Util::operator=(const Util &) { return (*this); }

Util::~Util(void) {}

int Util::stoi(const std::string & str) {
	int ret;
	std::istringstream iss(str);

	iss >> ret;
	if (iss.fail() || !iss.eof()) throw std::runtime_error("stoi");

	return (ret);
}

long Util::hexstol(const std::string & str) {
	typedef std::string::const_reverse_iterator	rIter;
	
	long ret(0), i(1);
	rIter iter(str.rbegin());
	
	for (; iter != str.rend(); ++iter, i *= 16) {
		long temp;
		if (isdigit(*iter)) {
			temp = *iter - '0';
		} else if ('a' <= *iter && *iter <= 'f') {
			temp = 10 + (*iter - 'a');
		} else if ('A' <= *iter && *iter <= 'F') {
			temp = 10 + (*iter - 'A');
		} else throw std::runtime_error("hexstol");

		temp *= i;
		ret += temp;
	}

	return (ret);
}

std::string Util::itos( const long num ) {
	std::stringstream ss;
	ss << num;
	return (ss.str());
}

std::string Util::itos( const std::size_t num ) {
	std::stringstream ss;
	ss << num;
	return (ss.str());
}

Util::stringVector Util::split(const std::string &str) {
	stringVector ret;
	std::istringstream iss(str);

	while (true) {
		std::string word;
		iss >> word;

		if (!word.empty()) ret.push_back(word);

		if (iss.eof()) break;
	}
	return (ret);
}

Util::stringVector Util::split(const std::string & str, char delim) {
	stringVector ret;
	std::string token;
	std::stringstream ss(str);
	
	while (std::getline(ss, token, delim)) {
		if (!token.empty()) ret.push_back(token);
	}
	return (ret);
}

std::string Util::joinPath( std::string p1, std::string p2 ) {
	if (p1.back() != '/') p1.push_back('/');
	if (p2.front() == '/') p2.erase(p2.begin());
	return (p1 + p2);
}

bool Util::compareUrl( std::string u1, std::string u2 ) {
	if (!u1.empty() && u1.front() == '/') u1.erase(u1.begin());
	if (!u1.empty() && u1.back() == '/') u1.pop_back();
	if (!u2.empty() && u2.front() == '/') u2.erase(u2.begin());
	if (!u2.empty() && u2.back() == '/') u2.pop_back();
	return (u1 == u2);
}

std::string	Util::extensionFinder(const std::string & str) {
	std::size_t	pos = str.rfind(".");
	if (pos == std::string::npos) return ("");
	
	return (str.substr(pos + 1)); 
}
