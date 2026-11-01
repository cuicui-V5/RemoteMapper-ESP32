#include "agent/event_journal.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    remote_event_journal_t journal;
    remote_event_t events[REMOTE_EVENT_BATCH];
    remote_event_journal_init(&journal);
    assert(remote_event_journal_push(&journal, 10, 40, true, REMOTE_EVENT_HOGP));
    assert(journal.count == 0);
    remote_event_journal_begin(&journal);
    assert(journal.stream_id == 1);
    for (size_t index = 0; index < REMOTE_EVENT_CAPACITY; index++) {
        assert(remote_event_journal_push(&journal, 100 + index, 40, index % 2, REMOTE_EVENT_HOGP));
    }
    assert(!remote_event_journal_push(&journal, 300, 40, true, REMOTE_EVENT_HOGP));
    assert(journal.dropped == 1);
    assert(!remote_event_journal_ack(&journal, 1));
    assert(remote_event_journal_read(&journal, events, REMOTE_EVENT_BATCH) == REMOTE_EVENT_BATCH);
    for (size_t index = 0; index < REMOTE_EVENT_BATCH; index++) {
        assert(events[index].sequence == index + 1);
        assert(events[index].time_ms == index + 100);
    }
    assert(remote_event_journal_read(&journal, events, 1) == 1);
    assert(journal.delivered == REMOTE_EVENT_BATCH);
    assert(!remote_event_journal_ack(&journal, REMOTE_EVENT_BATCH + 1));
    assert(remote_event_journal_ack(&journal, REMOTE_EVENT_BATCH));
    assert(remote_event_journal_ack(&journal, REMOTE_EVENT_BATCH));
    assert(!remote_event_journal_ack(&journal, REMOTE_EVENT_BATCH - 1));
    assert(journal.count == REMOTE_EVENT_CAPACITY - REMOTE_EVENT_BATCH);
    for (size_t index = 0; index < REMOTE_EVENT_BATCH; index++) {
        assert(remote_event_journal_push(&journal, 400 + index, 40, index % 2, REMOTE_EVENT_HOGP));
    }
    uint32_t previous = REMOTE_EVENT_BATCH;
    while (journal.count) {
        size_t count = remote_event_journal_read(&journal, events, REMOTE_EVENT_BATCH);
        for (size_t index = 0; index < count; index++) {
            assert(events[index].sequence > previous);
            previous = events[index].sequence;
        }
        assert(remote_event_journal_ack(&journal, previous));
    }
    assert(journal.dropped == 1);
    remote_event_journal_begin(&journal);
    assert(journal.stream_id == 2);
    assert(journal.dropped == 0);
    assert(journal.acknowledged == previous);
    assert(remote_event_journal_push(&journal, UINT64_C(4294967300), 0, false, REMOTE_EVENT_DISCONNECT));
    assert(remote_event_journal_read(&journal, events, REMOTE_EVENT_BATCH) == 1);
    assert(events[0].sequence == previous + 1);
    assert(events[0].time_ms == UINT64_C(4294967300));
    assert(events[0].source == REMOTE_EVENT_DISCONNECT);
    puts("event_journal: ordering, acknowledgement, overflow, wraparound and 64-bit time passed");
    return 0;
}
