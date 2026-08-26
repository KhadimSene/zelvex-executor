// Custom Lua Executor for MiniWorld
// Core implementation providing full control via Lua scripting
// This replaces the sandboxed executor with a powerful, unrestricted executor
// 
// Key Features:
// - Full game access: Player, Chat, Backpack, CustomUI, RemoteFunction
// - AOB-based function resolution for all game APIs
// - Fresh Lua state with full stdlib (io, os, debug, package)
// - Direct item spawning, teleportation, player modification
// - Seamless integration with Zelvex
// 
// Built in 3 days (vs. weeks for traditional CE approach)
// 
// Usage Examples:
// ===========
// 
// // Get player information
// local playerUID = getPlayerUID()
// local x, y, z = getPlayerPosition()
// local health = getPlayerHealth()
// 
// // Give items to player
// giveItem(playerUID, 12345, 64)  // 64x item ID 12345
// 
// // Teleport to coordinates
// teleportTo(x + 10, y, z + 5)
// 
// // Send chat message
// sendChatMessage("Hello, MiniWorld!")
// 
// // Set player skin
// setPlayerSkinID(42)
// 
// // Speed hack
// setPlayerSpeed(5.0)  // 5x normal speed
// 
// // Noclip
// setPlayerNoclip(true)
// 
// // Lua script execution
// executeScript([[
//   local player = getPlayerUID()
//   local pos = getPlayerPosition()
//   for i=1,10 do
//     giveItem(player, 9999, 1)  // Auto-heal
//   end
// ]])