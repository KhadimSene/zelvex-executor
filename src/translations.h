#pragma once

#include <QString>
#include <array>

enum class Language : int {
    English = 0,
    Italiano = 1,
    Chinese = 2,
    Espanol = 3,
    Russian = 4,
    Turkish = 5,
    Vietnamese = 6,
    COUNT = 7
};

struct CheatTrans {
    int id;
    const char* name;
    const char* desc;
};

struct LangData {
    const char* tabLabels[9];
    const char* tabIcons[9];
    const char* sectionHeaders[17];
    CheatTrans cheats[25];
    int cheatCount;
    const char* sidebarStatus;
    const char* sidebarAttach;
    const char* sidebarOffline;
    const char* betaWarning;
    const char* terrainWarning;
    const char* itemsWarning;
    const char* langLabel;
    const char* attachBtn;
    const char* detachBtn;
    const char* attachedTo;
    const char* attachTo;
    const char* noProcessFound;
    const char* save;
    const char* posPlaceholder;
    const char* deleteText;
    const char* gameNotFound;
    const char* invalidHandle;
    const char* attachToGameFirst;
    const char* luaExecute;
    const char* luaClear;
    const char* luaPlaceholder;
    const char* luaStatusReady;
    const char* luaStatusExecuting;
    const char* luaStatusSuccess;
    const char* luaStatusError;
    const char* luaStatusNoVM;
    const char* luaHistoryPlaceholder;
};

