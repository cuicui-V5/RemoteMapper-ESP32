#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define REMOTE_EVENT_CAPACITY 128
#define REMOTE_EVENT_BATCH 16

typedef enum {
    REMOTE_EVENT_HOGP = 1,
    REMOTE_EVENT_ATVV = 2,
    REMOTE_EVENT_INFERRED_RELEASE = 3,
    REMOTE_EVENT_DISCONNECT = 4
} remote_event_source_t;

typedef struct {
    uint64_t time_ms;
    uint32_t sequence;
    uint8_t key;
    uint8_t pressed;
    uint8_t source;
} remote_event_t;

typedef struct {
    remote_event_t entries[REMOTE_EVENT_CAPACITY];
    size_t head;
    size_t count;
    uint32_t next_sequence;
    uint32_t acknowledged;
    uint32_t delivered;
    uint32_t dropped;
    uint32_t stream_id;
    bool active;
} remote_event_journal_t;

void remote_event_journal_init(remote_event_journal_t *journal);
void remote_event_journal_begin(remote_event_journal_t *journal);
bool remote_event_journal_push(remote_event_journal_t *journal, uint64_t time_ms,
                               uint8_t key, bool pressed, remote_event_source_t source);
size_t remote_event_journal_read(remote_event_journal_t *journal, remote_event_t *output,
                                 size_t capacity);
bool remote_event_journal_ack(remote_event_journal_t *journal, uint32_t sequence);

#ifdef __cplusplus
}
#endif
