#include <sourcemod>
#define OUTPUT(%1,%2) PrintToServer("OUTPUT: " ... #%1 ... " = " ... %2, %1)

int g_Integer = 42;
float g_Float = 42.42;
char g_String[] = "FortyTwo";

int g_IntArray[20] = { 43, ... };
float g_FloatArray[20] = { 43.43, ... };
char g_StringArray[2][] = { "FortyThree", "FortyFour" };

int g_IntMultiArray[4][4] = { { 44, ... }, { 45, ... }, { 46, ... }, { 47, ... } };

#if SOURCEMOD_V_MINOR >= 10
enum struct SomeStruct {
    int esInteger;
    float esFloat;
    char esString[32];
}
#endif

public void OnPluginStart() {
    RegServerCmd("bp", Command_Bp);
}

public Action Command_Bp(int args) {
    PrintToServer("---- START DEBUG ----");

    int argInteger = 5;
    float argFloat = 6.0;
    char argString[] = "some string";
    int argIntArray[20] = {443, ... };
    float argFloatArray[20] = {443.43, ... };
    char argStringArray[2][] = { "FiftyThree", "FiftyFour" };
#if SOURCEMOD_V_MINOR >= 10
    SomeStruct argEnumStruct = { 66, 66.66, "some enumstruct" };
    BreakHere(argInteger, argFloat, argString, argIntArray, argFloatArray, argStringArray, argEnumStruct);
#else
    BreakHere(argInteger, argFloat, argString, argIntArray, argFloatArray, argStringArray);
#endif
    
    PrintToServer("---- STOP DEBUG ----");
    return Plugin_Handled;
}

#if SOURCEMOD_V_MINOR >= 10
void BreakHere(int argInteger, float argFloat, char[] argString, int[] argIntArray, float[] argFloatArray, char[][] argStringArray, SomeStruct argEnumStruct) {
#else
void BreakHere(int argInteger, float argFloat, char[] argString, int[] argIntArray, float[] argFloatArray, char[][] argStringArray) {
#endif
    int locInteger = 7;
    float locFloat = 8.0;
    char locString[] = "local variable";
    int locIntArray[20] = {51, ... };
    float locFloatArray[20] = {51.51, ... };
    char locStringArray[2][] = { "Hundred", "Twenty" };
    OUTPUT(locInteger, "%d");
    OUTPUT(locFloat, "%f");
    OUTPUT(locString, "%s");
    OUTPUT(locIntArray[5], "%d");
    OUTPUT(locFloatArray[7], "%f");
    OUTPUT(locStringArray[1], "%s");
    OUTPUT(argInteger, "%d");
    OUTPUT(argFloat, "%f");
    OUTPUT(argString, "%s");
    OUTPUT(argIntArray[5], "%d");
    OUTPUT(argFloatArray[7], "%f");
    OUTPUT(argStringArray[1], "%s");
#if SOURCEMOD_V_MINOR >= 10
    OUTPUT(argEnumStruct.esInteger, "%d");
    OUTPUT(argEnumStruct.esFloat, "%f");
    OUTPUT(argEnumStruct.esString, "%s");
#endif
    OUTPUT(g_Integer, "%d");
    OUTPUT(g_Float, "%f");
    OUTPUT(g_String, "%s");
    OUTPUT(g_IntArray[5], "%d");
    OUTPUT(g_FloatArray[7], "%f");
    OUTPUT(g_StringArray[1], "%s");
    OUTPUT(g_IntMultiArray[1][3], "%d");
}
