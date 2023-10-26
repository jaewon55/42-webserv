#ifndef REQUESTDATA_HPP
# define REQUESTDATA_HPP

# include <iostream>
# include <string>
# include <map>
# include <vector>
# include "webserv.hpp"

typedef std::vector<char>					charVector;
typedef charVector::iterator				charVectorIter;
typedef std::map<std::string, std::string>	stringMap;
typedef stringMap::iterator					stringMapIter;

class RequestData {
private:
	RequestDataState	state;

	// start-line
	RequestDataMethod	method;
	std::string			url;
	stringMap			query;
	std::string			queryString;
	std::string			version;

	// header-field
	stringMap			headers;

	// body-field
	charVector			body;
	long				bodyLength;

	// buffer
	charVector			buff;
	charVectorIter		newLine;

	// parsing methods
	void				parseStartLine( charVectorIter newLine );
	void				parseUrlQuery( void );
	void				parseHeader( charVectorIter newLine );
	void				parseBody( void );

	charVectorIter		findCRLF( void );
	charVectorIter		findCRLF(charVectorIter it);
	
public:
	RequestData( void );
	RequestData( const RequestData & src );
	~RequestData( void );

	RequestData &operator=( const RequestData & rhs );

	RequestDataState	getState( void ) const;
	int					getMethod( void ) const;
	std::string			getMethodString( void ) const;
	const std::string	&getUrl( void ) const;
	const stringMap		&getQuery( void ) const;
	const std::string	getQueryString( void ) const;
	const std::string	&getVersion( void ) const;
	const std::string	&getHeaderByKey(std::string key) const;
	const charVector	&getBody( void ) const;
	long				getBodyLength( void ) const;
	const charVector	&getBuff( void ) const;
	void				joinBuff(char *buff, long len);
	
	friend std::ostream &operator<<( std::ostream & o, RequestData const & i );
};

std::ostream &operator<<( std::ostream & o, RequestData const & i );

#endif
