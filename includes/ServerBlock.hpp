#ifndef SERVERBLOCK_HPP
# define SERVERBLOCK_HPP

# include <fstream>
# include <string>
# include <map>
# include <vector>
# include "webserv.hpp"

struct location {
	std::string								root;
	std::string								url;
	std::vector<RequestDataMethod>			methods;
	std::string								cgiPath;
	std::vector<std::string>				index;
	std::string								redirPath;
	std::string								directoryPage;

	std::string getPathWithUrl( const std::string & url ) const;
};

class ServerBlock {
private:
	int							port;
	unsigned char				host[4];
	unsigned int				hostData;
	std::string					root;
	std::string					storage;
	long						bodyLimit;
	std::vector<location>		locations;
	bool						directoryList;

	void parseLocationBlock( std::vector<std::string> & startLine, std::fstream & file );
	void parseListen( std::vector<std::string> & splitLine );
	void parseHost( std::vector<std::string> & splitLine );
	void parseRoot( std::vector<std::string> & splitLine );
	void parseStorage( std::vector<std::string> & splitLine );
	void parseBodyLimit( std::vector<std::string> & splitLine );
	void parseDirectoryList( std::vector<std::string> & splitLine );
	void updateServerBlock( void );
	void updateLocations( location & loc );


public:
	ServerBlock( void );
	ServerBlock( const ServerBlock & ref );
	~ServerBlock( void );

	ServerBlock &operator=( const ServerBlock & ref );
	void operator()( location & loc );

	void parseServerBlock(std::fstream &file);

	friend class ConfigData;

	friend std::ostream &operator<<( std::ostream & o, ServerBlock const & i );
};

std::ostream &operator<<( std::ostream & o, location const & loc );
std::ostream &operator<<( std::ostream & o, ServerBlock const & i );

#endif
