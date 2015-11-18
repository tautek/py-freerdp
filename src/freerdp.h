typedef unsigned long DWORD;

/**
 * Callback with instance pointer and no arguments.
 */
typedef void (*instance_callback_t)(void* instance);

/**
 * Start a session with the given command line arguments.
 */
void* start(int argc, char* argv[], instance_callback_t onConnect);

/**
 * Run a command in the session.
 */
void run_command(void* instance, char* command);

/**
 * Press key combinations. Expects RDP scancodes from freerdp/scancode.h
 */
void press_keys(void* void_instance, int count, DWORD* codes);

/**
 * Stop the session and disconnect.
 */
void stop(void* instance);

/**
 * Close all connections and shut down the client.
 */
void destroy(int ms_timeout);

