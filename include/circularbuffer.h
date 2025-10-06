#ifndef CIRCULARBUFFER_H
#define CIRCULARBUFFER_H


#define BUFFER_SIZE 1024 // when defined >255 adapt types head, tail, count etc


struct RingBuffer {
    uint32_t buffer[BUFFER_SIZE];
    uint16_t head;
    uint16_t tail;
    uint16_t count;
};
#endif