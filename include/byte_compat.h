#pragma once

// IceOps defines `byte` as unsigned char.
// GCC 14 defines std::byte.
// We rename IceOps' byte to ice_byte to avoid ambiguity.

#define byte ice_byte
typedef unsigned char ice_byte;
