/*  Routines for reading/writing Intel INHX8M and INHX32 files

    Copyright 2002 Brandon Fosdick (BSD License)
*/

#include <iostream>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "intelhex.h"

namespace intelhex
{

    #define	INH32M_HEADER	":020000040000FA"

    // Array access operator
    value_type& hex_data::operator[](hex_data::address_t address)
    {
	// Start at the end of the list and find the first (last) block with an address
	//  less than addr
	reverse_iterator i = blocks.rbegin();
	while( i != blocks.rend() )
	{
	    if( i->first <= address )
	    {
		// Use the block if address is interior or adjacent to the block
		if( (address - i->first) <= i->second.size() )
		    return i->second[address - i->first];
		break;
	    }
	    ++i;
	}
	return blocks[address][0];
    }

    // FIXME Nasty kludge
    //  I should really create an iterator class to handle this
    value_type hex_data::get(address_t address)
    {
	// Start at the end of the list and find the first (last) block with an address
	//  less than addr
	reverse_iterator i = blocks.rbegin();
	while( i != blocks.rend() )
	{
	    if( i->first <= address )
	    {
		// Use the block if address is interior or adjacent to the block
		if( (address - i->first) <= i->second.size() )
		    return i->second[address - i->first];
		break;
	    }
	    ++i;
	}
	return _fill;
    }

    // Delete all allocated memory
    void hex_data::clear()
    {
	_fill = 0;
	format = HEX_FORMAT_INHX8M;
	blocks.clear();
    }

    hex_data::size_type hex_data::size()
    {
	size_type s=0;

	for(iterator i=blocks.begin(); i!=blocks.end(); ++i)
	    s += i->second.size();

	return s;
    }

    // Returns the number of populated elements with addresses less than addr
    hex_data::size_type hex_data::size_below_addr(address_t addr)
    {
	size_type s=0;

	for(iterator i=blocks.begin(); i!=blocks.end(); ++i)
	{
	    if( (i->first + i->second.size()) < addr)
		s += i->second.size();
	    else if( i->first < addr )
		s += addr - i->first;
	}

	return s;
    }

    // number of words in [lo, hi)
    hex_data::size_type hex_data::size_in_range(address_t lo, address_t hi)
    {
	size_type s=0;

	for(iterator i=blocks.begin(); i!=blocks.end(); ++i)
	{
	    if( i->first < lo )
	    {
		const size_type a = i->first + i->second.size();
		if( a >= lo )
		    s += a  - lo;
	    }
	    else
	    {
		if( (i->first + i->second.size()) < hi)
		    s += i->second.size();
		else if( i->first < hi )
		    s += hi - i->first;
	    }
	}

	return s;
    }

    // Return the max address of all of the set words with addresses less than or equal to hi
    hex_data::address_t hex_data::max_addr_below(address_t hi)
    {
	address_t s=0;

	for(iterator i=blocks.begin(); i!=blocks.end(); ++i)
	{
	    if( i->first <= hi)
	    {
		const address_t a = i->first + i->second.size() - 1;	//Max address of this block
		if( a > s )
		    s = a;
	    }
	}
	if( s > hi )
	    return hi;
	else
	    return s;
    }

    //Return true if an element exists at addr
    bool hex_data::isset(address_t addr)
    {
	// Start at the end of the list and find the first (last) block with an address
	//  less than addr
	reverse_iterator i = blocks.rbegin();
	while( (i!=blocks.rend()) && (i->first > addr))
	    ++i;

	if( (addr - i->first) > i->second.size() )
	    return false;
	else
	    return true;
    }

    // Load a hex file from disk
    //  Destroys any data that has already been loaded
    bool hex_data::load(const char *path)
    {
	FILE	*fp;
	unsigned int	address, count, rtype, i;
	uint16_t	linear_address(0);

	if( (fp=fopen(path, "r"))==NULL )
	{
	    return false;
	}

	clear();		//First, clean house

	// Start parsing the file
	while(!feof(fp))
	{
	    if(fgetc(fp)==':')	//First character of every line should be ':'
	    {
		fscanf(fp, "%2x", &count);	//Read in byte count
		fscanf(fp, "%4x", &address);	//Read in address
		fscanf(fp, "%2x", &rtype);	//Read type

		switch(rtype)	//What type of record?
		{
		    case 0: 	//Data block so store it
		    {
			//Make a data block
			data_container& b = blocks[(static_cast<uint32_t>(linear_address) << 16) + address];
			b.resize(count);
			for(i=0; i < count; i++)			//Read all of the data bytes
			    fscanf(fp, "%2hhx", &(b[i]));
			break;
		    }
		    case 1:	//EOF
			break;
		    case 2:	//Segment address record (INHX32)
			segment_addr_rec = true;
			break;
		    case 4:	//Linear address record (INHX32)
			if( (0 == address) && (2 == count) )
			{
			    fscanf(fp, "%04hx", &linear_address);	//Get the new linear address
			    linear_addr_rec = true;
			}
			else
			{
			    //FIXME	There's a problem
			}
			break;
		}
		fscanf(fp,"%*[^\n]\n");		//Ignore the checksum and the newline
	    }
	    else
	    {
		printf("%s: Bad line\n", __FUNCTION__);
		fscanf(fp, "%*[^\n]\n");	//Ignore the rest of the line
	    }
	}
	fclose(fp);

	return true;
    }

