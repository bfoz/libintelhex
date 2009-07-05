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

    //Extend the data block array by one element
    //	and return a pointer to the new element
    hex_data::dblock* hex_data::new_block()
    {
	dblock b;
	blocks.push_back(b);
	return &blocks.back();
    }

    // Extend the data block array by one element
    //	and return a pointer to the new element
    //	Initialize the element with address and length
    hex_data::dblock* hex_data::add_block(address_t address, size_type length, element_t fill)
    {
	dblock db;	//A list of pointers would be faster, but this isn't too bad
	blocks.push_back(db);
	blocks.back().first = address;
	blocks.back().second.resize(length, fill);
	return &blocks.back();
    }

    // Array access operator
    // Assumes that the blocks have been sorted by address in ascending order
    //	Sort order is maintained
    hex_data::element_t &hex_data::operator[](hex_data::address_t addr)
    {
	if(blocks.size() == 0)	//Add a block if the sequence is empty
	    add_block(0,0);

	// Start at the end of the list and find the first (last) block with an address
	//  less than addr
	reverse_iterator i = blocks.rbegin();
	while( (i!=blocks.rend()) && (i->first > addr))
	    ++i;

	address_t relative_addr = addr - i->first;
	// If addr is outside of a block and not-adjacent to the end of any block add a new block
	if( relative_addr > i->second.size() )
	    return add_block(addr,1)->second[0];
	// If addr is adjacent to the end of the block resize it
	if( relative_addr == i->second.size() )
	    i->second.resize(i->second.size()+1, 0xFFFF);

	return i->second[relative_addr];
    }

    // FIXME Nasty kludge
    //  I should really create an iterator class to handle this
    hex_data::element_t hex_data::get(address_t addr, element_t blank)
    {
	// Start at the end of the list and find the first (last) block with an address
	//  less than addr
	reverse_iterator i = blocks.rbegin();
	while( (i!=blocks.rend()) && (i->first > addr))
	    ++i;

	// If no block can be found, return the blank value
	if( i == blocks.rend() )
	    return blank;

	element_t relative_addr = addr - i->first;
	// If relative_addr is past the end of the block, return blank
	if( relative_addr >= i->second.size() )
	    return blank;

	return i->second[relative_addr];
    }

    // Delete all allocated memory
    void hex_data::clear()
    {
	format = HEX_FORMAT_INHX8M;
	blocks.clear();
    }

    // Add a new word to the end of the sequence
    //  Assumes the sequence has been sorted
    void hex_data::push_back(hex_data::element_t a)
    {
	if(blocks.size() == 0)	//Add a block if the sequence is empty
	    add_block(0,0);
	blocks.back().second.push_back(a);	//Append the new word
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
	dblock	*db;		//Temporary pointer
	unsigned int	hi, lo, address, count, rtype, i;
	uint16_t	linear_address(0);
	uint32_t	a;

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

		unsigned numWords(count/2);	//Convert byte count to word count
		address /= 2;			//Byte address to word address

		switch(rtype)	//What type of record?
		{
		    case 0: 	//Data block so store it
			//Make a data block
			a = (static_cast<uint32_t>(linear_address) << 16) + address;
			db = add_block(a, numWords);
			for(i=0; i<numWords; i++)			//Read all of the data bytes
			{
				fscanf(fp, "%2x", &lo);			//Low byte
				fscanf(fp, "%2x", &hi);			//High byte
				db->second[i] = ((hi<<8)&0xFF00) | (lo&0x00FF);	//Assemble the word
			}
			break;
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

	blocks.sort();		//Sort the data blocks by address (ascending)
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

	truncate(8);				//Truncate each record to length=8 (purely aesthetic)
	blocks.sort();				//Sort the data blocks by address (ascending)

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
	    os << i->second.size()*2;	//Record length
	    checksum += i->second.size()*2;
	    os.width(4);
	    os << static_cast<uint16_t>(i->first*2);	//Address
	    checksum += (i->first * 2) + (i->first >> 7);
	    os << "00";											//Record type
	    for(unsigned j=0; j<i->second.size(); ++j)	//Store the data bytes, LSB first, ASCII HEX
	    {
		os.width(2);
		os << (i->second[j] & 0x00FF);
		os.width(2);
		os << ((i->second[j]>>8) & 0x00FF);
		checksum += static_cast<uint8_t>(i->second[j] & 0x00FF);
		checksum += static_cast<uint8_t>(i->second[j] >> 8);
	    }
	    checksum = 0x01 + ~checksum;
	    os.width(2);
	    // OSX (or maybe GCC), seems unable to handle uint8_t arguments to a stream
	    os << static_cast<uint16_t>(checksum);	// Checksum byte
	    os << std::endl;
	}
	os << ":00000001FF\n";			//EOF marker
    }

    // Truncate all of the blocks to a given length
    //  Maintains sort order
    void hex_data::truncate(hex_data::size_type len)
    {
	for(iterator i=blocks.begin(); i!=blocks.end(); i++)
	{
	    if(i->second.size() > len)	//If the block is too long...
	    {
		//Insert a new block
		iterator j(i);
		j = blocks.insert(++j, dblock());
		j->first = i->first + len;		//Give the new block an address

		//Make an interator that points to the first element to copy out of i->second
		dblock::second_type::iterator k(i->second.begin());
		advance(k, len);

		j->second.assign(k, i->second.end());	//Assign the extra bytes to the new block
		i->second.erase(k, i->second.end());	//Truncate the original block
	    }
	}
    }

    //Compare two sets of hex data
    //	Return true if every word in hex1 has a corresponding, and equivalent, word in hex2
    //	Assumes both data sets are sorted
    bool compare(hex_data& hex1, hex_data& hex2, hex_data::element_t mask, hex_data::address_t begin, hex_data::address_t end)
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

		// Look for a corresponding element in hex2
		const hex_data::element_t rhs = hex2.get(addr, mask);
		//Compare both sides through the given mask
		if( ((*j) & mask) != (rhs & mask) )
		    return false;
	    }
	}
	return true;
    }

}
