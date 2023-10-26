#ifndef UTIL_HPP
# define UTIL_HPP

# include <string>
# include <vector>

class Util {
private:
	Util( void );
	Util( const Util & ref );
	Util &operator=( const Util & ref );
	~Util( void );
public:
	typedef std::vector<std::string> stringVector;

	static int stoi( const std::string & str );
	static long hexstol( const std::string & str );
	static std::string itos( const long num );
	static std::string itos( const std::size_t num );
	static stringVector	split( const std::string & str );
	static stringVector split( const std::string & str, char delim );
	static std::string joinPath( std::string p1, std::string p2 );
	static bool compareUrl( std::string u1, std::string u2 );
	static std::string	extensionFinder( const std::string & str );

};

#endif