    // Write all data to a file
    void hex_data::write(const char *path)
    {
	std::ofstream	ofs(path);
	if(!ofs)
	{
	    std::cerr << "Couldn't open the output file stream\n";
	    exit(1);
	}
	write(ofs);
	ofs.close();
    }

    // Write all data to an output stream
    void hex_data::write(std::ostream &os)
    {
	uint8_t	checksum;
	uint16_t	linear_address(0);

	if(!os)
	{
	    std::cerr << "Couldn't open the output file stream\n";
	    exit(1);
	}

	os.setf(std::ios::hex, std::ios::basefield);	//Set the stream to ouput hex instead of decimal
	os.setf(std::ios::uppercase);			//Use uppercase hex notation
	os.fill('0');					//Pad with zeroes

	//If we already know that this is an INHX32M file, start with a segment address record
	//	otherwise check all of the blocks just to make sure
	if( linear_addr_rec )
	{
	    os << INH32M_HEADER << std::endl;
	}
	else
	{
	    for(iterator i=blocks.begin(); i!=blocks.end(); i++)
	    {
		if(i->first > 0xFFFF)	//Check the upper 16 bits
		{
		    linear_addr_rec = true;
		    os << INH32M_HEADER << std::endl;
		    break;	//Only need to find one
		}
	    }
	}

	for(iterator i=blocks.begin(); i!=blocks.end(); i++)
	{
	    // Check upper 16 bits of the block address for non-zero,
	    //  which indicates that a segment address record is needed
	    if( i->first > 0xFFFF )
	    {
		const uint16_t addr(i->first >> 16);
		//Has a record for this segment already been emitted?
		if( addr != linear_address )
		{
		    //Emit a new segment address record
		    os << ":02000004";
		    os.width(4);
		    os << addr;	//Address
		    // Create a checksum for the linear address record
		    checksum = 0x06 + addr + (addr >> 8);
		    checksum = 0x01 + ~checksum;
		    os.width(2);
		    // OSX (or maybe GCC), seems unable to handle uint8_t
		    //  arguments to a stream
		    os << static_cast<uint16_t>(checksum);	// Checksum byte
		    os << std::endl;
		    linear_address = addr;
		}
	    }
	    checksum = 0;
	    os << ':';	//Every line begins with ':'
	    os.width(2);
	    os << i->second.size();				//Length
	    checksum += i->second.size();
	    os.width(4);
	    os << static_cast<uint16_t>(i->first);		//Address
	    checksum += static_cast<uint8_t>(i->first);		// Low byte
	    checksum += static_cast<uint8_t>(i->first >> 8);	// High byte
	    os << "00";											//Record type
	    for(unsigned j=0; j<i->second.size(); ++j)	//Store the data bytes, LSB first, ASCII HEX
	    {
		os.width(2);
		// OSX (or maybe GCC), seems unable to handle uint8_t
		//  arguments to a stream
		os << static_cast<uint16_t>(i->second[j]);
		checksum += i->second[j];
	    }
	    checksum = 0x01 + ~checksum;
	    os.width(2);
	    // OSX (or maybe GCC), seems unable to handle uint8_t arguments to a stream
	    os << static_cast<uint16_t>(checksum);	// Checksum byte
	    os << std::endl;
	}
	os << ":00000001FF\n";			//EOF marker
    }

    // Make things pretty
    //  Truncate blocks to a given length as needed
    void hex_data::tidy(hex_data::size_type length)
    {
	for(iterator i=blocks.begin(); i!=blocks.end(); i++)
	{
	    if(i->second.size() > length)	//If the block is too long...
	    {
		//Make an interator that points to the first element to copy out of i->second
		data_container::iterator k(i->second.begin());
		advance(k, length);

		// Assign the extra elements to a new block and truncate the original
		blocks[i->first + length].assign(k, i->second.end());
		i->second.erase(k, i->second.end());
	    }
	}
    }

    //Compare two sets of hex data
    //	Return true if every word in hex1 has a corresponding, and equivalent, word in hex2
    bool compare(hex_data& hex1, hex_data& hex2, value_type mask, hex_data::address_t begin, hex_data::address_t end)
    {
	//Walk block list from hex1
	for( hex_data::iterator i = hex1.begin(); i != hex1.end(); ++i )
	{
	    //Walk the block
	    hex_data::address_t addr(i->first);
	    for( hex_data::data_container::iterator j = i->second.begin(); j != i->second.end(); ++j, ++addr)
	    {
		if( (addr < begin) || (addr > end) )
		    continue;

		//Compare both sides through the given mask
		if( ((*j) & mask) != (hex2.get(addr) & mask) )
		    return false;
	    }
	}
	return true;
    }

}
