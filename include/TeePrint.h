#ifndef _TEE_PRINT_
#define _TEE_PRINT_

#include <Print.h>

class TeePrint : public Print {
  private:
    Print &out1;
    Print &out2;
  public:
    TeePrint(Print &_out1, Print &_out2);
    virtual size_t write(uint8_t b);
};

extern TeePrint tee;

#endif
