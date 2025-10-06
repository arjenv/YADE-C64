

#include "main.h"
//#include <stdio.h>
//#include <stdlib.h>
//#include <stdbool.h>





void reset_RingBuffer(volatile struct RingBuffer *cb) {
    cb->head = 0;
    cb->tail = 0;
    cb->count = 0;
}

bool isFull(RingBuffer *cb) {
    return cb->count == BUFFER_SIZE;
}

bool isEmpty(RingBuffer *cb) {
    return cb->count == 0;
}

void push_ringbuffer(volatile RingBuffer *cb, uint8_t item) {
//    if (isFull(cb)) {
//        printf("Buffer is full! Overwriting oldest data.\n");
//        cb->tail = (cb->tail + 1) % BUFFER_SIZE; // Overwrite the oldest data
//    } else {
        cb->count++;
//    }
    cb->buffer[cb->head] = item;
    cb->head = (cb->head + 1) % BUFFER_SIZE;
}

void push_ringbuffer_twice(volatile RingBuffer *cb, uint8_t item) { // push it twice. User must check for enough space.
    cb->count++;
    cb->buffer[cb->head] = item;
    cb->head = (cb->head + 1) % BUFFER_SIZE;
    cb->count++;
    cb->buffer[cb->head] = item;
    cb->head = (cb->head + 1) % BUFFER_SIZE;    
}

uint8_t pull_ringbuffer(volatile RingBuffer *cb) {
//    if (isEmpty(cb)) {
//        printf("Buffer is empty! Cannot pull_ringbuffer.\n");
//        return -1; // Indicate that the buffer is empty
//    }
    uint8_t item = cb->buffer[cb->tail];
    cb->tail = (cb->tail + 1) % BUFFER_SIZE;
    cb->count--;
    return item;
}

void displayBuffer(volatile RingBuffer *cb) {
    printf("Buffer contents: ");
    for (int i = 0; i < cb->count; i++) {
        Serial.printf("%d ", cb->buffer[(cb->tail + i) % BUFFER_SIZE]);
    }
    Serial.println();
}
