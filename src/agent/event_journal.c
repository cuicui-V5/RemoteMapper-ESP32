#include "event_journal.h"

#include <assert.h>
#include <string.h>

void remote_event_journal_init(remote_event_journal_t *journal) {
    assert(journal);
    memset(journal, 0, sizeof(*journal));
    journal->next_sequence = 1;
}

void remote_event_journal_begin(remote_event_journal_t *journal) {
    assert(journal && journal->stream_id != UINT32_MAX);
    journal->head = 0;
    journal->count = 0;
    journal->dropped = 0;
    journal->acknowledged = journal->next_sequence - 1;
    journal->delivered = journal->acknowledged;
    journal->stream_id++;
    journal->active = true;
}

bool remote_event_journal_push(remote_event_journal_t *journal, uint64_t time_ms,
                               uint8_t key, bool pressed, remote_event_source_t source) {
    assert(journal);
    if (!journal->active) return true;
    assert(journal->next_sequence != UINT32_MAX);
    uint32_t sequence = journal->next_sequence++;
    if (journal->count == REMOTE_EVENT_CAPACITY) {
        assert(journal->dropped != UINT32_MAX);
        journal->dropped++;
        return false;
    }
    size_t index = (journal->head + journal->count) % REMOTE_EVENT_CAPACITY;
    journal->entries[index] = (remote_event_t){time_ms, sequence, key, pressed, source};
    journal->count++;
    return true;
}

size_t remote_event_journal_read(remote_event_journal_t *journal, remote_event_t *output,
                                 size_t capacity) {
    assert(journal && output && capacity);
    size_t count = journal->count < capacity ? journal->count : capacity;
    for (size_t index = 0; index < count; index++) {
        output[index] = journal->entries[(journal->head + index) % REMOTE_EVENT_CAPACITY];
    }
    if (count && output[count - 1].sequence > journal->delivered) {
        journal->delivered = output[count - 1].sequence;
    }
    return count;
}

bool remote_event_journal_ack(remote_event_journal_t *journal, uint32_t sequence) {
    assert(journal);
    if (sequence > journal->delivered || sequence < journal->acknowledged) return false;
    while (journal->count && journal->entries[journal->head].sequence <= sequence) {
        journal->head = (journal->head + 1) % REMOTE_EVENT_CAPACITY;
        journal->count--;
    }
    journal->acknowledged = sequence;
    return true;
}