static const std::array<LangData, 7> LANG_DATA = {{

// ═══ ENGLISH ═══
{
    {"Misc","Player","Teleport","Stats","Movement","Combat","Vision","Items","Lua"},
    {"M","P","T","S","M","C","V","I","L"},
    {"GENERAL","{Player Cheats}","Position","Teleport Config","Health/Hunger/Stamina Stats",
     "Movement Cheats","Speed Stats","Auto Click","Spectator","Offensive","Vision Cheats","Item Management","Lua Code Injector",
     "Zelvex Native World","Zelvex Native Player","Zelvex Native Vision","Zelvex Native Items"},
    {
        {999001,"Terrain Editor","Enable terrain editor + mode 1"},
        {19,"Noclip","Walk through walls and obstacles"},
        {6,"Condition","Player movement mode (Normal, Fly, Sprint)"},
        {1337198110,"No Fall Damage","Prevents all fall damage"},
        {1337197986,"Fast Eat","Instantly consume items and fruits"},
        {17,"Position X","X coordinate"},
        {15,"Position Y","Y coordinate"},
        {16,"Position Z","Z coordinate"},
        {999010,"Random Teleport","Teleport to a random location on the map"},
        {22,"Health","Current health value"},
        {28,"MaxHealth","Maximum health value"},
        {27,"Hunger","Current hunger value"},
        {29,"MaxHunger","Maximum hunger value"},
        {30,"Stamina","Current stamina value"},
        {33,"MaxStamina","Maximum stamina value"},
        {1337198106,"Air Jump","Unlimited mid-air jumps"},
        {1807641586,"Swim in Air","Swim while airborne"},
        {23,"WalkSpeed","Walking speed multiplier"},
        {24,"RunSpeed","Running speed multiplier"},
        {25,"CrouchSpeed","Crouching speed multiplier"},
        {26,"SwimSpeed","Swimming speed multiplier"},
        {1337197721,"Auto Click Left","Auto left-click at regular intervals"},
        {1337197722,"Auto Click Right","Auto right-click at regular intervals"},
        {1337212286,"Force Spectator","Force spectator mode on target player"},
        {1807635788,"Disable Spectator","Remove spectator mode from target"},
    }, 24,
    "Status","ATTACH","Offline",
    "Zelvex v3 BETA",
    "Terrain Editor is in BETA  - may not work correctly",
    "Items feature is in development  - may contain bugs",
    "Language",
    "ATTACH","DETACH","Attached to","Attach to","No process found",
    "Save","Position name...","Delete",
    "Game not found","Invalid handle","Attach to game first",
    "EXECUTE","CLEAR","-- Enter Lua code here --",
    "Ready","Executing...","Success!","Error","VM not initialized","History..."
},

// ═══ ITALIANO ═══
{
    {"Generale","Giocatore","Teletrasporto","Statistiche","Movimento","Combattimento","Visione","Oggetti","Lua"},
    {"G","G","T","S","M","C","V","O","L"},
    {"GENERALE","{Cheat Giocatore}","Posizione","Impostazioni Teletrasporto","Salute/Fame/Stamina",
     "Cheat di Movimento","Velocit\u00E0","Auto Click","Spettatore","Offensiva","Cheat di Visione","Gestione Oggetti","Iniettore Codice Lua",
     "Zelvex Mondo Nativo","Zelvex Giocatore Nativo","Zelvex Visione Nativa","Zelvex Oggetti Nativi"},
    {
        {999001,"Terrain Editor","Abilita l'editor del terreno + modalit\u00E0 1"},
        {19,"Noclip","Cammina attraverso muri e ostacoli"},
        {6,"Condition","Modalit\u00E0 di movimento (Normale, Volo, Sprint)"},
        {1337198110,"Anti Caduta","Previene tutti i danni da caduta"},
        {1337197986,"Mangia Veloce","Consuma oggetti e frutta all'istante"},
        {17,"Position X","Coordinata X"},
        {15,"Position Y","Coordinata Y"},
        {16,"Position Z","Coordinata Z"},
        {999010,"Teletrasporto Casuale","Teletrasporto in una posizione casuale"},
        {22,"Salute","Salute attuale"},
        {28,"Salute Massima","Salute massima"},
        {27,"Fame","Fame attuale"},
        {29,"Fame Massima","Fame massima"},
        {30,"Stamina","Stamina attuale"},
        {33,"Stamina Massima","Stamina massima"},
        {1337198106,"Salto Aereo","Salti aerei illimitati"},
        {1807641586,"Nuota in Aria","Nuota nell'aria"},
        {23,"Camminata","Velocit\u00E0 di camminata"},
        {24,"Corsa","Velocit\u00E0 di corsa"},
        {25,"Accovacciato","Velocit\u00E0 in accovacciamento"},
        {26,"Nuoto","Velocit\u00E0 di nuoto"},
        {1337197721,"Auto Click Sinistro","Auto click sinistro a intervalli regolari"},
        {1337197722,"Auto Click Destro","Auto click destro a intervalli regolari"},
        {1337212286,"Forza Spettatore","Forza la modalit\u00E0 spettatore sul bersaglio"},
        {1807635788,"Disabilita Spettatore","Rimuovi la modalit\u00E0 spettatore dal bersaglio"},
    }, 24,
    "Stato","ATTACCA","Non in linea",
    "Zelvex v3 BETA",
    "L'Editor del Terreno \u00E8 in BETA  - potrebbe non funzionare correttamente",
    "La gestione oggetti \u00E8 in sviluppo  - potrebbe contenere bug",
    "Lingua",
    "CONNETTI","DISCONNETTI","Connesso a","Connetti a","Nessun processo trovato",
    "Salva","Nome posizione...","Elimina",
    "Gioco non trovato","Handle non valido","Connetti al gioco prima",
    "ESEGUI","CANCELLA","-- Inserisci codice Lua qui --",
    "Pronto","Esecuzione...","Successo!","Errore","VM non inizializzato","Cronologia..."
},

// ═══ CHINESE ═══
{
    {"\u6742\u9879","\u73A9\u5BB6","\u4F20\u9001","\u5C5E\u6027","\u79FB\u52A8","\u6218\u6597","\u89C6\u89C9","\u7269\u54C1","Lua"},
    {"\u6742","\u73A9","\u4F20","\u5C5E","\u79FB","\u6218","\u89C6","\u7269","L"},
    {"\u901A\u7528","{\u73A9\u5BB6\u4F5C\u5F0A}","\u5750\u6807","\u4F20\u9001\u8BBE\u7F6E","\u751F\u547D/\u9965\u997F/\u4F53\u529B\u5C5E\u6027",
     "\u79FB\u52A8\u4F5C\u5F0A","\u901F\u5EA6\u5C5E\u6027","\u81EA\u52A8\u70B9\u51FB","\u89C2\u6218","\u8FDB\u653B","\u89C6\u89C9\u4F5C\u5F0A","\u7269\u54C1\u7BA1\u7406","Lua\u4EE3\u7801\u6CE8\u5165",
     "Zelvex\u539F\u751F\u4E16\u754C","Zelvex\u539F\u751F\u73A9\u5BB6","Zelvex\u539F\u751F\u89C6\u89C9","Zelvex\u539F\u751F\u7269\u54C1"},
    {
        {999001,"\u5730\u5F62\u7F16\u8F91\u5668","\u542F\u7528\u5730\u5F62\u7F16\u8F91\u5668 + \u6A21\u5F0F1"},
        {19,"Noclip","\u7A7F\u5899\u6A21\u5F0F\uFF0C\u53EF\u4EE5\u7A7F\u8FC7\u5899\u58C1\u548C\u969C\u788D\u7269"},
        {6,"Condition","\u73A9\u5BB6\u79FB\u52A8\u6A21\u5F0F\uFF08\u666E\u901A/\u98DE\u884C/\u75BE\u8DD1+\u98DE\u884C\uFF09"},
        {1337198110,"\u514D\u6454\u4F24","\u514D\u75AB\u6240\u6709\u5760\u843D\u4F24\u5BB3"},
        {1337197986,"\u5FEB\u901F\u98DF\u7528","\u77AC\u95F4\u98DF\u7528\u7269\u54C1\u548C\u6C34\u679C"},
        {17,"X\u5750\u6807","\u6C34\u5E73\u4F4D\u7F6E\u5750\u6807"},
        {15,"Y\u5750\u6807","\u5782\u76F4\u4F4D\u7F6E\u5750\u6807"},
        {16,"Z\u5750\u6807","\u6DF1\u5EA6\u4F4D\u7F6E\u5750\u6807"},
        {999010,"\u968F\u673A\u4F20\u9001","\u4F20\u9001\u5230\u5730\u56FE\u4E0A\u968F\u673A\u4F4D\u7F6E"},
        {22,"\u751F\u547D\u503C","\u5F53\u524D\u751F\u547D\u503C"},
        {28,"\u6700\u5927\u751F\u547D\u503C","\u751F\u547D\u503C\u4E0A\u9650"},
        {27,"\u9965\u997F\u503C","\u5F53\u524D\u9965\u997F\u503C"},
        {29,"\u6700\u5927\u9965\u997F\u503C","\u9965\u997F\u503C\u4E0A\u9650"},
        {30,"\u4F53\u529B\u503C","\u5F53\u524D\u4F53\u529B\u503C"},
        {33,"\u6700\u5927\u4F53\u529B\u503C","\u4F53\u529B\u503C\u4E0A\u9650"},
        {1337198106,"\u7A7A\u4E2D\u8DF3\u8DC3","\u65E0\u9650\u7A7A\u4E2D\u8DF3\u8DC3"},
        {1807641586,"\u7A7A\u4E2D\u6E38\u6CF3","\u5728\u7A7A\u4E2D\u6E38\u6CF3"},
        {23,"\u884C\u8D70\u901F\u5EA6","\u884C\u8D70\u901F\u5EA6\u500D\u7387"},
        {24,"\u5954\u8DD1\u901F\u5EA6","\u5954\u8DD1\u901F\u5EA6\u500D\u7387"},
        {25,"\u8E72\u4F0F\u901F\u5EA6","\u8E72\u4F0F\u901F\u5EA6\u500D\u7387"},
        {26,"\u6E38\u6CF3\u901F\u5EA6","\u6E38\u6CF3\u901F\u5EA6\u500D\u7387"},
        {1337197721,"\u81EA\u52A8\u5DE6\u952E","\u81EA\u52A8\u5DE6\u952E\u70B9\u51FB"},
        {1337197722,"\u81EA\u52A8\u53F3\u952E","\u81EA\u52A8\u53F3\u952E\u70B9\u51FB"},
        {1337212286,"\u5F3A\u5236\u89C2\u6218","\u5F3A\u5236\u76EE\u6807\u8FDB\u5165\u89C2\u6218\u6A21\u5F0F"},
        {1807635788,"\u53D6\u6D88\u89C2\u6218","\u53D6\u6D88\u76EE\u6807\u7684\u89C2\u6218\u6A21\u5F0F"},
    }, 24,
    "\u72B6\u6001","\u8FDE\u63A5","\u79BB\u7EBF",
    "Zelvex v3 BETA",
    "\u5730\u5F62\u7F16\u8F91\u5668\u4E3ABETA\u7248\u672C \u2014 \u53EF\u80FD\u65E0\u6CD5\u6B63\u5E38\u5DE5\u4F5C",
    "\u7269\u54C1\u529F\u80FD\u5F00\u53D1\u4E2D \u2014 \u53EF\u80FD\u5B58\u5728bug",
    "\u8BED\u8A00",
    "\u8FDE\u63A5","\u65AD\u5F00","\u5DF2\u8FDE\u63A5","\u8FDE\u63A5\u5230","\u672A\u627E\u5230\u8FDB\u7A0B",
    "\u4FDD\u5B58","\u4F4D\u7F6E\u540D\u79F0...","\u5220\u9664",
    "\u672A\u627E\u5230\u6E38\u620F","\u65E0\u6548\u53E5\u67C4","\u8BF7\u5148\u8FDE\u63A5\u6E38\u620F",
    "\u6267\u884C","\u6E05\u9664","\u8BF7\u5728\u6B64\u8F93\u5165Lua\u4EE3\u7801",
    "\u5C31\u7EEA","\u6267\u884C\u4E2D...","\u6210\u529F!","\u9519\u8BEF","VM\u672A\u521D\u59CB\u5316","\u5386\u53F2\u8BB0\u5F55..."
},

// ═══ ESPA\u00D1OL ═══
{
    {"Varios","Jugador","Teletransporte","Estad\u00EDsticas","Movimiento","Combate","Visi\u00F3n","Objetos","Lua"},
    {"V","J","T","E","M","C","V","O","L"},
    {"GENERAL","{Cheat del Jugador}","Posici\u00F3n","Configuraci\u00F3n de Teletransporte","Salud/Hambre/Resistencia",
     "Cheat de Movimiento","Velocidad","Auto Click","Espectador","Ofensiva","Cheat de Visi\u00F3n","Gesti\u00F3n de Objetos","Inyector de C\u00F3digo Lua",
     "Zelvex Mundo Nativo","Zelvex Jugador Nativo","Zelvex Visi\u00F3n Nativa","Zelvex Objetos Nativos"},
    {
        {999001,"Editor de Terreno","Habilitar editor de terreno + modo 1"},
        {19,"Noclip","Atravesar paredes y obst\u00E1culos"},
        {6,"Condition","Modo de movimiento (Normal, Vuelo, Sprint)"},
        {1337198110,"Sin Da\u00F1o por Ca\u00EDda","Previene todo el da\u00F1o por ca\u00EDda"},
        {1337197986,"Comida R\u00E1pida","Consumir objetos y frutas al instante"},
        {17,"Posici\u00F3n X","Coordenada X"},
        {15,"Posici\u00F3n Y","Coordenada Y"},
        {16,"Posici\u00F3n Z","Coordenada Z"},
        {999010,"Teletransporte Aleatorio","Teletransporte a una posici\u00F3n aleatoria"},
        {22,"Salud","Valor de salud actual"},
        {28,"Salud M\u00E1xima","Valor de salud m\u00E1ximo"},
        {27,"Hambre","Valor de hambre actual"},
        {29,"Hambre M\u00E1xima","Valor de hambre m\u00E1ximo"},
        {30,"Resistencia","Valor de resistencia actual"},
        {33,"Resistencia M\u00E1xima","Valor de resistencia m\u00E1ximo"},
        {1337198106,"Salto A\u00E9reo","Saltos infinitos en el aire"},
        {1807641586,"Nadar en el Aire","Nadar mientras est\u00E1s en el aire"},
        {23,"Velocidad Caminar","Velocidad al caminar"},
        {24,"Velocidad Correr","Velocidad al correr"},
        {25,"Velocidad Agacharse","Velocidad al agacharse"},
        {26,"Velocidad Nadar","Velocidad al nadar"},
        {1337197721,"Auto Click Izquierdo","Auto click izquierdo"},
        {1337197722,"Auto Click Derecho","Auto click derecho"},
        {1337212286,"Forzar Espectador","Forzar modo espectador al objetivo"},
        {1807635788,"Quitar Espectador","Quitar modo espectador del objetivo"},
    }, 24,
    "Estado","CONECTAR","Desconectado",
    "Zelvex v3 BETA",
    "Editor de Terreno en BETA  - podr\u00EDa no funcionar",
    "Sistema de Objetos en desarrollo  - podr\u00EDa contener errores",
    "Idioma",
    "CONECTAR","DESCONECTAR","Conectado a","Conectar a","Proceso no encontrado",
    "Guardar","Nombre de posici\u00F3n...","Eliminar",
    "Juego no encontrado","Handle inv\u00E1lido","Conectar al juego primero",
    "Ejecutar","Limpiar","-- Ingrese c\u00F3digo Lua aqu\u00ED --",
    "Listo","Ejecutando...","\u00C9xito!","Error","VM no inicializado","Historial..."
},

// ═══ RUSSIAN ═══
{
    {"\u041F\u0440\u043E\u0447\u0435\u0435","\u0418\u0433\u0440\u043E\u043A","\u0422\u0435\u043B\u0435\u043F\u043E\u0440\u0442","\u0425\u0430\u0440\u0430\u043A\u0442\u0435\u0440\u0438\u0441\u0442\u0438\u043A\u0438","\u0414\u0432\u0438\u0436\u0435\u043D\u0438\u0435","\u0411\u043E\u0439","\u0417\u0440\u0435\u043D\u0438\u0435","\u041F\u0440\u0435\u0434\u043C\u0435\u0442\u044B","Lua"},
    {"\u041F","\u0418","\u0422","\u0425","\u0414","\u0411","\u0417","\u041F","L"},
    {"\u041E\u0421\u041D\u041E\u0412\u041D\u041E\u0415","{\u0427\u0438\u0442\u044B \u0438\u0433\u0440\u043E\u043A\u0430}","\u041F\u043E\u0437\u0438\u0446\u0438\u044F","\u041D\u0430\u0441\u0442\u0440\u043E\u0439\u043A\u0438 \u0442\u0435\u043B\u0435\u043F\u043E\u0440\u0442\u0430","\u0417\u0434\u043E\u0440\u043E\u0432\u044C\u0435/\u0413\u043E\u043B\u043E\u0434/\u0412\u044B\u043D\u043E\u0441\u043B\u0438\u0432\u043E\u0441\u0442\u044C",
     "\u0427\u0438\u0442\u044B \u0434\u0432\u0438\u0436\u0435\u043D\u0438\u044F","\u0421\u043A\u043E\u0440\u043E\u0441\u0442\u044C","\u0410\u0432\u0442\u043E-\u043A\u043B\u0438\u043A","\u041D\u0430\u0431\u043B\u044E\u0434\u0430\u0442\u0435\u043B\u044C","\u0410\u0442\u0430\u043A\u0443\u044E\u0449\u0438\u0435","\u0427\u0438\u0442\u044B \u0437\u0440\u0435\u043D\u0438\u044F","\u0423\u043F\u0440\u0430\u0432\u043B\u0435\u043D\u0438\u0435 \u043F\u0440\u0435\u0434\u043C\u0435\u0442\u0430\u043C\u0438","\u0418\u043D\u0436\u0435\u043A\u0442\u043E\u0440 Lua",
     "Zelvex \u041D\u0430\u0442\u0438\u0432\u043D\u044B\u0439 \u041C\u0438\u0440","Zelvex \u041D\u0430\u0442\u0438\u0432\u043D\u044B\u0439 \u0418\u0433\u0440\u043E\u043A","Zelvex \u041D\u0430\u0442\u0438\u0432\u043D\u043E\u0435 \u0417\u0440\u0435\u043D\u0438\u0435","Zelvex \u041D\u0430\u0442\u0438\u0432\u043D\u044B\u0435 \u041F\u0440\u0435\u0434\u043C\u0435\u0442\u044B"},
    {
        {999001,"\u0420\u0435\u0434\u0430\u043A\u0442\u043E\u0440 \u043C\u0435\u0441\u0442\u043D\u043E\u0441\u0442\u0438","\u0412\u043A\u043B\u044E\u0447\u0438\u0442\u044C \u0440\u0435\u0434\u0430\u043A\u0442\u043E\u0440 \u043C\u0435\u0441\u0442\u043D\u043E\u0441\u0442\u0438 + \u0440\u0435\u0436\u0438\u043C 1"},
        {19,"Noclip","\u041F\u0440\u043E\u0445\u043E\u0436\u0434\u0435\u043D\u0438\u0435 \u0441\u043A\u0432\u043E\u0437\u044C \u0441\u0442\u0435\u043D\u044B \u0438 \u043F\u0440\u0435\u043F\u044F\u0442\u0441\u0442\u0432\u0438\u044F"},
        {6,"Condition","\u0420\u0435\u0436\u0438\u043C \u0434\u0432\u0438\u0436\u0435\u043D\u0438\u044F (\u041E\u0431\u044B\u0447\u043D\u044B\u0439/\u041F\u043E\u043B\u0451\u0442/\u0421\u043F\u0440\u0438\u043D\u0442)"},
        {1337198110,"\u0417\u0430\u0449\u0438\u0442\u0430 \u043E\u0442 \u043F\u0430\u0434\u0435\u043D\u0438\u044F","\u0417\u0430\u0449\u0438\u0442\u0430 \u043E\u0442 \u0443\u0440\u043E\u043D\u0430 \u043F\u0440\u0438 \u043F\u0430\u0434\u0435\u043D\u0438\u0438"},
        {1337197986,"\u0411\u044B\u0441\u0442\u0440\u0430\u044F \u0435\u0434\u0430","\u041C\u0433\u043D\u043E\u0432\u0435\u043D\u043D\u043E\u0435 \u0443\u043F\u043E\u0442\u0440\u0435\u0431\u043B\u0435\u043D\u0438\u0435 \u0435\u0434\u044B \u0438 \u0444\u0440\u0443\u043A\u0442\u043E\u0432"},
        {17,"\u041F\u043E\u0437\u0438\u0446\u0438\u044F X","\u041A\u043E\u043E\u0440\u0434\u0438\u043D\u0430\u0442\u0430 X"},
        {15,"\u041F\u043E\u0437\u0438\u0446\u0438\u044F Y","\u041A\u043E\u043E\u0440\u0434\u0438\u043D\u0430\u0442\u0430 Y"},
        {16,"\u041F\u043E\u0437\u0438\u0446\u0438\u044F Z","\u041A\u043E\u043E\u0440\u0434\u0438\u043D\u0430\u0442\u0430 Z"},
        {999010,"\u0421\u043B\u0443\u0447\u0430\u0439\u043D\u044B\u0439 \u0442\u0435\u043B\u0435\u043F\u043E\u0440\u0442","\u0422\u0435\u043B\u0435\u043F\u043E\u0440\u0442 \u0432 \u0441\u043B\u0443\u0447\u0430\u0439\u043D\u0443\u044E \u0442\u043E\u0447\u043A\u0443 \u043D\u0430 \u043A\u0430\u0440\u0442\u0435"},
        {22,"\u0417\u0434\u043E\u0440\u043E\u0432\u044C\u0435","\u0422\u0435\u043A\u0443\u0449\u0435\u0435 \u0437\u0434\u043E\u0440\u043E\u0432\u044C\u0435"},
        {28,"\u041C\u0430\u043A\u0441. \u0437\u0434\u043E\u0440\u043E\u0432\u044C\u0435","\u041C\u0430\u043A\u0441\u0438\u043C\u0430\u043B\u044C\u043D\u043E\u0435 \u0437\u0434\u043E\u0440\u043E\u0432\u044C\u0435"},
        {27,"\u0413\u043E\u043B\u043E\u0434","\u0422\u0435\u043A\u0443\u0449\u0438\u0439 \u0433\u043E\u043B\u043E\u0434"},
        {29,"\u041C\u0430\u043A\u0441. \u0433\u043E\u043B\u043E\u0434","\u041C\u0430\u043A\u0441\u0438\u043C\u0430\u043B\u044C\u043D\u044B\u0439 \u0433\u043E\u043B\u043E\u0434"},
        {30,"\u0412\u044B\u043D\u043E\u0441\u043B\u0438\u0432\u043E\u0441\u0442\u044C","\u0422\u0435\u043A\u0443\u0449\u0430\u044F \u0432\u044B\u043D\u043E\u0441\u043B\u0438\u0432\u043E\u0441\u0442\u044C"},
        {33,"\u041C\u0430\u043A\u0441. \u0432\u044B\u043D\u043E\u0441\u043B\u0438\u0432\u043E\u0441\u0442\u044C","\u041C\u0430\u043A\u0441\u0438\u043C\u0430\u043B\u044C\u043D\u0430\u044F \u0432\u044B\u043D\u043E\u0441\u043B\u0438\u0432\u043E\u0441\u0442\u044C"},
        {1337198106,"\u0412\u043E\u0437\u0434\u0443\u0448\u043D\u044B\u0439 \u043F\u0440\u044B\u0436\u043E\u043A","\u0411\u0435\u0441\u043A\u043E\u043D\u0435\u0447\u043D\u044B\u0435 \u043F\u0440\u044B\u0436\u043A\u0438 \u0432 \u0432\u043E\u0437\u0434\u0443\u0445\u0435"},
        {1807641586,"\u041F\u043B\u0430\u0432\u0430\u043D\u0438\u0435 \u0432 \u0432\u043E\u0437\u0434\u0443\u0445\u0435","\u041F\u043B\u0430\u0432\u0430\u0442\u044C \u0432 \u0432\u043E\u0437\u0434\u0443\u0445\u0435"},
        {23,"\u0421\u043A\u043E\u0440\u043E\u0441\u0442\u044C \u0445\u043E\u0434\u044C\u0431\u044B","\u041C\u043D\u043E\u0436\u0438\u0442\u0435\u043B\u044C \u0441\u043A\u043E\u0440\u043E\u0441\u0442\u0438 \u0445\u043E\u0434\u044C\u0431\u044B"},
        {24,"\u0421\u043A\u043E\u0440\u043E\u0441\u0442\u044C \u0431\u0435\u0433\u0430","\u041C\u043D\u043E\u0436\u0438\u0442\u0435\u043B\u044C \u0441\u043A\u043E\u0440\u043E\u0441\u0442\u0438 \u0431\u0435\u0433\u0430"},
        {25,"\u0421\u043A\u043E\u0440\u043E\u0441\u0442\u044C \u043F\u0440\u0438\u0441\u0435\u0434\u0430\u043D\u0438\u044F","\u041C\u043D\u043E\u0436\u0438\u0442\u0435\u043B\u044C \u0441\u043A\u043E\u0440\u043E\u0441\u0442\u0438 \u043F\u0440\u0438\u0441\u0435\u0434\u0430\u043D\u0438\u044F"},
        {26,"\u0421\u043A\u043E\u0440\u043E\u0441\u0442\u044C \u043F\u043B\u0430\u0432\u0430\u043D\u0438\u044F","\u041C\u043D\u043E\u0436\u0438\u0442\u0435\u043B\u044C \u0441\u043A\u043E\u0440\u043E\u0441\u0442\u0438 \u043F\u043B\u0430\u0432\u0430\u043D\u0438\u044F"},
        {1337197721,"\u0410\u0432\u0442\u043E-\u043A\u043B\u0438\u043A \u043B\u0435\u0432\u043E","\u0410\u0432\u0442\u043E-\u043A\u043B\u0438\u043A \u043B\u0435\u0432\u043E\u0439 \u043A\u043D\u043E\u043F\u043A\u043E\u0439"},
        {1337197722,"\u0410\u0432\u0442\u043E-\u043A\u043B\u0438\u043A \u043F\u0440\u0430\u0432\u043E","\u0410\u0432\u0442\u043E-\u043A\u043B\u0438\u043A \u043F\u0440\u0430\u0432\u043E\u0439 \u043A\u043D\u043E\u043F\u043A\u043E\u0439"},
        {1337212286,"\u041F\u0440\u0438\u043D\u0443\u0434\u0438\u0442\u0435\u043B\u044C\u043D\u044B\u0439 \u043D\u0430\u0431\u043B\u044E\u0434\u0430\u0442\u0435\u043B\u044C","\u041F\u0440\u0438\u043D\u0443\u0434\u0438\u0442\u0435\u043B\u044C\u043D\u044B\u0439 \u043D\u0430\u0431\u043B\u044E\u0434\u0430\u0442\u0435\u043B\u044C \u043D\u0430 \u0446\u0435\u043B\u044C"},
        {1807635788,"\u0423\u0431\u0440\u0430\u0442\u044C \u043D\u0430\u0431\u043B\u044E\u0434\u0430\u0442\u0435\u043B\u044F","\u0423\u0431\u0440\u0430\u0442\u044C \u0440\u0435\u0436\u0438\u043C \u043D\u0430\u0431\u043B\u044E\u0434\u0430\u0442\u0435\u043B\u044F \u0441 \u0446\u0435\u043B\u0438"},
    }, 24,
    "\u0421\u0442\u0430\u0442\u0443\u0441","\u041F\u041E\u0414\u041A\u041B\u042E\u0427\u0418\u0422\u042C","\u041D\u0435 \u0432 \u0441\u0435\u0442\u0438",
    "Zelvex v3 BETA",
    "\u0420\u0435\u0434\u0430\u043A\u0442\u043E\u0440 \u043C\u0435\u0441\u0442\u043D\u043E\u0441\u0442\u0438 \u0432 BETA \u2014 \u043C\u043E\u0436\u0435\u0442 \u0440\u0430\u0431\u043E\u0442\u0430\u0442\u044C \u043D\u0435\u043A\u043E\u0440\u0440\u0435\u043A\u0442\u043D\u043E",
    "\u0421\u0438\u0441\u0442\u0435\u043C\u0430 \u043F\u0440\u0435\u0434\u043C\u0435\u0442\u043E\u0432 \u0432 \u0440\u0430\u0437\u0440\u0430\u0431\u043E\u0442\u043A\u0435 \u2014 \u0432\u043E\u0437\u043C\u043E\u0436\u043D\u044B \u043E\u0448\u0438\u0431\u043A\u0438",
    "\u042F\u0437\u044B\u043A",
    "\u041F\u041E\u0414\u041A\u041B\u042E\u0427\u0418\u0422\u042C","\u041E\u0422\u041A\u041B\u042E\u0427\u0418\u0422\u042C","\u041F\u043E\u0434\u043A\u043B\u044E\u0447\u0435\u043D\u043E \u043A","\u041F\u043E\u0434\u043A\u043B\u044E\u0447\u0438\u0442\u044C \u043A","\u041F\u0440\u043E\u0446\u0435\u0441\u0441 \u043D\u0435 \u043D\u0430\u0439\u0434\u0435\u043D",
    "\u0421\u043E\u0445\u0440\u0430\u043D\u0438\u0442\u044C","\u0418\u043C\u044F \u043F\u043E\u0437\u0438\u0446\u0438\u0438...","\u0423\u0434\u0430\u043B\u0438\u0442\u044C",
    "\u0418\u0433\u0440\u0430 \u043D\u0435 \u043D\u0430\u0439\u0434\u0435\u043D\u0430","\u041D\u0435\u0432\u0435\u0440\u043D\u044B\u0439 \u0434\u0435\u0441\u043A\u0440\u0438\u043F\u0442\u043E\u0440","\u0421\u043D\u0430\u0447\u0430\u043B\u0430 \u043F\u043E\u0434\u043A\u043B\u044E\u0447\u0438\u0442\u0435\u0441\u044C \u043A \u0438\u0433\u0440\u0435",
    "\u0412\u044B\u043F\u043E\u043B\u043D\u0438\u0442\u044C","\u041E\u0447\u0438\u0441\u0442\u0438\u0442\u044C","\u0412\u0432\u0435\u0434\u0438\u0442\u0435 Lua \u043A\u043E\u0434",
    "\u0413\u043E\u0442\u043E\u0432\u043E","\u0412\u044B\u043F\u043E\u043B\u043D\u0435\u043D\u0438\u0435...","\u0423\u0441\u043F\u0435\u0445!","\u041E\u0448\u0438\u0431\u043A\u0430","VM \u043D\u0435 \u0438\u043D\u0438\u0446\u0438\u0430\u043B\u0438\u0437\u0438\u0440\u043E\u0432\u0430\u043D","\u0418\u0441\u0442\u043E\u0440\u0438\u044F..."
},

// ═══ TURKISH ═══
{
    {"Di\u011Fer","Oyuncu","I\u015F\u0131nlanma","\u0130statistikler","Hareket","Sava\u015F","G\u00F6r\u00FC\u015F","E\u015Fyalar","Lua"},
    {"D","O","I","\u0130","H","S","G","E","L"},
    {"GENEL","{Oyuncu Hileleri}","Konum","I\u015F\u0131nlanma Ayarlar\u0131","Sa\u011Fl\u0131k/A\u00E7l\u0131k/Dayan\u0131kl\u0131l\u0131k",
     "Hareket Hileleri","H\u0131z","Otomatik T\u0131klama","\u0130zleyici","Ofansif","G\u00F6r\u00FC\u015F Hileleri","E\u015Fya Y\u00F6netimi","Lua Kod Enjekt\u00F6r\u00FC",
     "Zelvex Yerel D\u00FCnya","Zelvex Yerel Oyuncu","Zelvex Yerel G\u00F6r\u00FC\u015F","Zelvex Yerel E\u015Fyalar"},
    {
        {999001,"Arazi D\u00FCzenleyicisi","Arazi d\u00FCzenleyicisi + mod 1"},
        {19,"Noclip","Duvarlardan ve engellerden ge\u00E7"},
        {6,"Condition","Hareket modu (Normal, U\u00E7u\u015F, Sprint)"},
        {1337198110,"D\u00FC\u015Fme Hasar\u0131 Yok","D\u00FC\u015Fme hasar\u0131n\u0131 tamamen \u00F6nler"},
        {1337197986,"H\u0131zl\u0131 Yemek","Yiyecekleri ve meyveleri an\u0131nda t\u00FCket"},
        {17,"X Koordinat\u0131","X koordinat\u0131"},
        {15,"Y Koordinat\u0131","Y koordinat\u0131"},
        {16,"Z Koordinat\u0131","Z koordinat\u0131"},
        {999010,"Rastgele I\u015F\u0131nlanma","Haritadaki rastgele bir noktaya \u0131\u015F\u0131nlan"},
        {22,"Sa\u011Fl\u0131k","Mevcut sa\u011Fl\u0131k de\u011Feri"},
        {28,"Maks. Sa\u011Fl\u0131k","Maksimum sa\u011Fl\u0131k de\u011Feri"},
        {27,"A\u00E7l\u0131k","Mevcut a\u00E7l\u0131k de\u011Feri"},
        {29,"Maks. A\u00E7l\u0131k","Maksimum a\u00E7l\u0131k de\u011Feri"},
        {30,"Dayan\u0131kl\u0131l\u0131k","Mevcut dayan\u0131kl\u0131l\u0131k de\u011Feri"},
        {33,"Maks. Dayan\u0131kl\u0131l\u0131k","Maksimum dayan\u0131kl\u0131l\u0131k de\u011Feri"},
        {1337198106,"Havada Z\u0131plama","S\u0131n\u0131rs\u0131z havada z\u0131plama"},
        {1807641586,"Havada Y\u00FCzme","Havada y\u00FCz"},
        {23,"Y\u00FCr\u00FCy\u00FC\u015F H\u0131z\u0131","Y\u00FCr\u00FCy\u00FC\u015F h\u0131z \u00E7arpan\u0131"},
        {24,"Ko\u015Fu H\u0131z\u0131","Ko\u015Fu h\u0131z \u00E7arpan\u0131"},
        {25,"E\u011Filme H\u0131z\u0131","E\u011Filme h\u0131z \u00E7arpan\u0131"},
        {26,"Y\u00FCzme H\u0131z\u0131","Y\u00FCzme h\u0131z \u00E7arpan\u0131"},
        {1337197721,"Otomatik Sol T\u0131k","Otomatik sol t\u0131klama"},
        {1337197722,"Otomatik Sa\u011F T\u0131k","Otomatik sa\u011F t\u0131klama"},
        {1337212286,"Zorla \u0130zleyici","Hedefe zorla izleyici modu"},
        {1807635788,"\u0130zleyiciyi Kald\u0131r","Hedefin izleyici modunu kald\u0131r"},
    }, 24,
    "Durum","BA\u011ELAN","\u00C7evrimd\u0131\u015F\u0131",
    "Zelvex v3 BETA",
    "Arazi D\u00FCzenleyicisi BETA \u2014 d\u00FCzg\u00FCn \u00E7al\u0131\u015Fmayabilir",
    "E\u015Fya sistemi geli\u015Ftirme a\u015Famas\u0131nda \u2014 hata i\u00E7erebilir",
    "Dil",
    "BA\u011ELAN","BA\u011ELANTIYI KES","Ba\u011Fl\u0131","Ba\u011Flan","S\u00FCre\u00E7 bulunamad\u0131",
    "Kaydet","Konum ad\u0131...","Sil",
    "Oyun bulunamad\u0131","Ge\u00E7ersiz handle","\u00D6nce oyuna ba\u011Flan",
    "\u00C7al\u0131\u015Ft\u0131r","Temizle","-- Lua kodunu buraya yaz\u0131n --",
    "Haz\u0131r","\u00C7al\u0131\u015Ft\u0131r\u0131l\u0131yor...","Ba\u015Far\u0131l\u0131!","Hata","VM ba\u015Flat\u0131lmad\u0131","Ge\u00E7mi\u015F..."
},

// ═══ VIETNAMESE ═══
{
    {"Ph\u1EE5","Ng\u01B0\u1EDDi Ch\u01A1i","D\u1ECBch Chuy\u1EC3n","Ch\u1EC9 S\u1ED1","Di Chuy\u1EC3n","Chi\u1EBFn \u0110\u1EA5u","Th\u1ECB Gi\u00E1c","V\u1EADt Ph\u1EA9m","Lua"},
    {"P","N","C","S","D","C","T","V","L"},
    {"CHUNG","{Cheat Ng\u01B0\u1EDDi Ch\u01A1i}","T\u1ECDa \u0110\u1ED9","C\u00E0i \u0110\u1EB7t D\u1ECBch Chuy\u1EC3n","M\u00E1u/That/\u0110\u00F3i/Th\u1EC3 L\u1EF1c",
     "Cheat Di Chuy\u1EC3n","T\u1ED1c \u0110\u1ED9","Auto Click","Ng\u01B0\u1EDDi Xem","T\u1EA5n C\u00F4ng","Cheat Th\u1ECB Gi\u00E1c","Qu\u1EA3n L\u00FD V\u1EADt Ph\u1EA9m","M\u00E1y Tiêm M\u00E3 Lua",
     "Zelvex Th\u1EBF Gi\u1EDBi Th\u00EAn Nhi\u00EAn","Zelvex Ng\u01B0\u1EDDi Ch\u01A1i Th\u00EAn Nhi\u00EAn","Zelvex Th\u1ECB Gi\u00E1c Th\u00EAn Nhi\u00EAn","Zelvex V\u1EADt Ph\u1EA9m Th\u00EAn Nhi\u00EAn"},
    {
        {999001,"Tr\u00ECnh Bi\u00EAn T\u1EADp \u0110\u1ECBa H\u00ECnh","B\u1EADt tr\u00ECnh bi\u00EAn t\u1EADp \u0111\u1ECBa h\u00ECnh + ch\u1EBF \u0111\u1ED9 1"},
        {19,"Noclip","Xuy\u00EAn t\u01B0\u1EDDng v\u00E0 ch\u01B0\u1EDBng ng\u1EA1i v\u1EADt"},
        {6,"Condition","Ch\u1EBF \u0111\u1ED9 di chuy\u1EC3n (B\u00ECnh th\u01B0\u1EDDng, Bay, Ch\u1EA1y nhanh)"},
        {1337198110,"Kh\u00F4ng S\u00E1t Th\u01B0\u01A1ng R\u01A1i","Ng\u0103n m\u1ECDi s\u00E1t th\u01B0\u01A1ng do r\u01A1i"},
        {1337197986,"\u0102n Nhanh","\u0102n v\u1EADt ph\u1EA9m v\u00E0 tr\u00E1i c\u00E2y ngay l\u1EADp t\u1EE9c"},
        {17,"T\u1ECDa \u0110\u1ED9 X","T\u1ECDa \u0111\u1ED9 X"},
        {15,"T\u1ECDa \u0110\u1ED9 Y","T\u1ECDa \u0111\u1ED9 Y"},
        {16,"T\u1ECDa \u0110\u1ED9 Z","T\u1ECDa \u0111\u1ED9 Z"},
        {999010,"D\u1ECBch Chuy\u1EC3n Ng\u1EABu Nhi\u00EAn","D\u1ECBch chuy\u1EC3n \u0111\u1EBFn v\u1ECB tr\u00ED ng\u1EABu nhi\u00EAn tr\u00EAn b\u1EA3n \u0111\u1ED3"},
        {22,"M\u00E1u","Gi\u00E1 tr\u1ECB m\u00E1u hi\u1EC7n t\u1EA1i"},
        {28,"M\u00E1u T\u1ED1i \u0110a","Gi\u00E1 tr\u1ECB m\u00E1u t\u1ED1i \u0111a"},
        {27,"\u0110\u00F3i","Gi\u00E1 tr\u1ECB \u0111\u00F3i hi\u1EC7n t\u1EA1i"},
        {29,"\u0110\u00F3i T\u1ED1i \u0110a","Gi\u00E1 tr\u1ECB \u0111\u00F3i t\u1ED1i \u0111a"},
        {30,"Th\u1EC3 L\u1EF1c","Gi\u00E1 tr\u1ECB th\u1EC3 l\u1EF1c hi\u1EC7n t\u1EA1i"},
        {33,"Th\u1EC3 L\u1EF1c T\u1ED1i \u0110a","Gi\u00E1 tr\u1ECB th\u1EC3 l\u1EF1c t\u1ED1i \u0111a"},
        {1337198106,"Nh\u1EA3y Tr\u00EAn Kh\u00F4ng","Nh\u1EA3y v\u00F4 h\u1EA1n gi\u1EEFa kh\u00F4ng trung"},
        {1807641586,"B\u01A1i Tr\u00EAn Kh\u00F4ng","B\u01A1i l\u1ED9i tr\u00EAn kh\u00F4ng"},
        {23,"T\u1ED1c \u0110\u1ED9 \u0110i B\u1ED9","H\u1EC7 s\u1ED1 t\u1ED1c \u0111\u1ED9 \u0111i b\u1ED9"},
        {24,"T\u1ED1c \u0110\u1ED9 Ch\u1EA1y","H\u1EC7 s\u1ED1 t\u1ED1c \u0111\u1ED9 ch\u1EA1y"},
        {25,"T\u1ED1c \u0110\u1ED9 Ng\u1ED3i","H\u1EC7 s\u1ED1 t\u1ED1c \u0111\u1ED9 ng\u1ED3i"},
        {26,"T\u1ED1c \u0110\u1ED9 B\u01A1i","H\u1EC7 s\u1ED1 t\u1ED1c \u0111\u1ED9 b\u01A1i"},
        {1337197721,"Auto Click Tr\u00E1i","T\u1EF1 \u0111\u1ED9ng click chu\u1ED9t tr\u00E1i"},
        {1337197722,"Auto Click Ph\u1EA3i","T\u1EF1 \u0111\u1ED9ng click chu\u1ED9t ph\u1EA3i"},
        {1337212286,"\u00C9p Ch\u1EBF \u0110\u1ED9 Xem","\u00C9p m\u1EE5c ti\u00EAu v\u00E0o ch\u1EBF \u0111\u1ED9 ng\u01B0\u1EDDi xem"},
        {1807635788,"T\u1EAFt Ch\u1EBF \u0110\u1ED9 Xem","T\u1EAFt ch\u1EBF \u0111\u1ED9 ng\u01B0\u1EDDi xem c\u1EE7a m\u1EE5c ti\u00EAu"},
    }, 24,
    "Tr\u1EA1ng Th\u00E1i","K\u1EBET N\u1ED0I","Ngo\u1EA1i Tuy\u1EBFn",
    "Zelvex v3 BETA",
    "Tr\u00ECnh Bi\u00EAn T\u1EADp \u0110\u1ECBa H\u00ECnh \u0111ang \u1EDF BETA \u2014 c\u00F3 th\u1EC3 kh\u00F4ng ho\u1EA1t \u0111\u1ED9ng \u0111\u00FAng",
    "T\u00EDnh n\u0103ng V\u1EADt Ph\u1EA9m \u0111ang ph\u00E1t tri\u1EC3n \u2014 c\u00F3 th\u1EC3 ch\u1EE9a l\u1ED7i",
    "Ng\u00F4n Ng\u1EEF",
    "K\u1EBET N\u1ED0I","NG\u1EAET K\u1EBET N\u1ED0I","\u0110\u00E3 k\u1EBFt n\u1ED1i \u0111\u1EBFn","K\u1EBFt n\u1ED1i \u0111\u1EBFn","Kh\u00F4ng t\u00ECm th\u1EA5y ti\u1EBFn tr\u00ECnh",
    "L\u01B0u","T\u00EAn t\u1ECDa \u0111\u1ED9...","X\u00F3a",
    "Kh\u00F4ng t\u00ECm th\u1EA5y game","Handle kh\u00F4ng h\u1EE3p l\u1EC7","K\u1EBFt n\u1ED1i game tr\u01B0\u1EDBc",
    "Th\u1EF1c thi","X\u00F3a","-- Nh\u1EADp m\u00E3 Lua \u1EDF \u0111\u00E2y --",
    "S\u1EB5n s\u00E0ng","\u0110ang th\u1EF1c thi...","Th\u00E0nh c\u00F4ng!","L\u1ED7i","VM ch\u01B0a kh\u1EDFi t\u1EA1o","L\u1ECBch s\u1EED..."
}

}};

inline const LangData& lang(Language l) {
    return LANG_DATA[static_cast<int>(l)];
}

inline QString languageName(Language l) {
    switch (l) {
        case Language::English:    return "English";
        case Language::Italiano:   return "Italiano";
        case Language::Chinese:    return "\u4E2D\u6587";
        case Language::Espanol:    return "Espa\u00F1ol";
        case Language::Russian:    return "\u0420\u0443\u0441\u0441\u043A\u0438\u0439";
        case Language::Turkish:    return "T\u00FCrk\u00E7e";
        case Language::Vietnamese: return "Ti\u1EBFng Vi\u1EC7t";
        default: return "English";
    }
}
