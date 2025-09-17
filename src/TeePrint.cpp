#include "TeePrint.h"

TeePrint::TeePrint(Print &_out1, Print &_out2) 
  : out1(_out1)
  , out2(_out2) {
}

size_t TeePrint::write(uint8_t b) {
  out1.write(b);
  out2.write(b);
  return 1;
}