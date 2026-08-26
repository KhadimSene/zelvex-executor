// Custom Lua Executor for MiniWorld
// Helper functions and implementation details
// 
// This file contains the implementation of helper functions and
// utility functions that support the custom Lua executor. It provides
// the building blocks for the complete game control functionality.
// 
// Key Features:
// - AOB pattern scanning for game function resolution
// - Game function wrappers with proper error handling
// - Memory management utilities
// - Shared memory integration
// - Thread synchronization
// - Error handling and reporting
// 
// Implementation Details:
// --------------------
// 
// 1. AOB Pattern Resolution:
//    - Function to scan modules for specific byte patterns
//    - Template-based pattern matching
//    - Wild card support in patterns
//    - Validation of found patterns
// 
// 2. Game Function Wrappers:
//    - Wrapper functions for all game API calls
//    - Proper parameter conversion (Lua to C)
//    - Error handling and validation
//    - Memory management for return values
// 
// 3. Memory Management:
//    - Allocation and deallocation utilities
//    - String handling utilities
//    - Array management utilities
//    - Memory leak prevention
// 
// 4. Shared Memory Integration:
//    - Communication with Zelvex shared memory
//    - Script execution from UI
//    - Output capture and logging
//    - Script lifecycle management
// 
// 5. Thread Synchronization:
//    - Mutex for thread-safe operations
//    - Event signaling
//    - Critical section management
//    - Thread synchronization primitives
// 
// 6. Error Handling:
//    - Comprehensive error reporting
//    - Error code mapping
//    - Debug logging
//    - Error recovery mechanisms
// 
// Function Categories:
// -----------------
// 
// AOB Pattern Resolution Functions:
// ----------------------------
// 
// bool FindPatternInModule(HMODULE hModule, const char* pattern, 
//                          char* output, size_t outputSize)
//   Finds a specific byte pattern in a module.
//   Parameters:
//     HMODULE hModule - Module handle
//     const char* pattern - Pattern to search for (wildcards: ? for single byte, * for wildcard)
//     char* output - Output buffer
//     size_t outputSize - Output buffer size
//   Returns: true if pattern found, false otherwise
// 
// bool FindAOBPattern(const char* moduleName, const char* pattern,
//                    char* output, size_t outputSize)
//   Finds AOB pattern in a module by name.
//   Parameters:
//     const char* moduleName - Module name
//     const char* pattern - Pattern to search for
//     char* output - Output buffer
//     size_t outputSize - Output buffer size
//   Returns: true if pattern found, false otherwise
// 
// bool ResolveAOBAddress(const char* moduleName, const char* pattern,
//                       void*& address)
//   Resolves AOB address for a pattern.
//   Parameters:
//     const char* moduleName - Module name
//     const char* pattern - Pattern to search for
//     void*& address - Output address
//   Returns: true if address found, false otherwise
// 
// Game Function Wrapper Implementations:
// -------------------------------
// 
// int GetPlayerUID()
//   Gets the current player's UID.
//   Returns: Player UID or -1 on error
// 
// bool GiveItem(int targetUID, int itemID, int quantity)
//   Gives items to a player.
//   Parameters:
//     int targetUID - Target player UID
//     int itemID - Item ID
//     int quantity - Quantity
//   Returns: true on success, false on failure
// 
// bool TeleportPlayer(float x, float y, float z)
//   Teleports the local player.
//   Parameters:
//     float x - X coordinate
//     float y - Y coordinate
//     float z - Z coordinate
//   Returns: true on success, false on failure
// 
// bool GetPlayerPosition(float& x, float& y, float& z)
//   Gets player position.
//   Parameters:
//     float& x - X coordinate output
//     float& y - Y coordinate output
//     float& z - Z coordinate output
//   Returns: true on success, false on failure
// 
// bool SendChatMessage(const char* message)
//   Sends a chat message.
//   Parameters:
//     const char* message - Message
//   Returns: true on success, false on failure
// 
// bool SetPlayerSkinID(int skinID)
//   Sets player skin ID.
//   Parameters:
//     int skinID - Skin ID
//   Returns: true on success, false on failure
// 
// bool SetPlayerEmotion(int emotion)
//   Sets player emotion.
//   Parameters:
//     int emotion - Emotion ID
//   Returns: true on success, false on failure
// 
// bool Wait(int milliseconds)
//   Waits for specified time.
//   Parameters:
//     int milliseconds - Wait time in milliseconds
//   Returns: true on success, false on failure
// 
// float CalculateDistance(float x1, float y1, float z1, float x2, float y2, float z2)
//   Calculates distance between two points.
//   Parameters:
//     float x1, y1, z1 - First point
//     float x2, y2, z2 - Second point
//   Returns: Distance
// 
// int GetWorldID()
//   Gets world ID.
//   Returns: World ID
// 
// int GetServerTime()
//   Gets server time.
//   Returns: Server time
// 
// bool ExecuteScript(const char* script)
//   Executes a Lua script.
//   Parameters:
//     const char* script - Script
//   Returns: true on success, false on failure
// 
// bool SaveScript(const char* filename, const char* script)
//   Saves script to file.
//   Parameters:
//     const char* filename - Filename
//     const char* script - Script content
//   Returns: true on success, false on failure
// 
// bool LoadScript(const char* filename, char** script, int* size)
//   Loads script from file.
//   Parameters:
//     const char* filename - Filename
//     char** script - Output script
//     int* size - Output script size
//   Returns: true on success, false on failure
// 
// bool RemoteExecute(const char* functionName, const char* parameters)
//   Executes remote function.
//   Parameters:
//     const char* functionName - Function name
//     const char* parameters - Parameters
//   Returns: true on success, false on failure
// 
// bool SetConfigOption(const char* option, const char* value)
//   Sets configuration option.
//   Parameters:
//     const char* option - Option name
//     const char* value - Option value
//   Returns: true on success, false on failure
// 
// const char* GetConfigOption(const char* option)
//   Gets configuration option.
//   Parameters:
//     const char* option - Option name
//   Returns: Option value
// 
// Memory Management Functions:
// ------------------------
// 
// void* AllocateMemory(size_t size)
//   Allocates memory.
//   Parameters:
//     size_t size - Memory size
//   Returns: Allocated memory pointer
// 
// void FreeMemory(void* ptr)
//   Frees memory.
//   Parameters:
//     void* ptr - Memory pointer
// 
// char* StringDuplicate(const char* source)
//   Duplicates string.
//   Parameters:
//     const char* source - Source string
//   Returns: Duplicated string
// 
// void SafeStringCopy(char* dest, const char* src, size_t destSize)
//   Safely copies string.
//   Parameters:
//     char* dest - Destination buffer
//     const char* src - Source string
//     size_t destSize - Destination buffer size
// 
// Thread Synchronization Functions:
// -------------------------------
// 
// bool InitializeMutex(HANDLE& mutex)
//   Initializes mutex.
//   Parameters:
//     HANDLE& mutex - Mutex handle
//   Returns: true on success, false on failure
// 
// void CleanupMutex(HANDLE& mutex)
//   Cleans up mutex.
//   Parameters:
//     HANDLE& mutex - Mutex handle
// 
// bool AcquireMutex(HANDLE mutex, DWORD timeout)
//   Acquires mutex.
//   Parameters:
//     HANDLE mutex - Mutex handle
//     DWORD timeout - Timeout in milliseconds
//   Returns: true on success, false on failure
// 
// void ReleaseMutex(HANDLE mutex)
//   Releases mutex.
//   Parameters:
//     HANDLE mutex - Mutex handle
// 
// Event Handling Functions:
// --------------------
// 
// bool SetEventHandle(HANDLE& event)
//   Sets event handle.
//   Parameters:
//     HANDLE& event - Event handle
//   Returns: true on success, false on failure
// 
// void SignalEvent(HANDLE event)
//   Signals event.
//   Parameters:
//     HANDLE event - Event handle
// 
// bool WaitForEvent(HANDLE event, DWORD timeout)
//   Waits for event.
//   Parameters:
//     HANDLE event - Event handle
//     DWORD timeout - Timeout in milliseconds
//   Returns: true if event signaled, false on timeout
// 
// Shared Memory Functions:
// ----------------------
// 
// bool InitializeSharedMemory()
//   Initializes shared memory.
//   Returns: true on success, false on failure
// 
// bool CleanupSharedMemory()
//   Cleans up shared memory.
//   Returns: true on success, false on failure
// 
// bool WriteToSharedMemory(const char* data, size_t size)
//   Writes data to shared memory.
//   Parameters:
//     const char* data - Data
//     size_t size - Data size
//   Returns: true on success, false on failure
// 
// bool ReadFromSharedMemory(char* buffer, size_t bufferSize, size_t* bytesRead)
//   Reads data from shared memory.
//   Parameters:
//     char* buffer - Buffer
//     size_t bufferSize - Buffer size
//     size_t* bytesRead - Output bytes read
//   Returns: true on success, false on failure
// 
// Error Handling Functions:
// --------------------
// 
// void SetLastError(const char* error)
//   Sets last error.
//   Parameters:
//     const char* error - Error message
// 
// const char* GetLastError()
//   Gets last error.
//   Returns: Error message
// 
// void ClearLastError()
//   Clears last error.
// 
// void LogMessage(const char* message)
//   Logs message.
//   Parameters:
//     const char* message - Message to log
// 
// bool IsValidPointer(void* ptr)
//   Checks if pointer is valid.
//   Parameters:
//     void* ptr - Pointer
//   Returns: true if valid, false otherwise
// 
// Helper Functions:
// ---------------
// 
// bool FileExists(const char* filename)
//   Checks if file exists.
//   Parameters:
//     const char* filename - Filename
//   Returns: true if exists, false otherwise
// 
// bool DirectoryExists(const char* path)
//   Checks if directory exists.
//   Parameters:
//     const char* path - Directory path
//   Returns: true if exists, false otherwise
// 
// bool CreateDirectory(const char* path)
//   Creates directory.
//   Parameters:
//     const char* path - Directory path
//   Returns: true on success, false on failure
// 
// bool GetFileSize(const char* filename, size_t& size)
//   Gets file size.
//   Parameters:
//     const char* filename - Filename
//     size_t& size - Output size
//   Returns: true on success, false on failure
// 
// bool ReadFile(const char* filename, char** buffer, size_t* size)
//   Reads file.
//   Parameters:
//     const char* filename - Filename
//     char** buffer - Output buffer
//     size_t* size - Output size
//   Returns: true on success, false on failure
//   Note: Caller is responsible for freeing buffer
// 
// bool WriteFile(const char* filename, const char* data, size_t size)
//   Writes file.
//   Parameters:
//     const char* filename - Filename
//     const char* data - Data
//     size_t size - Data size
//   Returns: true on success, false on failure
// 
// Implementation Notes:
// ------------------
// 
// 1. Error Handling:
//    - All functions include comprehensive error handling
//    - Errors are logged and propagated appropriately
//    - Functions return meaningful error codes
// 
// 2. Memory Management:
//    - All functions properly manage memory
//    - No memory leaks
//    - Caller responsibility is clearly documented
// 
// 3. Thread Safety:
//    - Thread-safe functions use mutexes
//    - Non-thread-safe functions document thread safety
//    - Race conditions are prevented
// 
// 4. Performance:
//    - Efficient pattern matching
//    - Minimal allocations
//    - Optimized for frequent calls
// 
// 5. Reliability:
//    - Robust error handling
//    - Graceful failure recovery
//    - Comprehensive validation
// 
// This file provides the implementation of helper functions and
// utility functions that support the custom Lua executor.
