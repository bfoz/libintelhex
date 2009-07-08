/*  Routines for reading/writing Intel INHX8M and INHX32 files

    Copyright 2002 Brandon Fosdick (BSD License)
*/

#ifndef INTELHEXH
#define INTELHEXH

#include <fstream>
#include <map>
#include <vector>

#include <unistd.h>

namespace intelhex
{

    #define	HEX_FORMAT_INHX8M	0x01
    #define	HEX_FORMAT_INHX32	0x02

    typedef uint8_t value_type;

    //The data set that results from parsing a hex file
    struct hex_data
    {
	//Each line of the hex file generates a block of memory at a particular address
	typedef	uint32_t	address_t;
	typedef	std::vector<value_type>	data_container;		//Element container
	typedef	std::map<address_t, data_container> container;	//List of data blocks

	typedef	container::iterator	iterator;
	typedef	container::reverse_iterator	reverse_iterator;
	typedef	data_container::size_type	size_type;
    private:
	char	format;				//Format of the parsed file (necessary?)
	bool	segment_addr_rec;		// Uses/Has a segment address record
	bool	linear_addr_rec;		// Uses/Has a linear address record
	container   blocks;			// List of data blocks

    public:
	hex_data() : segment_addr_rec(false), linear_addr_rec(false) {}
	hex_data(const std::string &s) : segment_addr_rec(false), linear_addr_rec(false)
	{
	    load(s);
	}
	iterator    begin() { return blocks.begin(); }
	iterator    end() { return blocks.end(); }
	void	clear();		//Delete everything
	size_type   size();
	size_type   size_below_addr(address_t);
	size_type   size_in_range(address_t, address_t);    //number of words in [lo, hi)
	address_t   max_addr_below(address_t);

	bool	isset(address_t);

	value_type& operator[](address_t);	//Array access operator
	value_type  get(address_t, value_type);	//FIXME	Nasty kludge

	bool	load(const char *);			//Load a hex file from disk
	bool	load(const std::string &s) {return load(s.c_str());}	//Load a hex file from disk
	void	write(const char *);			//Save hex data to a hex file
	void	write(std::ostream &);			//Write all data to an output stream
	void	tidy(size_type length);			// Make things pretty
    };

    bool compare(hex_data&, hex_data&, value_type, hex_data::address_t, hex_data::address_t);
}
#endif
