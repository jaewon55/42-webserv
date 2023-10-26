#ifndef RESPONSEDATA_HPP
# define RESPONSEDATA_HPP

# include <iostream>
# include <string>
# include <map>
# include <vector>
# include "webserv.hpp"

typedef std::vector<char>								charVector;
typedef charVector::iterator							charVectorIter;
typedef std::map<std::string, std::string>				stringMap;
typedef stringMap::iterator								stringMapIter;
typedef std::map<const std::string, const std::string>	statusMap;
typedef statusMap::value_type 							statusMapValue;

class ResponseData {
private:
	ResponseDataState	state;
	std::string			version;
	std::string			status;
	stringMap			header;
	charVector			body;
	charVector			CGIBuff;

public:
	ResponseData( void );
	ResponseData( const ResponseData & src );
	~ResponseData( void );

	ResponseData &operator=( const ResponseData & rhs );

	//getter;
	ResponseDataState	getState( void ) const;

	//setters;
	void	setState( const ResponseDataState s );
	void	setStatus( const char * code );
	void	addHeader( const std::string & key, const std::string & value );
	void	setBody( const charVector & data );
	void	addBody( const char *data, std::size_t size );
	void	setCGIBuff( const charVector & buff );

	//sender;
	charVector	createResponseMessage( void ) const;
};

#endif
