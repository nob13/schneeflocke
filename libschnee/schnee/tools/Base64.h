
//*********************************************************************
//* C_Base64 - a simple base64 encoder and decoder.
//*
//*     Copyright (c) 1999, Bob Withers - bwit@pobox.com
//*
//* This code may be freely used for any purpose, either personal
//* or commercial, provided the authors copyright notice remains
//* intact.
//*********************************************************************

/*
 * libschnee notice: copied from Google Talk, seems public domain as it is used
 * everywhere through the net.
 * 
 * Modificated for use with ByteArray
 */

#pragma once

///@cond DEV

#include <string>
#include <schnee/sftypes.h>

namespace sf {

/**
 * Base64 encoder by Bob Withers, freeware.
 * 
 * With some enhancements of Stanley Yamane and Norbert Schultz.
 */
class Base64
{
public:
  static std::string encode(const std::string & data);
  static std::string decode(const std::string & data);
  static void decodeToArray (const std::string & data, sf::ByteArray & ret);
  static std::string encodeFromArray(const char * data, size_t len);
  static std::string encodeFromArray(const ByteArray & arr);
private:
  static const std::string Base64Table;
  static const std::string::size_type DecodeTable[];
};

} // namespace talk_base

/// @endcond DEV
