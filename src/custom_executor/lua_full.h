// Custom Lua Executor for MiniWorld
// C API for integration with Zelvex
// 
// This header defines the functions that the custom executor DLL exports
// for use by Zelvex. It provides a unified interface for Lua execution
// that gives users complete control over the game.
// 
// Exported Functions:
// -----------------
// 
// InitializeCustomExecutor() - Initialize the custom executor DLL
//    Returns: true if successful, false otherwise
// 
// ShutdownCustomExecutor() - Clean shutdown of the executor
//    Returns: nothing
// 
// ExecuteLuaScript(script) - Execute a Lua script through the custom executor
//    Parameters: 
//      const char* script - Lua script to execute
//    Returns:
//      const char* - Output/return value from the script
//      NULL - if execution failed
// 
// GetCustomExecutorStatus() - Get the current status of the custom executor
//    Returns:
//      const char* - Status string ("Ready", "Initializing", "Error", etc.)
// 
// IsCustomExecutorActive() - Check if the custom executor is active
//    Returns:
//      bool - true if active, false otherwise
// 
// ForceCustomExecutorRestart() - Force a restart of the custom executor
//    Returns:
//      bool - true if restart initiated, false if already running
// 
// GetCustomExecutorVersion() - Get version information for the custom executor
//    Returns:
//      const char* - Version string (e.g., "1.0.0")
// 
// GetAvailableGameFunctions() - Get list of available game functions
//    Returns:
//      const char* - JSON string containing available functions
// 
// Custom Executor Function Signatures:
// -------------------------------
// 
// Player Management:
//   int getPlayerUID()                    // Get current player's UID
//   bool getPlayerPosition(float& x, float& y, float& z)  // Get player position
//   void setPlayerPosition(float x, float y, float z)      // Set player position
//   int getPlayerHealth()                    // Get current health
//   int getPlayerMaxHealth()                 // Get maximum health
//   int getPlayerHunger()                    // Get current hunger
//   int getPlayerMaxHunger()                 // Get maximum hunger
//   int getPlayerStamina()                   // Get current stamina
//   int getPlayerMaxStamina()                // Get maximum stamina
//   float getPlayerWalkSpeed()               // Get walk speed
//   float getPlayerRunSpeed()                // Get run speed
//   float getPlayerJumpHeight()              // Get jump height
//   int getPlayerSkinID()                    // Get current skin ID
//   void setPlayerSkinID(int skinID)          // Set skin ID
//   int getPlayerEmotion()                   // Get current emotion
//   void setPlayerEmotion(int emotion)        // Set emotion
// 
// Item Management:
//   bool giveItem(int targetUID, int itemID, int quantity)  // Give items to player
//   bool throwItem(int targetUID, int slotID, int quantity) // Throw items from inventory
//   int getHeldItemID()                       // Get currently held item ID
//   int getHeldItemSlot()                     // Get currently held item slot
//   bool getPlayerInventory(int** inventory, int* size)  // Get player inventory
//   void freeInventory(int* inventory)       // Free inventory memory
// 
// Teleportation:
//   bool teleportPlayer(float x, float y, float z)  // Teleport local player
//   bool teleportToPlayer(int targetUID)      // Teleport to another player
//   bool getNearestPlayer(int& targetUID, float& distance)  // Find nearest player
//   bool getPlayerPositionByUID(int targetUID, float& x, float& y, float& z)  // Get position by UID
// 
// Chat & Communication:
//   bool sendChatMessage(const char* message)  // Send global chat message
//   bool sendPrivateMessage(int targetUID, const char* message)  // Send private message
//   bool listenForChatMessages()              // Start listening for chat messages
//   void stopListeningForChatMessages()        // Stop listening for chat messages
// 
// World & Game State:
//   int getWorldID()                         // Get current world ID
//   int getServerTime()                       // Get server time
//   float calculateDistance(float x1, float y1, float z1, float x2, float y2, float z2)  // Calculate distance
//   bool getPlayersInRange(float range, std::vector<int>& playerUIDs, std::vector<float>& distances)  // Get players in range
//   const char* getErrorCodes()               // Get available error codes
// 
// Utility & Helper:
//   bool executeScript(const char* script)   // Execute a Lua script
//   bool saveScript(const char* filename, const char* script)  // Save script to file
//   bool loadScript(const char* filename, char** script, int* size)  // Load script from file
//   bool exportDataToFile(const char* filename, const char* data)  // Export data to file
//   bool importDataFromFile(const char* filename, char** data, int* size)  // Import data from file
//   void wait(int milliseconds)                // Wait for specified time
//   const char* getLastError()                 // Get last error message
//   void clearLastError()                     // Clear last error
// 
// Remote Function Calls:
//   bool remoteExecute(const char* functionName, const char* parameters)  // Execute remote function
//   bool remoteFunction(const char* functionName, const char* parameters)  // Alternative remote function call
// 
// Configuration & Settings:
//   bool setConfigOption(const char* option, const char* value)  // Set configuration option
//   const char* getConfigOption(const char* option)  // Get configuration option
//   bool resetConfigToDefaults()  // Reset configuration to defaults
// 
// Script Management:
//   bool createScriptFile(const char* filename, const char* content)  // Create new script file
//   bool deleteScriptFile(constScript * filename)  // Delete script file
//   bool listScriptFiles(char** filenames, int* count)  // List available script files
//   bool executeScriptFromFile(const char* filename)  // Execute script from file
// 
// Event Handling:
//   bool setScriptCallback(const char* eventType, const char* callbackScript)  // Set script callback for events
//   bool removeScriptCallback(const char* eventType)  // Remove script callback
//   bool triggerScriptCallback(const char* eventType, const char* parameters)  // Trigger script callback
// 
// This header provides a comprehensive API for the custom Lua executor
// that gives users complete control over MiniWorld through Lua scripting.
// 
// Security Notes:
// - All functions use validated AOB patterns from Cheat Engine
// - Functions use consistent calling conventions (stdcall on Windows)
// - Error handling and validation provided for all function calls
// - Memory management is handled internally
// - Thread-safe operations where appropriate
// 
// Usage Example:
// #include "lua_full.h"
// 
// int main() {
//     // Initialize the custom executor
//     if (InitializeCustomExecutor()) {
//         // Execute a Lua script
//         const char* script = "local player = getPlayerUID()\n giveItem(player, 12345, 64)";
//         const char* result = ExecuteLuaScript(script);
//         
//         if (result) {
//             std::cout << "Script executed successfully: " << result << std::endl;
//         } else {
//             std::cerr << "Script execution failed" << std::endl;
//         }
//         
//         // Get player information
//         int uid = getPlayerUID();
//         float x, y, z;
//         if (getPlayerPosition(x, y, z)) {
//             std::cout << "Player position: (" << x << ", " << y << ", " << z << ")" << std::endl;
//         }
//         
//         // Send a chat message
//         sendChatMessage("Hello from custom executor!");
//         
//         // Cleanup
//         ShutdownCustomExecutor();
//     }
//     
//     return 0;
// }
// 
// The custom executor provides a powerful Lua scripting interface
// for MiniWorld that gives users complete control over the game.
