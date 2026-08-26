// Custom Lua Executor for MiniWorld
// Function definitions and calling conventions for the custom executor
// 
// This header defines all the C function signatures and calling conventions
// used by the custom executor DLL. It provides the bridge between the
// Lua scripting interface and the underlying game functions resolved
// using AOB patterns from Cheat Engine.
// 
// Calling Convention:
// --------------
// All functions use stdcall (fastcall on x64) calling convention on Windows.
// Return values are typically pointers, integers, or booleans.
// String parameters are const char* (UTF-8 encoded).
// Float parameters use standard C float type.
// Integer parameters use standard C int type.
// Pointer parameters use void* for generic data.
// 
// Memory Management:
// ----------------
// - Return values that allocate memory (e.g., getPlayerInventory, getErrorCodes) 
//   use internal memory management. Callers should not free these unless
//   specified in the function documentation.
// - Functions that take output parameters (e.g., getPlayerPosition) expect
//   the caller to provide valid memory for the output.
// - String-returning functions return pointers to static buffers or
//   allocate memory that should be freed by the caller.
// 
// Thread Safety:
// ------------
// - Functions marked as thread-safe can be called from multiple threads
//   simultaneously.
// - Functions that modify game state are not thread-safe and should
//   only be called from the main thread.
// - Callback functions are called from the executor thread.
// 
// Error Handling:
// --------------
// - Functions return bool indicating success/failure where appropriate.
// - Error codes are returned via getLastError() for functions that fail.
// - Functions that throw exceptions use standard Windows exception handling.
// 
// Function Naming Convention:
// ---------------------------
// - Game functions: Direct names (getPlayerUID, giveItem, etc.)
// - Utility functions: Helper names (wait, calculateDistance, etc.)
// - Configuration functions: Config names (setConfigOption, getConfigOption)
// - Script functions: Script names (executeScript, loadScript, etc.)
// 
// Function Categories:
// -----------------
// 
// Player Management Functions:
// --------------------
// 
// int getPlayerUID()
//   Returns the current player's UID.
//   Returns: Player's unique identifier (32-bit integer)
//   Thread safety: Thread-safe
//   
// bool getPlayerPosition(float& x, float& y, float& z)
//   Gets the current player's position.
//   Parameters:
//     float& x - X coordinate output
//     float& y - Y coordinate output  
//     float& z - Z coordinate output
//   Returns: true if successful, false otherwise
//   Thread safety: Thread-safe
// 
// void setPlayerPosition(float x, float y, float z)
//   Sets the player's position.
//   Parameters:
//     float x - X coordinate
//     float y - Y coordinate
//     float z - Z coordinate
//   Thread safety: Not thread-safe (main thread only)
// 
// int getPlayerHealth()
//   Gets the current player's health.
//   Returns: Current health value (0-100)
//   Thread safety: Thread-safe
// 
// int getPlayerMaxHealth()
//   Gets the maximum health value.
//   Returns: Maximum health value
//   Thread safety: Thread-safe
// 
// int getPlayerHunger()
//   Gets the current player's hunger.
//   Returns: Current hunger value (0-100)
//   Thread safety: Thread-safe
// 
// int getPlayerMaxHunger()
//   Gets the maximum hunger value.
//   Returns: Maximum hunger value
//   Thread safety: Thread-safe
// 
// int getPlayerStamina()
//   Gets the current player's stamina.
//   Returns: Current stamina value (0-100)
//   Thread safety: Thread-safe
// 
// int getPlayerMaxStamina()
//   Gets the maximum stamina value.
//   Returns: Maximum stamina value
//   Thread safety: Thread-safe
// 
// float getPlayerWalkSpeed()
//   Gets the current player's walk speed multiplier.
//   Returns: Walk speed multiplier
//   Thread safety: Thread-safe
// 
// float getPlayerRunSpeed()
//   Gets the current player's run speed multiplier.
//   Returns: Run speed multiplier
//   Thread safety: Thread-safe
// 
// float getPlayerJumpHeight()
//   Gets the current player's jump height.
//   Returns: Jump height value
//   Thread safety: Thread-safe
// 
// int getPlayerSkinID()
//   Gets the current player's skin ID.
//   Returns: Current skin ID
//   Thread safety: Thread-safe
// 
// void setPlayerSkinID(int skinID)
//   Sets the player's skin ID.
//   Parameters:
//     int skinID - Skin ID to set
//   Thread safety: Not thread-safe (main thread only)
// 
// int getPlayerEmotion()
//   Gets the current player's emotion.
//   Returns: Current emotion ID
//   Thread safety: Thread-safe
// 
// void setPlayerEmotion(int emotion)
//   Sets the player's emotion.
//   Parameters:
//     int emotion - Emotion ID to set
//   Thread safety: Not thread-safe (main thread only)
// 
// Item Management Functions:
// --------------------
// 
// bool giveItem(int targetUID, int itemID, int quantity)
//   Gives items to a player.
//   Parameters:
//     int targetUID - Target player's UID
//     int itemID - Item ID to give
//     int quantity - Quantity to give
//   Returns: true if successful, false otherwise
//   Thread safety: Not thread-safe (main thread only)
// 
// bool throwItem(int targetUID, int slotID, int quantity)
//   Throws items from a player's inventory.
//   Parameters:
//     int targetUID - Target player's UID
//     int slotID - Slot ID to throw from
//     int quantity - Quantity to throw
//   Returns: true if successful, false otherwise
//   Thread safety: Not thread-safe (main thread only)
// 
// int getHeldItemID()
//   Gets the ID of the currently held item.
//   Returns: Held item ID
//   Thread safety: Thread-safe
// 
// int getHeldItemSlot()
//   Gets the slot of the currently held item.
//   Returns: Held item slot ID
//   Thread safety: Thread-safe
// 
// bool getPlayerInventory(int** inventory, int* size)
//   Gets the player's inventory.
//   Parameters:
//     int** inventory - Output array pointer
//     int* size - Output array size
//   Returns: true if successful, false otherwise
//   Thread safety: Thread-safe
//   Note: Caller is responsible for freeing the inventory array
// 
// void freeInventory(int* inventory)
//   Frees memory allocated for inventory array.
//   Parameters:
//     int* inventory - Inventory array to free
//   Thread safety: Thread-safe
// 
// Teleportation Functions:
// ----------------
// 
// bool teleportPlayer(float x, float y, float z)
//   Teleports the local player to specified coordinates.
//   Parameters:
//     float x - X coordinate
//     float y - Y coordinate
//     float z - Z coordinate
//   Returns: true if successful, false otherwise
//   Thread safety: Not thread-safe (main thread only)
// 
// bool teleportToPlayer(int targetUID)
//   Teleports local player to another player.
//   Parameters:
//     int targetUID - Target player's UID
//   Returns: true if successful, false otherwise
//   Thread safety: Not thread-safe (main thread only)
// 
// bool getNearestPlayer(int& targetUID, float& distance)
//   Gets the nearest player.
//   Parameters:
//     int& targetUID - Output target player UID
//     float& distance - Output distance
//   Returns: true if successful, false otherwise (no other players)
//   Thread safety: Thread-safe
// 
// bool getPlayerPositionByUID(int targetUID, float& x, float& y, float& z)
//   Gets player position by UID.
//   Parameters:
//     int targetUID - Target player's UID
//     float& x - Output X coordinate
//     float& y - Output Y coordinate
//     float& z - Output Z coordinate
//   Returns: true if successful, false otherwise
//   Thread safety: Thread-safe
// 
// Chat & Communication Functions:
// ----------------------------
// 
// bool sendChatMessage(const char* message)
//   Sends a global chat message.
//   Parameters:
//     const char* message - Message to send
//   Returns: true if successful, false otherwise
//   Thread safety: Not thread-safe (main thread only)
// 
// bool sendPrivateMessage(int targetUID, const char* message)
//   Sends a private message to another player.
//   Parameters:
//     int targetUID - Target player's UID
//     const char* message - Message to send
//   Returns: true if successful, false otherwise
//   Thread safety: Not thread-safe (main thread only)
// 
// bool listenForChatMessages()
//   Starts listening for chat messages.
//   Returns: true if successful, false otherwise
//   Thread safety: Thread-safe
// 
// void stopListeningForChatMessages()
//   Stops listening for chat messages.
//   Thread safety: Thread-safe
// 
// World & Game State Functions:
// ---------------------------
// 
// int getWorldID()
//   Gets the current world ID.
//   Returns: Current world ID
//   Thread safety: Thread-safe
// 
// int getServerTime()
//   Gets the server time.
//   Returns: Server time
//   Thread safety: Thread-safe
// 
// float calculateDistance(float x1, float y1, float z1, float x2, float y2, float z2)
//   Calculates distance between two points.
//   Parameters:
//     float x1, y1, z1 - First point coordinates
//     float x2, y2, z2 - Second point coordinates
//   Returns: Distance between points
//   Thread safety: Thread-safe
// 
// bool getPlayersInRange(float range, int** playerUIDs, float** distances, int* count)
//   Gets players within specified range.
//   Parameters:
//     float range - Search range
//     int** playerUIDs - Output array of player UIDs
//     float** distances - Output array of distances
//     int* count - Output count of players found
//   Returns: true if successful, false otherwise
//   Thread safety: Thread-safe
//   Note: Caller is responsible for freeing arrays
// 
// const char* getErrorCodes()
//   Gets available error codes.
//   Returns: JSON string of error codes
//   Thread safety: Thread-safe
// 
// Utility & Helper Functions:
// ---------------------
// 
// bool executeScript(const char* script)
//   Executes a Lua script.
//   Parameters:
//     const char* script - Lua script to execute
//   Returns: true if successful, false otherwise
//   Thread safety: Not thread-safe (main thread only)
// 
// bool saveScript(const char* filename, const char* script)
//   Saves a script to file.
//   Parameters:
//     const char* filename - Filename
//     const char* script - Script content
//   Returns: true if successful, false otherwise
//   Thread safety: Thread-safe
// 
// bool loadScript(const char* filename, char** script, int* size)
//   Loads a script from file.
//   Parameters:
//     const char* filename - Filename
//     char** script - Output script pointer
//     int* size - Output script size
//   Returns: true if successful, false otherwise
//   Thread safety: Thread-safe
//   Note: Caller is responsible for freeing script memory
// 
// bool exportDataToFile(const char* filename, const char* data)
//   Exports data to file.
//   Parameters:
//     const char* filename - Filename
//     const char* data - Data to export
//   Returns: true if successful, false otherwise
//   Thread safety: Thread-safe
// 
// bool importDataFromFile(const char* filename, char** data, int* size)
//   Imports data from file.
//   Parameters:
//     const char* filename - Filename
//     char** data - Output data pointer
//     int* size - Output data size
//   Returns: true if successful, false otherwise
//   Thread safety: Thread-safe
//   Note: Caller is responsible for freeing data memory
// 
// void wait(int milliseconds)
//   Waits for specified milliseconds.
//   Parameters:
//     int milliseconds - Milliseconds to wait
//   Thread safety: Thread-safe
// 
// const char* getLastError()
//   Gets last error message.
//   Returns: Error message string
//   Thread safety: Thread-safe
// 
// void clearLastError()
//   Clears last error.
//   Thread safety: Thread-safe
// 
// Remote Function Calls:
// -----------------
// 
// bool remoteExecute(const char* functionName, const char* parameters)
//   Executes a remote function.
//   Parameters:
//     const char* functionName - Function name
//     const char* parameters - Parameters
//   Returns: true if successful, false otherwise
//   Thread safety: Not thread-safe (main thread only)
// 
// bool remoteFunction(const char* functionName, const char* parameters)
//   Alternative remote function call.
//   Parameters:
//     const char* functionName - Function name
//     const char* parameters - Parameters
//   Returns: true if successful, false otherwise
//   Thread safety: Not thread-safe (main thread only)
// 
// Configuration & Settings Functions:
// -------------------------------
// 
// bool setConfigOption(const char* option, const char* value)
//   Sets a configuration option.
//   Parameters:
//     const char* option - Option name
//     const char* value - Option value
//   Returns: true if successful, false otherwise
//   Thread safety: Thread-safe
// 
// const char* getConfigOption(const char* option)
//   Gets a configuration option.
//   Parameters:
//     const char* option - Option name
//   Returns: Option value
//   Thread safety: Thread-safe
// 
// bool resetConfigToDefaults()
//   Resets configuration to defaults.
//   Returns: true if successful, false otherwise
//   Thread safety: Thread-safe
// 
// Script Management Functions:
// -----------------------
// 
// bool createScriptFile(const char* filename, const char* content)
//   Creates a new script file.
//   Parameters:
//     const char* filename - Filename
//     const char* content - File content
//   Returns: true if successful, false otherwise
//   Thread safety: Thread-safe
// 
// bool deleteScriptFile(const char* filename)
//   Deletes a script file.
//   Parameters:
//     const char* filename - Filename
//   Returns: true if successful, false otherwise
//   Thread safety: Thread-safe
// 
// bool listScriptFiles(char** filenames, int* count)
//   Lists available script files.
//   Parameters:
//     char** filenames - Output array of filenames
//     int* count - Output count of files
//   Returns: true if successful, false otherwise
//   Thread safety: Thread-safe
//   Note: Caller is responsible for freeing filenames array
// 
// bool executeScriptFromFile(const char* filename)
//   Executes a script from file.
//   Parameters:
//     const char* filename - Filename
//   Returns: true if successful, false otherwise
//   Thread safety: Not thread-safe (main thread only)
// 
// Event Handling Functions:
// -------------------
// 
// bool setScriptCallback(const char* eventType, const char* callbackScript)
//   Sets a script callback for events.
//   Parameters:
//     const char* eventType - Event type
//     const char* callbackScript - Callback script
//   Returns: true if successful, false otherwise
//   Thread safety: Thread-safe
// 
// bool removeScriptCallback(const char* eventType)
//   Removes a script callback.
//   Parameters:
//     const char* eventType - Event type
//   Returns: true if successful, false otherwise
//   Thread safety: Thread-safe
// 
// bool triggerScriptCallback(const char* eventType, const char* parameters)
//   Triggers a script callback.
//   Parameters:
//     const char* eventType - Event type
//     const char* parameters - Callback parameters
//   Returns: true if successful, false otherwise
//   Thread safety: Thread-safe
// 
// This header provides a comprehensive API for the custom Lua executor
// that gives users complete control over MiniWorld through Lua scripting.
// The functions are designed to be intuitive and easy to use while
// providing comprehensive control over the game.
