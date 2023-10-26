#ifndef CONFIGDATA_HPP
# define CONFIGDATA_HPP

# include "ServerBlock.hpp"
#include <fstream>
# include <string>
# include <map>
# include <vector>

class ConfigData {
private:
	std::string								defaultErrorPage;
	std::map<std::string, std::string>		mimeType;
	std::vector<ServerBlock>				serverBlocks;
	static ConfigData						*instance;

	ConfigData( void );
	ConfigData( const ConfigData & ref );
	ConfigData &operator=( const ConfigData & ref );
	static void destroy( void );

	const ServerBlock &findServerBlock( const int & port ) const;

	void parseMimeTypeBlock( std::fstream & file );

	void readDefaultErrorPage( std::string & errorPage );

	void checkServerBlockPort( void );

public:
	~ConfigData( void );

	static ConfigData *getInstance( void );

	void parseConfig( const char * file );

	const std::string &getServerRoot( const int & port ) const;

	const unsigned char *getHost( const int & port ) const;

	unsigned int	getHostData( const int & port ) const;

	const std::string &getStorage( const int & port ) const;

	const long &getBodyLimit( const int & port ) const;

	const bool &getDirectoryList( const int & port ) const;

	const location &getLocation( const int & port, const std::string & url ) const;

	const std::string getMimeTypeByExtension( const std::string & key ) const;

	const std::string &getDefaultErrorPage( void ) const;

	const std::vector<int> getPorts( void ) const;

	void makeDirectoryPage( void );

	friend std::ostream &operator<<( std::ostream & o, ConfigData const & i );
};

std::ostream &operator<<( std::ostream & o, ConfigData const & i );

#endif
