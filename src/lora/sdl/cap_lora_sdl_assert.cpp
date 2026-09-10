// The Pigweed Result type is part of the public CapLoRa1262 API.  SDL never
// dereferences an unsuccessful result, but its defensive accessors still
// reference Pigweed's trap hooks.  Keep the desktop build self-contained with
// no-op hooks; hardware builds link the real pw_assert_trap implementation.
extern "C" void pw_assert_trap_interrupt_lock(void) {}
extern "C" void pw_assert_trap_interrupt_unlock(void) {}
extern "C" void pw_assert_trap_HandleAssertFailure(const char*, int, const char*) {}
extern "C" void pw_assert_trap_HandleCheckFailure(const char*, int, const char*, const char*, ...) {}
