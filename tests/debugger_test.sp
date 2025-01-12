#include <sourcemod>
#define OUTPUT(%1,%2) PrintToServer("OUTPUT: " ... #%1 ... " = " ... %2, %1)

int g_Integer = 42;
float g_Float = 42.42;
char g_String[] = "FortyTwo";

int g_IntArray[20] = { 43, ... };
float g_FloatArray[20] = { 43.43, ... };
char g_StringArray[2][] = { "FortyThree", "FortyFour" };

public void OnPluginStart() {
    RegServerCmd("bp", Command_Bp);
}

public Action Command_Bp(int args) {
    PrintToServer("---- START DEBUG ----");

    int argInteger = 5;
    float argFloat = 6.0;
    BreakHere(argInteger, argFloat);
    
    PrintToServer("---- STOP DEBUG ----");
    return Plugin_Handled;
}

void BreakHere(int argInteger, float argFloat) {
    OUTPUT(argInteger, "%d");
    OUTPUT(argFloat, "%f");
    OUTPUT(g_Integer, "%d");
    OUTPUT(g_Float, "%f");
    OUTPUT(g_String, "%s");
    OUTPUT(g_IntArray[5], "%d");
    OUTPUT(g_FloatArray[7], "%f");
    OUTPUT(g_StringArray[1], "%s");
}
